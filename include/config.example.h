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

// Any entity exposed by Home Assistant, e.g. sensor.living_room_temperature.
constexpr char HA_ENTITY_ID[] = "sensor.example";

// E-paper refreshes are visible and have a finite lifetime. Five minutes is a
// sensible starting point for a wall display.
constexpr uint32_t POLL_INTERVAL_MS = 5UL * 60UL * 1000UL;

// Set true only if HA_BASE_URL begins with https:// and you accept an insecure
// TLS connection (common for a local, self-signed Home Assistant certificate).
// For a public CA certificate, keep this false and add certificate validation.
constexpr bool HA_ALLOW_INSECURE_TLS = false;
