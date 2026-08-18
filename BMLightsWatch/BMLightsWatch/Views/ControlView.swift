import SwiftUI

/// The control surface for one light, or for every connected light at once.
///
/// Four swipeable pages, ordered by how often you reach for them. Each page owns
/// the Digital Crown for its own value, which is why they are pages rather than
/// rows in a scrolling list - the crown can only do one job at a time.
struct ControlView<Target: BMControlTarget>: View {
    @ObservedObject var target: Target

    var body: some View {
        TabView {
            PowerPage(target: target)
            PalettePage(target: target)
            EffectPage(target: target)
            MotionPage(target: target)
        }
        .tabViewStyle(.page)
        .navigationTitle(target.title)
        .navigationBarTitleDisplayMode(.inline)
        .onAppear { target.refresh() }
    }
}

/// Page 1 - power and brightness. The crown drives brightness; the ring shows it
/// in the colors of whatever palette is playing.
struct PowerPage<Target: BMControlTarget>: View {
    @ObservedObject var target: Target
    @State private var crown: Double = 50

    var body: some View {
        VStack(spacing: 6) {
            ZStack {
                ValueRing(fraction: Double(target.brightness) / 100.0,
                          colors: target.palette.colors)

                VStack(spacing: 2) {
                    Button(action: togglePower) {
                        Image(systemName: "power")
                            .font(.system(size: 30, weight: .semibold))
                            .foregroundStyle(target.power ? Color.accentColor : Color.secondary)
                    }
                    .buttonStyle(.plain)
                    .disabled(!target.isReady)

                    Text("\(target.brightness)%")
                        .font(.system(.footnote, design: .rounded).weight(.semibold))
                        .foregroundStyle(.secondary)
                        .monospacedDigit()
                }
            }
            .padding(6)
            .crownControl($crown, range: 1...Double(max(1, target.maxBrightness)))

            Text(target.subtitle)
                .font(.caption2)
                .foregroundStyle(.secondary)
                .lineLimit(target.isReady ? 1 : 3)
                .minimumScaleFactor(0.8)
                .multilineTextAlignment(.center)
                .fixedSize(horizontal: false, vertical: true)
        }
        .padding(.horizontal, 4)
        .onAppear { crown = Double(target.brightness) }
        .onChange(of: crown) { _, newValue in
            let value = Int(newValue.rounded())
            guard value != target.brightness else { return }
            target.setBrightness(value)
        }
        .onChange(of: target.brightness) { _, newValue in
            guard Int(crown.rounded()) != newValue else { return }
            crown = Double(newValue)
        }
    }

    private func togglePower() {
        Haptics.tap()
        target.togglePower()
    }
}

/// Page 2 - the palette grid.
struct PalettePage<Target: BMControlTarget>: View {
    @ObservedObject var target: Target

    private let columns = [GridItem(.flexible(), spacing: 6), GridItem(.flexible(), spacing: 6)]

    var body: some View {
        ScrollView {
            LazyVGrid(columns: columns, spacing: 6) {
                ForEach(BMCatalog.palettes) { palette in
                    Button {
                        Haptics.tap()
                        target.setPalette(palette)
                    } label: {
                        PaletteSwatch(palette: palette, isSelected: palette.id == target.palette.id)
                    }
                    .buttonStyle(.plain)
                    .disabled(!target.isReady)
                }
            }
            .padding(.horizontal, 4)
            .padding(.bottom, 8)
        }
        .navigationTitle("Palette")
    }
}

private struct PaletteSwatch: View {
    let palette: BMPalette
    let isSelected: Bool

    var body: some View {
        VStack(spacing: 3) {
            ZStack {
                PaletteStrip(palette: palette)
                if isSelected {
                    Image(systemName: "checkmark.circle.fill")
                        .font(.system(size: 16, weight: .bold))
                        .foregroundStyle(.white)
                        .shadow(radius: 2)
                }
            }
            .frame(height: 34)
            .overlay(
                RoundedRectangle(cornerRadius: 8, style: .continuous)
                    .strokeBorder(isSelected ? Color.white : Color.clear, lineWidth: 2)
            )

            Text(palette.name)
                .font(.system(size: 10))
                .foregroundStyle(isSelected ? .primary : .secondary)
                .lineLimit(1)
                .minimumScaleFactor(0.7)
        }
    }
}

/// Page 3 - the effect list.
struct EffectPage<Target: BMControlTarget>: View {
    @ObservedObject var target: Target

    var body: some View {
        List {
            ForEach(BMCatalog.effects) { effect in
                Button {
                    Haptics.tap()
                    target.setEffect(effect)
                } label: {
                    HStack {
                        Text(effect.name)
                            .lineLimit(1)
                            .minimumScaleFactor(0.8)
                        Spacer()
                        if effect.id == target.effect.id {
                            Image(systemName: "checkmark")
                                .foregroundStyle(Color.accentColor)
                        }
                    }
                }
                .disabled(!target.isReady)
            }
        }
        .navigationTitle("Effect")
    }
}

/// Page 4 - speed on the crown, plus the direction the animation runs.
struct MotionPage<Target: BMControlTarget>: View {
    @ObservedObject var target: Target
    @State private var crown: Double = 50

    var body: some View {
        VStack(spacing: 8) {
            ZStack {
                ValueRing(fraction: Double(target.speedPercent) / 100.0,
                          colors: target.palette.colors)

                VStack(spacing: 0) {
                    Image(systemName: "hare.fill")
                        .font(.system(size: 18))
                        .foregroundStyle(.secondary)
                    Text("\(target.speedPercent)%")
                        .font(.system(.body, design: .rounded).weight(.semibold))
                        .monospacedDigit()
                }
            }
            .padding(6)
            .crownControl($crown)

            Button {
                Haptics.tap()
                target.setReversed(!target.reversed)
            } label: {
                Label(target.reversed ? "Reversed" : "Forward",
                      systemImage: target.reversed ? "arrow.left" : "arrow.right")
                    .font(.footnote)
            }
            .buttonStyle(.bordered)
            .disabled(!target.isReady)
        }
        .padding(.horizontal, 4)
        .navigationTitle("Speed")
        .onAppear { crown = Double(target.speedPercent) }
        .onChange(of: crown) { _, newValue in
            let value = Int(newValue.rounded())
            guard value != target.speedPercent else { return }
            target.setSpeedPercent(value)
        }
        .onChange(of: target.speedPercent) { _, newValue in
            guard Int(crown.rounded()) != newValue else { return }
            crown = Double(newValue)
        }
    }
}
