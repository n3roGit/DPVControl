# Create GitHub feature request issues for HeatControl features in DPVControl
# Requires: GitHub CLI (gh) - install via: winget install GitHub.cli
# Run: .\create_feature_issues.ps1

$ErrorActionPreference = "Stop"
$repo = "BubTec/DPVControl"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Host "GitHub CLI (gh) is not installed. Install via: winget install GitHub.cli" -ForegroundColor Red
    Write-Host "Alternatively, use the content from docs/feature_issues_to_create.md to create issues manually on GitHub." -ForegroundColor Yellow
    exit 1
}

$issues = @(
    @{
        title = "[FEATURE] Delayed restart (scheduleRestart) for reliable reboot response - PRIO 1"
        body = @"
**Problem Description**
After `/api/reboot`, the ESP32 restarts immediately. The HTTP response may be cut off before the client receives it, causing the UI to show an error.

**Proposed Solution**
Delay reboot by 1–2 seconds. Set global `volatile bool restartScheduled` and `volatile unsigned long restartAtMs` in the reboot handler; check in `loop()` and call `ESP.restart()` when time has elapsed. Reference: HeatControl `scheduleRestart()`.

**Technical Requirements**
- Software: `webserver.cpp` (reboot handler), `main.cpp` (loop check)
- New globals: `restartScheduled`, `restartAtMs` (volatile)

**Acceptance Criteria**
- [ ] Reboot handler sends 200 response, sets restartScheduled, returns without immediate restart
- [ ] loop() checks flag and restarts after delay (e.g. 1200 ms)
- [ ] API spec unchanged; existing tests pass

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- API spec unchanged; ensure existing /api/reboot tests still pass.
- Run `run_all_tests.ps1` before submitting.
- loop() runs on Core 1; restart check there is fine.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Consistent Cache-Control headers for API and HTML - PRIO 1"
        body = @"
**Problem Description**
API and HTML responses lack consistent Cache-Control headers; browsers may cache stale data.

**Proposed Solution**
Add `Cache-Control: no-cache, no-store, must-revalidate` to all HTTP responses (API JSON, HTML, embedded files). Modify `sendHttpResponse()`, `sendEmbeddedResponse()`, and any other response paths.

**Technical Requirements**
- Software: `webserver.cpp`, `embedded_webserver.cpp`

**Acceptance Criteria**
- [ ] All API responses include Cache-Control header
- [ ] Embedded file serving includes Cache-Control header
- [ ] No functional change; only headers added

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Find all response paths: sendHttpResponse(), sendEmbeddedResponse(), and any other code that sends HTTP headers.
- No API spec or version change (headers only).
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Serial log buffer and /api/logs endpoint - PRIO 2"
        body = @"
**Problem Description**
Log output is only visible via Serial; no way to view logs from the web UI. Useful for remote debugging.

**Proposed Solution**
Add rolling buffer (~12 KB), extend `log()` to append to buffer (with Mutex for multi-core safety). New endpoint `GET /api/logs` returns buffer as text/plain. Reference: HeatControl `logic_helpers::appendLineToRollingBuffer`, `serialLogBuffer`.

**Technical Requirements**
- Software: `log.cpp`, `log.h`, `webserver.cpp`
- Mutex for buffer (log() called from Core 0 and Core 1)
- api-specification.yaml: new path `/api/logs`

**Acceptance Criteria**
- [ ] All log() calls write to buffer (with mutex)
- [ ] GET /api/logs returns recent log lines as text/plain
- [ ] API spec updated; unit test for /api/logs
- [ ] No heap fragmentation; buffer fixed-size

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Use existing log() helper; extend it to append to buffer. Do NOT add new Serial.print() calls.
- Mutex required: log() is called from Core 0 and Core 1.
- API: add /api/logs to api-specification.yaml, increment API version.
- Add API test: success case (valid response) and error case (if applicable).
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Dynamic log level (Error/Info/Debug) - PRIO 2"
        body = @"
**Problem Description**
Only boolean `debugLoggingEnabled` exists; no granular control (Error/Info/Debug).

**Proposed Solution**
Add log level (Error=0, Info=1, Debug=2). New endpoint `/api/log-level` GET/POST or extend settings. Map existing `debugLoggingEnabled`: true→Info, false→Error. Web-UI: Dropdown in Settings or Log-Panel.

**Technical Requirements**
- Software: `log.cpp`, `settings.cpp`, `webserver.cpp`, `upload/` (Web-UI)
- Settings: new field `logLevel` or replace `debugLoggingEnabled`

**Acceptance Criteria**
- [ ] Log level persistent in settings
- [ ] Only messages at or below current level are logged
- [ ] Web-UI control to change level
- [ ] Migration: existing users get Info as default

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Use existing log() helper; filter by level inside log().
- If new endpoint /api/log-level: add to api-specification.yaml, increment API version, add API tests (success + error).
- If extending settings only: update settings schema in api-spec, add tests.
- upload/ changes: run `python embed_webfiles.py`, then full PlatformIO rebuild.
- Settings migration: map debugLoggingEnabled true→Info, false→Error.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Additional captive portal detection URLs - PRIO 2"
        body = @"
**Problem Description**
Some devices (e.g. certain Android versions) use different URLs for captive portal detection. DPVControl has `/generate_204`, `/ncsi.txt`, etc., but misses `/gen_204`, `/fwlink`, and others.

**Proposed Solution**
Add redirect handlers for: `/gen_204`, `/fwlink`, `/library/test/success.html`, `/connecttest.txt.gz`, `/connecttest.txt/index.htm`, `/connecttest.txt/index.htm.gz`. Reference: HeatControl web_server.cpp lines 789–799.

**Technical Requirements**
- Software: `webserver.cpp` (path handling in handleClient)

**Acceptance Criteria**
- [ ] All listed paths redirect to portal root (4.3.2.1)
- [ ] No regression for existing captive portal behavior

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Add handlers in webserver.cpp (handleClient path handling).
- No API spec change; consider adding test for new paths and regression for existing captive portal.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Serial log panel in web UI - PRIO 2"
        body = @"
**Problem Description**
Even with /api/logs, users need a UI to view logs. Currently no log viewer.

**Proposed Solution**
Add panel/modal with log display (pre/textarea), Log-Level dropdown, Auto-Scroll checkbox, Refresh button. Periodically fetch /api/logs. **Depends on #83** (Serial log buffer and /api/logs endpoint).

**Technical Requirements**
- Software: `upload/index.html`, `upload/app.js` (or equivalent)
- Run `embed_webfiles.py` after changes

**Acceptance Criteria**
- [ ] Log panel shows content from /api/logs
- [ ] Auto-scroll and manual refresh work
- [ ] Log-level control (or links to dynamic log level issue)

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- **Depends on #83** (Serial log buffer and /api/logs endpoint).
- upload/ changes: run `python embed_webfiles.py`, then full PlatformIO rebuild.
- Never edit src/generated/ directly.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] Client IP access control for write endpoints - PRIO 3"
        body = @"
**Problem Description**
All API endpoints are accessible from any client on the AP. Write endpoints (POST) should be restricted to local networks to reduce attack surface.

**Proposed Solution**
Before processing write requests (POST/PUT), check `client.remoteIP()`. Allow: 4.3.2.x (AP subnet), 10.x, 172.16–31.x, 192.168.x. Return 403 Forbidden for others. Reference: HeatControl `isAllowedWebClient()`.

**Technical Requirements**
- Software: `webserver.cpp` (helper function, call before each write handler)

**Acceptance Criteria**
- [ ] Local clients (AP, STA) can use write endpoints
- [ ] Non-local IPs receive 403
- [ ] Read endpoints (GET) remain unrestricted (or document if restricted)

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Allow: 4.3.2.x (AP subnet), 10.x, 172.16–31.x, 192.168.x. Return 403 Forbidden for others.
- Document 403 in api-specification.yaml for affected write endpoints.
- Add API tests: local IP succeeds, non-local IP returns 403.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] i18n: German/English language toggle - PRIO 4"
        body = @"
