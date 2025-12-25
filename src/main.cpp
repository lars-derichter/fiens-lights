#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// NeoPixel configuration
#define LED_PIN 6
#define LED_COUNT 12

// Button configuration
#define BUTTON_PIN 2

// Potentiometer configuration+
#define POT_PIN_BRIGHTNESS A0
#define POT_PIN_HUE A1
#define POT_PIN_SPEED A2

// Potentiometer calibration (actual min/max values due to hardware tolerance)
// Adjust these values based on your specific potentiometers
#define POT_MIN 15    // Typical low-end value (instead of 0)
#define POT_MAX 1000  // Typical high-end value (instead of 1023)

// Number of effects available
#define NUM_EFFECTS 8

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// Global variables
uint8_t currentEffect = 0;
bool lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

// --- Helper: read brightness knob and convert to brightness (0..255) ---
uint8_t readBrightnessFromPot()
{
  // Multiple dummy reads to fully settle ADC when switching pins
  for (int i = 0; i < 3; i++)
  {
    analogRead(POT_PIN_BRIGHTNESS);
  }
  delayMicroseconds(100);

  // Read raw pot value (0..1023)
  int raw = analogRead(POT_PIN_BRIGHTNESS);

  // Simple smoothing (exponential moving average)
  static int filtered = 512; // Start at mid brightness
  filtered = (filtered * 7 + raw) / 8;

  // Map actual pot range to 0..255 (NeoPixel brightness is 8-bit)
  uint8_t brightness = map(filtered, POT_MIN, POT_MAX, 0, 255);
  brightness = constrain(brightness, 0, 255); // Ensure we stay within bounds
  return brightness;
}

// --- Helper: read knob and convert to hue (0..65535) ---
uint16_t readHueFromPot()
{
  // Multiple dummy reads to fully settle ADC when switching pins
  for (int i = 0; i < 3; i++)
  {
    analogRead(POT_PIN_HUE);
  }
  delayMicroseconds(100);

  // Read raw pot value (0..1023)
  int raw = analogRead(POT_PIN_HUE);

  // Simple smoothing (exponential moving average)
  static int filtered = 0;
  filtered = (filtered * 7 + raw) / 8;

  // Map actual pot range to 0..65535 (NeoPixel HSV hue is 16-bit)
  uint32_t hue = map(filtered, POT_MIN, POT_MAX, 0, 65535);
  hue = constrain(hue, 0, 65535); // Ensure we stay within bounds
  return (uint16_t)hue;
}

// --- Helper: read speed knob and convert to delay time (10..1000ms) ---
uint16_t readSpeedFromPot()
{
  // Multiple dummy reads to fully settle ADC when switching pins
  for (int i = 0; i < 3; i++)
  {
    analogRead(POT_PIN_SPEED);
  }
  delayMicroseconds(100);

  // Read raw pot value (0..1023)
  int raw = analogRead(POT_PIN_SPEED);

  // Simple smoothing (exponential moving average)
  static int filtered = 512; // Start at mid speed
  filtered = (filtered * 7 + raw) / 8;

  // Map actual pot range to 1000..10 (delay in ms - lower = slower, higher = faster)
  uint16_t speed = map(filtered, POT_MIN, POT_MAX, 1000, 10);
  speed = constrain(speed, 10, 1000); // Ensure we stay within bounds
  return speed;
}

// Effect 0: Off
void effectOff()
{
  strip.clear();
  strip.setBrightness(0);
  strip.show();
  delay(100);
}

