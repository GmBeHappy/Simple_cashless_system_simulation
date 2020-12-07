#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "MFRC522.h"
#include "hd44780.h"
#include "hd44780_I2Cexp.h"
#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"

#define memorylength 2048
#define SS_PIN 10
#define RST_PIN 9 
#define BTN_1 4
#define OLED_RESET 4
#define BTN_2 5
#define BTN_3 6
#define BTN_4 7
#define EEPROM_DEVICE_ADDRESS 0x50
#define BUZZER_PIN 3
#define IDR_PIN A1

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
bool pendingState();
int getBalance(byte* uid);
bool updateBalance(int updateValue);
void alarm();
void saveBalance();
byte readEEPROM_byte(int device, unsigned int address);
void readEEPROM_page(int device, unsigned int address, byte *buffer, int length);
void writeEEPROM_page(int device, unsigned int address, byte* buffer, byte length);
void openBox();
void closeBox();

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
int userCredential = 0;
boolean isInTopUpState = false;
boolean isValidInsert = false;

// buzzer global variables
int highc = 523;

// stepmotor global variable
long startMotor;

void setup() {
  Wire.begin();
  Serial.begin(9600);
  initLCD();
  initOLED();
  initScanner();
  initButtons();
  initTimerInterrupts();
  pinMode(IDR_PIN, INPUT);
  clearAllDisplays();
  oled.clearDisplay();
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
    tone(BUZZER_PIN, highc, 100);
    if (getBalance(uid) != -1) {
      Serial.println(getBalance(uid));
      balance = getBalance(uid);
      mainState();
    } else {
      oled.clearDisplay();
      displayOLED(10, 28, "deny");
      delay(1500);
      oled.clearDisplay();
      oled.display();
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }

  alarm();
  balance = 0;
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
      tone(BUZZER_PIN, highc, 100);
      payState();
    }
    // top-up mode
    else if (!digitalRead(BTN_2)) {
      tone(BUZZER_PIN, highc, 100);
      topUpState();
    }
    // check-balance mode
    else if (!digitalRead(BTN_3)) {
      tone(BUZZER_PIN, highc, 100);
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

    alarm();
  }

  isExitMain = false;
  userCredential = 0;
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
      tone(BUZZER_PIN, highc, 100);
      paymentValue += 5;
      generalTimer = 0;
    } else if (!digitalRead(BTN_2)) {
      tone(BUZZER_PIN, highc, 100);
      if (paymentValue >= 5) {
        paymentValue -= 5;
        generalTimer = 0;
      }
    } else if (!digitalRead(BTN_3)) {
      tone(BUZZER_PIN, highc, 100);
      if (updateBalance(-1 * paymentValue)) {
        saveBalance();
        isExitMain = true;
        oled.clearDisplay();
        displayOLED(10, 28, "done");
        delay(1000);
        break;
      } else {
        oled.clearDisplay();
        displayOLED(2, 5, "not enough");
        displayOLED(15, 32, "money");
        delay(1500);
        oled.clearDisplay();
      }
    } else if (!digitalRead(BTN_4)) {
      tone(BUZZER_PIN, highc, 100);
      clearAllDisplays(); 
      generalTimer = 0;
      break;
    }

    if (generalTimer >= 8) {
      generalTimer = 0;
      clearAllDisplays();
      break;
    }

    alarm();
  }

  clearAllDisplays();
  oled.display();
}

