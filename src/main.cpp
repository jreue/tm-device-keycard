#include <Arduino.h>
#include <DIYables_LCD_I2C.h>
#include <Keypad.h>
#include <shared_hardware_config.h>

#include "Button.h"
#include "EspNowHelper.h"
#include "hardware_config.h"

uint8_t hubAddress[] = HUB_MAC_ADDRESS;
EspNowHelper espNowHelper;

void handleButtonPress(void* button_handle, void* usr_data);
void handleKeyboardInput();
void handleEnterKeyPressed();
void showInstructions();
void resetInput();
void playDeniedTone();
void playGrantedTone();
static void playTone(int freq, int duration_ms);
void notifyHub();

DIYables_LCD_I2C lcd(0x27, 20, 4);

const byte ROWS = 4;
const byte COLS = 3;

char keys[ROWS][COLS] = {{'1', '2', '3'}, {'4', '5', '6'}, {'7', '8', '9'}, {'*', '0', '#'}};

byte rowPins[ROWS] = {KEYPAD_R1, KEYPAD_R2, KEYPAD_R3, KEYPAD_R4};
byte colPins[COLS] = {KEYPAD_C1, KEYPAD_C2, KEYPAD_C3};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

enum State { STATE_INSTRUCTIONS, STATE_INPUT, STATE_DENIED, STATE_WIN };
State state;

const unsigned long DENIED_TIMEOUT_MS = 3000;

#define BUZZER_LEDC_CHANNEL 4
#define BUZZER_LEDC_RESOLUTION 8

int digitCount = 0;
int displayPos = 0;
unsigned long deniedAt = 0;

void setup() {
  Serial.begin(115200);

  espNowHelper.begin(DEVICE_ID);
  espNowHelper.addPeer(hubAddress);
  espNowHelper.sendModuleConnected(hubAddress);

  lcd.init();
  lcd.backlight();

  Button* btn = new Button(BUTTON_PIN, false);
  btn->attachSingleClickEventCb(&handleButtonPress, NULL);

  showInstructions();
}

void loop() {
  if (state == STATE_WIN) {
    notifyHub();
    return;
  }

  if (state == STATE_DENIED) {
    if (millis() - deniedAt >= DENIED_TIMEOUT_MS) {
      showInstructions();
    }
    return;
  }

  handleKeyboardInput();
}

void notifyHub() {
  static bool notified = false;
  if (!notified) {
    espNowHelper.sendModuleUpdated(hubAddress, true);
    notified = true;
  }
}

void showInstructions() {
  lcd.clear();
  state = STATE_INSTRUCTIONS;
  resetInput();

  char line1[21];
  snprintf(line1, sizeof(line1), "Enter %d Digit", NUM_DIGITS);
  int col1 = (20 - (int)strlen(line1)) / 2;

  const char* line2 = "Access Code";
  int col2 = (20 - (int)strlen(line2)) / 2;

  lcd.setCursor(col1, 1);
  lcd.print(line1);
  lcd.setCursor(col2, 2);
  lcd.print(line2);
}

void resetInput() {
  digitCount = 0;
  displayPos = 0;
}

void handleKeyboardInput() {
  char key = keypad.getKey();
  if (!key)
    return;

  Serial.println(key);

  // Ignore * — delimiter is system-only
  if (key == '*')
    return;

  // # is Submit/Enter
  if (key == '#') {
    if (state == STATE_INPUT) {
      handleEnterKeyPressed();
    }
    return;
  }

  // First digit clears instructions and starts input
  if (state == STATE_INSTRUCTIONS) {
    lcd.clear();
    state = STATE_INPUT;
  }

  // Cap at NUM_DIGITS
  if (digitCount >= NUM_DIGITS)
    return;

  // Print digit at current display position
  lcd.setCursor(displayPos % 20, displayPos / 20);
  lcd.print(key);
  displayPos++;
  digitCount++;

  // Auto-insert * delimiter after every 4th digit (not after the last)
  if (digitCount % 4 == 0 && digitCount < NUM_DIGITS) {
    lcd.setCursor(displayPos % 20, displayPos / 20);
    lcd.print('*');
    displayPos++;
  }
}

void handleEnterKeyPressed() {
  lcd.clear();
  const char* msg = "ACCESS DENIED";
  int col = (20 - (int)strlen(msg)) / 2;
  lcd.setCursor(col, 1);
  lcd.print(msg);
  state = STATE_DENIED;
  deniedAt = millis();
  resetInput();
  playDeniedTone();
}

void handleButtonPress(void* button_handle, void* usr_data) {
  lcd.clear();
  const char* msg = "ACCESS GRANTED";
  int col = (20 - (int)strlen(msg)) / 2;
  lcd.setCursor(col, 1);
  lcd.print(msg);
  state = STATE_WIN;
  playGrantedTone();
}

void playDeniedTone() {
  playTone(300, 220);
  delay(70);
  playTone(150, 370);
}

void playGrantedTone() {
  playTone(400, 150);
  delay(60);
  playTone(600, 150);
  delay(60);
  playTone(900, 260);
}

static void playTone(int freq, int duration_ms) {
  ledcSetup(BUZZER_LEDC_CHANNEL, freq, BUZZER_LEDC_RESOLUTION);
  ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
  ledcWriteTone(BUZZER_LEDC_CHANNEL, freq);
  delay(duration_ms);
  ledcWriteTone(BUZZER_LEDC_CHANNEL, 0);
  ledcDetachPin(BUZZER_PIN);
}