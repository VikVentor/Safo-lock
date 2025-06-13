#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>
#include <ESP32Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_I2C_ADDR 0x3C

#define ENCODER_CLK 32
#define ENCODER_DT  19
#define ENCODER_SW  18
#define SERVO_PIN   14

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Preferences prefs;
Servo vaultServo;

const int codeLength = 4;
int correctCode[codeLength] = {5, 4, 7, 6};
int enteredCode[codeLength] = {0};

int currentDigitValue = 0;
int currentIndex = 0;
bool readyToEnterNext = false;
int lastCLKState;
bool vaultUnlocked = false;
bool inMenu = false;
bool settingNewCode = false;
bool menuSelection = false; // false = Lock, true = Set Password

void setup() {
  Serial.begin(115200);

  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);

  Wire.begin(21, 22);  // SDA, SCL
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_I2C_ADDR);
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);

  vaultServo.attach(SERVO_PIN);
  vaultServo.write(0); // Start locked

  prefs.begin("vault", false);
  for (int i = 0; i < codeLength; i++) {
    correctCode[i] = prefs.getInt(("d" + String(i)).c_str(), correctCode[i]);
  }
  prefs.end();

  lastCLKState = digitalRead(ENCODER_CLK);
  showMessage("PIN 1:\nDIGITS:");
}

void loop() {
  int newCLKState = digitalRead(ENCODER_CLK);

  if (lastCLKState == HIGH && newCLKState == LOW) {
    int dtState = digitalRead(ENCODER_DT);
    if (!vaultUnlocked && !settingNewCode) {
      if (dtState == HIGH) currentDigitValue++;
      else currentDigitValue--;
    } else if (vaultUnlocked && inMenu) {
      menuSelection = !menuSelection;
    } else if (settingNewCode) {
      if (dtState == HIGH) currentDigitValue++;
      else currentDigitValue--;
    }
    if (currentDigitValue < 0) currentDigitValue = 0;
    if (currentDigitValue > 99) currentDigitValue = 99;
    updateDisplay();
  }

  lastCLKState = newCLKState;

  if (digitalRead(ENCODER_SW) == LOW && !readyToEnterNext) {
    delay(300);  // Debounce
    readyToEnterNext = true;

    if (!vaultUnlocked && !settingNewCode) {
      enteredCode[currentIndex] = currentDigitValue;
      currentDigitValue = 0;
      currentIndex++;
      if (currentIndex < codeLength) {
        updateDisplay();
      } else {
        bool match = true;
        for (int i = 0; i < codeLength; i++) {
          if (enteredCode[i] != correctCode[i]) {
            match = false;
            break;
          }
        }
        if (match) {
          vaultUnlocked = true;
          inMenu = true;
          vaultServo.write(90);  // Unlock position
          showMenu();
        } else {
          showMessage("Wrong PIN\nTry Again");
          currentIndex = 0;
          currentDigitValue = 0;
          delay(2000);
          showMessage("PIN 1:\nDIGITS:");
        }
      }
    } else if (vaultUnlocked && inMenu) {
      if (!menuSelection) {
        showMessage("Locked");
        vaultServo.write(0);  // Lock position
        delay(2000);
        currentIndex = 0;
        currentDigitValue = 0;
        vaultUnlocked = false;
        inMenu = false;
        showMessage("PIN 1:\nDIGITS:");
      } else {
        settingNewCode = true;
        vaultUnlocked = false;
        inMenu = false;
        currentIndex = 0;
        currentDigitValue = 0;
        showMessage("NEW PIN 1:\nDIGITS:");
      }
    } else if (settingNewCode) {
      correctCode[currentIndex] = currentDigitValue;
      currentIndex++;
      currentDigitValue = 0;
      if (currentIndex < codeLength) {
        updateDisplay();
      } else {
        prefs.begin("vault", false);
        for (int i = 0; i < codeLength; i++) {
          prefs.putInt(("d" + String(i)).c_str(), correctCode[i]);
        }
        prefs.end();
        showMessage("PIN Saved\nVault Locked");
        vaultServo.write(0);  // Lock position
        delay(2000);
        settingNewCode = false;
        currentIndex = 0;
        showMessage("PIN 1:\nDIGITS:");
      }
    }
  }

  if (digitalRead(ENCODER_SW) == HIGH) {
    readyToEnterNext = false;
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 0);
  if (!vaultUnlocked && !settingNewCode) {
    display.printf("PIN %d:\nDIGITS: %d", currentIndex + 1, currentDigitValue);
  } else if (settingNewCode) {
    display.printf("NEW %d:\nDIGITS: %d", currentIndex + 1, currentDigitValue);
  } else if (vaultUnlocked && inMenu) {
    showMenu();
    return;
  }
  display.display();
}

void showMessage(const char* msg) {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println(msg);
  display.display();
}

void showMenu() {
  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(2);
  display.println("Menu:");
  if (menuSelection) {
    display.println("> Set PIN");
    display.println("  Lock");
  } else {
    display.println("  Set PIN");
    display.println("> Lock");
  }
  display.display();
}