// Effect 1: White light with warmth control
void whiteLight()
{
  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawWarmth = analogRead(POT_PIN_HUE);

  uint8_t brightness = readBrightnessFromPot();

  // Use hue pot to control warmth (0 = very warm, 1023 = very cool)
  // Map actual pot range to 0-1023 to ensure full temperature range is accessible
  int warmth = map(rawWarmth, POT_MIN, POT_MAX, 0, 1023);
  warmth = constrain(warmth, 0, 1023);

  // Map warmth to RGB values with 8 points (5 warm, 3 cool)
  // Point 0 (0): Very warm candlelight (255, 147, 41)
  // Point 1 (146): Warm amber (255, 169, 87)
  // Point 2 (292): Warm (255, 197, 143)
  // Point 3 (438): Soft warm (255, 214, 170)
  // Point 4 (585): Warm white (255, 241, 224) - warmest midpoint
  // Point 5 (731): Slightly cool (245, 243, 255)
  // Point 6 (877): Cool (225, 235, 255)
  // Point 7 (1023): Very cool daylight (201, 226, 255)
  uint8_t r, g, b;

  if (warmth < 146)
  {
    // Very warm to warm amber (0-146)
    r = 255;
    g = map(warmth, 0, 146, 147, 169);
    b = map(warmth, 0, 146, 41, 87);
  }
  else if (warmth < 292)
  {
    // Warm amber to warm (146-292)
    r = 255;
    g = map(warmth, 146, 292, 169, 197);
    b = map(warmth, 146, 292, 87, 143);
  }
  else if (warmth < 438)
  {
    // Warm to soft warm (292-438)
    r = 255;
    g = map(warmth, 292, 438, 197, 214);
    b = map(warmth, 292, 438, 143, 170);
  }
  else if (warmth < 585)
  {
    // Soft warm to warm white (438-585)
    r = 255;
    g = map(warmth, 438, 585, 214, 241);
    b = map(warmth, 438, 585, 170, 224);
  }
  else if (warmth < 731)
  {
    // Warm white to slightly cool (585-731)
    r = map(warmth, 585, 731, 255, 245);
    g = map(warmth, 585, 731, 241, 243);
    b = map(warmth, 585, 731, 224, 255);
  }
  else if (warmth < 877)
  {
    // Slightly cool to cool (731-877)
    r = map(warmth, 731, 877, 245, 225);
    g = map(warmth, 731, 877, 243, 235);
    b = 255;
  }
  else
  {
    // Cool to very cool (877-1023)
    r = map(warmth, 877, 1023, 225, 201);
    g = map(warmth, 877, 1023, 235, 226);
    b = 255;
  }

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[White Light] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> ");
    Serial.print(brightness);
    Serial.print(" | Warmth pot: ");
    Serial.print(warmth);
    Serial.print(" -> RGB(");
    Serial.print(r);
    Serial.print(",");
    Serial.print(g);
    Serial.print(",");
    Serial.print(b);
    Serial.println(")");
    lastPrint = millis();
  }

  // Create color with gamma correction
  uint32_t color = strip.Color(r, g, b);
  color = strip.gamma32(color);

  // Set all pixels to the white color
  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, color);
  }

  strip.setBrightness(brightness);
  strip.show();
  delay(10);
}

// Effect 2: Knob controls hue (solid color)
void solidHue()
{
  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawHue = analogRead(POT_PIN_HUE);

  uint16_t hue = readHueFromPot();
  uint8_t brightness = readBrightnessFromPot();

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[Solid Hue] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> ");
    Serial.print(brightness);
    Serial.print(" | Hue pot: ");
    Serial.print(rawHue);
    Serial.print(" -> ");
    Serial.println(hue);
    lastPrint = millis();
  }

  // Full saturation & value gives vivid color
  uint32_t c = strip.ColorHSV(hue, 255, 255);

  // Gamma correction for perceptually linear brightness
  c = strip.gamma32(c);

  strip.setBrightness(brightness);

  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, c);
  }

  strip.show();
  delay(10);
}

