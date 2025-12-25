# CLAUDE.MD - AI Assistant Context for Fien's lights

This document provides technical context for AI assistants (like Claude Code) working on this project. It explains architecture, implementation details, and common modification patterns.

## Project Overview

**Type:** Arduino NeoPixel LED controller with real-time effects
**Platform:** PlatformIO (Arduino Nano)
**Language:** C++ (Arduino framework)
**Main File:** `src/main.cpp` (~540 lines)

**Purpose:** Control WS2812B LED strip with 8 different effects, adjustable via 3 potentiometers and 1 button.

## Architecture

### Hardware Interface
- **NeoPixel Strip**: Pin 6 (via 470Ω resistor), 12 LEDs (configurable via `LED_COUNT`)
  - **470Ω Resistor**: Between Pin 6 and LED data line (signal protection)
  - **Capacitor (220µF-1000µF)**: Across LED strip 5V/GND (power stability, prevents brownouts)
    - 220µF-470µF suitable for ≤12 LEDs; 1000µF for larger setups/longer wires
- **Button**: Pin 2 with internal pull-up (active LOW)
- **Potentiometers** (all 10kΩ):
  - A0: Brightness (POT_MIN-POT_MAX → 0-255)
  - A1: Hue/Warmth (POT_MIN-POT_MAX → 0-65535 or RGB values)
  - A2: Speed (POT_MIN-POT_MAX → 1000-10ms delay, inverted for intuitive control)
  - **Calibration**: POT_MIN and POT_MAX compensate for hardware tolerances (pots rarely reach exact 0/1023)

### Software Architecture

```
main.cpp
├── Configuration Defines (lines 4-22)
│   ├── Hardware pins (LED, button, pots)
│   └── Potentiometer calibration (POT_MIN, POT_MAX)
├── Helper Functions (lines 28-88)
│   ├── readBrightnessFromPot() → uint8_t
│   ├── readHueFromPot() → uint16_t
│   └── readSpeedFromPot() → uint16_t
├── Effect Functions (lines 90-558)
│   ├── effectOff()
│   ├── whiteLight()
│   ├── solidHue()
│   ├── pulseHue()
│   ├── chaseHue()
│   ├── rainbowFade()
│   ├── fireEffect()
│   └── whiteFastFlicker()
├── setup() (lines 428-459)
└── loop() (lines 461-end)
    ├── Button debouncing & effect switching
    └── Effect dispatcher (switch statement)
```

## Key Technical Details

### 1. ADC Crosstalk Prevention

**Problem:** Reading multiple analog pins quickly causes crosstalk (values from one pin affect another).

**Solution (lines 30-34, 51-55, 73-77):**
```cpp
for (int i = 0; i < 3; i++) {
    analogRead(POT_PIN);  // Dummy reads
}
delayMicroseconds(100);  // ADC settling time
int raw = analogRead(POT_PIN);  // Actual read
```

**Critical:** Do NOT remove these dummy reads or reduce delays without testing. Removing them will cause potentiometer values to jump erratically.

### 2. Potentiometer Calibration (POT_MIN / POT_MAX)

**Problem:** Potentiometers have hardware tolerances and rarely reach the theoretical full range (0-1023). This means some effect values become unreachable (e.g., Inferno fire palette, coolest white temperature, full brightness).

**Solution (lines 16-19):**
```cpp
#define POT_MIN 15    // Actual minimum value pots reach
#define POT_MAX 1000  // Actual maximum value pots reach
```

**Applied in all helper functions and special cases:**
- `readBrightnessFromPot()` (line 50): `map(filtered, POT_MIN, POT_MAX, 0, 255)`
- `readHueFromPot()` (line 73): `map(filtered, POT_MIN, POT_MAX, 0, 65535)`
- `readSpeedFromPot()` (line 96): `map(filtered, POT_MIN, POT_MAX, 1000, 10)`
- `whiteLight()` (line 120): Maps raw warmth value to 0-1023 for gradient calculations
- `fireEffect()` (line 441): Maps hue pot to palette selection (0-5)

**Important:** Each mapping is followed by `constrain()` to ensure values stay within bounds even if pot readings go slightly outside the calibrated range.

**To calibrate for specific hardware:**
1. Upload code and open Serial Monitor
2. Turn each pot to minimum and maximum positions
3. Note the actual raw values (not the mapped values)
4. Update POT_MIN and POT_MAX defines accordingly
5. Default values (15, 1000) work for most 10kΩ linear potentiometers

**Critical effects that require calibration:**
- **Fire Effect**: Without proper calibration, palette 5 (Inferno) may be unreachable
- **White Light**: Without calibration, coolest white temperatures (daylight) may be unreachable
- **All effects**: Without calibration, true minimum/maximum values may be inaccessible

