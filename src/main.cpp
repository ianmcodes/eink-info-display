#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <time.h>

#include <Adafruit_GFX.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include "green_epd.h"

#if __has_include("config.h")
#include "config.h"
#else
#include "config.example.h"
#endif

// Elecrow CrowPanel 4.2-inch E-paper HMI (SSD1683), verified against the
// vendor's published V1.0 pin map.
constexpr uint8_t EPD_SCK = 12;
constexpr uint8_t EPD_MOSI = 11;
constexpr uint8_t EPD_RST = 47;
constexpr uint8_t EPD_DC = 46;
constexpr uint8_t EPD_CS = 45;
constexpr uint8_t EPD_BUSY = 48;
// GPIO7 and GPIO41 both need to be enabled on the green-sticker board.
constexpr uint8_t EPD_POWER = 7;
constexpr uint8_t EPD_MCU_POWER = 41;

GreenEpd epd(EPD_SCK, EPD_MOSI, EPD_RST, EPD_DC, EPD_CS, EPD_BUSY);
GFXcanvas1 display(400, 300);

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t DISPLAY_TEST_HOLD_MS = 5000;
constexpr size_t MAX_ENTITIES = 6;

// Navigation buttons (active-low: pressed = LOW via internal pull-up).
constexpr uint8_t BTN_PREV = 6;  // "previous" button
constexpr uint8_t BTN_NEXT = 4;  // "next" button
// Minimum ms between two recognised button events (debounce).
constexpr uint32_t BTN_DEBOUNCE_MS = 250;

uint32_t lastBtnAt = 0;        // millis() of the last accepted press
bool prevBtnLast   = HIGH;     // last stable state of BTN_PREV
bool nextBtnLast   = HIGH;     // last stable state of BTN_NEXT

// ---------------------------------------------------------------------------
// Screen rotation state
// ---------------------------------------------------------------------------

enum Screen { SCREEN_HA, SCREEN_WEATHER, SCREEN_CLOCK };
constexpr Screen SCREEN_ORDER[] = { SCREEN_HA, SCREEN_WEATHER, SCREEN_CLOCK };
constexpr size_t NUM_SCREENS = sizeof(SCREEN_ORDER) / sizeof(SCREEN_ORDER[0]);

size_t currentScreenIndex = 0;
uint32_t nextScreenAt = 0;

// ---------------------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------------------

struct EntityData {
    String state;
    String name;
    String unit;
    String error;
    bool ok = false;
};

// Holds up to 4 half-day forecast periods (today through ~day 3).
constexpr size_t MAX_FORECAST_PERIODS = 4;
struct ForecastPeriod {
    String name;         // e.g. "Today", "Tonight", "Thursday"
    String shortForecast;
    int temperature = 0;
    char tempUnit = 'F';
    int precipPct = 0;
    String windSpeed;
};

struct WeatherData {
    ForecastPeriod periods[MAX_FORECAST_PERIODS];
    size_t count = 0;
    String error;
    bool ok = false;
};

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

bool isPlaceholder(const char* value)
{
    return !value || value[0] == '\0' || strstr(value, "YOUR_") ||
           strstr(value, "PASTE_");
}

String shorten(const String& value, size_t maxLength)
{
    if (value.length() <= maxLength) return value;
    return value.substring(0, maxLength - 3) + "...";
}

// ---------------------------------------------------------------------------
// Networking helpers
// ---------------------------------------------------------------------------

bool connectWiFi(String& error)
{
    if (WiFi.status() == WL_CONNECTED) return true;

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    const uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < WIFI_CONNECT_TIMEOUT_MS) {
        delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) return true;

    error = "Wi-Fi connection timed out";
    return false;
}

