#if targetEnvironment(simulator)
import Foundation

/// The simulator has no Bluetooth radio, so there is nothing to connect to and
/// no way to see the control screens. This stand-in drives them with local state
/// instead, which is enough to work on layout without putting a light on the
/// bench. It does not exist in a device build.
final class BMDemoTarget: ObservableObject, BMControlTarget {
    @Published private(set) var power = true
    @Published private(set) var brightness = 72
    @Published private(set) var speedPercent = 55
    @Published private(set) var reversed = false
    @Published private(set) var paletteId = "cosmicwaves"
    @Published private(set) var effectId = "fire_plasma"

    let title = "Demo Light"
    let subtitle = "Simulator · no radio"
    let isReady = true
    let maxBrightness = 100
    var palette: BMPalette { BMCatalog.palette(id: paletteId) ?? BMCatalog.palettes[0] }
    var effect: BMEffect { BMCatalog.effect(id: effectId) ?? BMCatalog.effects[0] }
    var singleDevice: BMDevice? { nil }

    func togglePower() { power.toggle() }
    func setBrightness(_ value: Int) { brightness = value.clamped(to: 1...100) }
    func setSpeedPercent(_ value: Int) { speedPercent = value.clamped(to: 1...100) }
    func setPalette(_ palette: BMPalette) { paletteId = palette.id }
    func setEffect(_ effect: BMEffect) { effectId = effect.id }
    func setReversed(_ value: Bool) { reversed = value }
    func refresh() {}
}
#endif
