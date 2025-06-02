# Webserver Refactoring

## Overview of Changes

### Current Issues
- Hard to maintain and unclear
- Large memory usage due to string literals

### Improvements
- Significantly reduced `webserver.cpp` size
- Better separation of concerns
- Improved maintainability

### New Structure
```
data/
├── index.html    # Main HTML file
├── style.css     # CSS styles
├── app.js        # Main JavaScript
├── chart.min.js  # Chart.js for data visualization
└── jszip.min.js  # JSZip for CSV export
```

### Key Changes
- Main HTML loads basic structure
- CSS: Styling and layout
- JavaScript: Functionality and interaction

### Implementation
- Files are transferred to ESP32 at build time
- Webserver loads files directly from filesystem
- Fallback system for missing files

### Usage
# Upload filesystem (after HTML changes)
```bash
pio run --target uploadfs
```

# Upload firmware (after C++ changes)
```bash
pio run --target upload
```

## Development Benefits
1. **Better Tools**: Syntax highlighting for HTML/CSS/JS
2. **Easier Maintenance**: Changes without C++ recompilation
3. **Faster Development**: Live preview possible
4. **Version Control**: Better diff view for web changes

## Backward Compatibility
- All API endpoints remain unchanged
- Same functionality as before
- JavaScript fallbacks for Chart.js and JSZip

## Next Steps
1. Migrate more JavaScript functions from original code
2. Add Progressive Web App (PWA) features
3. Add CSS framework for better mobile support

## Git Commit Message

```
Refactor webserver: Migrate to LittleFS-based HTML files

- Move HTML/CSS/JavaScript from embedded code to separate files
- Implement modular tab loading via AJAX
- Reduce webserver.cpp from 4400 to 1600 lines (-63%)
- Add build documentation for LittleFS workflow
- Maintain full API compatibility and functionality
``` 