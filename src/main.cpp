#include <Arduino.h> 
#include <Wire.h>
#include "hd44780.h"
#include "hd44780_I2Cexp.h"

#include <SPI.h>
#include "MFRC522.h"
#define SS_PIN 10
#define RST_PIN 9

#include "Adafruit_GFX.h"
#include "Adafruit_SSD1306.h"
#define OLED_RESET 4

#define BTN_1 4
#define BTN_2 5
#define BTN_3 6
#define BTN_4 7

void displayLCD(int row, int column, String text);
void displayOLED(int row, int column, String text);
void printHex(byte *buffer, byte bufferSize);
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
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;

// oled global variables
Adafruit_SSD1306 oled(OLED_RESET);

// general global variables
boolean isExitMain = false;
int sec = 0;
int sec_1 = 0;
int balance = 0;

void setup() {

  // init lcd
  int status;
  status = lcd.begin(LCD_COLS, LCD_ROWS);
  if(status) {
  hd44780::fatalError(status); 
  }

  // inti oled
  oled.begin(SSD1306_SWITCHCAPVCC, 0x3c);
  // oled.clearDisplay(); // clears the screen and buffer
  // oled.drawPixel(127, 63, WHITE);
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  
  // init scanner
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  }

  // init button
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
  pinMode(BTN_4, INPUT_PULLUP);

  // init timer interrupts
  noInterrupts(); // disable all interrupts
  // Clear Timer/Counter Control Register for Interrupt 1, bytes A and B (TCCR1?)
  TCCR1A = 0; // Clear TCCR1A/B registers
  TCCR1B = 0;
  TCNT1 = 0; // Initialize counter value to 0 (16-bit counter register)
  // set compare match register for TIMER1: CLOCKFREQUENCY / frequency / prescaler - 1
  OCR1A = 15624; // 16MHz/(1Hz*1024) – 1 (must be <65536)
  // Timer/Counter Control Register for Interrupt 1 on register B
  TCCR1B |= (1 << WGM12); // Mode 4, turn on CTC mode
  // Clock Select Bit, Set CS12, CS11 and CS10 bits
  TCCR1B |= (1 << CS12) | (1 << CS10); // Set CS10 and CS12 bits for 1024 prescaler
  TIMSK1 |= (1 << OCIE1A); // enable timer compare interrupt , The value in OCR1A is used for compare
  interrupts(); // enable all interrupts
}

void loop(){

  displayLCD(0, 0, "Please scan your");
  displayLCD(1, 0, "ID card");

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()){
    for (byte i = 0; i < 4; i++) {
          uid[i] = rfid.uid.uidByte[i];
    }
    printHex(rfid.uid.uidByte, rfid.uid.size);
    Serial.println();
    //if (getBalance(uid) != -1) {
    mainState();
    //}
    


    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  } 

}  

void mainState() {
  lcd.clear();
  oled.clearDisplay();

  sec_1 = 0;

  while(1) {
    displayLCD(0 , 0, "Select Mode :");
    displayLCD(1, 0, " Pay|Top-up|Bal");
    displayOLED(10, 28, "access");
    delay(1000);
    
    // pay mode
    if (!digitalRead(BTN_1)) {
      payState();
    }
    // top-up mode
    else if (!digitalRead(BTN_2)){
      topUpState();
    }
    else if (!digitalRead(BTN_3)) {
      balanceState();
    }

    if (isExitMain) {
      break;
    }

    if (sec_1 >= 5) {
      sec_1 = 0;
      lcd.clear();
      oled.clearDisplay();
      break;
    }

  }

  isExitMain = false;
  lcd.clear();
  oled.clearDisplay();
  oled.display();
}

void payState() {

  lcd.clear();
  oled.clearDisplay();

  int paymentValue = 0;
  sec_1 = 0;
  while (1) {
    displayLCD(0, 0, "$:" + String(paymentValue));
    displayLCD(1, 0, "+5|-5|Enter|Exit");
    displayOLED(10, 28, "pay");

    if (!digitalRead(BTN_1)) {
      Serial.println(paymentValue);
      paymentValue += 5;
      sec_1 = 0;
    }
    else if (!digitalRead(BTN_2)) {
      Serial.println(paymentValue);
      if (paymentValue >= 5) {
        paymentValue -=5;
        sec_1 = 0;
      }
    }
    else if (!digitalRead(BTN_3)) {
      
      if (updateBalance(-1*paymentValue)) {
        isExitMain = true;
        break;
      }
    }
    else if (!digitalRead(BTN_4)) {
      lcd.clear();
      oled.clearDisplay();
      sec_1 = 0;
      break;
    }

    if (sec_1 >= 8) {
      sec_1 = 0;
      lcd.clear();
      oled.clearDisplay();
      break;
    }
  }

  lcd.clear();
  oled.clearDisplay();
  oled.display();
}

void topUpState() {


  lcd.clear();
  oled.clearDisplay();

  int topUpValue = 0;
  while (1) {
    displayLCD(0, 0, "$:" + String(topUpValue));
    displayLCD(1, 0, "+5|-5|Enter|Exit");
    displayOLED(10, 28, "top-up");

    if (!digitalRead(BTN_1)) {
      Serial.println(topUpValue);
      topUpValue += 5;
      sec_1 = 0;  
    }
    else if (!digitalRead(BTN_2)) {
      Serial.println(topUpValue);
      if (topUpValue >= 5) {
        topUpValue -=5;
      }
      sec_1 = 0;
    }
    else if (!digitalRead(BTN_3)) {
      if (updateBalance(topUpValue)) {
        isExitMain = true;
        break;
      }
    }
    else if (!digitalRead(BTN_4)) {
      lcd.clear();
      oled.clearDisplay();
      sec_1 = 0;
      break;
    }

    if (sec_1 >= 8) {
      sec_1 = 0;
      lcd.clear();
      oled.clearDisplay();
      break;
    }
  }

  lcd.clear();
  oled.clearDisplay();
  oled.display();
}

void balanceState() {
  lcd.clear();
  oled.clearDisplay();
  oled.display();

  sec_1 = 0;
  while (1) {

    displayLCD(0, 0, "$:" + String(balance));
    displayOLED(10, 28, "balance");

    if (sec_1 >= 3) {
      sec_1 = 0;
      lcd.clear();
      oled.clearDisplay();
      break;
    }
  }
}

int getBalance(byte* uid) {
  return -1;
}

bool updateBalance(int updateValue) {
  if (balance + updateValue < 0) {
    return false;
  }
  balance += updateValue;
  return true;
}

void printHex(byte *buffer, byte bufferSize) {
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

ISR(TIMER1_COMPA_vect)
{
  sec++;
  sec_1++;
}

