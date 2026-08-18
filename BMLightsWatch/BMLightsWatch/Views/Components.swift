import SwiftUI
import WatchKit

/// Horizontal preview of a palette, used for the swatch grid and the row that
/// shows what is currently playing.
struct PaletteStrip: View {
    let palette: BMPalette
    var cornerRadius: CGFloat = 8

    var body: some View {
        RoundedRectangle(cornerRadius: cornerRadius, style: .continuous)
            .fill(LinearGradient(colors: palette.colors,
                                 startPoint: .leading,
                                 endPoint: .trailing))
    }
}

/// The ring that wraps the power button. Its fill shows the value, its colors
/// show the palette that is playing.
struct ValueRing: View {
    let fraction: Double
    let colors: [Color]
    var lineWidth: CGFloat = 9

    var body: some View {
        ZStack {
            Circle()
                .stroke(Color.white.opacity(0.12), lineWidth: lineWidth)
            Circle()
                .trim(from: 0, to: max(0.001, min(1, fraction)))
                .stroke(
                    AngularGradient(colors: colors + [colors.first ?? .accentColor],
                                    center: .center),
                    style: StrokeStyle(lineWidth: lineWidth, lineCap: .round)
                )
                .rotationEffect(.degrees(-90))
                .animation(.easeOut(duration: 0.15), value: fraction)
        }
    }
}

/// Small colored dot for connection state.
struct StatusDot: View {
    let state: BMDevice.ConnectionState

    private var color: Color {
        switch state {
        case .connected: return .green
        case .connecting: return .yellow
        case .failed: return .red
        case .disconnected: return .secondary
        }
    }

    var body: some View {
        Circle()
            .fill(color)
            .frame(width: 8, height: 8)
    }
}

/// Binds the Digital Crown to a value and takes focus when the page appears, so
/// the crown is live the moment you land on the screen.
struct CrownControl: ViewModifier {
    @Binding var value: Double
    var range: ClosedRange<Double> = 1...100
    @FocusState private var focused: Bool

    func body(content: Content) -> some View {
        content
            .focusable(true)
            .focused($focused)
            .digitalCrownRotation($value,
                                  from: range.lowerBound,
                                  through: range.upperBound,
                                  by: 1,
                                  sensitivity: .medium,
                                  isContinuous: false,
                                  isHapticFeedbackEnabled: true)
            .onAppear { focused = true }
    }
}

extension View {
    func crownControl(_ value: Binding<Double>, range: ClosedRange<Double> = 1...100) -> some View {
        modifier(CrownControl(value: value, range: range))
    }
}

enum Haptics {
    static func tap() { WKInterfaceDevice.current().play(.click) }
    static func confirm() { WKInterfaceDevice.current().play(.success) }
}
