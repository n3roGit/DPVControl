#pragma once

#include <Arduino.h>

// WiFi manager for AP + optional STA mode.
//
// - If staSSID is empty: AP-only (current behavior)
// - If STA connects: optionally disable AP (unless manual override)
// - If STA disconnected for apAutoOffMinutes: optionally disable WiFi entirely

void initWiFi();
void handleWifiLifetime();

// Status helpers for web UI
String getWifiModeString();
String getStaStatusString();
String getApStatusString();
String getStaIpString();
String getApIpString();
