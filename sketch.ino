#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_NeoPixel.h>

/* ============================================================================
 * FORMULA SAE DASHBOARD - ESP32 & ILI9341 TFT + NEOPIXEL SHIFT LIGHTS
 * ============================================================================
 * Hardware Pinout for Wokwi Simulation:
 *   - TFT Display (ILI9341 SPI):
 *       TFT_CS   -> GPIO 15
 *       TFT_DC   -> GPIO 2
 *       TFT_MOSI -> GPIO 23
 *       TFT_SCK  -> GPIO 18
 *       TFT_MISO -> GPIO 19
 *       TFT_RST  -> GPIO 4
 *       TFT_LED  -> 3.3V
 *   - Shift Lights (NeoPixel WS2812B):
 *       NEOPIXEL_PIN -> GPIO 5 (24 LEDs: 8 Green, 8 Red, 8 Blue)
 *   - Inputs (Buttons & Potentiometer):
 *       BTN_GEAR_UP   -> GPIO 12 (Active LOW, internal pull-up)
 *       BTN_GEAR_DOWN -> GPIO 14 (Active LOW, internal pull-up)
 *       BTN_MODE      -> GPIO 27 (Toggle Auto Demo / Manual)
 *       POT_THROTTLE  -> GPIO 34 (Analog In ADC1_6)
 * ============================================================================
 */

// ---------------- Pins Definitions ----------------
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST    4
#define TFT_MOSI  23
#define TFT_SCK   18
#define TFT_MISO  19

#define NEO_PIN    5
#define NUM_LEDS  24

#define BTN_UP    12
#define BTN_DOWN  14
#define BTN_MODE  27
#define POT_PIN   34

// ---------------- Color Palette ----------------
#define COLOR_BG        ILI9341_BLACK
#define COLOR_FRAME     ILI9341_WHITE
#define COLOR_TEXT      ILI9341_WHITE
#define COLOR_GREEN_BAR ILI9341_GREEN
#define COLOR_RED_BAR   ILI9341_RED
#define COLOR_TIRE_LINE ILI9341_GREEN
#define COLOR_TIRE_FILL 0x0320 // Dark racing green
#define COLOR_WARN_RED  ILI9341_RED
#define COLOR_GEAR_N    ILI9341_GREEN
#define COLOR_GEAR_NUM  ILI9341_WHITE

// ---------------- Display & NeoPixel Instances ----------------
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel strip(NUM_LEDS, NEO_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- Telemetry Data Structure ----------------
struct Telemetry {
  int gear;            // 0 = N, 1 to 6
  int speed;           // km/h
  int rpm;             // giri/m (RPM)
  int shifts;          // Remaining pneumatic shifts (air cylinder)
  float fuel;          // Liters
  int cm3;             // cm³ consumption readout
  int throttle;        // 0-100 %
  int brake;           // 0-100 %
  int waterTemp;       // °C
  int oilTemp;         // °C
  float battery;       // Volts
  float tirePress[4];  // FL, FR, RL, RR in psi
  int tireTemp[4];     // FL, FR, RL, RR in °C
};

Telemetry currentTelemetry;
Telemetry prevTelemetry;

// Simulation State
bool autoMode = true;
unsigned long lastUpdateMs = 0;
unsigned long lastSimStepMs = 0;
unsigned long lastShiftTime = 0;
int simStep = 0;
bool flashState = false;
unsigned long lastFlashMs = 0;

// Button debounce tracking
bool lastUpState = HIGH;
bool lastDownState = HIGH;
bool lastModeState = HIGH;
unsigned long lastDebounceUp = 0;
unsigned long lastDebounceDown = 0;
unsigned long lastDebounceMode = 0;

// ---------------- Helper Prototypes ----------------
void initDashboardData();
void drawStaticUI();
void updateTelemetryAuto();
void updateShiftLights(int rpm);
void renderChangedTelemetry();
void handleInputs();
void handleSerial();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=============================================");
  Serial.println("   FORMULA SAE DIGITAL DASHBOARD INITIALIZED");
  Serial.println("=============================================");
  Serial.println("Simulation running. Type 'help' for commands.");

  // Pin modes
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_MODE, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  // Initialize NeoPixel Shift Lights
  strip.begin();
  strip.setBrightness(180);
  strip.clear();
  strip.show();

  // Initialize TFT
  tft.begin();
  tft.setRotation(1); // Landscape 320x240
  tft.fillScreen(COLOR_BG);

  // Initialize Data
  initDashboardData();

  // Draw full static layout
  drawStaticUI();

  // Force first full telemetry render
  prevTelemetry = currentTelemetry;
  prevTelemetry.gear = -99; // force redraw
  prevTelemetry.rpm = -99;
  prevTelemetry.speed = -99;
  prevTelemetry.shifts = -99;
  prevTelemetry.fuel = -99;
  prevTelemetry.throttle = -99;
  prevTelemetry.brake = -99;
  prevTelemetry.waterTemp = -99;
  prevTelemetry.oilTemp = -99;
  prevTelemetry.battery = -99;
  for(int i = 0; i < 4; i++) {
    prevTelemetry.tirePress[i] = -99;
  }

  renderChangedTelemetry();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Process Physical & Virtual Inputs
  handleInputs();
  handleSerial();

  // 2. Telemetry Simulation (if auto mode active)
  if (autoMode && (now - lastSimStepMs >= 30)) {
    lastSimStepMs = now;
    updateTelemetryAuto();
  }

  // 3. Update Display & Rev Lights (~30 FPS)
  if (now - lastUpdateMs >= 35) {
    lastUpdateMs = now;
    renderChangedTelemetry();
    updateShiftLights(currentTelemetry.rpm);
  }
}

