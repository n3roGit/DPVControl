# Web Server Tests

This directory contains tests for the web server functionality of the DPV Control system.

## Test Structure

The tests are organized into several categories:

### Hardware Simulation
- `mock_hardware.h/cpp`: Mock implementations of hardware components
  - LED states and control
  - Motor control
  - Beeper control
  - Water sensors
  - Button states
  - LED bar (20 LEDs, split into two sections)
  - Front lamp PWM control
  - Click code functionality

### API Tests
- `test_api.cpp`: Tests for all API endpoints
  - Status endpoints
  - Data endpoints
  - Session management
  - Settings
  - Motor control
  - Lamp control
  - Beeper control
  - Version info
  - Chart data generation
  - Remote control functionality

## Test Categories

### LED Bar Tests
- Individual LED control
- Section-based control (2 sections of 10 LEDs each)
- Brightness control per section
- Pattern setting and verification
- Error cases and edge conditions

### Front Lamp PWM Tests
- PWM value setting and reading
- Duty cycle conversion
- Frequency control
- Resolution changes
- Value limits and validation

### Click Code Tests
- Basic click code functionality
- Code recording and verification
- Timeout handling
- Common click patterns
- Edge cases (very short/long clicks)
- Maximum code length

### Session Tests
- Session storage and retrieval
- CSV export functionality
- Session list management
- Data pagination
- Date range filtering

### Chart Tests
- Data format conversion
- Time range filtering
- Empty data handling
- Invalid session handling

### Remote Control Tests
- Motor control
- Lamp control
- Beeper control
- Water sensor interaction
- Button interaction
- State persistence

### Settings Tests
- Settings retrieval and update
- Settings validation
- Settings restore
- Settings persistence
- Water sensor interaction
- Button interaction

### Status Tests
- Status information retrieval
- Sensor value reporting
- System state reporting
- Water sensor states
- Button states

### Info Tests
- Version information
- Network status
- Hardware configuration
- Memory state
- Different version types

## Running Tests

To run the tests:

```bash
pio test -e native
```

## Test Coverage

The tests cover:
- All API endpoints
- Hardware interactions
- Data management
- User interface functionality
- Error handling
- Edge cases
- State management

## Adding New Tests

When adding new tests:
1. Add mock implementations in `mock_hardware.h/cpp` if needed
2. Create test cases in `test_api.cpp`
3. Follow the existing test patterns
4. Include error cases and edge conditions
5. Update this README if necessary

## Test Maintenance

- Run tests after any code changes
- Update tests when API or functionality changes
- Fix test failures before proceeding with changes
- Keep mock implementations in sync with real hardware 