**Problem Description**
Web UI is English-only (or mixed). German-speaking users would benefit from DE/EN switch.

**Proposed Solution**
Add i18n object with keys, `t(key)` function, data-i18n attributes on elements. Language toggle button. Reference: HeatControl `upload/index.html` (i18n object, langToggleBtn).

**Technical Requirements**
- Software: All files in `upload/` with user-facing text
- Run `embed_webfiles.py` after changes

**Acceptance Criteria**
- [ ] DE and EN translations for all UI strings
- [ ] Toggle persists (localStorage or similar)
- [ ] New strings use i18n; no hardcoded language-specific text

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- upload/ changes: run `python embed_webfiles.py`, then full PlatformIO rebuild.
- Never edit src/generated/ directly.
- Reference: HeatControl upload/index.html (i18n object, langToggleBtn).
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] HeimNetz (STA) + Auto AP shutdown - PRIO 5"
        body = @"
**Problem Description**
DPVControl is AP-only. Users with home WiFi want to connect via STA (HeimNetz) and optionally auto-disable AP to save power. Behavior like HeatControl.

**Proposed Solution**
Add STA (station) mode so the device can join a home network. When STA is connected, optionally disable AP to save power. When no STA for a configurable timeout, optionally disable WiFi entirely.

**Settings (new fields in settings.cpp/settings.h)**
- `staSSID` (String): Home network SSID. Empty = HeimNetz disabled, AP-only (current behavior).
- `staPassword` (String): Home network password.
- `apAutoOffMinutes` (uint16_t): Minutes without STA connection before disabling WiFi. 0 = disabled (AP stays on).
- `apManualOverride` (bool, optional): If true, keep AP on even when STA connected (e.g. for captive portal during setup).