// Effect 3: Pulse with hue control
void pulseHue()
{
  static uint8_t brightness = 0;
  static int8_t fadeAmount = 5;

  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawHue = analogRead(POT_PIN_HUE);
  int rawSpeed = analogRead(POT_PIN_SPEED);

  uint16_t hue = readHueFromPot();
  uint8_t maxBrightness = readBrightnessFromPot();
  uint8_t speed = readSpeedFromPot();

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[Pulse Hue] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> Max: ");
    Serial.print(maxBrightness);
    Serial.print(", Current: ");
    Serial.print(brightness);
    Serial.print(" | Hue pot: ");
    Serial.print(rawHue);
    Serial.print(" -> ");
    Serial.print(hue);
    Serial.print(" | Speed pot: ");
    Serial.print(rawSpeed);
    Serial.print(" -> ");
    Serial.println(speed);
    lastPrint = millis();
  }

  // Set all pixels to the selected hue
  uint32_t c = strip.ColorHSV(hue, 255, 255);
  c = strip.gamma32(c);

  for (int i = 0; i < LED_COUNT; i++)
  {
    strip.setPixelColor(i, c);
  }

  strip.setBrightness(brightness);
  strip.show();

  // Fade in and out
  brightness += fadeAmount;

  // Reverse fade direction at the ends (0 to maxBrightness)
  if (brightness <= 0 || brightness >= maxBrightness)
  {
    fadeAmount = -fadeAmount;
  }

  delay(speed);
}

// Effect 4: Chase effect with hue control
void chaseHue()
{
  static uint8_t position = 0;

  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawHue = analogRead(POT_PIN_HUE);
  int rawSpeed = analogRead(POT_PIN_SPEED);

  uint16_t hue = readHueFromPot();
  uint8_t brightness = readBrightnessFromPot();
  uint8_t speed = readSpeedFromPot();

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[Chase Hue] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> ");
    Serial.print(brightness);
    Serial.print(" | Hue pot: ");
    Serial.print(rawHue);
    Serial.print(" -> ");
    Serial.print(hue);
    Serial.print(" | Speed pot: ");
    Serial.print(rawSpeed);
    Serial.print(" -> ");
    Serial.print(speed);
    Serial.print(" | Position: ");
    Serial.println(position);
    lastPrint = millis();
  }

  // Clear all pixels
  strip.clear();

  // Set the selected hue
  uint32_t c = strip.ColorHSV(hue, 255, 255);
  c = strip.gamma32(c);

  // Light up the current position
  strip.setPixelColor(position, c);

  strip.setBrightness(brightness);
  strip.show();

  // Move to next position
  position++;
  if (position >= LED_COUNT)
  {
    position = 0;
  }

  delay(speed);
}

// Effect 5: Rainbow Fade In/Out
void rainbowFade()
{
  static uint16_t hue = 0;
  static uint8_t brightness = 0;
  static int8_t fadeAmount = 5;

  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawSpeed = analogRead(POT_PIN_SPEED);

  // Get max brightness and speed from potentiometers
  uint8_t maxBrightness = readBrightnessFromPot();
  uint8_t speed = readSpeedFromPot();

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[Rainbow Fade] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> Max: ");
    Serial.print(maxBrightness);
    Serial.print(", Current: ");
    Serial.print(brightness);
    Serial.print(" | Hue: N/A | Speed pot: ");
    Serial.print(rawSpeed);
    Serial.print(" -> ");
    Serial.println(speed);
    lastPrint = millis();
  }

  // Manually create rainbow with gamma correction
  for (int i = 0; i < LED_COUNT; i++)
  {
    uint16_t pixelHue = hue + (i * 65536L / LED_COUNT);
    uint32_t color = strip.ColorHSV(pixelHue, 255, 255);
    color = strip.gamma32(color);
    strip.setPixelColor(i, color);
  }

  strip.setBrightness(brightness);
  strip.show();

  brightness += fadeAmount;

  // Fade between 0 and the potentiometer value
  if (brightness <= 0 || brightness >= maxBrightness)
  {
    fadeAmount = -fadeAmount;
  }

  hue += 127;
  delay(speed);
}

