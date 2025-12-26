#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.h>
#include <DHT.h>
#include <Ds1302.h>

// ---------- PIN CONFIG ----------
const int PIN_IR_RECEIVE = 2;
const int PIN_IR_SEND    = 3;
const int PIN_DHT        = 4;
const int PIN_BUZZER     = 5;

// DS1302 pins: ENA(CE), CLK, DAT
const int PIN_RTC_ENA = 8;
const int PIN_RTC_CLK = 6;
const int PIN_RTC_DAT = 7;

// ---------- LCD ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---------- DHT ----------
#define DHTTYPE DHT11
DHT dht(PIN_DHT, DHTTYPE);

// ---------- RTC ----------
Ds1302 rtc(PIN_RTC_ENA, PIN_RTC_CLK, PIN_RTC_DAT);

// ---------- IR REMOTE CODES ----------
const uint32_t IR_UP     = 0xB946FF00;
const uint32_t IR_LEFT   = 0xBB44FF00;
const uint32_t IR_OK     = 0xBF40FF00;
const uint32_t IR_RIGHT  = 0xBC43FF00;
const uint32_t IR_DOWN   = 0xEA15FF00;
const uint32_t IR_1      = 0xE916FF00;
const uint32_t IR_2      = 0xE619FF00;
const uint32_t IR_3      = 0xF20DFF00;
const uint32_t IR_4      = 0xF30CFF00;
const uint32_t IR_5      = 0xE718FF00;
const uint32_t IR_6      = 0xA15EFF00;
const uint32_t IR_7      = 0xF708FF00;
const uint32_t IR_8      = 0xE31CFF00;
const uint32_t IR_9      = 0xA55AFF00;
const uint32_t IR_STAR   = 0xBD42FF00;
const uint32_t IR_0      = 0xAD52FF00;
const uint32_t IR_HASH   = 0xB54AFF00;

// ---------- AC COMMAND WORDS ----------
uint32_t AC_ON_30_HEAT[6] = {0x2, 0x6001,     0x2, 0x48280000, 0x2, 0x50000000}; // commands to turn on AC on heating mode
uint32_t AC_ON_16_COOL[6] = {0x2, 0x28006031, 0x2, 0x40280000, 0x2, 0x50000000}; // commands to turn on AC on cooling mode
uint32_t AC_OFF_GENERIC[6]= {0x2, 0x40006001, 0x2, 0x08280000, 0x2, 0x50000000}; // commands to turn off AC (both modes)

// ---------- CONSTANTS ----------
const float COMFORT_T = 22.0; // set the temperature considered "comfortable" (by default 22 degrees C)
const unsigned long AC_LEAD_MIN = 5; // minutes before alarm (by default 5)
const unsigned long LCD_UPDATE_INTERVAL = 250;
const unsigned long ALARM_CHECK_INTERVAL = 1000;

// ---------- STATE ----------
int alarmHour   = 6; // initial alarm set for 6:30am
int alarmMinute = 30;
bool alarmEnabled   = true; // by default, the alarm is on

bool preSignalSent  = false;   // AC pre-conditioning already done
bool alarmActive    = false;   // buzzer and code phase

// random code and user input
int randomCode[4];
int inputCode[4];
int inputIndex = 0;

// screens
enum Screen {
  SCREEN_MAIN,
  SCREEN_SET_ALARM,
  SCREEN_COUNTDOWN,   // <5 min to alarm: show signal + remaining time
  SCREEN_CODE         // alarm active: show code and user input
};
Screen currentScreen  = SCREEN_MAIN;
Screen previousScreen = SCREEN_MAIN;

// timing
unsigned long lastLcdUpdate  = 0;
unsigned long lastAlarmCheck = 0;

// ---------- PROTOTYPES ----------
void getNow(Ds1302::DateTime &now);
void readSensors(float &T, float &H);
void drawScreen(const Ds1302::DateTime &now, float T, float H);
void drawMain(const Ds1302::DateTime &now, float T, float H);
void drawSetAlarm();
void drawCountdown(const Ds1302::DateTime &now);
void drawCodeScreen();
void handleRemote(const Ds1302::DateTime &now);
void handleAlarm(const Ds1302::DateTime &now, float T);
bool timesEqual(const Ds1302::DateTime &now, int h, int m);
int  minutesToAlarm(const Ds1302::DateTime &now, int h, int m);
void sendACHeatOn();
void sendACCoolOn();
void sendACOff();
void sendCommandWords(uint32_t *w, int n);
void generateRandomCode();
void handleCodeInput(uint32_t code);
int  codeToDigit(uint32_t code);

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  lcd.init();
  lcd.backlight();

  dht.begin();

  IrReceiver.begin(PIN_IR_RECEIVE, ENABLE_LED_FEEDBACK);
  IrSender.begin(PIN_IR_SEND);

  rtc.init();  // DS1302 time must be set by separate sketch

  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);

  lcd.setCursor(0, 0);
  lcd.print("Weird AC Alarm");
  delay(1500);
}