**Boot & Connection Flow**
1. In setup(): Call initWiFi() before webserverTask starts. If staSSID non-empty: WiFi.mode(WIFI_AP_STA), start AP (existing logic), then WiFi.begin(staSSID, staPassword).
2. Register WiFi.onEvent() for ARDUINO_EVENT_WIFI_STA_CONNECTED. On connect: if !apManualOverride, schedule AP disable (e.g. set flag, disable in next loop iteration).
3. In loop(): handleWifiLifetime() – check apAutoOffMinutes timer; if STA disconnected for > apAutoOffMinutes, call WiFi.mode(WIFI_OFF) or similar.
4. webserverTask: Move AP setup into a shared initWiFi() called from setup(). webserverTask only handles HTTP, not WiFi init.

**Architecture**
- Move WiFi setup from webserverTask (line ~1906 in webserver.cpp) to setup() in main.cpp.
- New handleWifiLifetime() in main.cpp loop() – runs on Core 1 (fine for LED rule).
- WiFi.onEvent callback runs in WiFi task context; only set flags, do actual AP disable in loop() or webserverTask to avoid reentrancy.

**Web-UI (upload/)**
- New "WLAN" or "Network" section in Settings.
- Input: SSID (text), Password (password), AP auto-off (number, minutes; 0=disabled).
- Checkbox: "Keep AP on" (apManualOverride) for captive portal during first-time setup.
- Status display: "Connected to &lt;SSID&gt;" or "AP only", STA IP if connected, AP IP if AP active.

**Edge Cases**
- STA connect fails: Retry with backoff; show status in Web-UI. AP remains for configuration.
- User clears staSSID: On next settings save, stop STA, revert to AP-only.
- apAutoOffMinutes elapses: Disable WiFi. User must power-cycle or use physical access to re-enable (or add wake-on-WiFi if feasible).

**Reference**
- HeatControl: web_server.cpp (WiFi setup), settings for staSSID/staPassword/apAutoOffMinutes, handleWifiLifetime in loop.

