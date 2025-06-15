# Embedded Web Files System

This directory contains web files that are automatically embedded into the ESP32 firmware during compilation.

## How it works

1. **Pre-Build Process**: Before each compilation, the `embed_webfiles.py` script automatically:
   - Scans all files in this `/upload` directory
   - Converts them to C++ header files with PROGMEM data
   - Generates a registry system for the webserver
   - Places all generated files in `src/generated/` (ignored by git)

2. **Runtime**: The webserver serves files directly from embedded memory, eliminating the need for SPIFFS/LittleFS uploads

## Benefits

✅ **No Data Loss**: Web file updates no longer overwrite persistent ESP32 data (dive logs, settings, uptime)
✅ **Automatic**: Files are embedded during every build - no manual upload required
✅ **Faster**: Files are served from program memory instead of filesystem
✅ **Reliable**: No filesystem corruption issues
✅ **Version Control**: Web files are properly tracked in git

## File Structure

Place your web files directly in this directory:
- `index.html` - Main interface
- `style.css` - Styling
- `app.js` - JavaScript functionality
- `*.html` - Additional pages (settings, info, remote)
- `*.js` - JavaScript libraries
- `*.jpg`, `*.png` - Images
- Any other web assets

## Development Workflow

### Making Changes to Web Files

1. **Edit files** in this `/upload` directory
2. **Compile** normally: `python -m platformio run -e esp32dev`
3. **Upload** firmware: `python -m platformio run -e esp32dev -t upload`

The embedding happens automatically during compilation!

### File Size Considerations

- **Total recommended size**: < 1MB for all files combined
- **Individual file limit**: ~500KB per file
- **Current total**: ~640KB (see build output for exact size)

Large files (like chart.min.js) are automatically handled efficiently.

## Generated Files

The following files are auto-generated in `src/generated/` and ignored by git:
- `embedded_*.h` - Individual file headers
- `embedded_files_registry.h` - Registry header
- `embedded_files_registry.cpp` - Registry implementation

## Legacy Compatibility

The system maintains backward compatibility:
1. **First attempt**: Serve from embedded files
2. **Fallback**: Serve from SPIFFS/LittleFS if file not found in embedded storage

This ensures existing deployments continue working while benefiting from the new system.

## Migration from /data

The old `/data` directory is kept for reference but no longer used for active development. All files have been copied to this `/upload` directory. 