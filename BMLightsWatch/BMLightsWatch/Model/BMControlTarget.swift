import Foundation

/// What the control screens need in order to drive something. A single light and
/// the "All lights" group both satisfy it, so the same UI serves both.
protocol BMControlTarget: ObservableObject {
    var title: String { get }
    var subtitle: String { get }
    /// False while connecting, or when a group has nothing connected: the
    /// controls stay visible but inert.
    var isReady: Bool { get }

    var power: Bool { get }
    var brightness: Int { get }
    var maxBrightness: Int { get }
    var speedPercent: Int { get }
    var reversed: Bool { get }
    var palette: BMPalette { get }
    var effect: BMEffect { get }
    /// What the palette grid shows: the built-in catalog, plus whatever custom
    /// palettes the device is holding in its slots.
    var availablePalettes: [BMPalette] { get }

    func togglePower()
    func setBrightness(_ value: Int)
    func setSpeedPercent(_ value: Int)
    func setPalette(_ palette: BMPalette)
    func setEffect(_ effect: BMEffect)
    func setReversed(_ value: Bool)
    /// Ask the device(s) to push their current state.
    func refresh()

    /// The single light behind this target, if there is one. The details page
    /// uses it for firmware/owner/disconnect, which a group has no answer for.
    var singleDevice: BMDevice? { get }
}

extension BMDevice: BMControlTarget {
    var title: String { displayName }

    var subtitle: String {
        switch connectionState {
        case .connecting: return "Connecting…"
        case .failed(let message): return message
        case .disconnected: return "Tap to connect"
        case .connected:
            guard hasStatus else { return "Connected" }
            // Just the effect: the palette is already right there as the colour
            // strip on the end of the row, and naming it too overruns the width.
            return effect.name
        }
    }

    var isReady: Bool { connectionState.isConnected }

    func refresh() { requestStatus() }

    var singleDevice: BMDevice? { self }
}
