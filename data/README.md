# DPVControl Web Interface Files

Dieses Verzeichnis enthält alle Web-Interface-Dateien, die über LittleFS auf den ESP32 geladen werden.

## Dateien

- `index.html` - Haupt-HTML-Datei mit Status- und Charts-Tabs
- `style.css` - CSS-Styling für das gesamte Interface
- `app.js` - JavaScript-Funktionalität für das Interface
- `remote.html` - HTML für Remote Control Tab (wird via AJAX geladen)
- `settings.html` - HTML für Settings Tab (wird via AJAX geladen)
- `info.html` - HTML für Info Tab (wird via AJAX geladen)
- `chart.min.js` - Chart.js Bibliothek für Datenvisualisierung
- `jszip.min.js` - JSZip Bibliothek für CSV-Export
- `version.txt` - Versionsinformation

## Upload zu ESP32

Um die Dateien auf den ESP32 zu übertragen, verwende in PlatformIO:

```bash
# Upload des Dateisystems (muss vor dem ersten Upload gemacht werden)
pio run --target uploadfs

# Normaler Code-Upload (nach Änderungen am C++-Code)
pio run --target upload
```

## Entwicklung

Bei Änderungen an den HTML/CSS/JS-Dateien:

1. Dateien in diesem Verzeichnis bearbeiten
2. `pio run --target uploadfs` ausführen
3. ESP32 neustarten

**Wichtig:** Der ESP32 lädt die Dateien beim Start aus LittleFS in den Speicher. Nach Änderungen an den Web-Dateien muss ein Neustart erfolgen.

## Größenbeschränkungen

- LittleFS Partition: ~1.5MB (je nach ESP32-Konfiguration)
- Einzelne Dateien: Sollten unter 100KB bleiben
- Gesamte Web-Assets: Sollten unter 1MB bleiben

## Fallback-System

Falls Dateien nicht von LittleFS geladen werden können, stellt der Webserver JavaScript-Fallbacks für Chart.js und JSZip bereit. 