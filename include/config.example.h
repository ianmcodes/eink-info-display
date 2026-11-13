#pragma once

// Copy this file to include/config.h, then fill in your local details.
// config.h is intentionally ignored by Git: do not commit your HA token.

constexpr char WIFI_SSID[] = "YOUR_WIFI_NAME";
constexpr char WIFI_PASSWORD[] = "YOUR_WIFI_PASSWORD";

// No trailing slash. Examples: http://homeassistant.local:8123
// or https://homeassistant.example.net
constexpr char HA_BASE_URL[] = "http://homeassistant.local:8123";

// Create this in Home Assistant: Profile -> Security -> Long-Lived Access Tokens.
constexpr char HA_TOKEN[] = "PASTE_A_LONG_LIVED_ACCESS_TOKEN_HERE";

// List every entity you want to display. The label overrides the friendly_name
// from Home Assistant; leave it empty ("") to use the HA friendly_name instead.
// Up to 6 entities fit comfortably in the 2-column grid layout.
struct HaEntityConfig {
    const char* entityId;
    const char* label;  // "" to use HA friendly_name
};

constexpr HaEntityConfig HA_ENTITIES[] = {
    { "sensor.example_temperature", "Temperature" },
    { "sensor.example_humidity",    "Humidity"    },
};

// How long each screen is shown before rotating to the next one.
// E-paper refreshes are visible and have a finite lifetime; two minutes is a
// reasonable balance between freshness and display wear.
constexpr uint32_t SCREEN_DURATION_MS = 2UL * 60UL * 1000UL;

// Set true only if HA_BASE_URL begins with https:// and you accept an insecure
// TLS connection (common for a local, self-signed Home Assistant certificate).
// For a public CA certificate, keep this false and add certificate validation.
constexpr bool HA_ALLOW_INSECURE_TLS = false;

// ---------------------------------------------------------------------------
// Weather (NWS — US only, no API key required)
// ---------------------------------------------------------------------------

// Decimal latitude and longitude for your location (4 decimal places max).
// Used to resolve the local NWS forecast office via api.weather.gov/points.
constexpr float NWS_LATITUDE  = 0.0f;   // e.g. 41.8781
constexpr float NWS_LONGITUDE = 0.0f;   // e.g. -87.6298

// ---------------------------------------------------------------------------
// Clock / timezone
// ---------------------------------------------------------------------------

// POSIX tz string for your local timezone. This controls both the UTC offset
// and automatic DST transitions. Examples:
//   US Central:  "CST6CDT,M3.2.0,M11.1.0"
//   US Eastern:  "EST5EDT,M3.2.0,M11.1.0"
//   US Mountain: "MST7MDT,M3.2.0,M11.1.0"
//   US Pacific:  "PST8PDT,M3.2.0,M11.1.0"
//   UTC:         "UTC0"
constexpr char TZ_POSIX[] = "UTC0";

// ---------------------------------------------------------------------------
// Web Bluetooth configuration page URL
// ---------------------------------------------------------------------------
// Optional short URL directing to your hosted ble-config.html page.
// Shown on the e-paper display when the CONFIGURE screen is active.
constexpr char CONFIG_PAGE_URL[] = "";

