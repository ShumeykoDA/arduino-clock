#include <stdio.h>
#include "DS1302.h"
#include "TM1637.h"

////////////////// CONSTANTS /////////////////////
// Display Masks
const uint8_t dmNone = 0x00; // 00000000
const uint8_t dmAll = 0x0F;  // 00001111
const uint8_t dmHigh = 0x0C; // 00001100
const uint8_t dmLow = 0x03;  // 00000011

// Signal Pin
const int ledPin = LED_BUILTIN;

// DS1302 clock pins
const int kCePin   = 5;  // Chip Enable
const int kIoPin   = 6;  // Input/Output
const int kSclkPin = 7;  // Serial Clock

// TM1637 display pins
const int dClkPin = 12; // Clock
const int dIoPin = 11; // Input/Output

// Control buttons pins
const int bModePin = 2; // Mode Button
const int bIncPin = 3; // Increment Button

// Loop Delay
const int lDelay = 50;
DS1302 rtc(kCePin, kIoPin, kSclkPin); // RealTime Controller
TM1637 disp(dClkPin, dIoPin); // Display Controller

////////////////// Data Types /////////////////////
// Display Modes Enum
enum ModeEnum {
  meViewTime = 0,
  meViewDayMonth = 1,
  meViewYear = 2,
  meEditYear = 3,
  meEditMonth = 4,
  meEditDay = 5,
  meEditHour = 6,
  meEditMinute = 7,
};

/////////////////// Variables //////////////////////
int sPool = 0; // Second Pool
int mPool = 0; // Minute Pool
int8_t DispBuff[] = {1, 2, 3, 4}; // Create Display Output Array
unsigned int DispBuffLen = sizeof(DispBuff) / sizeof(DispBuff[0]);
bool dispPoint = false; // Display Point
bool dispData = true; // Display Data In Edit Mode
int8_t highlight = 2; // Highlight Value
ModeEnum clockMode = meViewTime; // Current Mode
Time editTime = rtc.time();

bool bHglState = false; // Current Highlight Button State (Down/Up)
bool bModeState = false; // Current Mode Button State (Down/Up)
bool bIncState = false; // Increment Button State (Down/Up)

// Procedure Print Number Value To Display
void IntToBuff(uint16_t value, int8_t* buff, uint8_t len, uint8_t mask = dmAll)
{
  int cnt = 0;
  while(cnt < len)
  {
      int curr = value % 10;
      int idx = len - cnt - 1;
      if ((idx == 0) && (curr == 0)) buff[idx] = INDEX_BLANK;
      else if (((mask >> (len - idx - 1)) & 0x01) != 1) buff[idx] = INDEX_BLANK; 
      else buff[idx] = int8_t(curr);
      value /= 10;
      for(int i = 0; i < 3; ++i) curr *= 10;
      ++cnt;  
  }
}

// Procedure Print RTC Time
void displayTime(Time t, bool editMode = false) {
  uint8_t mask = dmAll;
  if (editMode && !dispData) {
    if (clockMode == meEditHour) mask = dmLow;
    else if (clockMode == meEditMinute) mask = dmHigh;
  }
  IntToBuff(t.hr * 100 + t.min, DispBuff, DispBuffLen, mask);
  dispData = !dispData;
  dispPoint = !dispPoint;
  disp.point(dispPoint || editMode);
  disp.display(DispBuff);
}

void displayDayMonth(Time t, bool editMode = false) {
  uint8_t mask = dmAll;
  if (editMode && !dispData) {
    if (clockMode == meEditDay) mask = dmLow;
    else if (clockMode == meEditMonth) mask = dmHigh;
  }
  IntToBuff(t.date * 100 + t.mon, DispBuff, DispBuffLen, mask);
  dispData = !dispData;
  disp.point(false);
  disp.display(DispBuff);  
}

void displayYear(Time t, bool editMode = false) {
  uint8_t mask = dmAll;
  if (editMode && !dispData) mask = dmNone;
  IntToBuff(t.yr, DispBuff, DispBuffLen, mask);
  dispData = !dispData;
  disp.point(false);
  disp.display(DispBuff);
}

