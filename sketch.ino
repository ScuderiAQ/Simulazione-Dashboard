#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>

/* ============================================================================
 * FORMULA SAE DASHBOARD - STM32 NUCLEO-G431KB & ILI9341 SPI TFT
 * ============================================================================
 * Pinout:
 *   - TFT Display:
 *       TFT_CS   -> D3 (PB0)
 *       TFT_DC   -> D2 (PA10)
 *       TFT_MOSI -> D11 (PB5, Hardware SPI1 MOSI)
 *       TFT_SCK  -> D13 (PB3, Hardware SPI1 SCK)
 *       TFT_VCC  -> 3.3V, TFT_LED -> 3.3V, TFT_GND -> GND
 *   - Inputs (Full Manual Control):
 *       BTN_UP     -> D4 (PB7, Shift UP - Active LOW, pull-up)
 *       BTN_DOWN   -> D5 (PB6, Shift DOWN - Active LOW, pull-up)
 *       BTN_REFILL -> D7 (PC14, Ricarica Aria 50 Shifts - Active LOW, pull-up)
 *       POT_PIN    -> A0 (PA0, Throttle Potentiometer)
 * ============================================================================
 */

// ---------------- Pins Definitions ----------------
#define TFT_DC      2   // D2
#define TFT_CS      3   // D3
#define BTN_UP      4   // D4
#define BTN_DOWN    5   // D5
#define BTN_REFILL  7   // D7 (Refill Air)
#define POT_PIN    A0   // A0

// ---------------- Color Palette ----------------
#define COLOR_BG        ILI9341_BLACK
#define COLOR_FRAME     ILI9341_WHITE
#define COLOR_TEXT      ILI9341_WHITE
#define COLOR_GREEN_BAR ILI9341_GREEN
#define COLOR_RED_BAR   ILI9341_RED
#define COLOR_TIRE_LINE ILI9341_GREEN
#define COLOR_TIRE_FILL 0x0320
#define COLOR_WARN_RED  ILI9341_RED
#define COLOR_GEAR_N    ILI9341_GREEN
#define COLOR_GEAR_NUM  ILI9341_WHITE

// Display Instance
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC);

// ---------------- Telemetry Data Structure ----------------
struct Telemetry {
  int gear;            // 0 = N, 1 to 6
  int speed;           // km/h
  int rpm;             // giri/m
  int shifts;          // Remaining pneumatic shifts (50 initial)
  int fuel;            // Liters
  int throttle;        // 0-100 %
  int brake;           // 0-100 %
  int waterTemp;       // °C
  int oilTemp;         // °C
  int batteryDeciV;    // 133 = 13.3 V
  int tirePress[4];    // FL, FR, RL, RR
};

Telemetry currentTelemetry;
Telemetry prevTelemetry;

unsigned long lastUpdateMs = 0;
bool flashState = false;
unsigned long lastFlashMs = 0;

// Button debounce tracking
bool lastUpState = HIGH;
bool lastDownState = HIGH;
bool lastRefillState = HIGH;
unsigned long lastDebounceUp = 0;
unsigned long lastDebounceDown = 0;
unsigned long lastDebounceRefill = 0;

// ---------------- Helper Prototypes ----------------
void drawStaticUI();
void updateShiftLights(int rpm);
void renderChangedTelemetry();
void handleInputs();

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_REFILL, INPUT_PULLUP);
  pinMode(POT_PIN, INPUT);

  // Initialize display
  tft.begin();
  tft.setRotation(1); // Landscape 320x240
  tft.fillScreen(COLOR_BG);

  // Initial State (Matching bozza.jpeg)
  currentTelemetry.gear = 2;
  currentTelemetry.speed = 30;
  currentTelemetry.rpm = 5000;
  currentTelemetry.shifts = 50; // 50 initial pneumatic shifts
  currentTelemetry.fuel = 13;
  currentTelemetry.throttle = 45;
  currentTelemetry.brake = 0;
  currentTelemetry.waterTemp = 104;
  currentTelemetry.oilTemp = 80;
  currentTelemetry.batteryDeciV = 133; // 13.3 V
  for (int i = 0; i < 4; i++) currentTelemetry.tirePress[i] = 21;

  // Draw full static dashboard frame and fixed labels
  drawStaticUI();

  // Force first full draw
  prevTelemetry = currentTelemetry;
  prevTelemetry.gear = -99;
  prevTelemetry.rpm = -99;
  prevTelemetry.speed = -99;
  prevTelemetry.shifts = -99;
  prevTelemetry.fuel = -99;
  prevTelemetry.throttle = -99;
  prevTelemetry.brake = -99;
  prevTelemetry.waterTemp = -99;
  prevTelemetry.oilTemp = -99;
  prevTelemetry.batteryDeciV = -99;
  for (int i = 0; i < 4; i++) prevTelemetry.tirePress[i] = -99;

  renderChangedTelemetry();
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  unsigned long now = millis();

  // 1. Process Manual Inputs (Potentiometer & Buttons)
  handleInputs();

  // 2. Update Display & Shift Lights (~30 FPS)
  if (now - lastUpdateMs >= 35) {
    lastUpdateMs = now;
    renderChangedTelemetry();
    updateShiftLights(currentTelemetry.rpm);
  }
}

