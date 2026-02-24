#include "wifi_manager.h"

#include "log.h"
#include "settings.h"

#include <WiFi.h>

static volatile bool g_staConnected = false;
static volatile bool g_requestDisableAp = false;
static volatile bool g_requestWifiOff = false;

static unsigned long g_lastStaConnectedMs = 0;
static unsigned long g_lastStaDisconnectedMs = 0;

static void onWifiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      g_staConnected = true;
      g_lastStaConnectedMs = millis();
      g_lastStaDisconnectedMs = 0;
      if (!getApManualOverride()) {
        g_requestDisableAp = true;
      }
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      g_staConnected = false;
      g_lastStaDisconnectedMs = millis();
      break;
    default:
      break;
  }
}

static void startAccessPoint() {
  WiFi.softAPConfig(IPAddress(4, 3, 2, 1), IPAddress(4, 3, 2, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(getWifiSSID(), getWifiPassword());
  log(("AP started: " + WiFi.softAPIP().toString()).c_str());
}

void initWiFi() {
  log("initWiFi() called");

  WiFi.onEvent(onWifiEvent);

  const char* staSsid = getStaSSID();
  if (staSsid && strlen(staSsid) > 0) {
    WiFi.mode(WIFI_AP_STA);
    startAccessPoint();
    log(("STA connect: " + String(staSsid)).c_str());
    WiFi.begin(staSsid, getStaPassword());
  } else {
    WiFi.mode(WIFI_AP);
    startAccessPoint();
  }
}

static void disableAccessPointIfRequested() {
  if (!g_requestDisableAp) {
    return;
  }

  if (getApManualOverride()) {
    g_requestDisableAp = false;
    return;
  }

  // Only disable AP if STA is connected.
  if (!g_staConnected) {
    return;
  }

  log("Disabling AP because STA is connected");
  WiFi.softAPdisconnect(true);
  g_requestDisableAp = false;
}

static void disableWifiIfRequested() {
  if (!g_requestWifiOff) {
    return;
  }

  log("Disabling WiFi (WIFI_OFF)");
  WiFi.mode(WIFI_OFF);
  g_requestWifiOff = false;
}

void handleWifiLifetime() {
  disableAccessPointIfRequested();
  disableWifiIfRequested();

  const uint16_t minutes = getApAutoOffMinutes();
  if (minutes == 0) {
    return;
  }

  const char* staSsid = getStaSSID();
  if (!staSsid || strlen(staSsid) == 0) {
    // No STA configured => keep AP as usual.
    return;
  }

  if (g_staConnected) {
    return;
  }

  if (g_lastStaDisconnectedMs == 0) {
    // If we never saw a disconnect event, use boot time as reference.
    g_lastStaDisconnectedMs = millis();
  }

  unsigned long elapsedMs = millis() - g_lastStaDisconnectedMs;
  unsigned long timeoutMs = (unsigned long)minutes * 60UL * 1000UL;
  if (elapsedMs >= timeoutMs) {
    g_requestWifiOff = true;
  }
}

String getWifiModeString() {
  wifi_mode_t mode = WiFi.getMode();
  switch (mode) {
    case WIFI_OFF:
      return "off";
    case WIFI_STA:
      return "sta";
    case WIFI_AP:
      return "ap";
    case WIFI_AP_STA:
      return "ap+sta";
    default:
      return "unknown";
  }
}

String getStaStatusString() {
  const char* staSsid = getStaSSID();
  if (!staSsid || strlen(staSsid) == 0) {
    return "disabled";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return "connected";
  }
  return "disconnected";
}

String getApStatusString() {
  if (WiFi.getMode() == WIFI_OFF) {
    return "off";
  }
  if (WiFi.getMode() == WIFI_STA) {
    return "disabled";
  }

  // softAPIP is 0.0.0.0 when AP is not running.
  IPAddress ip = WiFi.softAPIP();
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
    return "disabled";
  }
  return "active";
}

String getStaIpString() {
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }
  return WiFi.localIP().toString();
}

String getApIpString() {
  IPAddress ip = WiFi.softAPIP();
  if (ip[0] == 0 && ip[1] == 0 && ip[2] == 0 && ip[3] == 0) {
    return "";
  }
  return ip.toString();
}