void minuteLoop() {
  mPool += 1;
  if (mPool >= 60) mPool = 0;
  
}

void onSecondTimer() {
  if ( clockMode == meViewTime || 
       clockMode == meViewDayMonth || 
       clockMode == meViewYear ) {
    if (mPool == 50) clockMode = meViewDayMonth;
    else if (mPool == 55) clockMode = meViewYear;
    else if (mPool == 0) clockMode = meViewTime;    
  }
  switch(clockMode) {
    case meViewTime:
      displayTime(rtc.time());
      break;
    case meViewDayMonth:
      displayDayMonth(rtc.time());
      break;
    case meViewYear:
      displayYear(rtc.time());
      break;
    case meEditYear:
      displayYear(editTime, true);
      break;
    case meEditDay:
    case meEditMonth:
      displayDayMonth(editTime, true);
      break;
    case meEditHour:
    case meEditMinute:
      displayTime(editTime, true);
      break;
    default:
      displayTime(rtc.time());
  }
  minuteLoop();
}

void halfSecondLoop() {
  sPool += lDelay;
  if (sPool >= 500) {
    sPool = 0;
    onSecondTimer();    
    disp.display(DispBuff);
  }
}

// Function Check Is Button Pressed & Call Callback Func
boolean checkButton(bool* prev, int8_t dPin, void (*callback)(void)) {
  const int8_t curr = digitalRead(dPin);
  if (*prev == 1 && curr == 0) {
    *prev = curr;
    if (callback != NULL) callback();
    return true;
  }
  *prev = curr;
  return false;
}

void onClickModeButton() {
  switch(clockMode) {
    case meViewTime:
    case meViewDayMonth:
    case meViewYear:
      editTime = rtc.time();
      clockMode = meEditYear;
      break;
    case meEditYear:
      clockMode = meEditMonth;
      break;
    case meEditMonth:
      clockMode = meEditDay;
      break;
    case meEditDay:
      clockMode = meEditHour;
      break;
    case meEditHour:
      clockMode = meEditMinute;
      break;
    case meEditMinute:
      rtc.time(editTime);
      clockMode = meViewTime;
      break;
    default:
      clockMode = meViewTime;
  }
}

void onClickIncrementButton() {
  switch(clockMode) {
    case meViewTime:
    case meViewDayMonth:
    case meViewYear:
      if (highlight == 0) { highlight = 2; }
      else if (highlight == 2) { highlight = 7; }
      else highlight = 0;
      disp.set(highlight);
      break;
    case meEditYear:
      if (editTime.yr < 2040) editTime.yr++;
      else editTime.yr = 2013;
      break;
    case meEditMonth:
      if (editTime.mon < 12) editTime.mon++;
      else editTime.mon = 1;
      break;
    case meEditDay:
      if (editTime.date < 31) editTime.date++;
      else editTime.date = 1;
      break;
    case meEditHour:
      if (editTime.hr < 23) editTime.hr++;
      else editTime.hr = 0;
      break;
    case meEditMinute:
      if (editTime.min < 59) editTime.min++;
      else editTime.min = 0;
      break;
  }
}

void clickModeButton() {
  bModeState = true;
  return;
}

void clickIncButtonHandler() {
  bIncState = true;
  return;
}

void setup() {
  Serial.begin(9600); // Initialize Serial Bus

  // Initialize Buttons & Led Pins
  pinMode(LED_BUILTIN, OUTPUT);
  
  pinMode(bModePin, INPUT); // Change Mode Pin
  attachInterrupt(0, clickModeButton, RISING);
  
  pinMode(bIncPin, INPUT); // Increment Value Pin
  attachInterrupt(1, clickIncButtonHandler, RISING);
  
  // Initialize Clock
  rtc.writeProtect(false);
  rtc.halt(false);

  // Initialize Display
  disp.init();
  disp.set(BRIGHT_TYPICAL);
} 

// Loop Event lDelay Miliseconds
void loop() {
  halfSecondLoop();  
  
  if (bModeState) {
    onClickModeButton();
    bModeState = false;
  }

  if (bIncState) {
    onClickIncrementButton();
    bIncState = false;
  }
  
  delay(lDelay);
}