// ============================================================================
// STATIC UI DRAWING (Drawn ONCE to save Flash and CPU)
// ============================================================================
void drawStaticUI() {
  tft.fillScreen(COLOR_BG);

  // Borders & Dividers
  tft.drawRect(0, 0, 320, 240, COLOR_FRAME);
  tft.drawFastHLine(0, 44, 320, COLOR_FRAME);
  tft.drawFastHLine(0, 178, 320, COLOR_FRAME);
  tft.drawFastVLine(82, 44, 134, COLOR_FRAME);
  tft.drawFastVLine(238, 44, 134, COLOR_FRAME);

  // Top Row Static Labels
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(3);
  tft.setCursor(44, 6);
  tft.print(" l");

  tft.setTextSize(2);
  tft.setCursor(8, 28);
  tft.print("fuel");

  tft.setTextSize(3);
  tft.setCursor(206, 8);
  tft.print("38 cm");
  tft.setTextSize(2);
  tft.setCursor(296, 4);
  tft.print('3');

  // Static unit labels for Center Column
  tft.setCursor(144, 120);
  tft.print("km/h");
  tft.setCursor(144, 146);
  tft.print("giri/m");

  // 4 Tires frames & static "psi" units
  tft.drawRect(12, 62, 24, 38, COLOR_TIRE_LINE);
  tft.fillRect(14, 64, 20, 34, COLOR_TIRE_FILL);

  tft.drawRect(46, 62, 24, 38, COLOR_TIRE_LINE);
  tft.fillRect(48, 64, 20, 34, COLOR_TIRE_FILL);

  tft.drawRect(12, 114, 24, 38, COLOR_TIRE_LINE);
  tft.fillRect(14, 116, 20, 34, COLOR_TIRE_FILL);

  tft.drawRect(46, 114, 24, 38, COLOR_TIRE_LINE);
  tft.fillRect(48, 116, 20, 34, COLOR_TIRE_FILL);

  tft.setTextSize(1);
  tft.setCursor(20, 50);  tft.print("psi");
  tft.setCursor(56, 50);  tft.print("psi");
  tft.setCursor(20, 160); tft.print("psi");
  tft.setCursor(56, 160); tft.print("psi");

  // Right Column label
  tft.setTextSize(2);
  tft.setCursor(244, 126);
  tft.print("shifts");

  // Bottom Row Labels
  tft.drawRect(8, 184, 62, 14, COLOR_GREEN_BAR);
  tft.setCursor(74, 184);
  tft.print("THR");

  tft.drawRect(8, 206, 62, 14, COLOR_RED_BAR);
  tft.setCursor(74, 206);
  tft.print("BRK");

  tft.setCursor(118, 210);
  tft.print("water");

  tft.setCursor(184, 210);
  tft.print("oil");

  tft.setCursor(236, 210);
  tft.print("battery");

  tft.setCursor(284, 186);
  tft.print(" V");

  // Degree symbols
  tft.fillRect(162, 186, 3, 3, COLOR_TEXT);
  tft.fillRect(212, 186, 3, 3, COLOR_TEXT);
}