// ---------- LOOP ----------
void loop() {
  Ds1302::DateTime now;
  getNow(now);

  float T, H;
  readSensors(T, H);

  handleRemote(now);
  handleAlarm(now, T);

  unsigned long ms = millis();
  if (ms - lastLcdUpdate >= LCD_UPDATE_INTERVAL) {
    lastLcdUpdate = ms;
    drawScreen(now, T, H);
  }
}

// ---------- TIME & SENSORS ----------
void getNow(Ds1302::DateTime &now) {
  rtc.getDateTime(&now); // obtains current time from RTC module
}

void readSensors(float &T, float &H) {
  H = dht.readHumidity(); // read humidity from sensor
  T = dht.readTemperature(); // read temperature from sensor
}

// ---------- LCD ----------
void drawScreen(const Ds1302::DateTime &now, float T, float H) {
  if (currentScreen != previousScreen) {
    lcd.clear();
    previousScreen = currentScreen;
  }

  if (currentScreen == SCREEN_MAIN) {
    drawMain(now, T, H);
  } else if (currentScreen == SCREEN_SET_ALARM) {
    drawSetAlarm();
  } else if (currentScreen == SCREEN_COUNTDOWN) {
    drawCountdown(now);
  } else if (currentScreen == SCREEN_CODE) {
    drawCodeScreen();
  }
}

void drawMain(const Ds1302::DateTime &now, float T, float H) {
  lcd.setCursor(0, 0);
  char buf[17];
  snprintf(buf, sizeof(buf), "%02u:%02u A%02d:%02d",
           now.hour, now.minute, alarmHour, alarmMinute);
  lcd.print(buf);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print("T:");
  lcd.print(T, 1);
  lcd.print((char)223);
  lcd.print(" H:");
  lcd.print(H, 0);
  lcd.print("%   ");
}

void drawSetAlarm() {
  lcd.setCursor(0, 0);
  lcd.print("Set Alarm Time  ");
  lcd.setCursor(0, 1);
  char buf[17];
  snprintf(buf, sizeof(buf), "A %02d:%02d        ", alarmHour, alarmMinute);
  lcd.print(buf);
}

void drawCountdown(const Ds1302::DateTime &now) {
  int mins = minutesToAlarm(now, alarmHour, alarmMinute);
  if (mins < 0) mins = 0;
  lcd.setCursor(0, 0);
  lcd.print("Signal sent     ");
  lcd.setCursor(0, 1);
  lcd.print("Alarm in ");
  if (mins < 10) lcd.print("0");
  lcd.print(mins);
  lcd.print(" min   ");
}

void drawCodeScreen() {
  lcd.setCursor(0, 0);
  lcd.print("Code:");
  for (int i = 0; i < 4; i++) lcd.print(randomCode[i]);
  lcd.print("  ");

  lcd.setCursor(0, 1);
  lcd.print("You:");
  for (int i = 0; i < 4; i++) {
    if (i < inputIndex && inputCode[i] >= 0) lcd.print(inputCode[i]);
    else lcd.print("_");
  }
  lcd.print("  ");
}

// ---------- REMOTE ----------
void handleRemote(const Ds1302::DateTime &now) {
  if (!IrReceiver.decode()) return;

  uint32_t code = IrReceiver.decodedIRData.decodedRawData;

  // Optional debug print (non-repeat)
  if (!(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT)) {
    Serial.print("IR: 0x");
    Serial.println(code, HEX);
  }

  if (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) {
    IrReceiver.resume();
    return;
  }

  if (currentScreen == SCREEN_MAIN) {
    // Enter alarm setup
    if (code == IR_OK) {
      currentScreen = SCREEN_SET_ALARM;
    }
  }
  else if (currentScreen == SCREEN_SET_ALARM) {
    if (code == IR_UP)         alarmHour   = (alarmHour + 1) % 24;
    else if (code == IR_DOWN)  alarmHour   = (alarmHour + 23) % 24;
    else if (code == IR_RIGHT) alarmMinute = (alarmMinute + 1) % 60;
    else if (code == IR_LEFT)  alarmMinute = (alarmMinute + 59) % 60;
    else if (code == IR_OK) {
      alarmEnabled  = true;
      preSignalSent = false;
      currentScreen = SCREEN_MAIN;
    }
  }
  else if (currentScreen == SCREEN_CODE && alarmActive) {
    // During alarm, use digits + OK to handle code
    handleCodeInput(code);
  }

  IrReceiver.resume();
}

