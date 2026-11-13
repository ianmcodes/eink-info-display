#pragma once

// ---------------------------------------------------------------------------
// BLE GATT provisioning service for the e-paper display.
//
// Uses NimBLE-Arduino v2.x.  The service is intentionally started and stopped
// alongside the Config screen so BLE power is not wasted while displaying
// other screens.
// ---------------------------------------------------------------------------

// Start BLE advertising as "EinkDisplay".  Safe to call multiple times; a
// second call while already running is a no-op.
void bleConfigBegin();

// Stop BLE advertising and release NimBLE resources.  Call when navigating
// away from the Config screen.
void bleConfigEnd();

// Returns true if at least one GATT client is currently connected.
bool bleConfigHasClients();

// Returns true (once) if any characteristic was written since the last call.
// The caller should reload gConfig and re-render the current screen on true.
bool bleConfigWasUpdated();