### 3. Exponential Moving Average (EMA) Smoothing

**Applied to all potentiometer readings (lines 47, 70, 93):**
```cpp
static int filtered = initial_value;
filtered = (filtered * 7 + raw) / 8;
```

**Ratio:** 7/8 provides good balance between responsiveness and stability.
- More responsive: 3/4 (faster response, more jitter)
- More stable: 15/16 (slower response, less jitter)

**Important:** Each helper function maintains its own `static` filtered value that persists between calls.

### 4. Effect System Pattern

**All effects follow this pattern:**
1. Read raw potentiometer values (for debug output)
2. Call helper functions to get smoothed/mapped values
3. Debug output (every 1000ms)
4. Apply effect logic
5. **Apply gamma correction** with `strip.gamma32(color)`
6. Update strip with `strip.show()`
7. Delay (usually `delay(speed)` or fixed)

**Static variables in effects:**
- Effects use `static` variables to maintain state between loop iterations
- Example: `pulseHue()` uses `static uint8_t brightness` to track fade position
- **Important:** When switching effects, static variables retain their values

### 5. Button Debouncing

**Implementation (lines 461-520):**
- Uses dual-state tracking: `reading` (raw) and `buttonState` (stable)
- 50ms debounce delay
- Only triggers on HIGH→LOW transition (button press, not release)
- 300ms delay after effect switch prevents multiple triggers

**DO NOT** simplify this logic - it's required for reliable button operation.

### 6. White Light Color Temperature

**8-point gradient (lines 123-180):**
- Raw pot value is mapped from POT_MIN-POT_MAX to 0-1023 (line 120) to ensure full temperature range
- Then mapped through 7 if/else blocks with RGB gradient calculations
- 5 warm points (0-585), 3 cool points (585-1023)
- Each segment maps RGB channels independently
- RGB values are scientifically chosen for realistic color temperatures

**To modify:** Adjust boundary values (146, 292, 438, 585, 731, 877) and RGB endpoints.

### 7. Gamma Correction

**Applied to all effects** for perceptually linear brightness.

**Purpose:** Compensates for the non-linear relationship between:
- Digital LED values (0-255)
- Actual LED brightness output
- Human brightness perception (logarithmic)

**Implementation:**
```cpp
uint32_t color = strip.ColorHSV(hue, 255, 255);
color = strip.gamma32(color);  // Apply gamma=2.6 correction
strip.setPixelColor(i, color);
```

**Benefits:**
- **Smoother fades**: Pulse and Rainbow effects fade perceptually evenly
- **Better color accuracy**: RGB ratios remain consistent at all brightness levels
- **Realistic fire**: Low-brightness flickers show better detail
- **Accurate white temperature**: Color temperature gradient maintains warmth appearance

**Where applied:**
- solidHue() - line 233
- pulseHue() - line 283
- chaseHue() - line 344
- rainbowFade() - lines 393-400 (manual rainbow with gamma)
- fireEffect() - line 554
- whiteFastFlicker() - line 590
- whiteLight() - line 196

**Note:** rainbowFade() manually implements the rainbow pattern (instead of using `strip.rainbow()`) to allow gamma correction on each pixel.

**To disable:** Comment out all `strip.gamma32()` calls. Colors will appear brighter at mid-values but fades will look uneven.

## Common Modification Patterns

### Adding a New Effect

1. **Increment NUM_EFFECTS** (line 17)
   ```cpp
   #define NUM_EFFECTS 9  // was 8
   ```

2. **Create effect function** (after line 558)
   ```cpp
   void myNewEffect()
   {
     // Read pots
     int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
     int rawHue = analogRead(POT_PIN_HUE);
     int rawSpeed = analogRead(POT_PIN_SPEED);

     uint8_t brightness = readBrightnessFromPot();
     uint16_t hue = readHueFromPot();
     uint16_t speed = readSpeedFromPot();

     // Debug output
     static unsigned long lastPrint = 0;
     if (millis() - lastPrint > 1000)
     {
       Serial.print("[My Effect] Brightness pot: ");
       Serial.print(rawBrightness);
       Serial.print(" -> ");
       Serial.print(brightness);
       // ... add more debug info
       Serial.println();
       lastPrint = millis();
     }

     // Effect logic here
     uint32_t color = strip.ColorHSV(hue, 255, 255);
     color = strip.gamma32(color);  // Apply gamma correction
     strip.setPixelColor(0, color);

     strip.show();
     delay(speed);
   }
   ```

