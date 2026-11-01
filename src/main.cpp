#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

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
uint32_t nextPollAt = 0;

struct EntityData {
  String state;
  String name;
  String unit;
  String error;
  bool ok = false;
};

bool isPlaceholder(const char* value)
{
  return !value || value[0] == '\0' || strstr(value, "YOUR_") ||
         strstr(value, "PASTE_") || strcmp(value, "sensor.example") == 0;
}

String shorten(const String& value, size_t maxLength)
{
  if (value.length() <= maxLength) return value;
  return value.substring(0, maxLength - 3) + "...";
}

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

EntityData fetchEntity()
{
  EntityData result;
  if (isPlaceholder(WIFI_SSID) || isPlaceholder(WIFI_PASSWORD) ||
      isPlaceholder(HA_BASE_URL) || isPlaceholder(HA_TOKEN) ||
      isPlaceholder(HA_ENTITY_ID)) {
    result.error = "Configure include/config.h first";
    return result;
  }

  if (!connectWiFi(result.error)) return result;

  const String endpoint = String(HA_BASE_URL) + "/api/states/" + HA_ENTITY_ID;
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
    result.error = "Home Assistant HTTP " + String(httpCode);
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
    result.error = String("Invalid HA response: ") + jsonError.c_str();
    return result;
  }

  result.state = document["state"].as<String>();
  result.name = document["attributes"]["friendly_name"] | HA_ENTITY_ID;
  result.unit = document["attributes"]["unit_of_measurement"].as<String>();
  if (result.state.isEmpty()) {
    result.error = "Entity returned no state";
    return result;
  }
  result.ok = true;
  return result;
}

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

void render(const EntityData& entity)
{
  display.fillScreen(1);
  display.setTextColor(0);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(12, 22);
  display.print("HOME ASSISTANT");
  display.drawLine(12, 32, display.width() - 12, 32, 0);

  int16_t y = 64;
  if (entity.ok) {
      display.setFont(&FreeMonoBold18pt7b);
      drawWrapped(shorten(entity.name, 55), 12, y, display.width() - 24, 29);
      y += 20;
      display.setFont(&FreeMonoBold24pt7b);
      display.setCursor(12, y + 40);
      display.print(shorten(entity.state, 18));
      if (!entity.unit.isEmpty()) {
        display.setFont(&FreeMonoBold18pt7b);
        display.setCursor(12, y + 72);
        display.print(shorten(entity.unit, 20));
      }
      display.setFont(&FreeMonoBold12pt7b);
      display.setCursor(12, display.height() - 12);
      display.print("Wi-Fi ");
      display.print(WiFi.RSSI());
      display.print(" dBm");
  } else {
      display.setFont(&FreeMonoBold18pt7b);
      display.setCursor(12, y);
      display.print("UPDATE FAILED");
      y += 36;
      display.setFont(&FreeMonoBold12pt7b);
      drawWrapped(entity.error, 12, y, display.width() - 24, 20);
      display.setCursor(12, display.height() - 12);
      display.print("Will retry automatically");
  }
  epd.display(display.getBuffer());
}

void updateDisplay()
{
  const EntityData entity = fetchEntity();
  Serial.printf("HA update: %s\n", entity.ok ? entity.state.c_str() : entity.error.c_str());
  render(entity);
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  pinMode(EPD_MCU_POWER, OUTPUT);
  digitalWrite(EPD_MCU_POWER, HIGH);
  pinMode(EPD_POWER, OUTPUT);
  digitalWrite(EPD_POWER, HIGH);
  delay(500);
  epd.begin();
  Serial.println("EPD clear test starting");
  epd.clear();
  renderTestPattern();
  Serial.println("EPD test pattern displayed");
  delay(DISPLAY_TEST_HOLD_MS);
  updateDisplay();
  nextPollAt = millis() + POLL_INTERVAL_MS;
}

void loop()
{
  if (static_cast<int32_t>(millis() - nextPollAt) >= 0) {
    updateDisplay();
    nextPollAt = millis() + POLL_INTERVAL_MS;
  }
  delay(100);
}