// Performs a plain HTTP GET and returns the response body, or sets error.
// Only used for https:// (NWS) connections with insecure TLS (api.weather.gov
// uses a public CA, but the ESP32 trust store may not have it by default).
bool httpGet(const String& url, String& body, String& error)
{
    HTTPClient http;
    WiFiClientSecure secureClient;
    WiFiClient plainClient;
    bool began = false;

    if (url.startsWith("https://")) {
        secureClient.setInsecure();   // NWS uses a public CA; insecure skips chain validation
        began = http.begin(secureClient, url);
    } else {
        began = http.begin(plainClient, url);
    }
    if (!began) {
        error = "Could not open connection";
        return false;
    }

    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    // NWS requires a User-Agent; use a generic identifier.
    http.addHeader("User-Agent", "ESP32-eInk-Display");
    http.addHeader("Accept", "application/json");

    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        error = "HTTP " + String(code);
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    return true;
}

// ---------------------------------------------------------------------------
// NTP / clock
// ---------------------------------------------------------------------------

void syncNtp()
{
    String err;
    if (!connectWiFi(err)) {
        Serial.println("NTP sync skipped: " + err);
        return;
    }
    // configTime sets the POSIX TZ env var and syncs from the pool.
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    setenv("TZ", TZ_POSIX, 1);
    tzset();

    // Wait up to 10 s for the time to be set.
    const uint32_t started = millis();
    time_t now = 0;
    while (now < 1000000000UL && millis() - started < 10000) {
        delay(200);
        time(&now);
    }
    if (now >= 1000000000UL) {
        struct tm t;
        localtime_r(&now, &t);
        Serial.printf("NTP synced: %04d-%02d-%02d %02d:%02d:%02d\n",
                      t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                      t.tm_hour, t.tm_min, t.tm_sec);
    } else {
        Serial.println("NTP sync timed out");
    }
}

// ---------------------------------------------------------------------------
// Home Assistant fetch
// ---------------------------------------------------------------------------

// Fetches a single entity by ID. If configLabel is non-empty it overrides the
// friendly_name returned by Home Assistant.
EntityData fetchEntity(const char* entityId, const char* configLabel)
{
    EntityData result;
    if (isPlaceholder(WIFI_SSID) || isPlaceholder(WIFI_PASSWORD) ||
        isPlaceholder(HA_BASE_URL) || isPlaceholder(HA_TOKEN) ||
        !entityId || entityId[0] == '\0') {
        result.error = "Configure include/config.h first";
        return result;
    }

    if (!connectWiFi(result.error)) return result;

    const String endpoint = String(HA_BASE_URL) + "/api/states/" + entityId;
    HTTPClient http;
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    bool began = false;

    if (endpoint.startsWith("https://")) {
        if (HA_ALLOW_INSECURE_TLS) secureClient.setInsecure();
        began = http.begin(secureClient, endpoint);
    } else {
        began = http.begin(plainClient, endpoint);
    }
    if (!began) {
        result.error = "Could not open HA connection";
        return result;
    }

    http.setConnectTimeout(10000);
    http.setTimeout(15000);
    http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);
    http.addHeader("Accept", "application/json");
    const int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        result.error = "HTTP " + String(httpCode);
        http.end();
        return result;
    }

    JsonDocument filter;
    filter["state"] = true;
    filter["attributes"]["friendly_name"] = true;
    filter["attributes"]["unit_of_measurement"] = true;
    JsonDocument document;
    const DeserializationError jsonError = deserializeJson(
        document, http.getStream(), DeserializationOption::Filter(filter));
    http.end();
    if (jsonError) {
        result.error = String("Bad JSON: ") + jsonError.c_str();
        return result;
    }

    result.state = document["state"].as<String>();
    // configLabel takes priority; fall back to HA friendly_name, then entityId.
    if (configLabel && configLabel[0] != '\0') {
        result.name = configLabel;
    } else {
        result.name = document["attributes"]["friendly_name"] | entityId;
    }
    result.unit = document["attributes"]["unit_of_measurement"] | "";
    if (result.state.isEmpty()) {
        result.error = "No state returned";
        return result;
    }
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// NWS weather fetch
// ---------------------------------------------------------------------------