// ---------- ALARM & AC CONTROL ----------
void handleAlarm(const Ds1302::DateTime &now, float T) {
  unsigned long ms = millis();
  if (ms - lastAlarmCheck < ALARM_CHECK_INTERVAL) return;
  lastAlarmCheck = ms;

  if (!alarmEnabled) return;

  int minsTo = minutesToAlarm(now, alarmHour, alarmMinute);

  // Pre‑conditioning exactly 5 minutes before alarm
  if (!preSignalSent && minsTo == (int)AC_LEAD_MIN) {
    if (!isnan(T)) {
      if (T < COMFORT_T) sendACHeatOn();
      else if (T > COMFORT_T) sendACCoolOn();
    }
    preSignalSent = true;
    currentScreen = SCREEN_COUNTDOWN;
  }

  // Alarm time reached
  if (!alarmActive && timesEqual(now, alarmHour, alarmMinute)) {
    if (isnan(T) || T < COMFORT_T - 0.5 || T > COMFORT_T + 0.5) {
      alarmActive = true;
      generateRandomCode();
      inputIndex = 0;
      tone(PIN_BUZZER, 2000);
      currentScreen = SCREEN_CODE;
      drawCodeScreen();   // show Code: xxxx / You: ____
    } else {
      // comfortable: no alarm
      alarmEnabled  = false;
      preSignalSent = false;
      currentScreen = SCREEN_MAIN;
    }
  }
}

bool timesEqual(const Ds1302::DateTime &now, int h, int m) {
  return (now.hour == h && now.minute == m);
}

int minutesToAlarm(const Ds1302::DateTime &now, int h, int m) {
  int nowMin   = now.hour * 60 + now.minute;
  int alarmMin = h * 60 + m;
  return alarmMin - nowMin;
}

// ---------- AC SEND ----------
void sendCommandWords(uint32_t *w, int n) {
  for (int i = 0; i < n; i++) {
    delay(50);
  }
}
void sendACHeatOn() { sendCommandWords(AC_ON_30_HEAT, 6); }
void sendACCoolOn() { sendCommandWords(AC_ON_16_COOL, 6); }
void sendACOff()    { sendCommandWords(AC_OFF_GENERIC, 6); }

// ---------- CODE / PASSWORD ----------
void generateRandomCode() { // code is randomly generated each time
  randomSeed(analogRead(0));
  for (int i = 0; i < 4; i++) {
    randomCode[i] = random(0, 10);
    inputCode[i]  = -1;
  }
  inputIndex = 0;
}

int codeToDigit(uint32_t code) {
  if (code == IR_0) return 0;
  if (code == IR_1) return 1;
  if (code == IR_2) return 2;
  if (code == IR_3) return 3;
  if (code == IR_4) return 4;
  if (code == IR_5) return 5;
  if (code == IR_6) return 6;
  if (code == IR_7) return 7;
  if (code == IR_8) return 8;
  if (code == IR_9) return 9;
  return -1;
}

void handleCodeInput(uint32_t code) {
  if (!alarmActive) return;

  int d = codeToDigit(code);

  // If a digit key is pressed, store it and update the screen
  if (d >= 0 && inputIndex < 4) {
    inputCode[inputIndex++] = d;
    currentScreen = SCREEN_CODE;   // ensure on code screen
    drawCodeScreen();              // immediate visual update: You: 1___, 11__, etc.
    return;
  }

  // Check only when 4 digits entered and OK pressed
  if (code == IR_OK && inputIndex == 4) {
    bool ok = true;
    for (int i = 0; i < 4; i++) {
      if (inputCode[i] != randomCode[i]) ok = false; // wrong code entered
    }

    if (ok) { // disable alarm when correct code entered
      noTone(PIN_BUZZER);
      sendACOff();
      alarmActive   = false;
      alarmEnabled  = false;
      preSignalSent = false;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Alarm off");
      lcd.setCursor(0, 1);
      lcd.print("Good morning");
      delay(1500);

      currentScreen = SCREEN_MAIN;
    } else {
      // Wrong: show error, then reset input and go back to blank "You:____"
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wrong code!");
      lcd.setCursor(0, 1);
      lcd.print("Try again...");
      delay(1500);

      inputIndex = 0;
      for (int i = 0; i < 4; i++) inputCode[i] = -1;

      currentScreen = SCREEN_CODE;
      drawCodeScreen();  // will show: Code: xxxx / You: ____
    }
  }
}
