import Combine
import CoreBluetooth
import Foundation

/// The watch's own BLE central. This is what makes the app standalone: it talks
/// to the lights directly, with no phone in the loop.
///
/// Connections are only ever opened because you asked for one. The watch has a
/// small ceiling on simultaneous BLE links, so spending them on lights you did
/// not pick means the one you did pick has nowhere to go. Saved lights are
/// listed and ready to tap; they are not dialled on launch.
final class BMCentral: NSObject, ObservableObject {
    /// Everything we can currently offer: saved lights plus whatever the scan
    /// has turned up.
    @Published private(set) var devices: [BMDevice] = []
    @Published private(set) var isScanning = false
    @Published private(set) var bluetoothState: CBManagerState = .unknown
    /// Published separately from the devices themselves: a device's own
    /// `connectionState` is nested state that the list would not see change.
    @Published private(set) var connectedIDs: Set<UUID> = []
    /// Lights we have connected to before and can offer without a scan.
    @Published private(set) var savedIDs: Set<UUID> = []
    /// Set when the watch reports a hard BLE ceiling, so the list can explain
    /// what to free up.
    @Published private(set) var capacityLimit: String?

    private var manager: CBCentralManager!
    private var devicesByID: [UUID: BMDevice] = [:]
    /// Devices the user disconnected on purpose, so a drop is not mistaken for
    /// something to reconnect.
    private var intentionalDisconnects: Set<UUID> = []
    private var scanStopWorkItem: DispatchWorkItem?
    /// A light that drops out mid-use is worth chasing briefly - you asked for
    /// that one. An unconditional retry, though, turns a single refusal into an
    /// endless stream of them.
    private var reconnectAttempts: [UUID: Int] = [:]
    private let maxReconnectAttempts = 3
    /// `connect` never times out on its own - CoreBluetooth keeps the attempt
    /// pending indefinitely. Left alone, a light that is off or out of range
    /// holds its slot forever.
    private var connectTimeouts: [UUID: DispatchWorkItem] = [:]
    private let connectTimeout: TimeInterval = 10

    /// Names of lights connected to before, keyed by peripheral UUID, so the
    /// list reads properly before any of them are in range.
    private var savedNames: [String: String] {
        get { UserDefaults.standard.dictionary(forKey: savedNamesKey) as? [String: String] ?? [:] }
        set {
            UserDefaults.standard.set(newValue, forKey: savedNamesKey)
            savedIDs = Set(newValue.keys.compactMap(UUID.init(uuidString:)))
        }
    }
    private let savedNamesKey = "known-devices"

    /// Scanning is the expensive part of BLE on a watch, so it runs in bursts
    /// rather than continuously.
    private let scanDuration: TimeInterval = 12

    override init() {
        super.init()
        savedIDs = Set(savedNames.keys.compactMap(UUID.init(uuidString:)))
        manager = CBCentralManager(delegate: self, queue: .main)
    }

    var isPoweredOn: Bool { bluetoothState == .poweredOn }
    var connectedDevices: [BMDevice] { devices.filter { connectedIDs.contains($0.id) } }
    func isSaved(_ device: BMDevice) -> Bool { savedIDs.contains(device.id) }

    // MARK: - Scanning

    func startScan() {
        guard isPoweredOn, !isScanning else { return }
        isScanning = true
        manager.scanForPeripherals(withServices: BMProfile.allServices,
                                   options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])

