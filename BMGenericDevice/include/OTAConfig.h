/**
 * OTA Configuration - Edit this file to enable HTTPS OTA updates
 *
 * Set OTA_ENABLED to 1 and fill in your WiFi credentials and firmware URL.
 * WARNING: Do not commit real WiFi credentials to git. Consider adding
 * OTAConfig.h to .gitignore or using build_flags for sensitive values.
 * Host your firmware at the URL (e.g. GitHub Releases, S3, your own server).
 *
 * Firmware URL examples:
 *   GitHub Releases: https://github.com/user/repo/releases/download/v1.0.0/firmware.bin
 *   Raw HTTP (dev):  http://your-server.local/firmware.bin
 */

#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

// Set to 1 to enable OTA updates (requires WiFi)
#define OTA_ENABLED 1

#if OTA_ENABLED

// Build-time WiFi credentials - a fallback so a freshly flashed board can reach
// the network without being provisioned in the app first. The app (BLE) still
// overrides these. Real credentials live in a gitignored WiFiCreds.h so they
// never get committed or baked into public GitHub-release binaries (CI has no
// WiFiCreds.h, so released firmware falls back to empty and app-only).
//
// To use: copy WiFiCreds.h.example to WiFiCreds.h and fill in your network.
#if defined(__has_include)
#  if __has_include("WiFiCreds.h")
#    include "WiFiCreds.h"
#  endif
#endif
#ifndef WIFI_CREDS_SSID
#  define WIFI_CREDS_SSID ""
#  define WIFI_CREDS_PASS ""
#endif
#define OTA_WIFI_SSID     WIFI_CREDS_SSID
#define OTA_WIFI_PASSWORD WIFI_CREDS_PASS

// Base URL for GitHub Releases - update with your repo. Tag is appended by release workflow.
// Format: https://github.com/OWNER/REPO/releases/download/TAG/firmware-TARGET.bin
// Note: this is the cody-sf/bmprojects repo, not the `origin` remote of this
// working copy - the local remote still shows the pre-rename username.
#define OTA_FIRMWARE_BASE  "https://github.com/cody-sf/bmprojects/releases/download"

// Version check URL (shared across all targets)
#define OTA_VERSION_URL    OTA_FIRMWARE_BASE "/latest/version.txt"

// Per-target firmware URLs (each build fetches its own binary). The filenames
// must match the artifacts the release workflow uploads (firmware-<env>.bin),
// or a device will pull the wrong build - e.g. the hotel sign landing on the
// generic esp32 image would lose its neon driver and pin map.
#if defined(TARGET_SLUT)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-slut.bin"
#elif defined(TARGET_ESP32_C6)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-c6.bin"
#elif defined(TARGET_HOTEL_SIGN)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-hotel_sign.bin"
#elif defined(TARGET_TAKOYAKI_SIGN)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-takoyaki_sign.bin"
#else
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-esp32.bin"
#endif

// How often to check for updates (ms). 0 = check once at boot, then every 24h
#define OTA_CHECK_INTERVAL_MS  (60 * 60 * 1000)  // 1 hour

// ── Local network flashing (ArduinoOTA / espota) ─────────────────────────────
// Independent of the GitHub pull above: push a locally-built image over WiFi
// straight from your laptop, no release and no USB cable. The device advertises
// an espota listener on port 3232 once it is on WiFi; upload with the *_ota
// PlatformIO envs (see platformio.ini). Set a password so only you can reflash
// it - pass it to the uploader with `--auth`.
#define LOCAL_OTA_ENABLED   1
#define LOCAL_OTA_PASSWORD  "cody"   // the *_ota envs pass this with --auth automatically

// ── WiFi power policy ────────────────────────────────────────────────────────
// Battery-powered wearables turn the WiFi radio fully off after each update
// check - staying associated costs tens of mA continuously on top of BLE, and
// was the single biggest battery drain in the whole firmware. Mains-powered
// signs keep it up: power is free there, and it keeps espota flashing and
// hourly checks available.
//
// A battery build still checks once at boot, and again whenever the app sends
// ota_check_now (0x7B) or fresh WiFi credentials. After one of those manual
// checks it lingers online for OTA_MANUAL_LINGER_MS so a local espota push is
// possible on demand ("Check for updates" in the app, then `pio run -e *_ota`).
#if defined(TARGET_HOTEL_SIGN) || defined(TARGET_TAKOYAKI_SIGN)
  #define OTA_KEEP_WIFI_ALIVE 1
#else
  #define OTA_KEEP_WIFI_ALIVE 0
#endif
#define OTA_MANUAL_LINGER_MS (5 * 60 * 1000)

// Delay before first OTA check (ms) - allows device to stabilize
#define OTA_BOOT_DELAY_MS  30000  // 30 seconds

// Server certificate for HTTPS (required for OTA)
// GitHub uses two cert chains: ECC (github.com) and RSA (CDN redirect).
// Both root CAs are included so the full redirect flow works.
#define OTA_SERVER_CERT    OTA_CERT_GITHUB

