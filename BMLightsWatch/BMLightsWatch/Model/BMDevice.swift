import Combine
import CoreBluetooth
import Foundation

/// A single discovered light: its connection state, the last status the firmware
/// pushed, and the controls that write back to it.
///
/// Local state is updated optimistically so the UI tracks the crown instantly;
/// the device's own status notifications overwrite it a moment later.
final class BMDevice: NSObject, ObservableObject, Identifiable {
    enum ConnectionState: Equatable {
        case disconnected
        case connecting
        case connected
        case failed(String)

        var isBusy: Bool { self == .connecting }
        var isConnected: Bool { self == .connected }

        var isFailed: Bool {
            if case .failed = self { return true }
            return false
        }
    }

    let id: UUID
    let peripheral: CBPeripheral

    @Published private(set) var advertisedName: String
    @Published private(set) var rssi: Int?
    @Published private(set) var connectionState: ConnectionState = .disconnected
    @Published private(set) var profile: BMProfile?
    /// True once the firmware has pushed at least one status payload.
    @Published private(set) var hasStatus = false

    // Live device state.
    @Published private(set) var power = false
    @Published private(set) var brightness = 50
    @Published private(set) var speedPercent = 50
    @Published private(set) var reversed = false
    @Published private(set) var paletteId = "cool"
    @Published private(set) var effectId = "palette_stream"
    @Published private(set) var maxBrightness = 100
    @Published private(set) var owner: String?
    @Published private(set) var firmwareVersion: String?
    @Published private(set) var configuredName: String?

    weak var central: BMCentral?

    private var featuresCharacteristic: CBCharacteristic?
    private var statusCharacteristic: CBCharacteristic?

    /// Coalescing for controls that fire continuously (crown, sliders): at most
    /// one write per `writeInterval` per feature, plus a trailing write so the
    /// value the user landed on always reaches the device.
    private var pendingWrites: [UInt8: BMCommand] = [:]
    private var lastWriteAt: [UInt8: Date] = [:]
    private var flushScheduled = false
    private let writeInterval: TimeInterval = 0.12

    init(peripheral: CBPeripheral, advertisedName: String, rssi: Int?, profile: BMProfile?) {
        self.id = peripheral.identifier
        self.peripheral = peripheral
        self.advertisedName = advertisedName
        self.rssi = rssi
        self.profile = profile
        super.init()
    }

    /// What to show in the list. The firmware's own `deviceName` is still the
    /// factory "BMDevice" on every generic device - nothing ever sets it - so it
    /// only counts when it says something, and the advertised name gets the
    /// generic identifier stripped off it.
    var displayName: String {
        BMNaming.resolve(id: id, advertised: advertisedName, configured: configuredName)
    }

    var palette: BMPalette { BMCatalog.palette(id: paletteId) ?? BMCatalog.palettes[0] }
    var effect: BMEffect { BMCatalog.effect(id: effectId) ?? BMCatalog.effects[0] }
    /// The bike has no status characteristic, so its state is write-only.
    var reportsStatus: Bool { profile?.status != nil }

    // MARK: - Controls

    func setPower(_ on: Bool) {
        power = on
        write(.power(on))
    }

    func togglePower() { setPower(!power) }

    func setBrightness(_ value: Int) {
        let clamped = value.clamped(to: 1...max(1, maxBrightness))
        brightness = clamped
        write(.brightness(clamped), coalesce: true)
    }

    func setSpeedPercent(_ value: Int) {
        let clamped = value.clamped(to: 1...100)
        speedPercent = clamped
        write(.speed(BMSpeed.raw(fromPercent: clamped)), coalesce: true)
    }

    func setPalette(_ palette: BMPalette) {
        paletteId = palette.id
        write(.palette(palette.id))
    }

    func setEffect(_ effect: BMEffect) {
        effectId = effect.id
        write(.effect(effect.id))
    }

    func setReversed(_ reversed: Bool) {
        self.reversed = reversed
        write(.direction(reversed))
    }

    func requestStatus() {
        guard reportsStatus else { return }
        write(.requestStatus)
    }

    // MARK: - Writes

    private func write(_ command: BMCommand, coalesce: Bool = false) {
        guard connectionState.isConnected else { return }
        guard coalesce else {
            pendingWrites[command.feature.rawValue] = nil
            send(command)
            return
        }

        let key = command.feature.rawValue
        let elapsed = Date().timeIntervalSince(lastWriteAt[key] ?? .distantPast)
        if elapsed >= writeInterval {
            send(command)
        } else {
            pendingWrites[key] = command
            scheduleFlush(after: writeInterval - elapsed)
        }
    }

    private func send(_ command: BMCommand) {
        lastWriteAt[command.feature.rawValue] = Date()
        central?.write(command.data, to: self)
    }

    private func scheduleFlush(after delay: TimeInterval) {
        guard !flushScheduled else { return }
        flushScheduled = true
        DispatchQueue.main.asyncAfter(deadline: .now() + delay) { [weak self] in
            guard let self else { return }
            self.flushScheduled = false
            let due = self.pendingWrites
            self.pendingWrites.removeAll()
            for command in due.values { self.send(command) }
        }
    }

    // MARK: - Central plumbing

    func update(advertisedName: String?, rssi: Int?, profile: BMProfile?) {
        if let advertisedName, !advertisedName.isEmpty { self.advertisedName = advertisedName }
        if let rssi { self.rssi = rssi }
        if let profile { self.profile = profile }
    }

    func setConnectionState(_ state: ConnectionState) {
        connectionState = state
        if state == .disconnected || state.isBusy {
            featuresCharacteristic = nil
            statusCharacteristic = nil
        }
    }

    func bind(features: CBCharacteristic?, status: CBCharacteristic?) {
        featuresCharacteristic = features
        statusCharacteristic = status
    }

    var writeTarget: CBCharacteristic? { featuresCharacteristic }

    /// Merge one status notification. The firmware sends state in several small
    /// JSON chunks, each a complete object holding a subset of the keys, so only
    /// the keys actually present may be applied.
    func apply(statusPayload data: Data) {
        guard let text = String(data: data, encoding: .utf8) else { return }
        let trimmed = text.trimmingCharacters(in: .whitespacesAndNewlines)
        guard let json = try? JSONSerialization.jsonObject(with: Data(trimmed.utf8)),
              let object = json as? [String: Any] else { return }

        if let value = object["pwr"] as? Bool { power = value }
        if let value = object["bri"] as? Int { brightness = value.clamped(to: 1...100) }
        if let value = object["spd"] as? Int { speedPercent = BMSpeed.percent(fromRaw: value) }
        if let value = object["dir"] as? Bool { reversed = value }
        if let value = object["fx"] as? String, let effect = BMCatalog.effect(id: value) {
            effectId = effect.id
        }
        if let value = object["pal"] as? String, let palette = BMCatalog.palette(id: value) {
            paletteId = palette.id
        }
        if let value = object["maxBri"] as? Int { maxBrightness = value.clamped(to: 1...100) }
        if let value = object["owner"] as? String, !value.isEmpty { owner = value }
        if let value = object["deviceName"] as? String, !value.isEmpty { configuredName = value }
        if let value = object["fwVer"] as? String, !value.isEmpty { firmwareVersion = value }

        hasStatus = true
    }
}