WeatherData fetchWeather()
{
    WeatherData result;

    if (isPlaceholder(WIFI_SSID) || isPlaceholder(WIFI_PASSWORD)) {
        result.error = "Configure Wi-Fi in config.h";
        return result;
    }
    if (!connectWiFi(result.error)) return result;

    // Step 1: resolve lat/lon to a NWS grid.
    char pointsUrl[64];
    snprintf(pointsUrl, sizeof(pointsUrl),
             "https://api.weather.gov/points/%.4f,%.4f",
             NWS_LATITUDE, NWS_LONGITUDE);

    String body;
    if (!httpGet(pointsUrl, body, result.error)) {
        result.error = "NWS /points: " + result.error;
        return result;
    }

    JsonDocument pointsDoc;
    if (deserializeJson(pointsDoc, body)) {
        result.error = "NWS /points bad JSON";
        return result;
    }
    const char* forecastUrl = pointsDoc["properties"]["forecast"];
    if (!forecastUrl || forecastUrl[0] == '\0') {
        result.error = "NWS no forecast URL";
        return result;
    }

    // Step 2: fetch the forecast.
    if (!httpGet(String(forecastUrl), body, result.error)) {
        result.error = "NWS /forecast: " + result.error;
        return result;
    }

    JsonDocument forecastDoc;
    if (deserializeJson(forecastDoc, body)) {
        result.error = "NWS /forecast bad JSON";
        return result;
    }

    JsonArray periods = forecastDoc["properties"]["periods"];
    if (periods.isNull()) {
        result.error = "NWS no periods";
        return result;
    }

    result.count = 0;
    for (JsonObject p : periods) {
        if (result.count >= MAX_FORECAST_PERIODS) break;
        ForecastPeriod& fp = result.periods[result.count++];
        fp.name          = p["name"].as<String>();
        fp.shortForecast = p["shortForecast"].as<String>();
        fp.temperature   = p["temperature"].as<int>();
        const char* tu   = p["temperatureUnit"] | "F";
        fp.tempUnit      = tu[0];
        fp.windSpeed     = p["windSpeed"].as<String>();
        // probabilityOfPrecipitation.value may be null — default to 0.
        fp.precipPct     = p["probabilityOfPrecipitation"]["value"] | 0;
    }

    if (result.count == 0) {
        result.error = "NWS returned 0 periods";
        return result;
    }
    result.ok = true;
    return result;
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

void drawWrapped(const String& text, int16_t x, int16_t& y, int16_t width, int16_t lineHeight)
{
    String line;
    int start = 0;
    while (start < static_cast<int>(text.length())) {
        const int space = text.indexOf(' ', start);
        const String word = text.substring(start, space < 0 ? text.length() : space);
        const String candidate = line.isEmpty() ? word : line + " " + word;
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(candidate, x, y, &x1, &y1, &w, &h);
        if (!line.isEmpty() && w > width) {
            display.setCursor(x, y);
            display.print(line);
            y += lineHeight;
            line = word;
        } else {
            line = candidate;
        }
        if (space < 0) break;
        start = space + 1;
    }
    if (!line.isEmpty()) {
        display.setCursor(x, y);
        display.print(line);
        y += lineHeight;
    }
}

// Draws a shared page header and returns the Y coordinate just below it.
int16_t drawHeader(const char* title)
{
    display.fillScreen(1);
    display.setTextColor(0);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(12, 22);
    display.print(title);
    display.drawLine(0, 28, display.width(), 28, 0);
    return 32;  // gridTop
}

// Draws the Wi-Fi RSSI footer.
void drawFooter()
{
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(12, display.height() - 12);
    display.print("Wi-Fi ");
    display.print(WiFi.RSSI());
    display.print(" dBm");
}

// ---------------------------------------------------------------------------
// Render: test pattern
// ---------------------------------------------------------------------------

void renderTestPattern()
{
    display.fillScreen(1);
    display.setTextColor(0);
    const int16_t right = display.width() - 1;
    const int16_t bottom = display.height() - 1;
    display.drawRect(0, 0, display.width(), display.height(), 0);
    display.drawRect(8, 8, display.width() - 16, display.height() - 16, 0);
    display.drawLine(0, 0, right, bottom, 0);
    display.drawLine(right, 0, 0, bottom, 0);
    display.drawLine(display.width() / 2, 8, display.width() / 2, bottom - 8, 0);
    display.drawLine(8, display.height() / 2, right - 8, display.height() / 2, 0);
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(28, 54);
    display.print("EPD TEST");
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(28, 82);
    display.print("GREEN BOARD / GPIO7+41 ON");
    display.setCursor(28, display.height() - 22);
    display.print("400 x 300");
    epd.display(display.getBuffer());
}

// ---------------------------------------------------------------------------
// Render: Home Assistant entities
// ---------------------------------------------------------------------------

// Draws a single entity card into a cell defined by (cellX, cellY, cellW, cellH).
void renderEntityCell(const EntityData& entity, int16_t cellX, int16_t cellY,
                      int16_t cellW, int16_t cellH)
{
    display.drawRect(cellX, cellY, cellW, cellH, 0);

    const int16_t pad = 4;
    const int16_t innerX = cellX + pad;
    const int16_t innerW = cellW - pad * 2;

    if (entity.ok) {
        display.setFont(&FreeMonoBold12pt7b);
        int16_t y = cellY + 18;
        drawWrapped(shorten(entity.name, 22), innerX, y, innerW, 18);

        display.drawLine(cellX + 1, y + 2, cellX + cellW - 2, y + 2, 0);
        y += 8;

        display.setFont(&FreeMonoBold18pt7b);
        String valueStr = shorten(entity.state, 10);
        if (!entity.unit.isEmpty()) {
            valueStr += " " + shorten(entity.unit, 6);
        }
        display.setCursor(innerX, y + 24);
        display.print(valueStr);
    } else {
        display.setFont(&FreeMonoBold18pt7b);
        display.setCursor(innerX, cellY + cellH / 2 + 8);
        display.print("--");
        display.setFont(&FreeMonoBold12pt7b);
        int16_t ey = cellY + cellH / 2 + 24;
        drawWrapped(shorten(entity.error, 22), innerX, ey, innerW, 16);
    }
}

void renderHA(const EntityData* entities, size_t count)
{
    const int16_t gridTop    = drawHeader("HOME ASSISTANT");
    const int16_t gridBottom = display.height() - 22;
    const int16_t gridH      = gridBottom - gridTop;
    const int16_t gridW      = display.width();

    const size_t cols  = 2;
    const size_t rows  = (count + cols - 1) / cols;
    const int16_t cellW = gridW / static_cast<int16_t>(cols);
    const int16_t cellH = (rows > 0) ? gridH / static_cast<int16_t>(rows) : gridH;

    for (size_t i = 0; i < count; ++i) {
        const size_t col = i % cols;
        const size_t row = i / cols;
        renderEntityCell(entities[i],
                         static_cast<int16_t>(col) * cellW,
                         gridTop + static_cast<int16_t>(row) * cellH,
                         cellW, cellH);
    }

    // drawFooter();
    epd.display(display.getBuffer());
}

// ---------------------------------------------------------------------------
// Render: weather forecast
// ---------------------------------------------------------------------------

void renderWeather(const WeatherData& wx)
{
    drawHeader("WEATHER");

    if (!wx.ok) {
        display.setFont(&FreeMonoBold18pt7b);
        int16_t y = 70;
        display.setCursor(12, y);
        display.print("FETCH FAILED");
        y += 32;
        display.setFont(&FreeMonoBold12pt7b);
        drawWrapped(wx.error, 12, y, display.width() - 24, 20);
        epd.display(display.getBuffer());
        return;
    }

    // Layout: up to 4 periods in a 2-column x 2-row grid.
    const int16_t gridTop    = 32;
    const int16_t gridBottom = display.height();
    const int16_t gridH      = gridBottom - gridTop;
    const int16_t gridW      = display.width();
    const int16_t cellW      = gridW / 2;
    const int16_t cellH      = gridH / 2;

    for (size_t i = 0; i < wx.count && i < 4; ++i) {
        const ForecastPeriod& p = wx.periods[i];
        const int16_t col = static_cast<int16_t>(i % 2);
        const int16_t row = static_cast<int16_t>(i / 2);
        const int16_t cx  = col * cellW;
        const int16_t cy  = gridTop + row * cellH;

        display.drawRect(cx, cy, cellW, cellH, 0);

        const int16_t pad   = 4;
        const int16_t innerX = cx + pad;
        const int16_t innerW = cellW - pad * 2;

        // Period name (12 pt)
        display.setFont(&FreeMonoBold12pt7b);
        int16_t y = cy + 18;
        drawWrapped(shorten(p.name, 16), innerX, y, innerW, 18);

        display.drawLine(cx + 1, y + 1, cx + cellW - 2, y + 1, 0);
        y += 6;

        // Temperature and precip % (18 pt)
        display.setFont(&FreeMonoBold18pt7b);
        y += 22;
        display.setCursor(innerX, y);
        display.print(p.temperature);
        display.print(p.tempUnit);
        if (p.precipPct > 0) {
            display.print(" ");
            display.print(p.precipPct);
            display.print("%");
        }

        // Short forecast (12 pt)
        display.setFont(&FreeMonoBold12pt7b);
        y += 20;
        drawWrapped(shorten(p.shortForecast, 20), innerX, y, innerW, 16);
    }

    epd.display(display.getBuffer());
}

// ---------------------------------------------------------------------------
// Render: clock
// ---------------------------------------------------------------------------

void renderClock()
{
    drawHeader("CLOCK");

    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);

    const char* days[]   = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun",
                             "Jul","Aug","Sep","Oct","Nov","Dec"};

    // Line 1: time in large font, centred.
    display.setFont(&FreeMonoBold24pt7b);
    char timeBuf[9];  // "HH:MM AM\0"
    int hour12 = t.tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    const char* ampm = t.tm_hour < 12 ? "AM" : "PM";
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d %s", hour12, t.tm_min, ampm);

    int16_t tx1, ty1;
    uint16_t tw, th;
    display.getTextBounds(timeBuf, 0, 0, &tx1, &ty1, &tw, &th);
    const int16_t timeX = (display.width() - static_cast<int16_t>(tw)) / 2 - tx1;
    const int16_t timeY = 110;
    display.setCursor(timeX, timeY);
    display.print(timeBuf);

    // Line 2: day of week, centred.
    display.setFont(&FreeMonoBold18pt7b);
    const char* dayStr = days[t.tm_wday];

    int16_t wx1, wy1;
    uint16_t ww, wh;
    display.getTextBounds(dayStr, 0, 0, &wx1, &wy1, &ww, &wh);
    const int16_t dayX = (display.width() - static_cast<int16_t>(ww)) / 2 - wx1;
    const int16_t dayY = timeY + 42;
    display.setCursor(dayX, dayY);
    display.print(dayStr);

    // Line 3: date (e.g. "Nov 4 2026"), centred.
    char dateBuf[16];
    snprintf(dateBuf, sizeof(dateBuf), "%s %d %04d",
             months[t.tm_mon], t.tm_mday, t.tm_year + 1900);

    int16_t dx1, dy1;
    uint16_t dw, dh;
    display.getTextBounds(dateBuf, 0, 0, &dx1, &dy1, &dw, &dh);
    const int16_t dateX = (display.width() - static_cast<int16_t>(dw)) / 2 - dx1;
    display.setCursor(dateX, dayY + 36);
    display.print(dateBuf);

    drawFooter();
    epd.display(display.getBuffer());
}