// ============================================================================
// DEFAULT DATA INITIALIZATION
// ============================================================================
void initDashboardData() {
  // Matching values from bozza.jpeg with pneumatic shifts initialized to 50:
  // Fuel: 13 l, cm3: 38 cm3, Gear: 2, Speed: 30 km/h, RPM: 5000 giri/m,
  // Shifts: 50 (Pneumatic shifts capacity), Water: 104°, Oil: 80°, Battery: 13.3 V, Tires: 21 psi
  currentTelemetry.gear = 2;
  currentTelemetry.speed = 30;
  currentTelemetry.rpm = 5000;
  currentTelemetry.shifts = 50; // Initial pneumatic bottle capacity (50 shifts)
  currentTelemetry.fuel = 13.0;
  currentTelemetry.cm3 = 38;
  currentTelemetry.throttle = 45;
  currentTelemetry.brake = 0;
  currentTelemetry.waterTemp = 104;
  currentTelemetry.oilTemp = 80;
  currentTelemetry.battery = 13.3;

  for (int i = 0; i < 4; i++) {
    currentTelemetry.tirePress[i] = 21.0;
    currentTelemetry.tireTemp[i] = 72;
  }
}

// ============================================================================
// STATIC UI DRAWING (Borders, Dividers, Fixed Labels)
// ============================================================================
void drawStaticUI() {
  tft.fillScreen(COLOR_BG);

  // Outer Screen Border
  tft.drawRect(0, 0, 320, 240, COLOR_FRAME);

  // Horizontal Separator 1 (Under Top Row)
  tft.drawFastHLine(0, 44, 320, COLOR_FRAME);

  // Horizontal Separator 2 (Above Bottom Row)
  tft.drawFastHLine(0, 178, 320, COLOR_FRAME);

  // Vertical Separators (Middle Section Columns)
  tft.drawFastVLine(82, 44, 134, COLOR_FRAME);
  tft.drawFastVLine(238, 44, 134, COLOR_FRAME);

  // ---------------- TOP SECTION STATIC LABELS ----------------
  // Fuel label under fuel quantity
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(8, 28);
  tft.print("fuel");

  // Superscript 3 for cm³
  tft.setTextSize(3);
  tft.setCursor(206, 8);
  tft.print("38 cm");
  tft.setTextSize(2);
  tft.setCursor(296, 4);
  tft.print("3");

  // ---------------- MIDDLE SECTION STATIC ELEMENTS ----------------
  // 4 Tires frames (FL, FR, RL, RR)
  // FL
  tft.drawRoundRect(12, 62, 24, 38, 4, COLOR_TIRE_LINE);
  tft.fillRoundRect(14, 64, 20, 34, 3, COLOR_TIRE_FILL);
  // FR
  tft.drawRoundRect(46, 62, 24, 38, 4, COLOR_TIRE_LINE);
  tft.fillRoundRect(48, 64, 20, 34, 3, COLOR_TIRE_FILL);
  // RL
  tft.drawRoundRect(12, 114, 24, 38, 4, COLOR_TIRE_LINE);
  tft.fillRoundRect(14, 116, 20, 34, 3, COLOR_TIRE_FILL);
  // RR
  tft.drawRoundRect(46, 114, 24, 38, 4, COLOR_TIRE_LINE);
  tft.fillRoundRect(48, 116, 20, 34, 3, COLOR_TIRE_FILL);

  // Static label under shifts count
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(244, 126);
  tft.print("shifts");

  // ---------------- BOTTOM SECTION STATIC ELEMENTS ----------------
  // Throttle bar frame & label
  tft.drawRect(8, 184, 62, 14, COLOR_GREEN_BAR);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(74, 184);
  tft.print("THR");

  // Brake bar frame & label
  tft.drawRect(8, 206, 62, 14, COLOR_RED_BAR);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(74, 206);
  tft.print("BRK");

  // Temperature labels
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setCursor(118, 210);
  tft.print("water");

  tft.setCursor(184, 210);
  tft.print("oil");

  // Battery label
  tft.setCursor(236, 210);
  tft.print("battery");

  // Degree symbols (drawn as small circles)
  tft.drawCircle(162, 188, 2, COLOR_TEXT);
  tft.drawCircle(212, 188, 2, COLOR_TEXT);
}

