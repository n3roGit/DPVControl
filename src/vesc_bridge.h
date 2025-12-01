/*
 * VESC Bridge - Bluetooth to VESC UART bridge control
 *
 * This module provides a runtime-toggleable bridge between the ESP32
 * Bluetooth SPP interface and the VESC UART port. It is designed so that:
 *  - The bridge code is always compiled into the firmware.
 *  - The bridge is disabled on boot and only enabled on demand via WebUI.
 *  - When enabled, the regular VESC control loop is paused so that the
 *    VESC Tool can control the motor directly via the bridge.
 */

#ifndef VESC_BRIDGE_H
#define VESC_BRIDGE_H

#include <Arduino.h>

// Returns true if the firmware supports the VESC bridge feature.
// In the current ESP32 build this always returns true, but having
// a function allows future builds to disable the feature at compile time.
bool vescBridgeIsSupported();

// Returns true if the VESC bridge is currently active and forwarding
// data between Bluetooth and the VESC UART.
bool vescBridgeIsActive();

// Enable the VESC bridge.
// Returns true on success, false if the bridge could not be started.
// On success, the normal VESC control loop is paused automatically.
bool vescBridgeEnable();

// Disable the VESC bridge and release all associated resources.
// After this call the normal VESC control loop is resumed.
void vescBridgeDisable();

#endif