// ---------------------------------------------------------------------------
// Screen update dispatch
// ---------------------------------------------------------------------------

void updateHA()
{
    constexpr size_t numEntities = sizeof(HA_ENTITIES) / sizeof(HA_ENTITIES[0]);
    static_assert(numEntities <= MAX_ENTITIES,
                  "Too many entities in HA_ENTITIES — reduce to 6 or fewer.");

    EntityData results[MAX_ENTITIES];
    for (size_t i = 0; i < numEntities; ++i) {
        results[i] = fetchEntity(HA_ENTITIES[i].entityId, HA_ENTITIES[i].label);
        Serial.printf("HA [%s]: %s\n",
                      HA_ENTITIES[i].entityId,
                      results[i].ok ? results[i].state.c_str() : results[i].error.c_str());
    }
    renderHA(results, numEntities);
}

void updateWeather()
{
    Serial.println("Fetching NWS weather...");
    const WeatherData wx = fetchWeather();
    Serial.printf("Weather: %s\n", wx.ok ? "OK" : wx.error.c_str());
    renderWeather(wx);
}

void updateClock()
{
    Serial.println("Rendering clock");
    renderClock();
}

void showCurrentScreen()
{
    switch (SCREEN_ORDER[currentScreenIndex]) {
        case SCREEN_HA:      updateHA();      break;
        case SCREEN_WEATHER: updateWeather(); break;
        case SCREEN_CLOCK:   updateClock();   break;
    }
}