// ============================================================================
// DYNAMIC FLICKER-FREE RENDERING (Numeric and Color values only)
// ============================================================================
void renderChangedTelemetry() {
  // 1. Fuel Quantity
  if (currentTelemetry.fuel != prevTelemetry.fuel) {
    tft.setTextSize(3);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(8, 6);
    if (currentTelemetry.fuel < 10) tft.print(' ');
    tft.print(currentTelemetry.fuel);
    prevTelemetry.fuel = currentTelemetry.fuel;
  }

  // 2. Tire Pressures
  for (int i = 0; i < 4; i++) {
    if (currentTelemetry.tirePress[i] != prevTelemetry.tirePress[i]) {
      tft.setTextSize(1);
      tft.setTextColor(COLOR_TEXT, COLOR_BG);
      if (i == 0)      tft.setCursor(8,  50);
      else if (i == 1) tft.setCursor(44, 50);
      else if (i == 2) tft.setCursor(8, 160);
      else if (i == 3) tft.setCursor(44, 160);
      if (currentTelemetry.tirePress[i] < 10) tft.print(' ');
      tft.print(currentTelemetry.tirePress[i]);
      prevTelemetry.tirePress[i] = currentTelemetry.tirePress[i];
    }
  }

  // 3. Gear Indicator
  if (currentTelemetry.gear != prevTelemetry.gear) {
    tft.setTextSize(7);
    tft.setCursor(140, 52);
    if (currentTelemetry.gear == 0) {
      tft.setTextColor(COLOR_GEAR_N, COLOR_BG);
      tft.print('N');
    } else {
      tft.setTextColor(COLOR_GEAR_NUM, COLOR_BG);
      tft.print(currentTelemetry.gear);
    }
    prevTelemetry.gear = currentTelemetry.gear;
  }

  // 4. Speed (Only redraw numeric digits)
  if (currentTelemetry.speed != prevTelemetry.speed) {
    tft.setTextSize(2);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(102, 120);
    if (currentTelemetry.speed < 100) tft.print(' ');
    if (currentTelemetry.speed < 10)  tft.print(' ');
    tft.print(currentTelemetry.speed);
    prevTelemetry.speed = currentTelemetry.speed;
  }

  // 5. RPM (Only redraw numeric digits)
  if (currentTelemetry.rpm != prevTelemetry.rpm) {
    tft.setTextSize(2);
    if (currentTelemetry.rpm >= 11500)      tft.setTextColor(ILI9341_RED, COLOR_BG);
    else if (currentTelemetry.rpm >= 9000) tft.setTextColor(ILI9341_YELLOW, COLOR_BG);
    else                                   tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(78, 146);
    if (currentTelemetry.rpm < 10000) tft.print(' ');
    if (currentTelemetry.rpm < 1000)  tft.print(' ');
    tft.print(currentTelemetry.rpm);
    prevTelemetry.rpm = currentTelemetry.rpm;
  }

  // 6. Pneumatic Shifts Remaining
  if (currentTelemetry.shifts != prevTelemetry.shifts) {
    tft.setTextSize(6);
    if (currentTelemetry.shifts <= 0)       tft.setTextColor(COLOR_WARN_RED, COLOR_BG);
    else if (currentTelemetry.shifts <= 10) tft.setTextColor(ILI9341_YELLOW, COLOR_BG);
    else                                    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setCursor(244, 60);
    int s = (currentTelemetry.shifts > 0) ? currentTelemetry.shifts : 0;
    if (s < 10) tft.print(' ');
    tft.print(s);
    prevTelemetry.shifts = currentTelemetry.shifts;
  }

  // 7. Throttle Bar
  if (currentTelemetry.throttle != prevTelemetry.throttle) {
    int fillW = (constrain(currentTelemetry.throttle, 0, 100) * 58) / 100;
    tft.fillRect(10, 186, fillW, 10, COLOR_GREEN_BAR);
    if (fillW < 58) tft.fillRect(10 + fillW, 186, 58 - fillW, 10, COLOR_BG);
    prevTelemetry.throttle = currentTelemetry.throttle;
  }

  // 8. Brake Bar
  if (currentTelemetry.brake != prevTelemetry.brake) {
    int fillW = (constrain(currentTelemetry.brake, 0, 100) * 58) / 100;
    tft.fillRect(10, 208, fillW, 10, COLOR_RED_BAR);
    if (fillW < 58) tft.fillRect(10 + fillW, 208, 58 - fillW, 10, COLOR_BG);
    prevTelemetry.brake = currentTelemetry.brake;
  }

  // 9. Water Temp
  if (currentTelemetry.waterTemp != prevTelemetry.waterTemp) {
    tft.setTextSize(2);
    tft.setTextColor(currentTelemetry.waterTemp >= 108 ? COLOR_WARN_RED : COLOR_TEXT, COLOR_BG);
    tft.setCursor(124, 186);
    if (currentTelemetry.waterTemp < 100) tft.print(' ');
    tft.print(currentTelemetry.waterTemp);
    prevTelemetry.waterTemp = currentTelemetry.waterTemp;
  }

  // 10. Oil Temp
  if (currentTelemetry.oilTemp != prevTelemetry.oilTemp) {
    tft.setTextSize(2);
    tft.setTextColor(currentTelemetry.oilTemp >= 115 ? COLOR_WARN_RED : COLOR_TEXT, COLOR_BG);
    tft.setCursor(184, 186);
    if (currentTelemetry.oilTemp < 100) tft.print(' ');
    tft.print(currentTelemetry.oilTemp);
    prevTelemetry.oilTemp = currentTelemetry.oilTemp;
  }

  // 11. Battery Voltage
  if (currentTelemetry.batteryDeciV != prevTelemetry.batteryDeciV) {
    tft.setTextSize(2);
    tft.setTextColor(currentTelemetry.batteryDeciV < 118 ? COLOR_WARN_RED : COLOR_TEXT, COLOR_BG);
    tft.setCursor(236, 186);
    tft.print(currentTelemetry.batteryDeciV / 10);
    tft.print('.');
    tft.print(currentTelemetry.batteryDeciV % 10);
    prevTelemetry.batteryDeciV = currentTelemetry.batteryDeciV;
  }
}