// USERTrust ECC root (github.com) + Comodo AAA root (release-assets CDN)
#define OTA_CERT_GITHUB \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIICjzCCAhWgAwIBAgIQXIuZxVqUxdJxVt7NiYDMJjAKBggqhkjOPQQDAzCBiDEL\n" \
    "MAkGA1UEBhMCVVMxEzARBgNVBAgTCk5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNl\n" \
    "eSBDaXR5MR4wHAYDVQQKExVUaGUgVVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMT\n" \
    "JVVTRVJUcnVzdCBFQ0MgQ2VydGlmaWNhdGlvbiBBdXRob3JpdHkwHhcNMTAwMjAx\n" \
    "MDAwMDAwWhcNMzgwMTE4MjM1OTU5WjCBiDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n" \
    "Ck5ldyBKZXJzZXkxFDASBgNVBAcTC0plcnNleSBDaXR5MR4wHAYDVQQKExVUaGUg\n" \
    "VVNFUlRSVVNUIE5ldHdvcmsxLjAsBgNVBAMTJVVTRVJUcnVzdCBFQ0MgQ2VydGlm\n" \
    "aWNhdGlvbiBBdXRob3JpdHkwdjAQBgcqhkjOPQIBBgUrgQQAIgNiAAQarFRaqflo\n" \
    "I+d61SRvU8Za2EurxtW20eZzca7dnNYMYf3boIkDuAUU7FfO7l0/4iGzzvfUinng\n" \
    "o4N+LZfQYcTxmdwlkWOrfzCjtHDix6EznPO/LlxTsV+zfTJ/ijTjeXmjQjBAMB0G\n" \
    "A1UdDgQWBBQ64QmG1M8ZwpZ2dEl23OA1xmNjmjAOBgNVHQ8BAf8EBAMCAQYwDwYD\n" \
    "VR0TAQH/BAUwAwEB/zAKBggqhkjOPQQDAwNoADBlAjA2Z6EWCNzklwBBHU6+4WMB\n" \
    "zzuqQhFkoJ2UOQIReVx7Hfpkue4WQrO/isIJxOzksU0CMQDpKmFHjFJKS04YcPbW\n" \
    "RNZu9YO6bVi9JNlWSOrvxKJGgYhqOkbRqZtNyWHa0V1Xahg=\n" \
    "-----END CERTIFICATE-----\n" \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIEMjCCAxqgAwIBAgIBATANBgkqhkiG9w0BAQUFADB7MQswCQYDVQQGEwJHQjEb\n" \
    "MBkGA1UECAwSR3JlYXRlciBNYW5jaGVzdGVyMRAwDgYDVQQHDAdTYWxmb3JkMRow\n" \
    "GAYDVQQKDBFDb21vZG8gQ0EgTGltaXRlZDEhMB8GA1UEAwwYQUFBIENlcnRpZmlj\n" \
    "YXRlIFNlcnZpY2VzMB4XDTA0MDEwMTAwMDAwMFoXDTI4MTIzMTIzNTk1OVowezEL\n" \
    "MAkGA1UEBhMCR0IxGzAZBgNVBAgMEkdyZWF0ZXIgTWFuY2hlc3RlcjEQMA4GA1UE\n" \
    "BwwHU2FsZm9yZDEaMBgGA1UECgwRQ29tb2RvIENBIExpbWl0ZWQxITAfBgNVBAMM\n" \
    "GEFBQSBDZXJ0aWZpY2F0ZSBTZXJ2aWNlczCCASIwDQYJKoZIhvcNAQEBBQADggEP\n" \
    "ADCCAQoCggEBAL5AnfRu4ep2hxxNRUSOvkbIgwadwSr+GB+O5AL686tdUIoWMQua\n" \
    "BtDFcCLNSS1UY8y2bmhGC1Pqy0wkwLxyTurxFa70VJoSCsN6sjNg4tqJVfMiWPPe\n" \
    "3M/vg4aijJRPn2jymJBGhCfHdr/jzDUsi14HZGWCwEiwqJH5YZ92IFCokcdmtet4\n" \
    "YgNW8IoaE+oxox6gmf049vYnMlhvB/VruPsUK6+3qszWY19zjNoFmag4qMsXeDZR\n" \
    "rOme9Hg6jc8P2ULimAyrL58OAd7vn5lJ8S3frHRNG5i1R8XlKdH5kBjHYpy+g8cm\n" \
    "ez6KJcfA3Z3mNWgQIJ2P2N7Sw4ScDV7oL8kCAwEAAaOBwDCBvTAdBgNVHQ4EFgQU\n" \
    "oBEKIz6W8Qfs4q8p74Klf9AwpLQwDgYDVR0PAQH/BAQDAgEGMA8GA1UdEwEB/wQF\n" \
    "MAMBAf8wewYDVR0fBHQwcjA4oDagNIYyaHR0cDovL2NybC5jb21vZG9jYS5jb20v\n" \
    "QUFBQ2VydGlmaWNhdGVTZXJ2aWNlcy5jcmwwNqA0oDKGMGh0dHA6Ly9jcmwuY29t\n" \
    "b2RvLm5ldC9BQUFDZXJ0aWZpY2F0ZVNlcnZpY2VzLmNybDANBgkqhkiG9w0BAQUF\n" \
    "AAOCAQEACFb8AvCb6P+k+tZ7xkSAzk/ExfYAWMymtrwUSWgEdujm7l3sAg9g1o1Q\n" \
    "GE8mTgHj5rCl7r+8dFRBv/38ErjHT1r0iWAFf2C3BUrz9vHCv8S5dIa2LX1rzNLz\n" \
    "Rt0vxuBqw8M0Ayx9lt1awg6nCpnBBYurDC/zXDrPbDdVCYfeU0BsWO/8tqtlbgT2\n" \
    "G9w84FoVxp7Z8VlIMCFlA2zs6SFz7JsDoeA3raAVGI/6ugLOpyypEBMs1OUIJqsi\n" \
    "l2D4kF501KKaU73yqWjgom7C12yxow+ev+to51byrvLjKzg6CYG1a4XXvi3tPxq3\n" \
    "smPi9WIsgtRqAEFQ8TmDn5XpNpaYbg==\n" \
    "-----END CERTIFICATE-----\n"

#endif  // OTA_ENABLED

#endif  // OTA_CONFIG_H
