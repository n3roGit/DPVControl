# Web Interface Files

This directory contains all web interface files that are loaded onto the ESP32 via LittleFS.

## File Structure
- `index.html` - Main HTML file with status and charts
- `style.css` - CSS styling for the entire interface
- `app.js` - JavaScript functionality for the interface
- `remote.html` - HTML for Remote Control tab (loaded via AJAX)
- `settings.html` - HTML for Settings tab (loaded via AJAX)
- `info.html` - HTML for Info tab (loaded via AJAX)
- `chart.min.js` - Chart.js library for data visualization
- `jszip.min.js` - JSZip library for CSV export

## Upload Process

To upload the files to the ESP32, use in PlatformIO:

```bash
pio run --target uploadfs
```

## Development Workflow

# Normal code upload (after C++ changes)
```bash
pio run --target upload
```

When making changes to HTML/CSS/JS files:
1. Edit files in `data/` directory
2. Run `pio run --target uploadfs`

**Important:** The ESP32 loads the files from LittleFS into memory at startup. After changes to web files, a restart is required.

## Size Limitations
- Total filesystem size: ~1.5MB
- Individual file size: ~500KB
- Recommended total size: <1MB

If files cannot be loaded from LittleFS, the webserver provides JavaScript fallbacks for Chart.js and JSZip. 