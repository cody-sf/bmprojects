import SwiftUI

@main
struct BMLightsWatchApp: App {
    @StateObject private var central = BMCentral()
    @Environment(\.scenePhase) private var scenePhase

    var body: some Scene {
        WindowGroup {
            DeviceListView(central: central)
                .environmentObject(central)
        }
        // Scanning is the expensive part of BLE, so it only runs while the app
        // is actually on screen. Connections themselves survive the wrist drop.
        .onChange(of: scenePhase) { _, phase in
            switch phase {
            case .active: central.rescan()
            case .inactive, .background: central.stopScan()
            @unknown default: break
            }
        }
    }
}