3. **Add to switch statement** (around line 680)
   ```cpp
   case 8:
     myNewEffect();
     break;
   ```

4. **Add effect name** (around line 638)
   ```cpp
   case 8:
     Serial.println("My New Effect");
     break;
   ```

### Changing Speed Range

Edit `readSpeedFromPot()` line 96:
```cpp
// Current: 10-1000ms (inverted - pot left = slow, pot right = fast)
uint16_t speed = map(filtered, POT_MIN, POT_MAX, 1000, 10);

// Overall slower effects:
uint16_t speed = map(filtered, POT_MIN, POT_MAX, 2000, 50);

// Overall faster effects:
uint16_t speed = map(filtered, POT_MIN, POT_MAX, 500, 5);
```

**Important:** The speed mapping is inverted (high pot value = low delay = fast) for intuitive control. The return type is `uint16_t` to accommodate delays >255ms.

### Changing LED Count

Edit line 6:
```cpp
#define LED_COUNT 24  // was 12
```

**Note:** Ensure adequate power supply for more LEDs (60mA per LED at full white).

### Adjusting Smoothing Responsiveness

Edit the ratio in helper functions (lines 47, 70, 93):
```cpp
// Current (balanced):
filtered = (filtered * 7 + raw) / 8;

// More responsive (faster, more jitter):
filtered = (filtered * 3 + raw) / 4;

// More stable (slower, less jitter):
filtered = (filtered * 15 + raw) / 16;
```

### Calibrating Potentiometers

If certain effect ranges are unreachable (e.g., can't select Inferno palette, can't reach coolest white), calibrate the pot range:

1. **Test current range:**
   - Upload code, open Serial Monitor
   - Turn each pot fully left, note lowest raw value
   - Turn each pot fully right, note highest raw value

2. **Update defines (lines 18-19):**
   ```cpp
   #define POT_MIN 15    // Your lowest observed value
   #define POT_MAX 1000  // Your highest observed value
   ```

3. **Verify calibration:**
   - Turn pots to extremes again
   - Mapped values should now reach true min/max (0, 255, 65535, etc.)
   - Fire effect should cycle through all 6 palettes
   - White light should reach both very warm and very cool

**Default values (15, 1000)** work for most 10kΩ linear potentiometers, but adjust if needed.

## Debug Output Format

**Standard format (consistent across all effects):**
```
[Effect Name] Brightness pot: RAW -> CALCULATED | Hue pot: RAW -> CALCULATED | Speed pot: RAW -> CALCULATED
```

**Example:**
```
[Pulse Hue] Brightness pot: 512 -> Max: 128, Current: 64 | Hue pot: 300 -> 19200 | Speed pot: 750 -> 83
```

**Debug timing:** All effects output every 1000ms (line 160, 200, 246, 303, 360, 402)

## Important Constraints

### 1. Memory Limitations (Arduino Nano)
- **SRAM:** 2KB total, ~1.5KB available after globals
- Each static variable in effects uses SRAM
- Avoid:
  - Large arrays
  - String concatenation
  - Recursive functions
  - Dynamic memory allocation

### 2. NeoPixel Timing
- `strip.show()` blocks for ~30μs per LED (360μs for 12 LEDs)
- Delay between updates prevents flickering
- Minimum recommended: 10ms between `strip.show()` calls

### 3. Serial Output
- Baud rate: 9600 (line 430)
- Excessive Serial.print() calls can slow effects
- Debug output limited to 1Hz per effect

## Testing Guidelines

### When Making Changes

1. **Test potentiometer readings first:**
   - Open Serial Monitor
   - Turn each pot individually
   - Verify values range 0-1023
   - Check for crosstalk (one pot affecting another)

2. **Test effect transitions:**
   - Cycle through all effects with button
   - Verify each effect starts correctly
   - Check for LED artifacts during transitions

3. **Test edge cases:**
   - Pot at minimum (0)
   - Pot at maximum (1023)
   - Rapid pot changes
   - Rapid button presses

### Common Issues & Fixes

**Issue:** Potentiometer values jump randomly
- **Cause:** ADC crosstalk or loose wiring
- **Fix:** Check dummy reads and delays in helper functions

**Issue:** Button triggers multiple times
- **Cause:** Insufficient debouncing
- **Fix:** Increase `debounceDelay` (line 25) or add post-switch delay (line 208)

**Issue:** Effects lag when changing pots
- **Cause:** Smoothing too aggressive
- **Fix:** Reduce ratio in EMA (use 3/4 instead of 7/8)

**Issue:** Colors don't match expected values
- **Cause:** Color order configuration or gamma correction disabled
- **Fix:** Verify `NEO_GRB` flag (line 19), ensure `strip.gamma32()` calls are present in effects

