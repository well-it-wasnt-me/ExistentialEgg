#include "pet.h"

#include <esp_adc_cal.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

/**
 * @file pet.cpp
 * @brief Core state lifecycle, persistence, and time simulation.
 */

Preferences prefs;
PetState gState;
RuntimeState gRun;
Ink_Sprite gSprite(&M5.M5Ink);

const char *const kStageNames[] = {
    "Egg", "Baby", "Child", "Teen", "Adult", "Elder"};

const char *const kMoodNames[] = {
    "Happy", "Ok", "Sad", "Sleepy", "Sick"};

const ItemDef kItems[ITEM_COUNT] = {
    {"Food", 3},
    {"Snack", 5},
    {"Med", 8},
    {"Toy", 6}};

const char *const kMenuItems[] = {
    "Feed",      "Play",  "Clean", "Sleep", "Med",
    "Train",     "Inv",   "Game",  "Status", "Helper"};

const uint8_t kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

/** @copydoc clampU8 */
uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return static_cast<uint8_t>(v);
}

static float getBatVoltage() {
  static bool adcInit = false;
  static esp_adc_cal_characteristics_t adc_chars;
  if (!adcInit) {
    analogSetPinAttenuation(35, ADC_11db);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12,
                             3600, &adc_chars);
    adcInit = true;
  }
  uint16_t ADCValue = analogRead(35);
  uint32_t BatVolmV = esp_adc_cal_raw_to_voltage(ADCValue, &adc_chars);
  float BatVol = float(BatVolmV) * 25.1f / 5.1f / 1000.0f;
  return BatVol;
}

/** @copydoc getBatteryPercent */
uint8_t getBatteryPercent() {
  const float v = getBatVoltage();
  const float vMin = 3.2f;
  const float vMax = 4.2f;
  int pct = (int)((v - vMin) * 100.0f / (vMax - vMin) + 0.5f);
  return clampU8(pct);
}

static uint16_t crc16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc ^= data[i];
    for (int b = 0; b < 8; ++b) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xA001;
      else
        crc >>= 1;
    }
  }
  return crc;
}

