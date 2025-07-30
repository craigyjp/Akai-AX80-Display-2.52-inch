#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>  // Hardware-specific library
#include <Adafruit_ST7735.h>  // Hardware-specific library
#include <RoxMux.h>

#define COLOR_BLACK ST77XX_WHITE  // Was showing black as white
#define COLOR_WHITE ST77XX_BLACK  // Was showing white as black
#define COLOR_YELLOW ST77XX_BLUE  // Yellow showed up as blue
#define COLOR_CYAN ST77XX_RED     // Cyan showed up as red

#define SCREEN_WIDTH 284
#define SCREEN_HEIGHT 76

// OLED display pin definitions
#define TFT_CS 5
#define TFT_RST 4
#define TFT_DC 2

// Used for SPI connectivity
#define TFT_SCK 18
#define TFT_MOSI 23

// Setup the ST7735 LCD Display
Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

// Interrupt pins
const uint8_t INTERRUPT1 = 13;
const uint8_t INTERRUPT2 = 34;
const uint8_t INTERRUPT3 = 14;
const uint8_t INTERRUPT4 = 27;
const uint8_t INTERRUPT5 = 26;
const uint8_t INTERRUPT6 = 25;
const uint8_t INTERRUPT7 = 21;
const uint8_t INTERRUPT8 = 35;

#define LABEL_SET 3  // Define which label set to use (values from 0 to 4)

const int DISPLAY_COLUMN_ENABLE[5][9] = {
  // Columns: 0  1  2  3  4  5  6  7  8
  { -1, -1, 1, 2, 3, 4, 5, 6, -1 },  // Set 0
  { -1, 0, 1, 2, 3, 4, 5, 6, -1 },  // Set 1
  { -1, -1, -1, 0, 1, 2, 3, 4, 5 },  // Set 2
  { -1, -1, 0, 1, 2, 3, 4, -1, -1 },  // Set 3
  { 0, 1, 2, 3, 4, 5, -1, 6, 7 }   // Set 4
};

// const int INTERRUPT_TO_YINDEX[5][9] = {
//   // Label Set 0: {0–7} for mapped interrupt input, -1 if unused
//   { -1, 0, 1, 2, 3, 4, 5, -1, -1},    // Set 0
//   { -1, 0, 1, 2, 3, 4, 5, 6, -1},     // Set 1
//   { -1, 0, 1, 2, 3, 4, 5, -1, -1},    // Set 2
//   { -1, 0, 1, 2, 3, -1, -1, -1, -1},  // Set 3
//   { 0, 1, 2, 3, 4, 5, 6, 7, -1}       // Set 4 (example mix)
// };


// Input pins for multiplexer (Updated to avoid conflicts with SPI0)
int yPins[13] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
int yValues[9][13];  // Array to store multiplexer values
int prevYValues[9][13] = { 0 };  // Track last drawn values


volatile bool updateDisplay = false;                                                               // Flag to signal that the display should be updated
volatile bool interruptTriggered[8] = { false, false, false, false, false, false, false, false };  // Flags for interrupts


#define MUX_TOTAL 2
Rox74HC165<MUX_TOTAL> mux;

// Pins for 74HC165
#define PIN_DATA 22  // pin 9 on 74HC165 (DATA)
#define PIN_LOAD 33  // pin 1 on 74HC165 (LOAD)
#define PIN_CLK 32   // pin 2 on 74HC165 (CLK))

TaskHandle_t TaskDisplay;  // Handle for the display update task

// Interrupt handlers
void IRAM_ATTR xPin1() {
  interruptTriggered[0] = true;
}
void IRAM_ATTR xPin2() {
  interruptTriggered[1] = true;
}
void IRAM_ATTR xPin3() {
  interruptTriggered[2] = true;
}
void IRAM_ATTR xPin4() {
  interruptTriggered[3] = true;
}
void IRAM_ATTR xPin5() {
  interruptTriggered[4] = true;
}
void IRAM_ATTR xPin6() {
  interruptTriggered[5] = true;
}
void IRAM_ATTR xPin7() {
  interruptTriggered[6] = true;
}
void IRAM_ATTR xPin8() {
  interruptTriggered[7] = true;
}