// ============================================================================
// SHIFT LIGHTS (24 LED BLOCKS ACROSS HEADER)
// ============================================================================
void updateShiftLights(int rpm) {
  const int RPM_IDLE    = 2500;
  const int RPM_REDLINE = 11500;
  const int RPM_LIMITER = 12000;

  bool isLimiter = (rpm >= RPM_LIMITER);
  if (isLimiter) {
    unsigned long now = millis();
    if (now - lastFlashMs >= 70) {
      lastFlashMs = now;
      flashState = !flashState;
    }
  }

  int ledsToLight = 0;
  if (rpm > RPM_IDLE) {
    int clampedRpm = (rpm > RPM_REDLINE) ? RPM_REDLINE : rpm;
    ledsToLight = ((clampedRpm - RPM_IDLE) * 24) / (RPM_REDLINE - RPM_IDLE);
  }

  // Draw 24 Rectangular Shift LEDs in Center Header (X: 86 to 204, Y: 16)
  const int startX = 86;
  const int startY = 16;
  for (int i = 0; i < 24; i++) {
    int px = startX + (i * 5);
    uint16_t col = COLOR_BG;
    if (isLimiter) {
      col = flashState ? ILI9341_BLUE : COLOR_BG;
    } else if (i < ledsToLight) {
      if (i < 8) col = ILI9341_GREEN;
      else if (i < 16) col = ILI9341_RED;
      else col = ILI9341_BLUE;
    }
    tft.fillRect(px, startY, 4, 6, col);
  }
}

// ============================================================================
// FULL MANUAL INPUT CONTROL (POTENTIOMETER & BUTTONS)
// ============================================================================
void handleInputs() {
  unsigned long now = millis();

  // 1. Read Potentiometer for Throttle (0 to 100%)
  int potVal = analogRead(POT_PIN);
  if (potVal > 1023) potVal = potVal >> 2; // Normalize 12-bit (0-4095) down to 10-bit if needed
  currentTelemetry.throttle = (constrain(potVal, 0, 1023) * 100) / 1023;

  // Brake is 0 in normal throttle drive, or simulates trail-braking if throttle is closed
  currentTelemetry.brake = (currentTelemetry.throttle == 0) ? 10 : 0;

  // Calculate RPM and Speed according to current Gear
  if (currentTelemetry.gear == 0) {
    // Neutral revving
    currentTelemetry.rpm = 2000 + ((currentTelemetry.throttle * 10000) / 100);
    currentTelemetry.speed = 0;
  } else {
    // In Gear driving
    int maxSpd = currentTelemetry.gear * 24 + 10;
    currentTelemetry.rpm = 3000 + ((currentTelemetry.throttle * 9000) / 100);
    currentTelemetry.speed = (8 * currentTelemetry.gear) + (((currentTelemetry.rpm - 3000) * (maxSpd - (8 * currentTelemetry.gear))) / 9000);
  }

  // Slight natural fluctuations on temperatures and battery
  int cycle = (now / 300) % 10;
  currentTelemetry.batteryDeciV = 133 + (cycle / 3);
  currentTelemetry.waterTemp = 104 + (cycle / 4);
  currentTelemetry.oilTemp = 80 + (cycle / 5);

  // 2. Shift UP Button (D4) - Consumes Pneumatic Air
  int upReading = digitalRead(BTN_UP);
  if (upReading == LOW && lastUpState == HIGH && (now - lastDebounceUp > 200)) {
    lastDebounceUp = now;
    if (currentTelemetry.gear < 6 && currentTelemetry.shifts > 0) {
      currentTelemetry.gear++;
      currentTelemetry.shifts--;
    }
  }
  lastUpState = upReading;

  // 3. Shift DOWN Button (D5) - Consumes Pneumatic Air
  int downReading = digitalRead(BTN_DOWN);
  if (downReading == LOW && lastDownState == HIGH && (now - lastDebounceDown > 200)) {
    lastDebounceDown = now;
    if (currentTelemetry.gear > 0 && currentTelemetry.shifts > 0) {
      currentTelemetry.gear--;
      currentTelemetry.shifts--;
    }
  }
  lastDownState = downReading;

  // 4. Refill Air Bottle Button (D7) - Resets Pneumatic Air to 50
  int refillReading = digitalRead(BTN_REFILL);
  if (refillReading == LOW && lastRefillState == HIGH && (now - lastDebounceRefill > 300)) {
    lastDebounceRefill = now;
    currentTelemetry.shifts = 50; // Refill air tank to 50 shifts
  }
  lastRefillState = refillReading;
}
