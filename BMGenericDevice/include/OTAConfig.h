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
#define OTA_ENABLED 0

#if OTA_ENABLED

// WiFi credentials - set via BLE from RNUmbrella app (preferred) or fallback to these
// When empty strings, device uses credentials from app only
#define OTA_WIFI_SSID     ""
#define OTA_WIFI_PASSWORD ""

// Base URL for GitHub Releases - update with your repo. Tag is appended by release workflow.
// Format: https://github.com/OWNER/REPO/releases/download/TAG/firmware-TARGET.bin
#define OTA_FIRMWARE_BASE  "https://github.com/cody-sf/bmprojects/releases/download"

// Per-target firmware URLs (each build fetches its own binary)
#if defined(TARGET_SLUT)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-slut.bin"
#elif defined(TARGET_ESP32_C6)
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-c6.bin"
#else
  #define OTA_FIRMWARE_URL  OTA_FIRMWARE_BASE "/latest/firmware-esp32.bin"
#endif

// How often to check for updates (ms). 0 = check once at boot, then every 24h
#define OTA_CHECK_INTERVAL_MS  (60 * 60 * 1000)  // 1 hour

// Delay before first OTA check (ms) - allows device to stabilize
#define OTA_BOOT_DELAY_MS  30000  // 30 seconds

// Server certificate for HTTPS (required for OTA)
// - Use OTA_CERT_LETS_ENCRYPT for GitHub Releases, most public HTTPS (Let's Encrypt)
// - Or provide your own PEM string for custom/self-signed certs
#define OTA_SERVER_CERT    OTA_CERT_LETS_ENCRYPT

// Let's Encrypt ISRG Root X1 - validates GitHub, S3, most public HTTPS
#define OTA_CERT_LETS_ENCRYPT \
    "-----BEGIN CERTIFICATE-----\n" \
    "MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n" \
    "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n" \
    "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n" \
    "WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n" \
    "ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n" \
    "MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n" \
    "h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n" \
    "0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n" \
    "A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n" \
    "T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n" \
    "B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n" \
    "B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n" \
    "KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n" \
    "OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n" \
    "jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrb\n" \
    "wqHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hvtYMhDZGx6JCXc8h2\n" \
    "qvWE5xFx3nOF1XX1LR0CGQSM+nLGCB7E+QjBBtXYe+FV6LYj3gVJXnqJbN6Q3Jq\n" \
    "uW0wLV3yEAoOGxH1O2tU6d9PHJyMB0vZfHZJxQpYpqmzD0=\n" \
    "-----END CERTIFICATE-----"

#endif  // OTA_ENABLED

#endif  // OTA_CONFIG_H