void setup() {
  // Initialize serial communication for debugging
  Serial.begin(115200);

  // Initialize the multiplexer
  mux.begin(PIN_DATA, PIN_LOAD, PIN_CLK);

  // Set up interrupt pins and attach interrupts
  pinMode(INTERRUPT1, INPUT_PULLUP);
  attachInterrupt(INTERRUPT1, xPin1, FALLING);

  pinMode(INTERRUPT2, INPUT_PULLUP);
  attachInterrupt(INTERRUPT2, xPin2, FALLING);

  pinMode(INTERRUPT3, INPUT_PULLUP);
  attachInterrupt(INTERRUPT3, xPin3, FALLING);

  pinMode(INTERRUPT4, INPUT_PULLUP);
  attachInterrupt(INTERRUPT4, xPin4, FALLING);

  pinMode(INTERRUPT5, INPUT_PULLUP);
  attachInterrupt(INTERRUPT5, xPin5, FALLING);

  pinMode(INTERRUPT6, INPUT_PULLUP);
  attachInterrupt(INTERRUPT6, xPin6, FALLING);

  pinMode(INTERRUPT7, INPUT_PULLUP);
  attachInterrupt(INTERRUPT7, xPin7, FALLING);

  pinMode(INTERRUPT8, INPUT_PULLUP);
  attachInterrupt(INTERRUPT8, xPin8, FALLING);

  // Initialize the display
  tft.init(76, 284);
  tft.setRotation(3);

  tft.fillScreen(COLOR_BLACK);

  // Create a task to run the display update on core 0
  xTaskCreatePinnedToCore(
    displayTask,    // Function to implement the task
    "DisplayTask",  // Name of the task
    10000,          // Stack size in words
    NULL,           // Task input parameter
    1,              // Priority of the task
    &TaskDisplay,   // Task handle
    0               // Core where the task should run
  );

  memset(prevYValues, -1, sizeof(prevYValues));  // -1 ensures mismatch on first compare
  updateDisplay = true;
}

void loop() {
  // Check if any interrupt has triggered and update accordingly
  for (int i = 0; i < 8; i++) {
    if (interruptTriggered[i]) {
      interruptTriggered[i] = false;  // Reset the flag

      delayMicroseconds(70);  // Small delay to allow signals to settle

      mux.update();  // Perform the multiplexer update
      for (uint8_t j = 0; j < 13; j++) {
        yValues[i][j] = mux.read(j);
      }
      updateDisplay = true;  // Set the flag to update the display
    }
  }
}

void displayTask(void* parameter) {
  const int row0Height = (SCREEN_HEIGHT / 13) * 2;
  const int row0Gap = 2;
  const int remainingHeight = SCREEN_HEIGHT - row0Height - row0Gap;
  const int blockHeight = remainingHeight / 12;
  const int blockWidth = 24;

  const int* interruptMap = DISPLAY_COLUMN_ENABLE[LABEL_SET];
  const float totalSlots = 9;
  const float slotWidth = SCREEN_WIDTH / totalSlots;

  while (true) {
    if (updateDisplay) {
      updateDisplay = false;

      // --- Draw Row 0 ---
      for (int col = 0; col < 9; col++) {
        int interrupt = interruptMap[col];
        if (interrupt < 0 || interrupt > 7) continue;

        int xPos = round(col * slotWidth + (slotWidth - blockWidth) / 2);
        int yPos = SCREEN_HEIGHT - row0Height;

        if (yValues[interrupt][0] != prevYValues[interrupt][0]) {
          prevYValues[interrupt][0] = yValues[interrupt][0];

          if (yValues[interrupt][0] == 0) {
            tft.fillRect(xPos, yPos, blockWidth, row0Height, COLOR_YELLOW);
          } else {
            tft.fillRect(xPos, yPos, blockWidth, row0Height, COLOR_BLACK);
            tft.drawRect(xPos, yPos, blockWidth, row0Height, COLOR_YELLOW);
          }
        }
      }

      // --- Draw Rows 1–12 ---
      for (int col = 0; col < 9; col++) {
        int interrupt = interruptMap[col];
        if (interrupt < 0 || interrupt > 7) continue;

        int xPos = round(col * slotWidth + (slotWidth - blockWidth) / 2);

        for (int y = 1; y < 13; y++) {
          if (yValues[interrupt][y] != prevYValues[interrupt][y]) {
            prevYValues[interrupt][y] = yValues[interrupt][y];

            int yPos = SCREEN_HEIGHT - row0Height - row0Gap - (y * blockHeight);

            if (yValues[interrupt][y] == 0) {
              tft.fillRect(xPos + 1, yPos + 1, blockWidth - 2, blockHeight - 2, COLOR_CYAN);
            } else {
              tft.fillRect(xPos + 1, yPos + 1, blockWidth - 2, blockHeight - 2, COLOR_BLACK);
            }
          }
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}




