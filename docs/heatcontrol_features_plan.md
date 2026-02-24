# HeatControl-Features in DPVControl übernehmen

> **GitHub Issues:** Alle Features sind als Issues angelegt. Siehe:
> [#81](https://github.com/BubTec/DPVControl/issues/81) Verzögerter Neustart |
> [#82](https://github.com/BubTec/DPVControl/issues/82) Cache-Control-Header |
> [#83](https://github.com/BubTec/DPVControl/issues/83) Serial Log |
> [#84](https://github.com/BubTec/DPVControl/issues/84) Dynamisches Log-Level |
> [#85](https://github.com/BubTec/DPVControl/issues/85) Erweiterte Captive-URLs |
> [#86](https://github.com/BubTec/DPVControl/issues/86) Serial-Log-Panel |
> [#87](https://github.com/BubTec/DPVControl/issues/87) Client-Zugriffskontrolle |
> [#88](https://github.com/BubTec/DPVControl/issues/88) i18n DE/EN |
> [#90](https://github.com/BubTec/DPVControl/issues/90) HeimNetz + Auto AP |
> [#89](https://github.com/BubTec/DPVControl/issues/89) OTA Update

---

## Übersicht beider Projekte

| Aspekt             | HeatControl                        | DPVControl                        |
| ------------------ | ---------------------------------- | --------------------------------- |
| **Plattform**      | ESP32-C3                           | ESP32 (esp32dev)                  |
| **Webserver**      | AsyncWebServer (ESPAsyncWebServer) | Synchroner WebServer (WiFiServer) |
| **WiFi**           | WIFI_AP_STA (AP + HeimNetz/STA)    | WIFI_AP (nur AP)                  |
| **Storage**        | EEPROM                             | LittleFS (JSON)                   |
| **OTA**            | `/update` mit multipart upload     | Kein OTA                          |
| **Serial Log**     | Rolling buffer + `/logs` Endpoint  | Nur Serial.print, kein Buffer     |
| **AP-Abschaltung** | Auto-Off nach X Min ohne STA       | Nicht vorhanden                   |

---

## 1. Verzögerter Neustart (scheduleRestart) – Risiko 1/10 → [#81](https://github.com/BubTec/DPVControl/issues/81)

**HeatControl:** `scheduleRestart(delayMs)` – Neustart erst nach X ms, damit HTTP-Response vollständig gesendet wird.

**DPVControl:** Sofortiger `ESP.restart()` nach `/api/reboot`; Response könnte abgeschnitten werden.

**Übernahme:** Reboot um 1–2 Sekunden verzögern; in der Zwischenzeit weiterhin Requests bedienen. Globale Variablen `volatile bool restartScheduled`, `volatile unsigned long restartAtMs` – werden vom Webserver (Core 0) gesetzt, von loop() (Core 1) gelesen.

**Risiko: 1/10** – Sehr gering. Verbessert Zuverlässigkeit der Reboot-Response.

---

## 2. Cache-Control-Header – Risiko 1/10 → [#82](https://github.com/BubTec/DPVControl/issues/82)

**HeatControl:** `DefaultHeaders::Instance().addHeader("Cache-Control", "no-cache, no-store, must-revalidate")` für alle Responses.

**DPVControl:** Teilweise Cache-Header; nicht einheitlich.

**Übernahme:** Bei allen API-Responses und HTML-Seiten konsistent `Cache-Control: no-cache` setzen.

**Risiko: 1/10** – Sehr gering. Reine Header-Ergänzung.

---

## 3. Serial Log – Risiko 2/10 → [#83](https://github.com/BubTec/DPVControl/issues/83)

**HeatControl:** Rolling-Buffer (`serialLogBuffer[12001]`), `appendLineToRollingBuffer()` in logic_helpers.cpp, alle `logLine()`/`logf()` schreiben zusätzlich in den Buffer, Endpoint `/logs` liefert Buffer als text/plain.

**DPVControl:** [log.cpp](../src/log.cpp) nutzt nur `Serial.print()`; kein Buffer, kein Log-Endpoint.

**Übernahme:**

- Rolling-Buffer-Funktion portieren (ohne HeatControl-spezifische Abhängigkeiten) oder vereinfachte Variante in DPVControl implementieren
- **Mutex** für Buffer-Zugriff (log() wird von Core 0 und Core 1 aufgerufen – webserver, vesc, datalog, main loop)
- `log()` in [log.cpp](../src/log.cpp) erweitern: zusätzlich in Buffer schreiben
- Neuer API-Endpoint `/api/logs` (GET) – liefert Buffer als text/plain
- [api-specification.yaml](../api-specification.yaml) und Web-UI anpassen (z.B. Log-Tab oder -Modal)

**Risiko: 2/10** – Gering. Mutex erforderlich wegen Multi-Core-Zugriff.

---

## 4. Dynamisches Log-Level (setLogLevel) – Risiko 2/10 → [#84](https://github.com/BubTec/DPVControl/issues/84)

**HeatControl:** Log-Level (Error/Info/Debug) per POST `/setLogLevel` umschaltbar, persistent in EEPROM.

**DPVControl:** `debugLoggingEnabled` (boolean) in Settings, nur über Settings-API änderbar.

**Übernahme:** Entweder `debugLoggingEnabled` beibehalten oder dreistufiges Log-Level einführen. Neuer Endpoint `/api/log-level` (GET/POST) oder Erweiterung von Settings.

**Web-UI:** Dropdown/Select (Error/Info/Debug) in Settings oder im Serial-Log-Panel (Feature 6); Speichern-Button.

**Risiko: 2/10** – Gering. Bestehende Logik anpassen; Debug-Logging könnte Performance beeinträchtigen.

---

## 5. Erweiterte Captive-Portal-URLs – Risiko 2/10 → [#85](https://github.com/BubTec/DPVControl/issues/85)

**HeatControl:** Zusätzliche Handler: `/gen_204`, `/fwlink`, `/library/test/success.html`, `/connecttest.txt.gz`, etc. Plus `CaptiveRequestHandler` für unbekannte Hosts.

**DPVControl:** Hat bereits `/generate_204`, `/ncsi.txt`, `/hotspot-detect.html`, etc. Fehlt u.a. `/gen_204`, `/fwlink`, einige Varianten.

**Übernahme:** Fehlende Pfade ergänzen; ggf. Host-basierte Redirect-Logik nachbilden.

**Risiko: 2/10** – Gering. Nur zusätzliche Redirect-Pfade; Verbesserung der Captive-Portal-Erkennung.

---

## 6. Serial-Log-Panel in der Web-UI – Risiko 2/10 → [#86](https://github.com/BubTec/DPVControl/issues/86)

**HeatControl:** Eigenes Panel mit Log-Level-Dropdown, Auto-Scroll, periodischem Abruf von `/logs`.

**DPVControl:** Kein Log-Panel.

**Übernahme:** Nach Implementierung von Serial Log (Feature 3) ein Log-Panel/Modal in der Web-UI einbauen – analog HeatControl. **Abhängigkeit:** Feature 3 muss zuerst implementiert sein.

**Web-UI:** Log-Anzeige (pre/textarea), Log-Level-Dropdown, Auto-Scroll, Aktualisieren-Button.

**Risiko: 2/10** – Gering. Abhängig von Feature 3; nur Frontend.

---

## 7. Client-Zugriffskontrolle (isAllowedWebClient) – Risiko 3/10 → [#87](https://github.com/BubTec/DPVControl/issues/87)

**HeatControl:** Schreibende Endpoints (POST/PUT) prüfen Client-IP. Erlaubt: 4.3.2.x (AP), 10.x, 172.16–31.x, 192.168.x. Andere IPs erhalten 403 Forbidden.

**DPVControl:** Keine Zugriffskontrolle – jeder im AP kann alle Endpoints nutzen.

**Übernahme:** Vor schreibenden API-Handlern Client-IP prüfen; bei ungültiger IP 403 zurückgeben. DPVControl nutzt `client.remoteIP()` – analog implementierbar.

**Risiko: 3/10** – Gering. Nur zusätzliche Prüfung; falsche Konfiguration könnte legitime Clients blockieren.

---

## 8. Mehrsprachigkeit (i18n DE/EN) – Risiko 4/10 → [#88](https://github.com/BubTec/DPVControl/issues/88)

**HeatControl:** Vollständige Übersetzung in der Web-UI (data-i18n, `t()`-Funktion, langToggleBtn). DE/EN wechselbar.

**DPVControl:** Web-UI nur auf Englisch (bzw. gemischt).

**Übernahme:** i18n-Objekt mit Keys, `t(key)`-Funktion, data-i18n-Attribute, Sprachumschalter. Relativ viel UI-Arbeit.

**Web-UI:** Sprachumschalter-Button (DE/EN); alle Texte mit data-i18n versehen.

**Risiko: 4/10** – Mittel. Kein Funktionsrisiko, aber viele Strings; fehlende Keys zeigen Key-Namen.

---

## 9. HeimNetz + Auto AP Abschaltung – Risiko 5/10 → [#90](https://github.com/BubTec/DPVControl/issues/90)

**HeatControl:**

- WIFI_AP_STA: AP und STA gleichzeitig
- STA verbindet sich mit HeimNetz (activeSsid/activePassword)
- Wenn STA verbunden und AP nicht manuell aktiviert → AP wird abgeschaltet
- `apAutoOffMinutes`: Wenn nach X Minuten kein STA → AP aus, WiFi komplett aus

**DPVControl:**

- Nur AP; `wifiSSID`/`wifiPassword` sind AP-Credentials
- Keine STA-Verbindung, kein Auto-Off

**Übernahme:**

- **Neue Settings:** `staSSID`, `staPassword` (HeimNetz), `apAutoOffMinutes` (0 = deaktiviert)
- **AP-Credentials:** Bleiben `wifiSSID`/`wifiPassword` (oder umbenennen zu `apSSID`/`apPassword` für Klarheit)
- **WiFi-Start:** `WiFi.mode(WIFI_AP_STA)` wenn STA-Credentials gesetzt, sonst weiterhin nur AP
- **STA-Connect:** `WiFi.begin(staSSID, staPassword)` im Setup
- **Loop-Logik (wie HeatControl):**
  - Wenn STA verbunden und AP nicht manuell aktiviert → `WiFi.softAPdisconnect()`, `WiFi.mode(WIFI_STA)` → **primär über HeimNetz erreichbar**
  - Wenn `apAutoOffMinutes > 0` und kein STA nach Timeout → AP aus, ggf. WiFi komplett aus
- **API:** Settings um neue Felder erweitern; optional `/api/wifi` oder ähnlich für Status (STA verbunden, AP an/aus)
- **Web-UI:** Neue WLAN-Sektion mit Eingabefeldern (HeimNetz-SSID, HeimNetz-Passwort, AP-SSID, AP-Passwort, AP Auto-Off in Min), Speichern-Button; Status-Anzeige (STA verbunden?, AP aktiv?, IP-Adressen)

**Architektur-Anpassung:** DPVControl macht WiFi-Setup aktuell in `webserverTask`. Für WIFI_AP_STA und handleWifiLifetime in loop() empfohlen: WiFi-Setup in `setup()` vor `setupWebserver()` verschieben (analog HeatControl). Task übernimmt nur server.begin() und Request-Loop.

**Risiko: 5/10** – Mittelhoch. WiFi-Verhalten ändert sich; Captive Portal und DNS müssen weiterhin korrekt funktionieren. Gründlich testen.

---

## 10. Auto Update – Risiko 8/10 → [#89](https://github.com/BubTec/DPVControl/issues/89)

**HeatControl:**

- AsyncWebServer mit multipart-Upload-Callback
- `/update` GET: HTML-Formular
- `/update` POST (multipart): Chunkweiser Upload, `Update.begin()`, `Update.write()`, `Update.end()`
- Web-UI: GitHub-Release-Check, Download firmware.bin, POST an `/update`

**DPVControl:**

- Synchroner WebServer; kein nativer multipart-Upload-Handler
- Kein OTA-Endpoint

**Entscheidung:** OTA auf dem bestehenden sync WebServer (Option B). Kein Wechsel zu AsyncWebServer – der aktuelle WebServer funktioniert zuverlässig. Multipart-Parsing per `client.read()` in Chunks, direkt an `Update.write()`.

**Übernahme:**

- `/update` GET: Einfache HTML-Seite mit File-Input (analog HeatControl)
- `/update` POST: Multipart-Parsing im sync WebServer, `Update`-API nutzen
- **Web-UI:** Buttons „Update prüfen“, „Auto-Update“, „Update-Seite öffnen“; Update-Banner bei neuer Version (analog HeatControl)
- **Firmware-URL:** GitHub-Release `BubTec/DPVControl` (analog HeatControl mit BubTec/HeatControl)

**Risiko: 8/10** – Hoch. OTA-Fehler können Brick verursachen. Mindestgröße prüfen, nur .bin akzeptieren, Tests mit Mock.

---

## Zusammenfassung: Risiko-Matrix (sortiert gering → hoch)

| #   | Feature                  | Issue | Risiko | Kurzbeschreibung                       |
| --- | ------------------------ | ----- | ------ | -------------------------------------- |
| 1   | Verzögerter Neustart     | #81   | 1      | scheduleRestart(delayMs)               |
| 2   | Cache-Control-Header     | #82   | 1      | Konsistente No-Cache-Header            |
| 3   | Serial Log               | #83   | 2      | Rolling-Buffer + /api/logs             |
| 4   | Dynamisches Log-Level    | #84   | 2      | Error/Info/Debug umschaltbar           |
| 5   | Erweiterte Captive-URLs  | #85   | 2      | gen_204, fwlink, etc.                  |
| 6   | Serial-Log-Panel (UI)    | #86   | 2      | Log-Anzeige in Web-UI                  |
| 7   | Client-Zugriffskontrolle | #87   | 3      | 403 für nicht-lokale IPs               |
| 8   | i18n DE/EN               | #88   | 4      | Mehrsprachige Web-UI                   |
| 9   | HeimNetz + Auto AP       | #90   | 5      | WIFI_AP_STA, STA-Credentials, Auto-Off |
| 10  | Auto Update              | #89   | 8      | OTA multipart, Brick-Risiko            |

---

## Empfohlene Reihenfolge (nach Risiko)

**Phase 1 – Risiko 1–2 (Features 1–6):**

1. Verzögerter Neustart (#81)
2. Cache-Control-Header (#82)
3. Serial Log (#83)
4. Dynamisches Log-Level (#84)
5. Erweiterte Captive-URLs (#85)
6. Serial-Log-Panel in Web-UI (#86)

**Phase 2 – Risiko 3–5 (Features 7–9):**

7. Client-Zugriffskontrolle (#87)
8. i18n DE/EN (#88) – optional, Aufwand relativ hoch
9. HeimNetz + Auto AP (#90)

**Phase 3 – Risiko 8 (Feature 10):**

10. Auto Update (#89) – zuletzt, gründlich testen

---

## Feature-Abhängigkeiten

| Feature                     | Issue | Abhängigkeiten             | Hinweis                                  |
| --------------------------- | ----- | -------------------------- | ---------------------------------------- |
| 1. Verzögerter Neustart     | #81   | keine                      |                                          |
| 2. Cache-Control-Header     | #82   | keine                      |                                          |
| 3. Serial Log               | #83   | keine                      | Backend-Basis für Feature 6              |
| 4. Dynamisches Log-Level    | #84   | keine                      | Kann mit Feature 6 kombiniert werden     |
| 5. Erweiterte Captive-URLs  | #85   | keine                      |                                          |
| 6. Serial-Log-Panel         | #86   | **#83** (Serial Log)       | Braucht `/api/logs` Endpoint             |
| 7. Client-Zugriffskontrolle | #87   | keine                      |                                          |
| 8. i18n DE/EN               | #88   | keine                      |                                          |
| 9. HeimNetz + Auto AP       | #90   | keine                      | Benötigt eigene WLAN-Sektion in Settings |
| 10. Auto Update             | #89   | keine                      |                                          |

**Reihenfolge bei Abhängigkeiten:** #86 erst nach #83 implementieren.

---

## Tiefenanalyse: Risiken, Abhängigkeiten und Konfliktstellen

### 1. Threading und Concurrency (Core 0 vs. Core 1)

| Komponente      | Core | Aufrufer von log()                                         |
| --------------- | ---- | ---------------------------------------------------------- |
| main loop()     | 1    | buttonLoop, motorLoop, checkForLeak, logVehicleState, etc. |
| webserverTask   | 0    | handleClient → alle API-Handler                            |
| vescTask        | 0    | VESC-Kommunikation, battery updates                        |
| datalogger task | 0    | datalogLoop                                                |

**Kritisch – Serial Log (#83):** `log()` wird von **beiden Cores** aufgerufen. Der Rolling-Buffer (`serialLogBuffer`) wird damit **concurrent** beschrieben. Ohne Synchronisation: Datenraces, korrupter Buffer.

**Lösung:** Mutex (xSemaphoreCreateMutex) um `appendLineToRollingBuffer()`; alternativ portMUX für kurze kritische Sektion.

**Kritisch – scheduleRestart (#81):** `restartScheduled` und `restartAtMs` werden vom Webserver (Core 0) gesetzt, von `loop()` (Core 1) gelesen. **Lösung:** `volatile` oder atomare Zugriffe.

### 2. WiFi-Architektur: DPVControl vs. HeatControl

| Aspekt      | HeatControl                     | DPVControl                                               |
| ----------- | ------------------------------- | -------------------------------------------------------- |
| WiFi-Setup  | In `setup()` vor setupWebServer | **In webserverTask** (nach Task-Start)                   |
| Reihenfolge | setup() → WiFi → setupWebServer | setup() → setupWebserver() → Task startet → WiFi in Task |

**Problem bei #90 (HeimNetz + Auto AP):** In DPVControl macht `webserverTask` den kompletten WiFi-Setup. Für WIFI_AP_STA brauchen wir:

- `WiFi.onEvent()` für staConnected
- `handleWifiLifetime()` in `loop()` (Core 1) – ruft `WiFi.softAPdisconnect()` auf

**Lösung:** WiFi-Setup in `setup()` vor `setupWebserver()` verschieben; Task übernimmt nur `server.begin()` und Request-Loop.

### 3. Konflikt-Matrix

| Feature A           | Feature B             | Konflikt                                                                                         |
| ------------------- | --------------------- | ------------------------------------------------------------------------------------------------ |
| 3 (Serial Log)      | 4 (Log-Level)         | Keiner – Feature 4 steuert, was Feature 3 puffert                                                |
| 3 (Serial Log)      | 6 (Log-Panel)         | Keiner – 6 nutzt 3                                                                               |
| 9 (HeimNetz)        | 5 (Captive-URLs)      | Wenn AP aus: Captive-Portal nicht mehr relevant (nur STA)                                        |
| 9 (HeimNetz)        | 7 (Zugriffskontrolle) | isAllowedWebClient muss STA-IP (192.168.x) erlauben – bereits in 10.x, 172.16–31.x, 192.168.x    |
| 1 (scheduleRestart) | 10 (OTA)              | Nach OTA: Reboot. scheduleRestart verzögert – OTA sollte eigenes Reboot nach Update.end() nutzen |

### 4. Offene Punkte (entschieden)

1. **AsyncWebServer vs. sync WebServer für OTA:** Der bestehende sync WebServer kann OTA mit manuellem Multipart-Parsing. **OTA auf dem bestehenden WebServer** (Option B). Multipart-Parsing per `client.read()` in Chunks, direkt an `Update.write()`.
2. **HeimNetz-Priorität:** Wenn STA verbunden und AP nicht manuell aktiviert → AP wird abgeschaltet. **Primär über HeimNetz erreichbar**.
3. **Firmware-URL für Auto-Update:** GitHub-Release-URL fest auf `BubTec/DPVControl`.

---

## Weitere Dokumentation

- [feature_issues_to_create.md](feature_issues_to_create.md) – Manuelle Issue-Vorlagen (falls GitHub CLI nicht verfügbar)
- [.cursor/rules](../.cursor/rules) – Projektregeln für Implementierung
