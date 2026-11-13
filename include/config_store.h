#pragma once

#include <Arduino.h>
#include <string>

// ---------------------------------------------------------------------------
// AppConfig — runtime configuration, backed by NVS (Preferences).
// All string fields use std::string so they can grow/shrink freely.
// ---------------------------------------------------------------------------

struct AppConfig {
    std::string wifiSsid;
    std::string wifiPassword;

    std::string haBaseUrl;
    std::string haToken;
    bool        haAllowInsecureTls = false;

    float    nwsLatitude  = 0.0f;
    float    nwsLongitude = 0.0f;

    std::string tzPosix;

    uint32_t screenDurationMs = 120000UL;  // 2 minutes default

    // JSON array: [{"id":"sensor.foo","label":"Foo"}, ...]
    std::string entitiesJson;

    // URL of the hosted web config page (shown as a hint on the Config screen).
    std::string configPageUrl;
};

// Global config instance — defined in config_store.cpp, used everywhere.
extern AppConfig gConfig;

// Load from NVS; fall back to compile-time constants for any missing key.
void loadConfig();

// Persist the given config to NVS.
void saveConfig(const AppConfig& cfg);