static uint16_t dateToDays(uint16_t y, uint8_t m, uint8_t d) {
  if (y >= 2000) y -= 2000;
  static const uint8_t daysInMonth[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  uint16_t days = d;
  for (uint8_t i = 1; i < m; ++i) {
    days += daysInMonth[i - 1];
  }
  if (m > 2 && (y % 4) == 0) {
    days += 1;
  }
  return days + 365 * y + (y + 3) / 4 - 1;
}

static uint32_t toEpoch(uint16_t year, uint8_t month, uint8_t day, uint8_t hour,
                        uint8_t minute, uint8_t second) {
  const uint32_t SECONDS_FROM_1970_TO_2000 = 946684800UL;
  uint16_t days = dateToDays(year, month, day);
  uint32_t t = ((uint32_t)days * 24UL + hour) * 3600UL +
               (uint32_t)minute * 60UL + second;
  return t + SECONDS_FROM_1970_TO_2000;
}

static bool getRtcEpoch(uint32_t &outEpoch) {
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  M5.Rtc.GetTime(&t);
  M5.Rtc.GetDate(&d);

  uint16_t year = d.Year;
  if (year < 100) year += 2000;

  if (year < 2024) {
    return false;
  }

  outEpoch = toEpoch(year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds);
  return true;
}

static uint8_t monthFromStr(const char *m) {
  if (!m) return 1;
  if (m[0] == 'J' && m[1] == 'a') return 1;
  if (m[0] == 'F') return 2;
  if (m[0] == 'M' && m[2] == 'r') return 3;
  if (m[0] == 'A' && m[1] == 'p') return 4;
  if (m[0] == 'M' && m[2] == 'y') return 5;
  if (m[0] == 'J' && m[2] == 'n') return 6;
  if (m[0] == 'J' && m[2] == 'l') return 7;
  if (m[0] == 'A' && m[1] == 'u') return 8;
  if (m[0] == 'S') return 9;
  if (m[0] == 'O') return 10;
  if (m[0] == 'N') return 11;
  if (m[0] == 'D') return 12;
  return 1;
}

static void setRtcToBuildTime() {
  char monthStr[4] = {0};
  int day = 1;
  int year = 2024;
  int hour = 0, minute = 0, second = 0;

  sscanf(__DATE__, "%3s %d %d", monthStr, &day, &year);
  sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  t.Hours = hour;
  t.Minutes = minute;
  t.Seconds = second;

  if (sizeof(d.Year) == 1 && year >= 2000) {
    d.Year = year - 2000;
  } else {
    d.Year = year;
  }
  d.Month = monthFromStr(monthStr);
  d.Date = day;

  M5.Rtc.SetTime(&t);
  M5.Rtc.SetDate(&d);
}

/** @copydoc markDirty */
void markDirty() {
  gRun.dirty = true;
  gRun.lastUiActionMs = millis();
}

/** @copydoc showMessage */
void showMessage(const char *msg, uint32_t durationMs) {
  strncpy(gRun.message, msg, sizeof(gRun.message) - 1);
  gRun.message[sizeof(gRun.message) - 1] = '\0';
  gRun.messageUntilMs = millis() + durationMs;
  gRun.lastScreen = gRun.screen;
  gRun.screen = SCREEN_MESSAGE;
  markDirty();
}

/** @copydoc defaultState */
void defaultState() {
  memset(&gState, 0, sizeof(gState));
  gState.magic = MAGIC;
  gState.version = STATE_VERSION;
  gState.lastEpoch = 0;
  gState.ageMinutes = 0;
  gState.coins = 10;
  gState.stage = STAGE_EGG;
  gState.hunger = 80;
  gState.happiness = 70;
  gState.cleanliness = 80;
  gState.energy = 70;
  gState.health = 90;
  gState.discipline = 50;
  gState.weight = 50;
  gState.poop = 0;
  gState.asleep = false;
  gState.sick = false;
  gState.invFood = 3;
  gState.invSnack = 2;
  gState.invMed = 1;
  gState.invToy = 1;
}

/** @copydoc loadState */
bool loadState() {
  prefs.begin("tama", true);
  if (prefs.getBytesLength("state") != sizeof(PetState)) {
    prefs.end();
    return false;
  }

  PetState tmp;
  prefs.getBytes("state", &tmp, sizeof(tmp));
  prefs.end();

  if (tmp.magic != MAGIC || tmp.version != STATE_VERSION) {
    return false;
  }

  PetState check = tmp;
  uint16_t saved = check.crc;
  check.crc = 0;
  uint16_t calc = crc16(reinterpret_cast<uint8_t *>(&check), sizeof(PetState));
  if (calc != saved) {
    return false;
  }

  gState = tmp;
  return true;
}

/** @copydoc saveState */
void saveState(bool force) {
  uint32_t now = millis();
  if (!force && (now - gRun.lastSaveMs < SAVE_INTERVAL_MS)) {
    return;
  }

  gRun.lastSaveMs = now;

  PetState tmp = gState;
  tmp.magic = MAGIC;
  tmp.version = STATE_VERSION;
  tmp.crc = 0;
  uint32_t nowEpoch = 0;
  if (getRtcEpoch(nowEpoch)) {
    tmp.lastEpoch = nowEpoch;
  }
  tmp.crc = crc16(reinterpret_cast<uint8_t *>(&tmp), sizeof(PetState));

  prefs.begin("tama", false);
  prefs.putBytes("state", &tmp, sizeof(tmp));
  prefs.end();
}

static void applyClamp() {
  gState.hunger = clampU8(gState.hunger);
  gState.happiness = clampU8(gState.happiness);
  gState.cleanliness = clampU8(gState.cleanliness);
  gState.energy = clampU8(gState.energy);
  gState.health = clampU8(gState.health);
  gState.discipline = clampU8(gState.discipline);
  gState.weight = clampU8(gState.weight);
}

static void evolveIfNeeded() {
  uint32_t age = gState.ageMinutes;
  Stage stage = static_cast<Stage>(gState.stage);

  if (age < 60) stage = STAGE_EGG;
  else if (age < 12 * 60) stage = STAGE_BABY;
  else if (age < 48 * 60) stage = STAGE_CHILD;
  else if (age < 5 * 24 * 60) stage = STAGE_TEEN;
  else if (age < 12 * 24 * 60) stage = STAGE_ADULT;
  else stage = STAGE_ELDER;

  gState.stage = static_cast<uint8_t>(stage);
}

static void adjustForOffline(uint32_t minutes) {
  if (minutes > MAX_OFFLINE_MINUTES) minutes = MAX_OFFLINE_MINUTES;

  int hungerRate = gState.asleep ? 6 : 12;
  int happinessRate = 8;
  int cleanRate = 10;
  int energyRate = gState.asleep ? -18 : 14; // negative regen when awake

  int hungerDrop = (hungerRate * (int)minutes + 59) / 60;
  int happyDrop = (happinessRate * (int)minutes + 59) / 60;
  int cleanDrop = (cleanRate * (int)minutes + 59) / 60;
  int energyDelta = (energyRate * (int)minutes) / 60;

  gState.hunger = clampU8((int)gState.hunger - hungerDrop);
  gState.happiness = clampU8((int)gState.happiness - happyDrop);
  gState.cleanliness = clampU8((int)gState.cleanliness - cleanDrop);
  gState.energy = clampU8((int)gState.energy - energyDelta);

  int poopAdd = (int)minutes / 45;
  gState.poop = clampU8((int)gState.poop + poopAdd);
  if (gState.poop > 3) {
    gState.cleanliness =
        clampU8((int)gState.cleanliness - (gState.poop - 3) * 2);
  }

  int healthDelta = 0;
  if (gState.hunger < 20) healthDelta -= 6 * (int)minutes / 60;
  if (gState.cleanliness < 25) healthDelta -= 6 * (int)minutes / 60;
  if (gState.energy < 15) healthDelta -= 4 * (int)minutes / 60;
  if (gState.sick) healthDelta -= 8 * (int)minutes / 60;

  if (gState.hunger > 60 && gState.cleanliness > 60 && gState.energy > 50 &&
      !gState.sick) {
    healthDelta += 4 * (int)minutes / 60;
  }

  gState.health = clampU8((int)gState.health + healthDelta);

  if (!gState.sick && gState.cleanliness < 30) {
    uint32_t chance = (uint32_t)minutes * 3; // 3% per hour
    uint32_t roll = esp_random() % 10000;
    if (roll < chance * 100) {
      gState.sick = true;
    }
  }

  gState.ageMinutes += minutes;
  gState.coins += (uint16_t)((minutes + 9) / 10); // 1 coin per 10 minutes

  if (gState.coins > 999) gState.coins = 999;

  evolveIfNeeded();
  applyClamp();
}

/** @copydoc currentMood */
Mood currentMood() {
  if (gState.sick || gState.health < 35) return MOOD_SICK;
  if (gState.energy < 25) return MOOD_SLEEPY;
  int avg = (gState.hunger + gState.happiness + gState.cleanliness +
             gState.energy + gState.health) /
            5;
  if (avg > 70) return MOOD_HAPPY;
  if (avg > 45) return MOOD_OK;
  return MOOD_SAD;
}

/** @copydoc advanceTime */
void advanceTime() {
  uint32_t now = millis();
  if (now - gRun.lastTickMs < TICK_INTERVAL_MS) {
    return;
  }

  uint32_t elapsedMinutes = (now - gRun.lastTickMs) / TICK_INTERVAL_MS;
  gRun.lastTickMs += elapsedMinutes * TICK_INTERVAL_MS;

  adjustForOffline(elapsedMinutes);

  uint32_t epoch = 0;
  if (getRtcEpoch(epoch)) {
    gState.lastEpoch = epoch;
  }

  markDirty();
  saveState(false);
}

/** @copydoc applyOfflineProgress */
void applyOfflineProgress() {
  uint32_t nowEpoch = 0;
  if (!getRtcEpoch(nowEpoch)) {
    setRtcToBuildTime();
    if (!getRtcEpoch(nowEpoch)) {
      return;
    }
  }

  if (gState.lastEpoch == 0) {
    gState.lastEpoch = nowEpoch;
    return;
  }

  if (nowEpoch <= gState.lastEpoch) {
    gState.lastEpoch = nowEpoch;
    return;
  }

  uint32_t minutes = (nowEpoch - gState.lastEpoch) / 60;
  if (minutes > 0) {
    adjustForOffline(minutes);
  }
  gState.lastEpoch = nowEpoch;
}