// Effect 6: Fire Effect
void fireEffect()
{
  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawHue = analogRead(POT_PIN_HUE);
  int rawSpeed = analogRead(POT_PIN_SPEED);

  uint8_t brightness = readBrightnessFromPot();
  uint16_t huePot = readHueFromPot();
  uint16_t speed = readSpeedFromPot();

  // Map hue pot to select fire color palette (using calibrated range -> 0-5)
  uint8_t palette = map(analogRead(POT_PIN_HUE), POT_MIN, POT_MAX, 0, 5);
  palette = constrain(palette, 0, 5); // Ensure we can reach all palettes

  // Debug output every 1000ms
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[Fire Effect] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> ");
    Serial.print(brightness);
    Serial.print(" | Hue pot: ");
    Serial.print(rawHue);
    Serial.print(" -> Palette ");
    Serial.print(palette);
    Serial.print(" (");
    switch (palette)
    {
    case 0:
      Serial.print("Classic Fire");
      break;
    case 1:
      Serial.print("Hot Fire");
      break;
    case 2:
      Serial.print("Toxic Fire");
      break;
    case 3:
      Serial.print("Purple Fire");
      break;
    case 4:
      Serial.print("Ice Fire");
      break;
    case 5:
      Serial.print("Inferno");
      break;
    }
    Serial.print(") | Speed pot: ");
    Serial.print(rawSpeed);
    Serial.print(" -> ");
    Serial.println(speed);
    lastPrint = millis();
  }

  strip.clear();
  strip.setBrightness(brightness);

  // Color palette definitions (HSV hue values: 0-65535)
  // Each palette has 3 colors: inner (bottom), middle, outer (top)
  uint16_t innerHue, middleHue, outerHue;

  switch (palette)
  {
  case 0:             // Classic Fire: Red → Orange → Yellow
    innerHue = 0;     // Red
    middleHue = 5461; // Orange (30°)
    outerHue = 10923; // Yellow (60°)
    break;
  case 1:              // Hot Fire: Orange → Yellow → White-ish
    innerHue = 5461;   // Orange
    middleHue = 10923; // Yellow
    outerHue = 16384;  // Yellow-white
    break;
  case 2:              // Toxic Fire: Green → Cyan → Blue
    innerHue = 21845;  // Green (120°)
    middleHue = 32768; // Cyan (180°)
    outerHue = 43691;  // Blue (240°)
    break;
  case 3:              // Purple Fire: Purple → Magenta → Pink
    innerHue = 49152;  // Purple (270°)
    middleHue = 54613; // Magenta (300°)
    outerHue = 60075;  // Pink (330°)
    break;
  case 4:              // Ice Fire: Blue → Cyan → White-ish
    innerHue = 43691;  // Blue (240°)
    middleHue = 32768; // Cyan (180°)
    outerHue = 16384;  // Light cyan
    break;
  case 5:             // Inferno: Dark Red → Red → Orange
    innerHue = 60000; // Dark red/maroon
    middleHue = 0;    // Red
    outerHue = 5461;  // Orange
    break;
  }

  // Draw fire on each LED
  for (int i = 0; i < LED_COUNT; i++)
  {
    // Calculate position ratio (0.0 at bottom, 1.0 at top)
    float position = (float)i / (float)(LED_COUNT - 1);

    // Select color based on position
    uint16_t hue;
    uint8_t sat = 255; // Full saturation by default

    if (position < 0.33)
    {
      // Bottom third: inner fire color
      hue = innerHue;
    }
    else if (position < 0.66)
    {
      // Middle third: blend between inner and middle
      float blend = (position - 0.33) / 0.33;
      hue = innerHue + (uint16_t)((middleHue - innerHue) * blend);
    }
    else
    {
      // Top third: blend between middle and outer
      float blend = (position - 0.66) / 0.34;
      hue = middleHue + (uint16_t)((outerHue - middleHue) * blend);
      // Reduce saturation at the tips for white-hot effect
      sat = 255 - (uint8_t)(blend * 80);
    }

    // Add random flicker to brightness (60-100% of set brightness)
    uint8_t flicker = random(153, 256); // 60-100% of 255
    uint8_t val = (uint8_t)((255 * flicker) / 256);

    // Additional flicker: randomly dim some LEDs more
    if (random(0, 100) < 30) // 30% chance
    {
      val = val / 2; // Dim to 50%
    }

    uint32_t color = strip.ColorHSV(hue, sat, val);
    color = strip.gamma32(color);
    strip.setPixelColor(i, color);
  }

  strip.show();
  delay(speed / 2); // Faster updates for more dynamic flicker
}