void advanceScreen()
{
    currentScreenIndex = (currentScreenIndex + 1) % NUM_SCREENS;
}

void retreatScreen()
{
    currentScreenIndex = (currentScreenIndex + NUM_SCREENS - 1) % NUM_SCREENS;
}

// Reads both nav buttons and triggers a screen change if a fresh press is
// detected.  Uses time-based debouncing: only one event is accepted per
// BTN_DEBOUNCE_MS window regardless of which button caused it.
// Returns true if a button was acted on (so loop() can skip the auto-advance
// check for that iteration).
bool checkButtons()
{
    const bool prevNow = digitalRead(BTN_PREV);
    const bool nextNow = digitalRead(BTN_NEXT);
    const uint32_t now = millis();

    // Detect a falling edge (HIGH→LOW = button pressed) on either pin.
    const bool prevPressed = (prevBtnLast == HIGH && prevNow == LOW);
    const bool nextPressed = (nextBtnLast == HIGH && nextNow == LOW);

    prevBtnLast = prevNow;
    nextBtnLast = nextNow;

    if ((prevPressed || nextPressed) &&
        static_cast<int32_t>(now - lastBtnAt) >= static_cast<int32_t>(BTN_DEBOUNCE_MS)) {
        lastBtnAt = now;

        if (prevPressed) {
            Serial.println("BTN: previous screen");
            retreatScreen();
        } else {
            Serial.println("BTN: next screen");
            advanceScreen();
        }

        showCurrentScreen();
        nextScreenAt = millis() + SCREEN_DURATION_MS;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Arduino lifecycle
// ---------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);
    delay(200);
    pinMode(EPD_MCU_POWER, OUTPUT);
    digitalWrite(EPD_MCU_POWER, HIGH);
    pinMode(EPD_POWER, OUTPUT);
    digitalWrite(EPD_POWER, HIGH);
    delay(500);
    // Navigation buttons: active-low with internal pull-up resistors.
    pinMode(BTN_PREV, INPUT_PULLUP);
    pinMode(BTN_NEXT, INPUT_PULLUP);
    epd.begin();
    Serial.println("EPD clear test starting");
    epd.clear();
    renderTestPattern();
    Serial.println("EPD test pattern displayed");
    delay(DISPLAY_TEST_HOLD_MS);

    syncNtp();

    currentScreenIndex = 0;
    showCurrentScreen();
    nextScreenAt = millis() + SCREEN_DURATION_MS;
}

void loop()
{
    // Button presses take priority; if one was handled skip the auto-advance
    // check so we don't immediately roll to the next screen.
    if (!checkButtons()) {
        if (static_cast<int32_t>(millis() - nextScreenAt) >= 0) {
            advanceScreen();
            showCurrentScreen();
            nextScreenAt = millis() + SCREEN_DURATION_MS;
        }
    }
    delay(100);
}
