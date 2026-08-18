import CoreBluetooth
import SwiftUI

/// Root screen: what is connected, what is saved, what is nearby.
///
/// Tapping is the only thing that opens a connection. Saved lights sit here
/// ready to use but idle, because the watch has few BLE slots and they belong to
/// whatever you actually picked.
struct DeviceListView: View {
    @EnvironmentObject private var central: BMCentral
    @StateObject private var group: BMGroup
    @State private var route: Route?
    @State private var confirmForgetAll = false
#if targetEnvironment(simulator)
    @StateObject private var demo = BMDemoTarget()
#endif

    init(central: BMCentral) {
        _group = StateObject(wrappedValue: BMGroup(central: central))
    }

    private enum Route: Hashable {
        case device(UUID)
        case allLights
        case demo
    }

    var body: some View {
        NavigationStack {
            List {
                if let capacityLimit = central.capacityLimit {
                    capacityBanner(capacityLimit)
                }

                if central.isPoweredOn {
                    if central.connectedDevices.count > 1 {
                        Button {
                            group.seedFromDevices()
                            route = .allLights
                        } label: {
                            allLightsRow
                        }
                    }

                    section("Connected", devices: connected)
                    section("Saved", devices: saved)
                    section("Nearby", devices: nearby)

                    scanRow

                    if !central.savedIDs.isEmpty {
                        Button("Remove All Saved", role: .destructive) {
                            confirmForgetAll = true
                        }
                        .font(.footnote)
                    }
                } else {
                    bluetoothStateRow
                }
#if targetEnvironment(simulator)
                Button("Demo Light") { route = .demo }
#endif
            }
            .navigationTitle("Lights")
            .confirmationDialog("Remove all saved lights?",
                                isPresented: $confirmForgetAll,
                                titleVisibility: .visible) {
                Button("Remove All", role: .destructive) { central.forgetAllSaved() }
                Button("Cancel", role: .cancel) {}
            }
            .navigationDestination(item: $route) { route in
                switch route {
                case .allLights:
                    ControlView(target: group)
                case .device(let id):
                    if let device = central.devices.first(where: { $0.id == id }) {
                        ControlView(target: device)
                    } else {
                        Text("Light went away")
                    }
                case .demo:
#if targetEnvironment(simulator)
                    ControlView(target: demo)
#else
                    EmptyView()
#endif
                }
            }
        }
    }

    // MARK: - Groupings

    private var connected: [BMDevice] {
        central.devices.filter { central.connectedIDs.contains($0.id) }.sorted(by: byName)
    }

    private var saved: [BMDevice] {
        central.devices
            .filter { central.savedIDs.contains($0.id) && !central.connectedIDs.contains($0.id) }
            .sorted(by: byName)
    }

    private var nearby: [BMDevice] {
        central.devices
            .filter { !central.savedIDs.contains($0.id) && !central.connectedIDs.contains($0.id) }
            .sorted(by: byName)
    }

    private func byName(_ lhs: BMDevice, _ rhs: BMDevice) -> Bool {
        lhs.displayName.localizedCaseInsensitiveCompare(rhs.displayName) == .orderedAscending
    }

    @ViewBuilder
    private func section(_ title: String, devices: [BMDevice]) -> some View {
        if !devices.isEmpty {
            Section(title) {
                ForEach(devices) { device in
                    DeviceRow(device: device) { select(device) }
                        .swipeActions(edge: .trailing, allowsFullSwipe: true) {
                            Button(role: .destructive) {
                                Haptics.tap()
                                central.forget(device)
                            } label: {
                                Label("Remove", systemImage: "trash")
                            }

                            if central.connectedIDs.contains(device.id) {
                                Button {
                                    Haptics.tap()
                                    central.disconnect(device)
                                } label: {
                                    Label("Disconnect", systemImage: "bolt.slash")
                                }
                            }
                        }
                }
            }
        }
    }

    private func select(_ device: BMDevice) {
        if central.connectedIDs.contains(device.id) {
            route = .device(device.id)
        } else {
            Haptics.tap()
            central.connect(device)
        }
    }

    // MARK: - Rows

    private func capacityBanner(_ message: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(message)
                .font(.caption2)
                .foregroundStyle(.red)
                .fixedSize(horizontal: false, vertical: true)
            Text("Swipe a light you are not using and disconnect it, then tap the one you want.")
                .font(.caption2)
                .foregroundStyle(.secondary)
                .fixedSize(horizontal: false, vertical: true)
        }
    }

    private var allLightsRow: some View {
        HStack(spacing: 8) {
            Image(systemName: "sparkles")
                .foregroundStyle(Color.accentColor)
            VStack(alignment: .leading, spacing: 1) {
                Text("All Lights").font(.headline)
                Text(group.subtitle)
                    .font(.caption2)
                    .foregroundStyle(.secondary)
            }
        }
    }

    @ViewBuilder
    private var scanRow: some View {
        if central.isScanning {
            HStack(spacing: 6) {
                ProgressView()
                Text("Scanning…")
                    .font(.footnote)
                    .foregroundStyle(.secondary)
            }
        } else {
            Button {
                central.rescan()
            } label: {
                Label(central.devices.isEmpty ? "Scan for lights" : "Scan again",
                      systemImage: "arrow.clockwise")
                    .font(.footnote)
            }
        }
    }

    private var bluetoothStateRow: some View {
        Text(bluetoothMessage)
            .font(.footnote)
            .foregroundStyle(.secondary)
            .fixedSize(horizontal: false, vertical: true)
    }

    private var bluetoothMessage: String {
        switch central.bluetoothState {
        case .poweredOff: return "Bluetooth is off. Turn it on in Settings to reach your lights."
        case .unauthorized: return "BM Lights needs Bluetooth access. Enable it in Settings › Privacy."
        case .unsupported: return "This watch cannot use Bluetooth LE."
        case .resetting: return "Bluetooth is restarting…"
        default: return "Starting Bluetooth…"
        }
    }
}

private struct DeviceRow: View {
    @ObservedObject var device: BMDevice
    let action: () -> Void

    var body: some View {
        Button(action: action) {
            HStack(spacing: 8) {
                StatusDot(state: device.connectionState)

                VStack(alignment: .leading, spacing: 1) {
                    Text(device.displayName)
                        .font(.headline)
                        .lineLimit(1)
                    Text(device.subtitle)
                        .font(.caption2)
                        .foregroundStyle(device.connectionState.isFailed ? Color.red : Color.secondary)
                        .lineLimit(device.connectionState.isFailed ? 4 : 1)
                        .minimumScaleFactor(0.8)
                        .fixedSize(horizontal: false, vertical: true)
                }

                Spacer(minLength: 0)

                if device.connectionState.isConnected {
                    PaletteStrip(palette: device.palette, cornerRadius: 3)
                        .frame(width: 14, height: 22)
                }
            }
        }
    }
}