        scanStopWorkItem?.cancel()
        let work = DispatchWorkItem { [weak self] in self?.stopScan() }
        scanStopWorkItem = work
        DispatchQueue.main.asyncAfter(deadline: .now() + scanDuration, execute: work)
    }

    func stopScan() {
        scanStopWorkItem?.cancel()
        scanStopWorkItem = nil
        guard isScanning else { return }
        manager.stopScan()
        isScanning = false
    }

    func rescan() {
        stopScan()
        startScan()
    }

    // MARK: - Connecting

    /// The only path to an open connection. Always the result of a tap.
    func connect(_ device: BMDevice) {
        guard isPoweredOn else { return }
        // Picking a light clears the ceiling: a slot may have just been freed.
        capacityLimit = nil
        reconnectAttempts[device.id] = 0
        intentionalDisconnects.remove(device.id)
        device.setConnectionState(.connecting)
        manager.connect(device.peripheral, options: nil)
        armConnectTimeout(for: device)
    }

    private func armConnectTimeout(for device: BMDevice) {
        connectTimeouts[device.id]?.cancel()
        let work = DispatchWorkItem { [weak self, weak device] in
            guard let self, let device, !self.connectedIDs.contains(device.id) else { return }
            self.connectTimeouts.removeValue(forKey: device.id)
            self.intentionalDisconnects.insert(device.id)
            self.manager.cancelPeripheralConnection(device.peripheral)
            device.setConnectionState(.failed("Did not answer - is it powered on?"))
        }
        connectTimeouts[device.id] = work
        DispatchQueue.main.asyncAfter(deadline: .now() + connectTimeout, execute: work)
    }

    private func clearConnectTimeout(for id: UUID) {
        connectTimeouts.removeValue(forKey: id)?.cancel()
    }

    /// Drop the link but keep the light in the saved list.
    func disconnect(_ device: BMDevice) {
        intentionalDisconnects.insert(device.id)
        clearConnectTimeout(for: device.id)
        manager.cancelPeripheralConnection(device.peripheral)
    }

    /// Drop the link and forget the light entirely. It comes back in Nearby if
    /// it is still advertising.
    func forget(_ device: BMDevice) {
        disconnect(device)
        savedNames.removeValue(forKey: device.id.uuidString)
        devicesByID.removeValue(forKey: device.id)
        devices.removeAll { $0.id == device.id }
        connectedIDs.remove(device.id)
        reconnectAttempts.removeValue(forKey: device.id)
        clearConnectTimeout(for: device.id)
    }

    /// Clear the whole saved list in one go. Anything still in range reappears
    /// under Nearby on the next scan.
    func forgetAllSaved() {
        for device in devices where savedIDs.contains(device.id) {
            forget(device)
        }
        savedNames = [:]
    }

    /// Put the saved lights on screen without dialling any of them.
    private func adoptSavedDevices() {
        let saved = savedNames
        let ids = saved.keys.compactMap(UUID.init(uuidString:))
        guard !ids.isEmpty else { return }
        for peripheral in manager.retrievePeripherals(withIdentifiers: ids) {
            let name = peripheral.name ?? saved[peripheral.identifier.uuidString]
            adopt(peripheral, advertisedName: name, rssi: nil, profile: nil)
        }
    }

    /// Drop a connection and mean it: without marking the disconnect as
    /// intentional, the healing path below would immediately reconnect and we
    /// would spin on a peripheral that cannot serve us.
    private func giveUp(on device: BMDevice, reason: String, forget forgetDevice: Bool) {
        device.setConnectionState(.failed(reason))
        connectedIDs.remove(device.id)
        intentionalDisconnects.insert(device.id)
        clearConnectTimeout(for: device.id)
        if forgetDevice { savedNames.removeValue(forKey: device.id.uuidString) }
        manager.cancelPeripheralConnection(device.peripheral)
    }

    /// Record a failure, and stop reaching for more radio once the watch says no.
    private func noteFailure(_ error: Error?, on device: BMDevice) {
        let message = BMConnectionError.describe(error)
        device.setConnectionState(.failed(message))
        connectedIDs.remove(device.id)
        if BMConnectionError.isCapacityLimit(error) {
            capacityLimit = message
            reconnectAttempts[device.id] = maxReconnectAttempts
        }
    }

    func write(_ data: Data, to device: BMDevice) {
        guard let characteristic = device.writeTarget else { return }
        // The firmware declares the features characteristic write-with-response
        // only (BLERead | BLEWrite in BMBluetoothHandler), so anything sent
        // without a response would be dropped on the floor.
        device.peripheral.writeValue(data, for: characteristic, type: .withResponse)
    }

    // MARK: - Device bookkeeping

    @discardableResult
    private func adopt(_ peripheral: CBPeripheral, advertisedName: String?, rssi: Int?, profile: BMProfile?) -> BMDevice {
        if let existing = devicesByID[peripheral.identifier] {
            existing.update(advertisedName: advertisedName, rssi: rssi, profile: profile)
            return existing
        }
        let device = BMDevice(peripheral: peripheral,
                              advertisedName: advertisedName ?? "Unknown device",
                              rssi: rssi,
                              profile: profile)
        device.central = self
        peripheral.delegate = self
        devicesByID[peripheral.identifier] = device
        devices.append(device)
        return device
    }

    private func device(for peripheral: CBPeripheral) -> BMDevice? {
        devicesByID[peripheral.identifier]
    }
}

// MARK: - CBCentralManagerDelegate

