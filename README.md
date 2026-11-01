# Elecrow 4.2-inch Home Assistant e-paper display

Firmware for the Elecrow CrowPanel 4.2-inch E-paper HMI with an
ESP32-S3-WROOM-1-N8R8. It connects to Wi-Fi, reads one Home Assistant entity
from the REST API, and redraws the 400x300 black-and-white screen on a fixed
interval.

The display wiring is set to Elecrow's published V1.0 SSD1683 pin map:
SCK 12, MOSI 11, RST 47, DC 46, CS 45, and BUSY 48. GPIO7 is the display
power-enable pin and is driven high before initialization. The firmware uses
the GxEPD2 SSD1683 driver for the GDEY042T81 panel.

## Configure

1. Install [PlatformIO](https://platformio.org/) (the VS Code extension is the
   easiest option).
2. Copy `include/config.example.h` to `include/config.h`.
3. Set the Wi-Fi credentials, base URL, long-lived Home Assistant access token,
   and entity ID in `config.h`.

Create a token in Home Assistant at **Profile → Security → Long-Lived Access
Tokens**. It needs access to the entity being displayed. Keep `config.h`
private; it is already in `.gitignore`.

For a typical local installation, use:

```cpp
constexpr char HA_BASE_URL[] = "http://homeassistant.local:8123";
constexpr char HA_ENTITY_ID[] = "sensor.living_room_temperature";
```

If your Home Assistant uses HTTPS with a self-signed local certificate, set
`HA_ALLOW_INSECURE_TLS` to `true`. Prefer a certificate trusted by the device
for any network beyond a trusted home LAN.

## Build and flash

Connect the panel over USB-C and run:

```sh
pio run -t upload
pio device monitor
```

If automatic upload cannot find the port, append (for example)
`--upload-port COM5`. Serial output reports each Home Assistant fetch, while
the display shows either the entity's friendly name, state, and unit, or a
useful connection/API error.

## Notes

- The first update occurs after boot; later ones use `POLL_INTERVAL_MS`, five
  minutes by default. E-paper redraws are visibly slow, so avoid aggressive
  polling.
- The state endpoint is `GET /api/states/<entity_id>` and uses an `Authorization:
  Bearer <token>` header.
- Elecrow has released at least two display-driver revisions. This project is
  for the vendor's V1.0 SSD1683 map. A newer V1.2/green-sticker board uses a
  UC8276C controller and requires the matching `GxEPD2_420_SE0420NQ04` driver;
  the SPI/control pins are otherwise the same. Check the board revision before
  changing the driver.