// ============================================================================
// DYNAMIC FLICKER-FREE DISPLAY RENDERING
// ============================================================================
void renderChangedTelemetry() {
  char buf[20];

  // 1. FUEL QUANTITY (Top Left)
  if (currentTelemetry.fuel != prevTelemetry.fuel) {
    tft.setTextSize(3);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(8, 6);
    snprintf(buf, sizeof(buf), "%2.0f l", currentTelemetry.fuel);
    tft.print(buf);
    prevTelemetry.fuel = currentTelemetry.fuel;
  }

  // 2. TIRE PRESSURES (Middle Left)
  for (int i = 0; i < 4; i++) {
    if (currentTelemetry.tirePress[i] != prevTelemetry.tirePress[i]) {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      snprintf(buf, sizeof(buf), "%2.0fpsi", currentTelemetry.tirePress[i]);
      if (i == 0)      { tft.setCursor(8,  50); tft.print(buf); } // FL
      else if (i == 1) { tft.setCursor(44, 50); tft.print(buf); } // FR
      else if (i == 2) { tft.setCursor(8, 160); tft.print(buf); } // RL
      else if (i == 3) { tft.setCursor(44, 160); tft.print(buf); } // RR
      prevTelemetry.tirePress[i] = currentTelemetry.tirePress[i];
    }
  }

  // 3. GEAR INDICATOR (Middle Center - Large)
  if (currentTelemetry.gear != prevTelemetry.gear) {
    tft.setTextSize(7);
    tft.setCursor(140, 52);
    if (currentTelemetry.gear == 0) {
      tft.setTextColor(COLOR_GEAR_N, COLOR_BG);
      tft.print("N");
    } else {
      tft.setTextColor(COLOR_GEAR_NUM, COLOR_BG);
      tft.print(currentTelemetry.gear);
    }
    prevTelemetry.gear = currentTelemetry.gear;
  }

  // 4. SPEED (Middle Center)
  if (currentTelemetry.speed != prevTelemetry.speed) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(114, 120);
    snprintf(buf, sizeof(buf), "%3d km/h", currentTelemetry.speed);
    tft.print(buf);
    prevTelemetry.speed = currentTelemetry.speed;
  }

  // 5. ENGINE RPM (Middle Center)
  if (currentTelemetry.rpm != prevTelemetry.rpm) {
    tft.setTextSize(2);
    // Highlight RPM yellow/red near redline
    if (currentTelemetry.rpm >= 11500) {
      tft.setTextColor(ILI9341_RED, COLOR_BG);
    } else if (currentTelemetry.rpm >= 9000) {
      tft.setTextColor(ILI9341_YELLOW, COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
    }
    tft.setCursor(94, 146);
    snprintf(buf, sizeof(buf), "%5d giri/m", currentTelemetry.rpm);
    tft.print(buf);
    prevTelemetry.rpm = currentTelemetry.rpm;
  }

  // 6. PNEUMATIC SHIFTS REMAINING (Middle Right)
  if (currentTelemetry.shifts != prevTelemetry.shifts) {
    tft.setTextSize(6);
    // Warning colors for pneumatic air reservoir level:
    if (currentTelemetry.shifts <= 0) {
      tft.setTextColor(COLOR_WARN_RED, COLOR_BG); // Depleted air
    } else if (currentTelemetry.shifts <= 10) {
      tft.setTextColor(ILI9341_YELLOW, COLOR_BG); // Low air warning
    } else {
      tft.setTextColor(COLOR_TEXT, COLOR_BG);     // Optimal air
    }
    tft.setCursor(244, 60);
    snprintf(buf, sizeof(buf), "%2d", max(0, currentTelemetry.shifts));
    tft.print(buf);
    prevTelemetry.shifts = currentTelemetry.shifts;
  }

  // 7. THROTTLE BAR (Bottom Left)
  if (currentTelemetry.throttle != prevTelemetry.throttle) {
    int maxW = 58;
    int fillW = map(constrain(currentTelemetry.throttle, 0, 100), 0, 100, 0, maxW);
    tft.fillRect(10, 186, fillW, 10, COLOR_GREEN_BAR);
    if (fillW < maxW) {
      tft.fillRect(10 + fillW, 186, maxW - fillW, 10, COLOR_BG);
    }
    prevTelemetry.throttle = currentTelemetry.throttle;
  }

  // 8. BRAKE BAR (Bottom Left)
  if (currentTelemetry.brake != prevTelemetry.brake) {
    int maxW = 58;
    int fillW = map(constrain(currentTelemetry.brake, 0, 100), 0, 100, 0, maxW);
    tft.fillRect(10, 208, fillW, 10, COLOR_RED_BAR);
    if (fillW < maxW) {
      tft.fillRect(10 + fillW, 208, maxW - fillW, 10, COLOR_BG);
    }
    prevTelemetry.brake = currentTelemetry.brake;
  }

  // 9. WATER TEMPERATURE (Bottom Center)
  if (currentTelemetry.waterTemp != prevTelemetry.waterTemp) {
    tft.setTextSize(2);
    if (currentTelemetry.waterTemp >= 108) {
      tft.setTextColor(COLOR_WARN_RED, COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
    }
    tft.setCursor(124, 186);
    snprintf(buf, sizeof(buf), "%3d", currentTelemetry.waterTemp);
    tft.print(buf);
    prevTelemetry.waterTemp = currentTelemetry.waterTemp;
  }

  // 10. OIL TEMPERATURE (Bottom Center)
  if (currentTelemetry.oilTemp != prevTelemetry.oilTemp) {
    tft.setTextSize(2);
    if (currentTelemetry.oilTemp >= 115) {
      tft.setTextColor(COLOR_WARN_RED, COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
    }
    tft.setCursor(184, 186);
    snprintf(buf, sizeof(buf), "%2d", currentTelemetry.oilTemp);
    tft.print(buf);
    prevTelemetry.oilTemp = currentTelemetry.oilTemp;
  }

  // 11. BATTERY VOLTAGE (Bottom Right)
  if (currentTelemetry.battery != prevTelemetry.battery) {
    tft.setTextSize(2);
    if (currentTelemetry.battery < 11.8) {
      tft.setTextColor(COLOR_WARN_RED, COLOR_BG);
    } else {
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
    }
    tft.setCursor(236, 186);
    snprintf(buf, sizeof(buf), "%4.1f V", currentTelemetry.battery);
    tft.print(buf);
    prevTelemetry.battery = currentTelemetry.battery;
  }
}

// ============================================================================
// NEOPIXEL SHIFT LIGHTS (24 LEDs: 8 Green, 8 Red, 8 Blue)
// ============================================================================
void updateShiftLights(int rpm) {
  const int RPM_IDLE   = 2500;
  const int RPM_REDLINE = 11500;
  const int RPM_LIMITER = 12000;

  // Limiter Flashing
  if (rpm >= RPM_LIMITER) {
    unsigned long now = millis();
    if (now - lastFlashMs >= 70) {
      lastFlashMs = now;
      flashState = !flashState;
    }
    if (flashState) {
      for (int i = 0; i < NUM_LEDS; i++) {
        strip.setPixelColor(i, strip.Color(0, 80, 255)); // Bright Blue / Shift
      }
    } else {
      strip.clear();
    }
    strip.show();
    return;
  }

  // Progressive LED Bar
  int ledsToLight = 0;
  if (rpm > RPM_IDLE) {
    ledsToLight = map(constrain(rpm, RPM_IDLE, RPM_REDLINE), RPM_IDLE, RPM_REDLINE, 0, NUM_LEDS);
  }

  for (int i = 0; i < NUM_LEDS; i++) {
    if (i < ledsToLight) {
      if (i < 8) {
        // First 8 LEDs: GREEN
        strip.setPixelColor(i, strip.Color(0, 180, 0));
      } else if (i < 16) {
        // Middle 8 LEDs: RED
        strip.setPixelColor(i, strip.Color(220, 0, 0));
      } else {
        // Last 8 LEDs: BLUE
        strip.setPixelColor(i, strip.Color(0, 40, 255));
      }
    } else {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  strip.show();
}

// ============================================================================
// REALISTIC FORMULA SAE LAP SIMULATION (AUTO MODE)
// ============================================================================
void updateTelemetryAuto() {
  static unsigned long stateTimer = 0;
  static int trackPhase = 0; // 0=Acc1, 1=Acc2, 2=Acc3, 3=Braking, 4=Corner, 5=Straight
  unsigned long now = millis();

  // Subtle natural fluctuations
  currentTelemetry.battery = 13.2 + 0.3 * sin(now / 1500.0);
  currentTelemetry.waterTemp = 104 + (int)(1.5 * sin(now / 8000.0));
  currentTelemetry.oilTemp = 80 + (int)(1.0 * cos(now / 7000.0));

  // Slow fuel consumption
  static unsigned long lastFuelDrop = 0;
  if (now - lastFuelDrop > 20000) {
    lastFuelDrop = now;
    if (currentTelemetry.fuel > 1.0) currentTelemetry.fuel -= 0.1;
  }

  // Lap State Machine
  switch (trackPhase) {
    case 0: // Acceleration in Gear 1
      currentTelemetry.gear = 1;
      currentTelemetry.throttle = 95;
      currentTelemetry.brake = 0;
      currentTelemetry.rpm += 120;
      currentTelemetry.speed = map(currentTelemetry.rpm, 3000, 11600, 15, 45);
      if (currentTelemetry.rpm >= 11600) {
        if (currentTelemetry.shifts > 0) {
          currentTelemetry.gear = 2;
          currentTelemetry.shifts--; // Pneumatic shift consumes air
          currentTelemetry.rpm = 7200;
          trackPhase = 1;
        } else {
          // Out of air! Pit stop refill after 3 seconds
          static unsigned long pitTimer = 0;
          if (pitTimer == 0) pitTimer = now;
          if (now - pitTimer > 3000) {
            currentTelemetry.shifts = 50;
            pitTimer = 0;
            Serial.println(">> [PIT STOP] Air bottle refilled to 50 shifts!");
          }
        }
      }
      break;

    case 1: // Acceleration in Gear 2
      currentTelemetry.gear = 2;
      currentTelemetry.throttle = 100;
      currentTelemetry.brake = 0;
      currentTelemetry.rpm += 90;
      currentTelemetry.speed = map(currentTelemetry.rpm, 7200, 11700, 45, 78);
      if (currentTelemetry.rpm >= 11700) {
        if (currentTelemetry.shifts > 0) {
          currentTelemetry.gear = 3;
          currentTelemetry.shifts--; // Pneumatic shift consumes air
          currentTelemetry.rpm = 7800;
          trackPhase = 2;
        } else {
          // Out of air!
          currentTelemetry.rpm = 11700; // Hit limiter in gear 2
          trackPhase = 3; // Force braking
        }
      }
      break;

    case 2: // High Speed Straight in Gear 3
      currentTelemetry.gear = 3;
      currentTelemetry.throttle = 100;
      currentTelemetry.brake = 0;
      currentTelemetry.rpm += 70;
      currentTelemetry.speed = map(currentTelemetry.rpm, 7800, 11800, 78, 105);
      if (currentTelemetry.rpm >= 11800) {
        stateTimer = now;
        trackPhase = 3; // Shift to heavy braking
      }
      break;

    case 3: // Heavy Braking into Hairpin
      currentTelemetry.throttle = 0;
      currentTelemetry.brake = 85;
      if (currentTelemetry.rpm > 4500) currentTelemetry.rpm -= 180;
      if (currentTelemetry.speed > 28) currentTelemetry.speed -= 2;

      // Downshift to Gear 2
      if (currentTelemetry.speed < 55 && currentTelemetry.gear == 3) {
        if (currentTelemetry.shifts > 0) {
          currentTelemetry.gear = 2;
          currentTelemetry.shifts--; // Pneumatic shift consumes air
          currentTelemetry.rpm = 6500;
        }
      }
      if (currentTelemetry.speed <= 30) {
        currentTelemetry.brake = 0;
        trackPhase = 4; // Corner apex
      }
      break;

    case 4: // Corner Apex & Exit
      currentTelemetry.gear = 2;
      currentTelemetry.throttle = 50;
      currentTelemetry.brake = 0;
      currentTelemetry.speed = 30 + (int)(3.0 * sin(now / 400.0));
      currentTelemetry.rpm = 5000 + (int)(250.0 * sin(now / 400.0));
      if (now - stateTimer > 3500) {
        trackPhase = 0; // Repeat lap cycle
      }
      break;
  }
}

// ============================================================================
// HARDWARE INPUTS (Buttons, Potentiometer, Mode Switch)
// ============================================================================
void handleInputs() {
  unsigned long now = millis();

  // Mode Toggle Button
  int modeReading = digitalRead(BTN_MODE);
  if (modeReading == LOW && lastModeState == HIGH && (now - lastDebounceMode > 250)) {
    lastDebounceMode = now;
    autoMode = !autoMode;
    Serial.print(">> Mode changed to: ");
    Serial.println(autoMode ? "AUTO DEMO (Lap Sim)" : "MANUAL CONTROL");
  }
  lastModeState = modeReading;

  // If in Manual Mode, read Potentiometer and Gear Buttons
  if (!autoMode) {
    // Read Potentiometer for Throttle / RPM
    int potVal = analogRead(POT_PIN); // 0 to 4095 on ESP32
    currentTelemetry.throttle = map(potVal, 0, 4095, 0, 100);

    // Map Throttle to RPM and Speed based on current gear
    if (currentTelemetry.gear == 0) {
      // Neutral
      currentTelemetry.rpm = map(currentTelemetry.throttle, 0, 100, 2000, 12500);
      currentTelemetry.speed = 0;
    } else {
      // In gear
      int maxSpdGear = currentTelemetry.gear * 22 + 10;
      currentTelemetry.rpm = map(currentTelemetry.throttle, 0, 100, 3000, 12200);
      currentTelemetry.speed = map(currentTelemetry.rpm, 3000, 12200, 10 * currentTelemetry.gear, maxSpdGear);
    }

    // Shift UP Button
    int upReading = digitalRead(BTN_UP);
    if (upReading == LOW && lastUpState == HIGH && (now - lastDebounceUp > 200)) {
      lastDebounceUp = now;
      if (currentTelemetry.gear < 6) {
        if (currentTelemetry.shifts > 0) {
          currentTelemetry.gear++;
          currentTelemetry.shifts--; // Consume 1 pneumatic shift
          Serial.printf(">> Upshift! Gear: %d | Pneumatic shifts remaining: %d\n", currentTelemetry.gear, currentTelemetry.shifts);
        } else {
          Serial.println(">> [PNEUMATIC ERROR] No compressed air remaining! Cannot upshift.");
        }
      }
    }
    lastUpState = upReading;

    // Shift DOWN Button
    int downReading = digitalRead(BTN_DOWN);
    if (downReading == LOW && lastDownState == HIGH && (now - lastDebounceDown > 200)) {
      lastDebounceDown = now;
      if (currentTelemetry.gear > 0) {
        if (currentTelemetry.shifts > 0) {
          currentTelemetry.gear--;
          currentTelemetry.shifts--; // Consume 1 pneumatic shift
          Serial.printf(">> Downshift! Gear: %d | Pneumatic shifts remaining: %d\n", currentTelemetry.gear, currentTelemetry.shifts);
        } else {
          Serial.println(">> [PNEUMATIC ERROR] No compressed air remaining! Cannot downshift.");
        }
      }
    }
    lastDownState = downReading;
  }
}

// ============================================================================
// SERIAL COMMAND INTERFACE
// ============================================================================
void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.equalsIgnoreCase("help")) {
    Serial.println("\n--- FSAE DASHBOARD COMMANDS ---");
    Serial.println("  auto             : Toggle Auto Demo vs Manual mode");
    Serial.println("  refill           : Refill pneumatic bottle to 50 shifts");
    Serial.println("  air <val>        : Set remaining pneumatic shifts (0-99)");
    Serial.println("  gear <0-6>       : Set gear (0=N, 1-6) - consumes 1 shift");
    Serial.println("  rpm <val>        : Set RPM (e.g. rpm 8500)");
    Serial.println("  spd <val>        : Set Speed (e.g. spd 65)");
    Serial.println("  thr <0-100>      : Set Throttle % (e.g. thr 80)");
    Serial.println("  brk <0-100>      : Set Brake % (e.g. brk 40)");
    Serial.println("  water <val>      : Set Water Temp °C");
    Serial.println("  oil <val>        : Set Oil Temp °C");
    Serial.println("  bat <val>        : Set Battery Volts");
    Serial.println("  fuel <val>       : Set Fuel liters");
    Serial.println("  tire <psi>       : Set All Tires psi");
    Serial.println("--------------------------------\n");
  } else if (cmd.equalsIgnoreCase("auto")) {
    autoMode = !autoMode;
    Serial.print(">> Auto Demo mode: ");
    Serial.println(autoMode ? "ENABLED" : "DISABLED");
  } else if (cmd.equalsIgnoreCase("refill")) {
    currentTelemetry.shifts = 50;
    Serial.println(">> Pneumatic air bottle REFILLED to 50 shifts!");
  } else if (cmd.startsWith("air ")) {
    currentTelemetry.shifts = constrain(cmd.substring(4).toInt(), 0, 99);
    Serial.printf(">> Pneumatic air shifts set to: %d\n", currentTelemetry.shifts);
  } else if (cmd.startsWith("gear ")) {
    autoMode = false;
    int targetGear = constrain(cmd.substring(5).toInt(), 0, 6);
    if (targetGear != currentTelemetry.gear) {
      if (currentTelemetry.shifts > 0) {
        currentTelemetry.gear = targetGear;
        currentTelemetry.shifts--;
        Serial.printf(">> Gear set to %d | Air shifts remaining: %d\n", currentTelemetry.gear, currentTelemetry.shifts);
      } else {
        Serial.println(">> [PNEUMATIC ERROR] No compressed air remaining! Shift command rejected.");
      }
    }
  } else if (cmd.startsWith("rpm ")) {
    autoMode = false;
    currentTelemetry.rpm = constrain(cmd.substring(4).toInt(), 0, 14000);
    Serial.printf(">> RPM set to %d\n", currentTelemetry.rpm);
  } else if (cmd.startsWith("spd ")) {
    autoMode = false;
    currentTelemetry.speed = constrain(cmd.substring(4).toInt(), 0, 160);
    Serial.printf(">> Speed set to %d km/h\n", currentTelemetry.speed);
  } else if (cmd.startsWith("thr ")) {
    autoMode = false;
    currentTelemetry.throttle = constrain(cmd.substring(4).toInt(), 0, 100);
    Serial.printf(">> Throttle set to %d%%\n", currentTelemetry.throttle);
  } else if (cmd.startsWith("brk ")) {
    autoMode = false;
    currentTelemetry.brake = constrain(cmd.substring(4).toInt(), 0, 100);
    Serial.printf(">> Brake set to %d%%\n", currentTelemetry.brake);
  } else if (cmd.startsWith("water ")) {
    currentTelemetry.waterTemp = cmd.substring(6).toInt();
    Serial.printf(">> Water Temp set to %d °C\n", currentTelemetry.waterTemp);
  } else if (cmd.startsWith("oil ")) {
    currentTelemetry.oilTemp = cmd.substring(4).toInt();
    Serial.printf(">> Oil Temp set to %d °C\n", currentTelemetry.oilTemp);
  } else if (cmd.startsWith("bat ")) {
    currentTelemetry.battery = cmd.substring(4).toFloat();
    Serial.printf(">> Battery set to %.1f V\n", currentTelemetry.battery);
  } else if (cmd.startsWith("fuel ")) {
    currentTelemetry.fuel = cmd.substring(5).toFloat();
    Serial.printf(">> Fuel set to %.1f l\n", currentTelemetry.fuel);
  } else if (cmd.startsWith("tire ")) {
    float p = cmd.substring(5).toFloat();
    for (int i = 0; i < 4; i++) currentTelemetry.tirePress[i] = p;
    Serial.printf(">> Tires set to %.1f psi\n", p);
  }
}