**Issue:** Can't reach certain effect values (e.g., Inferno palette, coolest white, full brightness)
- **Cause:** Potentiometer hardware tolerances prevent reaching theoretical 0/1023 range
- **Fix:** Calibrate POT_MIN and POT_MAX defines (lines 18-19) using Serial Monitor readings

## Code Style Notes

- **Indentation:** 2 spaces (consistent throughout)
- **Braces:** Opening brace on same line for functions/conditionals
- **Naming:**
  - Functions: camelCase (e.g., `readBrightnessFromPot`)
  - Constants: UPPER_SNAKE_CASE (e.g., `POT_PIN_BRIGHTNESS`)
  - Variables: camelCase (e.g., `rawBrightness`)
- **Comments:** Use for complex logic, not obvious statements

## Libraries Used

**Adafruit_NeoPixel.h** (required)
- Version: 1.x compatible
- Key functions used:
  - `strip.begin()`
  - `strip.show()`
  - `strip.clear()`
  - `strip.setBrightness(0-255)`
  - `strip.setPixelColor(index, r, g, b)` or `(index, color)`
  - `strip.ColorHSV(hue, sat, val)` - hue: 0-65535, sat/val: 0-255
  - `strip.Color(r, g, b)` - creates 32-bit color from RGB values
  - `strip.gamma32(color)` - gamma correction (gamma=2.6, applied to all effects)

## Performance Notes

- **Loop frequency:** Variable, 1-100Hz depending on speed pot setting (10-1000ms delay)
- **Button check frequency:** Every loop iteration
- **Pot read frequency:** Every effect iteration (includes smoothing)
- **Serial output frequency:** 1Hz per effect
- **Memory usage:** ~40% SRAM, ~30% Flash (with 8 effects)

## Future Enhancement Ideas

Common requests that can be implemented:

1. **Save last effect to EEPROM** (persist across power cycles)
2. **Add more white temperature points** (modify whiteLight() function)
3. **Implement effect presets** (save pot positions per effect)
4. **Add brightness fade on effect switch** (smooth transitions)
5. **Add "rainbow chase" effect** (combine chase + rainbow)
6. **Two-button control** (up/down instead of cycle)
7. **Add IR remote support** (requires IR receiver library)

## Version History Context

This project evolved through these key stages:
1. Initial rainbow effect only
2. Added button switching between effects
3. Added potentiometer control (with crosstalk issues)
4. Fixed ADC crosstalk with dummy reads and delays
5. Added white light with color temperature
6. Expanded to 7 effects with comprehensive debugging
7. Added extensive documentation (README.md, CLAUDE.MD)
8. Added hardware protection (470Ω resistor, 220µF-1000µF capacitor)
9. Improved speed control: inverted mapping (10-1000ms) for intuitive feel
10. Added fire effect with 6 color palettes (classic, hot, toxic, purple, ice, inferno)
11. Implemented gamma correction (gamma=2.6) across all effects for perceptually linear brightness
12. Added potentiometer calibration (POT_MIN/POT_MAX) to compensate for hardware tolerances and ensure full range accessibility

## Quick Reference: Pin Assignments

| Component | Pin | Direction | Notes |
|-----------|-----|-----------|-------|
| NeoPixel Data | D6 | Output | Via 470Ω resistor; PWM capable, but not used for PWM |
| Button | D2 | Input | Pull-up enabled, active LOW |
| Brightness Pot | A0 | Input | Analog 0-1023 |
| Hue Pot | A1 | Input | Analog 0-1023 |
| Speed Pot | A2 | Input | Analog 0-1023, inverted mapping (low=slow) |

## Critical: Before Committing Changes

- [ ] Test all 8 effects
- [ ] Verify Serial Monitor output
- [ ] Check potentiometer responsiveness
- [ ] Verify pot calibration allows reaching all effect ranges (Inferno palette, coolest white, etc.)
- [ ] Test button switching
- [ ] Verify no compiler warnings
- [ ] Check SRAM usage (`pio run -t size` or Arduino IDE output)
- [ ] Update README.md if adding features
- [ ] Update this file if changing architecture

---

**For AI Assistants:** When modifying this code, prioritize stability and user experience. The ADC crosstalk fix, smoothing algorithms, and potentiometer calibration system are critical - don't simplify them without explicit user request and thorough testing. When adding new effects or modifying existing ones that use raw analog reads, always use POT_MIN/POT_MAX for mapping instead of hardcoded 0/1023 values. Always maintain the consistent debug output format and effect structure patterns.