void topUpState() {
  clearAllDisplays();
  oled.display();

  isInTopUpState = true;
  int topUpValue = 0;
  while (1) {
    displayLCD(0, 0, "$:" + String(topUpValue));
    displayLCD(1, 0, "+5|-5|Enter|Exit");
    displayOLED(10, 28, "top-up");

    if (!digitalRead(BTN_1)) {
      tone(BUZZER_PIN, highc, 100);
      topUpValue += 5;
      generalTimer = 0;
    } else if (!digitalRead(BTN_2)) {
      tone(BUZZER_PIN, highc, 100);
      if (topUpValue >= 5) {
        topUpValue -= 5;
      }
      generalTimer = 0;
    } else if (!digitalRead(BTN_3)) {
      tone(BUZZER_PIN, highc, 100);
      generalTimer = 0;
      if (topUpValue > 0 && pendingState() && updateBalance(topUpValue)) {
        saveBalance();
        isExitMain = true;
        break;
      }
    } else if (!digitalRead(BTN_4)) {
      tone(BUZZER_PIN, highc, 100);
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

  isInTopUpState = false;
  clearAllDisplays();
}

void balanceState() {
  clearAllDisplays();
  oled.display();                   

  balanceStateTimer = 0; 
  while(1) {
    displayLCD(0, 0, "$:" + String(balance));
    displayOLED(10, 28, "balance");

    if (balanceStateTimer >= 4) {
      balanceStateTimer = 0;
      generalTimer = 0;
      clearAllDisplays();
      break;
    }

    alarm();
  }
}

bool pendingState() {
  clearAllDisplays();
  oled.clearDisplay();
  openBox();

  while(1) {
    displayLCD(0, 0, "Insert money");
    displayLCD(1, 0, "Done|Cancel");
    displayOLED(10, 25, "pending");

    if(!digitalRead(BTN_1)) {
      tone(BUZZER_PIN, highc, 100);
      generalTimer = 0;
      closeBox();
      return true;
    } else if (!digitalRead(BTN_2)) {
      tone(BUZZER_PIN, highc, 100);
      generalTimer = 0;
      closeBox();
      oled.clearDisplay();
      displayOLED(10, 28, "done");
      delay(1000);
      clearAllDisplays();
      return false;
    }

    if (generalTimer >= 8) {
      generalTimer = 0;
      closeBox();
      clearAllDisplays();
      return false;
    }
  }

  clearAllDisplays();
}

ISR(TIMER1_COMPA_vect) {
  balanceStateTimer++;
  generalTimer++;
  // check thief

  if (!isInTopUpState && analogRead(IDR_PIN) > 100) {
    isValidInsert = false;
  } else {
    isValidInsert = true;
  }
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

int getBalance(byte* uid) { 
  for(int i = 0; i < 2; i++){
    byte savedUid[4]; 
    byte memoryAddress = 0x00 + (i*16);
    readEEPROM_page(EEPROM_DEVICE_ADDRESS, memoryAddress, savedUid, sizeof(savedUid));
    if (uid[0] == savedUid[0] &&
        uid[1] == savedUid[1] &&
        uid[2] == savedUid[2] &&
        uid[3] == savedUid[3]) {
          String savedBalance = "";
          byte readValue = readEEPROM_byte(EEPROM_DEVICE_ADDRESS, memoryAddress+4);
          while (readValue != 0xFF) {
            savedBalance += String(char(readValue));
            memoryAddress++;
            readValue = readEEPROM_byte(EEPROM_DEVICE_ADDRESS, memoryAddress+4);
          }

          userCredential = i+1;

          return savedBalance.toInt();
        }
  }

  return -1; 
 }

bool updateBalance(int updateValue) {
  if (balance + updateValue < 0) {
    return false;
  }
  balance += updateValue;
  return true;
}

void saveBalance() {
  String stringBalance = String(balance);
  char charBalance[stringBalance.length() + 1];
  stringBalance.toCharArray(charBalance, stringBalance.length() + 1);
  Serial.println(userCredential);
  Serial.println(stringBalance);
  for (auto ch: charBalance) {
    Serial.print(" ch ");
    Serial.println(ch);
  }

  Serial.println(0x00 + ((userCredential-1) * 16) + 0x04, HEX);
  byte cleanEEPROM[12] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
  writeEEPROM_page(EEPROM_DEVICE_ADDRESS, 0x00 + ((userCredential-1) * 16) + 0x04, (byte *)cleanEEPROM, sizeof(cleanEEPROM));
  writeEEPROM_page(EEPROM_DEVICE_ADDRESS, 0x00 + ((userCredential-1) * 16) + 0x04, (byte *)charBalance, sizeof(charBalance));

}

void alarm() {
  Serial.println(analogRead(IDR_PIN));
  if (!isValidInsert) {
    tone(BUZZER_PIN, highc, 1000);
  }
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

byte readEEPROM_byte(int device, unsigned int address ) {
  byte rdata = 0;
  Wire.beginTransmission(device | (int)(address >> 8));
  Wire.write((int)(address & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(device, 1);
  if (Wire.available())
  rdata = Wire.read();
  return rdata;
}

void readEEPROM_page(int device, unsigned int address, byte *buffer, int length )
{
  byte i;
  Wire.beginTransmission(device | (int)(address >> 8));
  Wire.write((int)(address & 0xFF)); // LSB
  Wire.endTransmission();
  Wire.requestFrom(device, length);
  for ( i = 0; i < length; i++ )
  if (Wire.available())
  buffer[i] = Wire.read();
}

void writeEEPROM_page(int device, unsigned int address, byte* buffer, byte length ) {
  byte i;
  Wire.beginTransmission(device | (int)(address >> 8));
  Wire.write((int)(address & 0xFF)); // LSB
  for ( i = 0; i < length; i++)
  Wire.write(buffer[i]);
  Wire.endTransmission();
  delay(10);
}

void closeBox(){
  Serial.println("Close Box");
  byte address,data,device;
  address = 0x23;
  device = address; 
  startMotor = millis();
  while ((millis()-startMotor)<=2800){
    data = 0x10;
    for (int i = 1 ; i <= 4; i++)
    {
      Wire.beginTransmission(device);
      Wire.write(data);
      Wire.endTransmission();
      delay(5);
      data = data << 1;
    }
  }
}

void openBox(){
  Serial.println("Open Box");
  byte address,data,device;
  address = 0x23;
  device = address; 
  startMotor = millis();
  while ((millis()-startMotor)<=2800){
    data = 0x80;
    for (int i = 1 ; i <= 4; i++)
    {
      Wire.beginTransmission(device);
      Wire.write(data);
      Wire.endTransmission();
      delay(5);
      data = data >> 1;
    }
  }
}