extension BMCentral: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        bluetoothState = central.state
        guard central.state == .poweredOn else {
            isScanning = false
            for device in devices where device.connectionState.isConnected {
                device.setConnectionState(.disconnected)
            }
            connectedIDs.removeAll()
            capacityLimit = nil
            return
        }
        adoptSavedDevices()
        startScan()
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any],
                        rssi RSSI: NSNumber) {
        let services = advertisementData[CBAdvertisementDataServiceUUIDsKey] as? [CBUUID] ?? []
        let profile = services.compactMap(BMProfile.profile(forService:)).first
        let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? peripheral.name
        adopt(peripheral, advertisedName: name, rssi: RSSI.intValue, profile: profile)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        guard let device = device(for: peripheral) else { return }
        device.setConnectionState(.connected)
        connectedIDs.insert(device.id)
        clearConnectTimeout(for: device.id)
        reconnectAttempts[device.id] = 0
        capacityLimit = nil
        // Connecting is what saves a light: it is now one you have chosen.
        savedNames[device.id.uuidString] = device.displayName
        peripheral.delegate = self
        peripheral.discoverServices(BMProfile.allServices)
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        guard let device = device(for: peripheral) else { return }
        clearConnectTimeout(for: device.id)
        noteFailure(error, on: device)
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        guard let device = device(for: peripheral) else { return }
        device.setConnectionState(.disconnected)
        device.bind(features: nil, status: nil)
        connectedIDs.remove(device.id)

        // A drop we did not ask for (out of range, light power-cycled) is worth
        // chasing for a moment, but never past the point of usefulness.
        if !intentionalDisconnects.contains(device.id) {
            if BMConnectionError.isCapacityLimit(error) {
                noteFailure(error, on: device)
            } else {
                let attempts = reconnectAttempts[device.id, default: 0]
                if attempts < maxReconnectAttempts {
                    reconnectAttempts[device.id] = attempts + 1
                    manager.connect(peripheral, options: nil)
                    device.setConnectionState(.connecting)
                    armConnectTimeout(for: device)
                } else {
                    device.setConnectionState(.failed(BMConnectionError.describe(error)))
                }
            }
        }
        intentionalDisconnects.remove(device.id)
    }
}

// MARK: - CBPeripheralDelegate

extension BMCentral: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let device = device(for: peripheral) else { return }
        if let error {
            // A transient failure - keep it saved so a later tap still tries.
            giveUp(on: device, reason: BMConnectionError.describe(error), forget: false)
            return
        }
        guard let service = peripheral.services?.first(where: { BMProfile.profile(forService: $0.uuid) != nil }),
              let profile = BMProfile.profile(forService: service.uuid) else {
            giveUp(on: device, reason: "Not a light", forget: true)
            return
        }
        device.update(advertisedName: nil, rssi: nil, profile: profile)
        peripheral.discoverCharacteristics([profile.features, profile.status].compactMap { $0 },
                                           for: service)
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        guard let device = device(for: peripheral),
              let profile = BMProfile.profile(forService: service.uuid) else { return }

        let characteristics = service.characteristics ?? []
        guard let features = characteristics.first(where: { $0.uuid == profile.features }) else {
            giveUp(on: device, reason: "No control characteristic", forget: true)
            return
        }
        let status = profile.status.flatMap { uuid in characteristics.first { $0.uuid == uuid } }
        device.bind(features: features, status: status)

        guard let status else {
            // No status characteristic at all (the bike): write-only, nothing
            // to subscribe to and nothing to ask for.
            return
        }
        // Subscribe, but do NOT ask for status yet. The firmware drops a status
        // burst on the floor unless the subscription is already registered
        // (`isSubscribed()` in BMBluetoothHandler), and the CCCD write has not
        // landed at this point. The request goes out from
        // didUpdateNotificationStateFor instead.
        peripheral.setNotifyValue(true, for: status)
        peripheral.readValue(for: status)
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let device = device(for: peripheral),
              characteristic.uuid == device.profile?.status,
              error == nil,
              characteristic.isNotifying else { return }

        // Notifications are live now, so it is safe to ask for the first
        // snapshot - the same handshake the phone app does on connect. These
        // devices no longer push status on a timer; they report on connect, on
        // a change made from the app or the encoder, and when asked.
        device.requestStatus()

        // If nothing came back, the burst was lost. One retry covers it without
        // turning into a poll.
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak device] in
            guard let device, !device.hasStatus, device.connectionState.isConnected else { return }
            device.requestStatus()
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        guard let device = device(for: peripheral),
              let data = characteristic.value,
              characteristic.uuid == device.profile?.status else { return }
        device.apply(statusPayload: data)
        if savedNames[device.id.uuidString] != nil {
            savedNames[device.id.uuidString] = device.displayName
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didReadRSSI RSSI: NSNumber, error: Error?) {
        device(for: peripheral)?.update(advertisedName: nil, rssi: RSSI.intValue, profile: nil)
    }
}
