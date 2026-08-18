import Foundation

/// Naming, kept deliberately in step with `resolveDeviceName` in
/// `RNUmbrella/helpers.ts`.
///
/// Devices advertise as `<identifier> - <owner>`: "BMDevice - Cody",
/// "Umbrella-CL". That leading identifier is how a scan recognises our gear, so
/// it stays on the wire — but "BMDevice" is a category, not a name, and it has
/// no business being the thing on screen.
///
/// Real device words (Umbrella, Backpack, Bike…) are descriptive and stay put.
/// Only the generic identifier is dropped.
enum BMNaming {
    private static let genericIdentifiers: Set<String> = ["BMDevice", "BMDevice - New"]
    private static let placeholders: Set<String> = ["", "new", "unknown device", "bmdevice"]

    /// Last four characters of a peripheral id, for telling twins apart.
    static func shortID(_ id: UUID) -> String {
        String(id.uuidString.replacingOccurrences(of: "-", with: "").suffix(4)).uppercased()
    }

    /// The advertised name with the generic identifier removed, or nil when
    /// nothing meaningful is left ("BMDevice", "BMDevice - New").
    static func meaningfulName(from raw: String?) -> String? {
        let name = (raw ?? "").trimmingCharacters(in: .whitespaces)
        guard !name.isEmpty else { return nil }

        let candidate: String
        if let separator = name.firstIndex(of: "-") {
            let identifier = String(name[name.startIndex..<separator]).trimmingCharacters(in: .whitespaces)
            let remainder = String(name[name.index(after: separator)...]).trimmingCharacters(in: .whitespaces)
            candidate = genericIdentifiers.contains(identifier) ? remainder : name
        } else {
            candidate = genericIdentifiers.contains(name) ? "" : name
        }

        guard !placeholders.contains(candidate.lowercased()) else { return nil }
        return candidate
    }

    /// What to show a person, best source first.
    static func resolve(id: UUID, advertised: String?, configured: String?) -> String {
        if let configured = meaningfulName(from: configured) { return configured }
        if let advertised = meaningfulName(from: advertised) { return advertised }
        return "Light \(shortID(id))"
    }
}
