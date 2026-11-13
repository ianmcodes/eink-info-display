#include "ble_config.h"
#include "config_store.h"

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>

// ---------------------------------------------------------------------------
// UUIDs
// ---------------------------------------------------------------------------

static constexpr char SVC_UUID[]       = "12345678-1234-1234-1234-123456789ABC";
static constexpr char WIFI_SSID_UUID[] = "12345678-0001-1234-1234-123456789ABC";
static constexpr char WIFI_PASS_UUID[] = "12345678-0002-1234-1234-123456789ABC";
static constexpr char HA_URL_UUID[]    = "12345678-0003-1234-1234-123456789ABC";
static constexpr char HA_TOKEN_UUID[]  = "12345678-0004-1234-1234-123456789ABC";
static constexpr char HA_TLS_UUID[]    = "12345678-0005-1234-1234-123456789ABC";
static constexpr char NWS_LAT_UUID[]   = "12345678-0006-1234-1234-123456789ABC";
static constexpr char NWS_LON_UUID[]   = "12345678-0007-1234-1234-123456789ABC";
static constexpr char TZ_POSIX_UUID[]  = "12345678-0008-1234-1234-123456789ABC";
static constexpr char SCREEN_DUR_UUID[]= "12345678-0009-1234-1234-123456789ABC";
static constexpr char ENTITIES_UUID[]  = "12345678-000A-1234-1234-123456789ABC";
static constexpr char CFG_URL_UUID[]   = "12345678-000B-1234-1234-123456789ABC";

// ---------------------------------------------------------------------------
// Module state
// ---------------------------------------------------------------------------

static bool              s_bleRunning  = false;
static volatile bool     s_updated     = false;
static NimBLEServer*     s_server      = nullptr;
static NimBLEService*    s_service     = nullptr;

// ---------------------------------------------------------------------------
// Server callbacks — track connections
// ---------------------------------------------------------------------------

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
        Serial.printf("[BLE] Client connected: %s\n",
                      info.getAddress().toString().c_str());
        // Allow more connections; re-start advertising for additional clients.
        server->startAdvertising();
    }
    void onDisconnect(NimBLEServer* server, NimBLEConnInfo& info, int reason) override {
        Serial.printf("[BLE] Client disconnected (reason %d)\n", reason);
        server->startAdvertising();
    }
};

static ServerCallbacks s_serverCallbacks;

// ---------------------------------------------------------------------------
// Characteristic callbacks — handle reads and writes
// ---------------------------------------------------------------------------

// Helper: convert a NimBLEAttValue to a std::string safely.
static std::string attValToStr(const NimBLEAttValue& val)
{
    return std::string(reinterpret_cast<const char*>(val.data()), val.length());
}

// Helper: set a characteristic value from a std::string.
static void setCharValue(NimBLECharacteristic* ch, const std::string& s)
{
    ch->setValue(reinterpret_cast<const uint8_t*>(s.data()), s.size());
}

class WifiSsidCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.wifiSsid = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] wifiSsid updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.wifiSsid);
    }
};

class WifiPassCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.wifiPassword = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] wifiPassword updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.wifiPassword);
    }
};

class HaUrlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.haBaseUrl = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] haBaseUrl updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.haBaseUrl);
    }
};

class HaTokenCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.haToken = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] haToken updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.haToken);
    }
};

class HaTlsCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        const NimBLEAttValue& val = ch->getValue();
        if (val.length() >= 1) {
            gConfig.haAllowInsecureTls = (val[0] != 0);
        }
        saveConfig(gConfig);
        s_updated = true;
        Serial.printf("[BLE] haAllowInsecureTls = %d\n", gConfig.haAllowInsecureTls);
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        uint8_t v = gConfig.haAllowInsecureTls ? 1 : 0;
        ch->setValue(&v, 1);
    }
};

class NwsLatCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.nwsLatitude = atof(attValToStr(ch->getValue()).c_str());
        saveConfig(gConfig);
        s_updated = true;
        Serial.printf("[BLE] nwsLatitude = %.4f\n", gConfig.nwsLatitude);
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.6f", gConfig.nwsLatitude);
        ch->setValue(buf);
    }
};

class NwsLonCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.nwsLongitude = atof(attValToStr(ch->getValue()).c_str());
        saveConfig(gConfig);
        s_updated = true;
        Serial.printf("[BLE] nwsLongitude = %.4f\n", gConfig.nwsLongitude);
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.6f", gConfig.nwsLongitude);
        ch->setValue(buf);
    }
};

class TzPosixCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.tzPosix = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] tzPosix updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.tzPosix);
    }
};

class ScreenDurCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        const std::string s = attValToStr(ch->getValue());
        const uint32_t v = static_cast<uint32_t>(strtoul(s.c_str(), nullptr, 10));
        if (v > 0) {
            gConfig.screenDurationMs = v;
        }
        saveConfig(gConfig);
        s_updated = true;
        Serial.printf("[BLE] screenDurationMs = %u\n", gConfig.screenDurationMs);
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        char buf[12];
        snprintf(buf, sizeof(buf), "%u", gConfig.screenDurationMs);
        ch->setValue(buf);
    }
};

class EntitiesCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.entitiesJson = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] entitiesJson updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.entitiesJson);
    }
};

class CfgUrlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        gConfig.configPageUrl = attValToStr(ch->getValue());
        saveConfig(gConfig);
        s_updated = true;
        Serial.println("[BLE] configPageUrl updated");
    }
    void onRead(NimBLECharacteristic* ch, NimBLEConnInfo&) override {
        setCharValue(ch, gConfig.configPageUrl);
    }
};

// Static callback instances (never freed; tiny, long-lived).
static WifiSsidCallbacks  s_cbSsid;
static WifiPassCallbacks  s_cbPass;
static HaUrlCallbacks     s_cbHaUrl;
static HaTokenCallbacks   s_cbHaToken;
static HaTlsCallbacks     s_cbHaTls;
static NwsLatCallbacks    s_cbLat;
static NwsLonCallbacks    s_cbLon;
static TzPosixCallbacks   s_cbTz;
static ScreenDurCallbacks s_cbScreenDur;
static EntitiesCallbacks  s_cbEntities;
static CfgUrlCallbacks    s_cbCfgUrl;

// ---------------------------------------------------------------------------
// Helper: add a READ+WRITE characteristic with the given callbacks
// ---------------------------------------------------------------------------

static NimBLECharacteristic* addChar(NimBLEService* svc,
                                     const char* uuid,
                                     NimBLECharacteristicCallbacks* cbs)
{
    NimBLECharacteristic* ch = svc->createCharacteristic(
        uuid,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );
    ch->setCallbacks(cbs);
    return ch;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void bleConfigBegin()
{
    if (s_bleRunning) return;

    Serial.println("[BLE] Starting BLE provisioning service");

    NimBLEDevice::init("EinkDisplay");
    NimBLEDevice::setPower(9);  // +9 dBm

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(&s_serverCallbacks);

    s_service = s_server->createService(SVC_UUID);

    addChar(s_service, WIFI_SSID_UUID, &s_cbSsid);
    addChar(s_service, WIFI_PASS_UUID, &s_cbPass);
    addChar(s_service, HA_URL_UUID,    &s_cbHaUrl);
    addChar(s_service, HA_TOKEN_UUID,  &s_cbHaToken);
    addChar(s_service, HA_TLS_UUID,    &s_cbHaTls);
    addChar(s_service, NWS_LAT_UUID,   &s_cbLat);
    addChar(s_service, NWS_LON_UUID,   &s_cbLon);
    addChar(s_service, TZ_POSIX_UUID,  &s_cbTz);
    addChar(s_service, SCREEN_DUR_UUID,&s_cbScreenDur);
    addChar(s_service, ENTITIES_UUID,  &s_cbEntities);
    addChar(s_service, CFG_URL_UUID,   &s_cbCfgUrl);

    // Note: in NimBLE-Arduino v2 services start automatically with the server.
    // s_service->start() is a no-op / deprecated; we just start the server.
    s_server->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(SVC_UUID);
    adv->enableScanResponse(true);
    adv->start();

    s_bleRunning = true;
    Serial.println("[BLE] Advertising as 'EinkDisplay'");
}

void bleConfigEnd()
{
    if (!s_bleRunning) return;

    Serial.println("[BLE] Stopping BLE");

    NimBLEDevice::getAdvertising()->stop();
    // Disconnect all clients cleanly by iterating connected peers.
    const uint8_t count = s_server->getConnectedCount();
    for (uint8_t i = 0; i < count; ++i) {
        NimBLEConnInfo info = s_server->getPeerInfo(0);  // always index 0 after each disconnect
        s_server->disconnect(info);
        delay(10);  // brief yield so the stack can process the disconnect
    }
    // Deinitialize NimBLE so the radio is fully released for Wi-Fi.
    NimBLEDevice::deinit(/*clearAll=*/true);

    s_server  = nullptr;
    s_service = nullptr;
    s_bleRunning = false;

    Serial.println("[BLE] BLE stopped");
}

bool bleConfigHasClients()
{
    if (!s_bleRunning || s_server == nullptr) return false;
    return s_server->getConnectedCount() > 0;
}

bool bleConfigWasUpdated()
{
    if (s_updated) {
        s_updated = false;
        return true;
    }
    return false;
}