// Effect 7: White Flicker
void whiteFastFlicker()
{
  int rawBrightness = analogRead(POT_PIN_BRIGHTNESS);
  int rawSpeed = analogRead(POT_PIN_SPEED);

  uint8_t brightness = readBrightnessFromPot();
  uint8_t speed = readSpeedFromPot();

  // Debug output every 1000ms (1 second)
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 1000)
  {
    Serial.print("[White Flicker] Brightness pot: ");
    Serial.print(rawBrightness);
    Serial.print(" -> ");
    Serial.print(brightness);
    Serial.print(" | Hue: N/A | Speed pot: ");
    Serial.print(rawSpeed);
    Serial.print(" -> ");
    Serial.println(speed);
    lastPrint = millis();
  }

  strip.clear();
  strip.setBrightness(brightness);

  uint32_t white = strip.Color(255, 255, 255);
  white = strip.gamma32(white);

  for (int i = 0; i < 3; i++)
  {
    int randomPixel = random(0, LED_COUNT);
    strip.setPixelColor(randomPixel, white);
  }

  strip.show();
  delay(speed);
}

void setup()
{
  Serial.begin(9600);

  strip.begin();
  strip.show();
  strip.setBrightness(0);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("Setup complete. Current effect: 0");

  // Test potentiometer wiring
  Serial.println("\n=== Potentiometer Test ===");
  Serial.println("Testing all three potentiometers...");
  for (int i = 0; i < 2; i++)
  {
    int hueValue = analogRead(POT_PIN_HUE);
    int brightnessValue = analogRead(POT_PIN_BRIGHTNESS);
    int speedValue = analogRead(POT_PIN_SPEED);
    Serial.print("Brightness (A0): ");
    Serial.print(brightnessValue);
    Serial.print(" | Hue (A1): ");
    Serial.print(hueValue);
    Serial.print(" | Speed (A2): ");
    Serial.println(speedValue);
    delay(50);
  }
  Serial.println("Expected: values should range from ~0 to ~1023");
  Serial.println("=========================\n");
}

void loop()
{
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState)
  {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay)
  {
    static bool buttonState = HIGH;

    if (reading != buttonState)
    {
      buttonState = reading;

      if (buttonState == LOW)
      {
        currentEffect = (currentEffect + 1) % NUM_EFFECTS;

        // Show effect name
        Serial.print("Button pressed! Switching to effect ");
        Serial.print(currentEffect);
        Serial.print(": ");
        switch (currentEffect)
        {
        case 0:
          Serial.println("Off");
          break;
        case 1:
          Serial.println("White Light");
          break;
        case 2:
          Serial.println("Solid Hue");
          break;
        case 3:
          Serial.println("Pulse Hue");
          break;
        case 4:
          Serial.println("Chase Hue");
          break;
        case 5:
          Serial.println("Rainbow Fade");
          break;
        case 6:
          Serial.println("Fire Effect");
          break;
        case 7:
          Serial.println("White Flicker");
          break;
        default:
          Serial.println("Unknown");
          break;
        }

        strip.clear();
        strip.show();

        delay(300);
      }
    }
  }

  lastButtonState = reading;

  switch (currentEffect)
  {
  case 0:
    effectOff();
    break;
  case 1:
    whiteLight();
    break;
  case 2:
    solidHue();
    break;
  case 3:
    pulseHue();
    break;
  case 4:
    chaseHue();
    break;
  case 5:
    rainbowFade();
    break;
  case 6:
    fireEffect();
    break;
  case 7:
    whiteFastFlicker();
    break;

  default:
    effectOff();
    break;
  }
}
