#include "config_store.h"

#include <Preferences.h>
#include <ArduinoJson.h>

// Pull in compile-time defaults.  config.h is git-ignored; config.example.h
// is always present and provides safe placeholder values.
#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

// ---------------------------------------------------------------------------
// Global instance
// ---------------------------------------------------------------------------

AppConfig gConfig;

// ---------------------------------------------------------------------------
// NVS namespace and key names
// ---------------------------------------------------------------------------

static constexpr char NVS_NS[]        = "eink_cfg";
static constexpr char KEY_SSID[]      = "wifi_ssid";
static constexpr char KEY_PASS[]      = "wifi_pass";
static constexpr char KEY_HA_URL[]    = "ha_url";
static constexpr char KEY_HA_TOKEN[]  = "ha_token";
static constexpr char KEY_HA_TLS[]    = "ha_tls";
static constexpr char KEY_LAT[]       = "nws_lat";
static constexpr char KEY_LON[]       = "nws_lon";
static constexpr char KEY_TZ[]        = "tz_posix";
static constexpr char KEY_SCREEN_DUR[]= "screen_dur";
static constexpr char KEY_ENTITIES[]  = "entities";
static constexpr char KEY_CFG_URL[]   = "cfg_url";

// ---------------------------------------------------------------------------
// Helper: build default entitiesJson from compile-time HA_ENTITIES array
// ---------------------------------------------------------------------------

static std::string defaultEntitiesJson()
{
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    const size_t n = sizeof(HA_ENTITIES) / sizeof(HA_ENTITIES[0]);
    for (size_t i = 0; i < n; ++i) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]    = HA_ENTITIES[i].entityId;
        obj["label"] = HA_ENTITIES[i].label;
    }
    std::string out;
    serializeJson(doc, out);
    return out;
}

// ---------------------------------------------------------------------------
// loadConfig
// ---------------------------------------------------------------------------

void loadConfig()
{
    Preferences prefs;
    prefs.begin(NVS_NS, /*readOnly=*/true);

    // ---- Wi-Fi ----
    if (prefs.isKey(KEY_SSID)) {
        gConfig.wifiSsid = prefs.getString(KEY_SSID, "").c_str();
    } else {
        gConfig.wifiSsid = WIFI_SSID;
    }

    if (prefs.isKey(KEY_PASS)) {
        gConfig.wifiPassword = prefs.getString(KEY_PASS, "").c_str();
    } else {
        gConfig.wifiPassword = WIFI_PASSWORD;
    }

    // ---- Home Assistant ----
    if (prefs.isKey(KEY_HA_URL)) {
        gConfig.haBaseUrl = prefs.getString(KEY_HA_URL, "").c_str();
    } else {
        gConfig.haBaseUrl = HA_BASE_URL;
    }

    if (prefs.isKey(KEY_HA_TOKEN)) {
        gConfig.haToken = prefs.getString(KEY_HA_TOKEN, "").c_str();
    } else {
        gConfig.haToken = HA_TOKEN;
    }

    if (prefs.isKey(KEY_HA_TLS)) {
        gConfig.haAllowInsecureTls = prefs.getBool(KEY_HA_TLS, HA_ALLOW_INSECURE_TLS);
    } else {
        gConfig.haAllowInsecureTls = HA_ALLOW_INSECURE_TLS;
    }

    // ---- NWS ----
    if (prefs.isKey(KEY_LAT)) {
        gConfig.nwsLatitude = prefs.getFloat(KEY_LAT, NWS_LATITUDE);
    } else {
        gConfig.nwsLatitude = NWS_LATITUDE;
    }

    if (prefs.isKey(KEY_LON)) {
        gConfig.nwsLongitude = prefs.getFloat(KEY_LON, NWS_LONGITUDE);
    } else {
        gConfig.nwsLongitude = NWS_LONGITUDE;
    }

    // ---- Timezone ----
    if (prefs.isKey(KEY_TZ)) {
        gConfig.tzPosix = prefs.getString(KEY_TZ, "").c_str();
    } else {
        gConfig.tzPosix = TZ_POSIX;
    }

    // ---- Screen duration ----
    if (prefs.isKey(KEY_SCREEN_DUR)) {
        gConfig.screenDurationMs = prefs.getUInt(KEY_SCREEN_DUR, SCREEN_DURATION_MS);
    } else {
        gConfig.screenDurationMs = SCREEN_DURATION_MS;
    }

    // ---- Entities JSON ----
    if (prefs.isKey(KEY_ENTITIES)) {
        gConfig.entitiesJson = prefs.getString(KEY_ENTITIES, "").c_str();
    } else {
        gConfig.entitiesJson = defaultEntitiesJson();
    }

    // ---- Config page URL ----
    if (prefs.isKey(KEY_CFG_URL)) {
        gConfig.configPageUrl = prefs.getString(KEY_CFG_URL, "").c_str();
    } else {
        gConfig.configPageUrl = CONFIG_PAGE_URL;
    }

    prefs.end();

    Serial.println("[cfg] Config loaded from NVS");
}

// ---------------------------------------------------------------------------
// saveConfig
// ---------------------------------------------------------------------------

void saveConfig(const AppConfig& cfg)
{
    Preferences prefs;
    prefs.begin(NVS_NS, /*readOnly=*/false);

    prefs.putString(KEY_SSID,       cfg.wifiSsid.c_str());
    prefs.putString(KEY_PASS,       cfg.wifiPassword.c_str());
    prefs.putString(KEY_HA_URL,     cfg.haBaseUrl.c_str());
    prefs.putString(KEY_HA_TOKEN,   cfg.haToken.c_str());
    prefs.putBool  (KEY_HA_TLS,     cfg.haAllowInsecureTls);
    prefs.putFloat (KEY_LAT,        cfg.nwsLatitude);
    prefs.putFloat (KEY_LON,        cfg.nwsLongitude);
    prefs.putString(KEY_TZ,         cfg.tzPosix.c_str());
    prefs.putUInt  (KEY_SCREEN_DUR, cfg.screenDurationMs);
    prefs.putString(KEY_ENTITIES,   cfg.entitiesJson.c_str());
    prefs.putString(KEY_CFG_URL,    cfg.configPageUrl.c_str());

    prefs.end();

    Serial.println("[cfg] Config saved to NVS");
}
