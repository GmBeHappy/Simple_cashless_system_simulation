#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "MFRC522.h"
#include "hd44780.h"
#include "hd44780_I2Cexp.h"
#define SS_PIN 10
#define RST_PIN 9

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#define OLED_RESET 4

#define BTN_1 4
#define BTN_2 5
#define BTN_3 6
#define BTN_4 7

void initLCD();
void initOLED();
void initScanner();
void initButtons();
void initTimerInterrupts();
void displayLCD(int row, int column, String text);
void displayOLED(int row, int column, String text);
void clearAllDisplays();
void printHex(byte* buffer, byte bufferSize);
void mainState();
void payState();
void topUpState();
void balanceState();
int getBalance(byte* uid);
bool updateBalance(int updateValue);

// lcd global variables
hd44780_I2Cexp lcd(0x27);
const int LCD_COLS = 16;
const int LCD_ROWS = 2;

// uid scanner global variables
byte uid[4];
MFRC522 rfid(SS_PIN, RST_PIN);  // Instance of the class
MFRC522::MIFARE_Key key;

// oled global variables
Adafruit_SSD1306 oled(OLED_RESET);

// general global variables
boolean isExitMain = false;
int balanceStateTimer = 0;
int generalTimer = 0;
int balance = 0;

void setup() {
  initLCD();
  initOLED();
  initScanner();
  initButtons();
  initTimerInterrupts();
}

void loop() {
  displayLCD(0, 0, "Please scan your");
  displayLCD(1, 0, "ID card");

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    for (byte i = 0; i < 4; i++) {
      uid[i] = rfid.uid.uidByte[i];
    }
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    // if (getBalance(uid) != -1) {
    mainState();
    //}
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void mainState() {
  clearAllDisplays();
  oled.display();

  generalTimer = 0;

  while (1) {
    displayLCD(0, 0, "Select Mode :");
    displayLCD(1, 0, " Pay|Top-up|Bal");
    displayOLED(10, 28, "access");
    delay(1000);

    // pay mode
    if (!digitalRead(BTN_1)) {
      payState();
    }
    // top-up mode
    else if (!digitalRead(BTN_2)) {
      topUpState();
    }
    // check-balance mode
    else if (!digitalRead(BTN_3)) {
      balanceState();
    }

    if (isExitMain) {
      break;
    }

    if (generalTimer >= 5) {
      generalTimer = 0;
      clearAllDisplays();
      oled.display();
      break;
    }
  }

  isExitMain = false;
  clearAllDisplays();
  oled.display();
}

void payState() {
  clearAllDisplays();
  oled.display();

  int paymentValue = 0;
  generalTimer = 0;
  while (1) {
    displayLCD(0, 0, "$:" + String(paymentValue));
    displayLCD(1, 0, "+5|-5|Enter|Exit");
    displayOLED(10, 28, "pay");

    if (!digitalRead(BTN_1)) {
      paymentValue += 5;
      generalTimer = 0;
    } else if (!digitalRead(BTN_2)) {
      if (paymentValue >= 5) {
        paymentValue -= 5;
        generalTimer = 0;
      }
    } else if (!digitalRead(BTN_3)) {
      if (updateBalance(-1 * paymentValue)) {
        isExitMain = true;
        break;
      }
    } else if (!digitalRead(BTN_4)) {
      clearAllDisplays() generalTimer = 0;
      break;
    }

    if (generalTimer >= 8) {
      generalTimer = 0;
      clearAllDisplays() break;
    }
  }

  clearAllDisplays();
  oled.display();
}

void topUpState() {
  clearAllDisplays();
  oled.display();

  int topUpValue = 0;
  while (1) {
    displayLCD(0, 0, "$:" + String(topUpValue));
    displayLCD(1, 0, "+5|-5|Enter|Exit");
    displayOLED(10, 28, "top-up");

    if (!digitalRead(BTN_1)) {
      topUpValue += 5;
      generalTimer = 0;
    } else if (!digitalRead(BTN_2)) {
      if (topUpValue >= 5) {
        topUpValue -= 5;
      }
      generalTimer = 0;
    } else if (!digitalRead(BTN_3)) {
      if (updateBalance(topUpValue)) {
        isExitMain = true;
        break;
      }
    } else if (!digitalRead(BTN_4)) {
      clearAllDisplays();
      generalTimer = 0;
      break;
    }

    if (generalTimer >= 8) {
      balanceStateTimer = 0;
      clearAllDisplays();
      break;
    }
  }

  clearAllDisplays();
}

void balanceState() {
  clearAllDisplays();
  oled.display();

  balanceStateTimer while (1) {
    displayLCD(0, 0, "$:" + String(balance));
    displayOLED(10, 28, "balance");

    if (balanceStateTimer >= 3) {
      balanceStateTimer = 0;
      clearAllDisplays();
      break;
    }
  }
}

ISR(TIMER1_COMPA_vect) {
  balanceStateTimer++;
  generalTimer++;
}

void initLCD() {
  // initial lcd
  int status;
  status = lcd.begin(LCD_COLS, LCD_ROWS);
  if (status) {
    hd44780::fatalError(status);
  }
}

void initOLED() {
  // intial oled
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
}

void initScanner() {
  // initial scanner
  Serial.begin(9600);
  SPI.begin();      // Init SPI bus
  rfid.PCD_Init();  // Init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }
}

void initButtons() {
  // initial button
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT_PULLUP);
}

void initTimerInterrupts() {
  // init timer interrupts
  noInterrupts();  // disable all interrupts
  // Clear Timer/Counter Control Register for Interrupt 1, bytes A and B
  // (TCCR1?)
  TCCR1A = 0;  // Clear TCCR1A/B registers
  TCCR1B = 0;
  TCNT1 = 0;  // Initialize counter value to 0 (16-bit counter register)
  // set compare match register for TIMER1: CLOCKFREQUENCY / frequency /
  // prescaler - 1
  OCR1A = 15624;  // 16MHz/(1Hz*1024) – 1 (must be <65536)
  // Timer/Counter Control Register for Interrupt 1 on register B
  TCCR1B |= (1 << WGM12);  // Mode 4, turn on CTC mode
  // Clock Select Bit, Set CS12, CS11 and CS10 bits
  TCCR1B |=
      (1 << CS12) | (1 << CS10);  // Set CS10 and CS12 bits for 1024 prescaler
  TIMSK1 |= (1 << OCIE1A);  // enable timer compare interrupt , The value in
                            // OCR1A is used for compare
  interrupts();             // enable all interrupts
}

int getBalance(byte* uid) { return -1; }

bool updateBalance(int updateValue) {
  if (balance + updateValue < 0) {
    return false;
  }
  balance += updateValue;
  return true;
}

void printHex(byte* buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

void displayLCD(int row, int column, String text) {
  lcd.setCursor(column, row);
  lcd.print(text);
}

void displayOLED(int row, int column, String text) {
  oled.setCursor(column, row);
  oled.print(text);
  oled.display();
}

void clearAllDisplays() {
  lcd.clear();
  oled.clearDisplay();
}
