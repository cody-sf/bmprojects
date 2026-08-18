import CoreBluetooth
import Foundation

/// CoreBluetooth's own error text is written for a Mac dialog, not a 40mm
/// screen: "The system has reached the maximum number of connections." loses
/// everything that identifies it once the watch clips it to one line.
///
/// So these get rewritten short, and the numeric CBError code is always kept -
/// an unfamiliar failure is still diagnosable if you can read the number.
enum BMConnectionError {
    static func describe(_ error: Error?) -> String {
        guard let error else { return "Could not connect" }
        let nsError = error as NSError
        guard nsError.domain == CBErrorDomain,
              let code = CBError.Code(rawValue: nsError.code) else {
            return nsError.localizedDescription
        }
        return "\(summary(for: code, fallback: nsError.localizedDescription)) (\(nsError.code))"
    }

    private static func summary(for code: CBError.Code, fallback: String) -> String {
        switch code {
        case .connectionLimitReached:
            // The watch allows far fewer simultaneous BLE links than a phone.
            return "Watch is at its BLE connection limit"
        case .tooManyLEPairedDevices:
            return "Too many paired BLE devices"
        case .leGattExceededBackgroundNotificationLimit:
            return "Too many status subscriptions"
        case .leGattNearBackgroundNotificationLimit:
            return "Near the status subscription limit"
        case .connectionTimeout:
            return "Connection timed out"
        case .peripheralDisconnected:
            return "Light dropped the connection"
        case .connectionFailed:
            return "Connection failed"
        case .unknownDevice:
            return "Watch no longer knows this light"
        case .peerRemovedPairingInformation:
            return "Light forgot this watch"
        case .encryptionTimedOut:
            return "Pairing timed out"
        case .operationNotSupported:
            return "Not supported"
        default:
            return fallback
        }
    }

    /// True for the failures that mean "you are at a hard ceiling" - retrying
    /// those just reproduces the error and burns battery.
    static func isCapacityLimit(_ error: Error?) -> Bool {
        guard let error else { return false }
        let nsError = error as NSError
        guard nsError.domain == CBErrorDomain,
              let code = CBError.Code(rawValue: nsError.code) else { return false }
        switch code {
        case .connectionLimitReached, .tooManyLEPairedDevices,
             .leGattExceededBackgroundNotificationLimit:
            return true
        default:
            return false
        }
    }
}
