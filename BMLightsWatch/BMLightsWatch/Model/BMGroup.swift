import Combine
import Foundation

/// "All lights": one set of controls that fans every command out to every
/// connected device. Handy when the whole camp's kit is on one wrist.
///
/// It keeps its own state rather than mirroring the devices, because the devices
/// may disagree - the group's job is to force them into agreement.
final class BMGroup: ObservableObject, BMControlTarget {
    @Published private(set) var power = true
    @Published private(set) var brightness = 50
    @Published private(set) var speedPercent = 50
    @Published private(set) var reversed = false
    @Published private(set) var paletteId = "cool"
    @Published private(set) var effectId = "palette_stream"

    private unowned let central: BMCentral

    init(central: BMCentral) {
        self.central = central
    }

    private var targets: [BMDevice] { central.connectedDevices }

    var title: String { "All Lights" }

    var subtitle: String {
        let count = targets.count
        return count == 1 ? "1 light connected" : "\(count) lights connected"
    }

    var isReady: Bool { !targets.isEmpty }
    var maxBrightness: Int { targets.map(\.maxBrightness).min() ?? 100 }
    var palette: BMPalette { BMCatalog.palette(id: paletteId) ?? BMCatalog.palettes[0] }
    var effect: BMEffect { BMCatalog.effect(id: effectId) ?? BMCatalog.effects[0] }
    var singleDevice: BMDevice? { nil }

    /// Adopt the state of whichever light has reported one, so opening the group
    /// does not snap everything to arbitrary defaults.
    func seedFromDevices() {
        guard let reference = targets.first(where: { $0.hasStatus }) ?? targets.first else { return }
        power = reference.power
        brightness = reference.brightness
        speedPercent = reference.speedPercent
        reversed = reference.reversed
        paletteId = reference.paletteId
        effectId = reference.effectId
    }

    func togglePower() {
        power.toggle()
        let value = power
        targets.forEach { $0.setPower(value) }
    }

    func setBrightness(_ value: Int) {
        brightness = value.clamped(to: 1...max(1, maxBrightness))
        targets.forEach { $0.setBrightness(brightness) }
    }

    func setSpeedPercent(_ value: Int) {
        speedPercent = value.clamped(to: 1...100)
        targets.forEach { $0.setSpeedPercent(speedPercent) }
    }

    func setPalette(_ palette: BMPalette) {
        paletteId = palette.id
        targets.forEach { $0.setPalette(palette) }
    }

    func setEffect(_ effect: BMEffect) {
        effectId = effect.id
        targets.forEach { $0.setEffect(effect) }
    }

    func setReversed(_ value: Bool) {
        reversed = value
        targets.forEach { $0.setReversed(value) }
    }

    func refresh() {
        targets.forEach { $0.requestStatus() }
    }
}