**Technical Requirements**
- Software: `main.cpp`, `webserver.cpp`, `settings.cpp`, `settings.h`, `upload/`
- api-specification.yaml: extend settings schema with staSSID, staPassword, apAutoOffMinutes (and apManualOverride if added)
- Migration: empty staSSID = no HeimNetz; apAutoOffMinutes=0 = disabled

**Acceptance Criteria**
- [ ] STA connects when credentials set; AP disabled when STA connected (unless apManualOverride)
- [ ] AP auto-off after apAutoOffMinutes when no STA
- [ ] Web-UI: WLAN config (SSID, password, auto-off), status (connected/AP-only, IPs)
- [ ] Captive portal still works when AP active
- [ ] Backwards compatible: empty staSSID = current AP-only behavior

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- **LED rule:** handleWifiLifetime() in loop() (Core 1) is fine. Do NOT trigger LED updates from webserverTask (Core 0).
- Settings migration: empty staSSID = no HeimNetz, apAutoOffMinutes=0 = disabled.
- Update api-specification.yaml settings schema, increment API version, add API tests for new settings.
- upload/ changes: run `python embed_webfiles.py`, then full PlatformIO rebuild.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    },
    @{
        title = "[FEATURE] OTA firmware update via web - PRIO 8"
        body = @"
**Problem Description**
Firmware updates require USB/serial connection. OTA allows updates via web browser.

**Proposed Solution**
Add `/update` GET (HTML form) and POST (multipart upload). Use existing sync WebServer; parse multipart manually with `client.read()` in chunks; stream to `Update.write()`. Chunk size ~4 KB. Min firmware size check (~100 KB). Only .bin accepted. Web-UI: ""Update prüfen"", ""Auto-Update"", ""Update-Seite öffnen""; update banner. Firmware URL: `BubTec/DPVControl` GitHub releases.

**Technical Requirements**
- Software: `webserver.cpp`, `upload/`, `api-specification.yaml`
- Include `<Update.h>` (ESP32)
- Chunk streaming; no full firmware in RAM
- yield()/vTaskDelay(0) during upload to avoid watchdog

**Acceptance Criteria**
- [ ] GET /update serves HTML form
- [ ] POST /update accepts multipart .bin, flashes, reboots
- [ ] Invalid/small files rejected
- [ ] Web-UI: update check, auto-update from GitHub
- [ ] Unit test with mock (no real flash)
- [ ] API spec updated

**Implementation Notes**
- See .cursor/rules and README for project conventions.
- **Plan:** [docs/heatcontrol_features_plan.md](https://github.com/BubTec/DPVControl/blob/main/docs/heatcontrol_features_plan.md) – full context, dependencies, risk analysis.
- Add /update to api-specification.yaml, increment API version.
- Chunk streaming (~4 KB); no full firmware in RAM. yield()/vTaskDelay(0) during upload to avoid watchdog.
- Unit test: use mock for Update.write(); no real flash in tests.
- upload/ changes: run `python embed_webfiles.py`, then full PlatformIO rebuild.
- Run `run_all_tests.ps1` before submitting.
"@
        labels = "enhancement"
    }
)

$created = @()
foreach ($issue in $issues) {
    Write-Host "Creating: $($issue.title)" -ForegroundColor Cyan
    $bodyFile = [System.IO.Path]::GetTempFileName()
    $issue.body | Out-File -FilePath $bodyFile -Encoding utf8
    try {
        $result = gh issue create --repo $repo --title $issue.title --body-file $bodyFile --label $issue.labels
        $created += $result
        Write-Host "  Created: $result" -ForegroundColor Green
    } catch {
        Write-Host "  Failed: $_" -ForegroundColor Red
    } finally {
        Remove-Item $bodyFile -ErrorAction SilentlyContinue
    }
}

Write-Host "`nCreated $($created.Count) issues." -ForegroundColor Green
