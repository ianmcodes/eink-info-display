# Repository Guidelines

## Project Structure & Module Organization

This is a PlatformIO Arduino project for the Elecrow CrowPanel 4.2-inch ESP32-S3 e-paper display.

- `src/main.cpp` contains Wi-Fi, Home Assistant REST polling, display rendering, and hardware initialization.
- `include/config.example.h` documents required local configuration. Copy it to the ignored `include/config.h` and fill in device-specific values.
- `platformio.ini` defines the board, framework, dependencies, flash layout, and monitor speed.
- `partitions_8mb.csv` defines the 8 MB flash partition table.
- `.pio/` contains generated PlatformIO build artifacts and dependencies; do not edit or commit it.
- There is currently no dedicated test directory or automated test suite.

## Build, Test, and Development Commands

Run these commands from the repository root:

```sh
pio run                         # Compile the firmware
pio run -t upload               # Compile and upload to the connected board
pio device monitor              # Open the 115200-baud serial monitor
pio run -t clean                # Remove generated build output
```

If PlatformIO cannot identify the device, provide an explicit port, for example `pio run -t upload --upload-port COM5`.

## Coding Style & Naming Conventions

Use four-space indentation, braces on the same line as declarations, and clear `camelCase` function names. Use `PascalCase` for type aliases and structs, `UPPER_SNAKE_CASE` for compile-time pin and timeout constants, and descriptive `constexpr` values for hardware configuration. Keep rendering, networking, and hardware concerns in focused functions. Follow the existing Arduino/GxEPD2 APIs and avoid unnecessary dynamic allocation in frequently called display code.

## Testing Guidelines

No automated tests are configured. At minimum, run `pio run` after source or configuration changes. Hardware changes should be validated by uploading the firmware, checking serial output at 115200 baud, and confirming that the display refreshes without `Busy Timeout!`. Test both successful and failed Home Assistant responses when changing the REST or rendering logic.

## Commit & Pull Request Guidelines

The repository has no commit history yet, so no established commit convention can be inferred. Use short, imperative subjects such as `Enable e-paper power rail` or `Handle HA request failures`. Pull requests should describe the hardware/firmware change, identify any required board revision or wiring assumptions, include build results, and attach a display photo or serial output for display-related changes.

## Security & Configuration Tips

Never commit `include/config.h`, Wi-Fi credentials, or Home Assistant tokens. Use `include/config.example.h` for safe placeholders. Rotate any token that has been exposed outside the device or local secrets storage.
