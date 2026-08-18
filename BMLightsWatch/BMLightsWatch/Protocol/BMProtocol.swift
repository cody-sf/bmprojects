import CoreBluetooth
import Foundation

/// One device family: the service it advertises plus its two characteristics.
///
/// Mirrors `DEVICE_UUIDS` in `RNUmbrella/constants.ts`. Only the light-carrying
/// families are here - the boofer (flamethrower) and the stoplight are
/// deliberately left off the wrist.
struct BMProfile: Identifiable, Hashable {
    let id: String
    let label: String
    let service: CBUUID
    let features: CBUUID
    /// Nil for families whose firmware has no status characteristic (the bike),
    /// where we can write but never read state back.
    let status: CBUUID?

    static let bmDevice = BMProfile(
        id: "BMDevice",
        label: "BM Device",
        service: CBUUID(string: "4746ABE4-2135-4A84-8F2F-F47F3A73E73B"),
        features: CBUUID(string: "3927E9DB-012B-4DB9-8890-984FE28FAF83"),
        status: CBUUID(string: "C054C450-CF93-4E6F-848E-2C521E739F4B")
    )

    static let backpack = BMProfile(
        id: "backpack",
        label: "Backpack",
        service: CBUUID(string: "BE03096F-9322-4360-BC84-0F977C5C3C10"),
        features: CBUUID(string: "24DCB43C-D457-4DE0-A968-9CDC9D60392C"),
        status: CBUUID(string: "71A0CB09-7998-4774-83B5-1A5F02F205FB")
    )

    static let umbrella = BMProfile(
        id: "umbrella",
        label: "Umbrella",
        service: CBUUID(string: "87748ABC-E491-43A1-92BD-20B94BA20DF4"),
        features: CBUUID(string: "E06544BC-1989-4C0B-9ADA-8CD4491D23A5"),
        status: CBUUID(string: "0B95CC9E-288E-49D2-A2AA-7230ED489EC8")
    )

    static let bike = BMProfile(
        id: "bike",
        label: "Bike",
        service: CBUUID(string: "E75D21D2-2482-4BA2-BED5-C59BC8D7AA2C"),
        features: CBUUID(string: "E2D56095-0DA0-45F3-9494-2369305334C1"),
        status: nil
    )

    static let all: [BMProfile] = [.bmDevice, .backpack, .umbrella, .bike]

    static let allServices: [CBUUID] = all.map(\.service)

    static func profile(forService uuid: CBUUID) -> BMProfile? {
        all.first { $0.service == uuid }
    }
}

/// Feature codes written to the `features` characteristic.
/// Mirrors `COMMON_DEVICE_COMMANDS` in `RNUmbrella/constants.ts`.
enum BMFeature: UInt8 {
    case power = 0x01
    case requestStatus = 0x02
    case brightness = 0x04
    case speed = 0x05
    case direction = 0x06
    case palette = 0x08
    case effect = 0x0A
}

/// A single write to the `features` characteristic: one command byte followed by
/// a payload whose shape depends on the command.
enum BMCommand {
    case power(Bool)
    case requestStatus
    /// 1...100, scaled to 1...255 on the device.
    case brightness(Int)
    /// Raw firmware speed, 5 (fastest) ... 200 (slowest).
    case speed(Int)
    case direction(Bool)
    case palette(String)
    case effect(String)

    var feature: BMFeature {
        switch self {
        case .power: return .power
        case .requestStatus: return .requestStatus
        case .brightness: return .brightness
        case .speed: return .speed
        case .direction: return .direction
        case .palette: return .palette
        case .effect: return .effect
        }
    }

    var data: Data {
        var data = Data([feature.rawValue])
        switch self {
        case .power(let on), .direction(let on):
            data.append(on ? 0x01 : 0x00)
        case .requestStatus:
            break
        case .brightness(let value), .speed(let value):
            withUnsafeBytes(of: Int32(value).littleEndian) { data.append(contentsOf: $0) }
        case .palette(let name), .effect(let name):
            // The firmware reads a 2-byte write as a numeric id, so a one-character
            // name would land on the wrong branch. Every name we ship is longer.
            data.append(contentsOf: Array(name.utf8))
        }
        return data
    }
}

/// The phone app shows speed as 1-100 (higher = faster) while the firmware wants
/// a frame duration in 5...200 (lower = faster). Same mapping as
/// `handleSpeedChange` in `pages/BMDevice/BMDevice.tsx`.
enum BMSpeed {
    static let rawMin = 5
    static let rawMax = 200

    static func raw(fromPercent percent: Int) -> Int {
        let inverse = Double(100 - percent.clamped(to: 1...100))
        let raw = inverse * Double(rawMax - rawMin) / 100.0 + Double(rawMin)
        return Int(raw.rounded()).clamped(to: rawMin...rawMax)
    }

    static func percent(fromRaw raw: Int) -> Int {
        let clamped = Double(raw.clamped(to: rawMin...rawMax))
        let percent = 100.0 - (clamped - Double(rawMin)) * 100.0 / Double(rawMax - rawMin)
        return Int(percent.rounded()).clamped(to: 1...100)
    }
}

extension Comparable {
    func clamped(to range: ClosedRange<Self>) -> Self {
        min(max(self, range.lowerBound), range.upperBound)
    }
}
