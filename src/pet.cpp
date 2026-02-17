#include "pet.h"

#include <esp_adc_cal.h>
#include <esp_system.h>
#include <limits.h>
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
    "Feed",      "Play",  "Clean", "Light", "Med",
    "Scold",     "Inv",   "Game",  "Status", "Helper"};

const uint8_t kMenuCount = sizeof(kMenuItems) / sizeof(kMenuItems[0]);

static const uint32_t SECONDS_PER_MINUTE = 60;
static const uint32_t SECONDS_PER_HOUR = 60 * SECONDS_PER_MINUTE;
static const uint32_t MINUTES_PER_DAY = 24 * 60;

static const uint32_t ATTENTION_DELAY_SECONDS = 15 * SECONDS_PER_MINUTE;
static const uint32_t ATTENTION_COOLDOWN_SECONDS = 30 * SECONDS_PER_MINUTE;
static const uint32_t TANTRUM_MIN_SECONDS = 3 * SECONDS_PER_HOUR;
static const uint32_t TANTRUM_MAX_SECONDS = 6 * SECONDS_PER_HOUR;
static const uint32_t TANTRUM_DURATION_SECONDS = 10 * SECONDS_PER_MINUTE;

/** @copydoc clampU8 */
uint8_t clampU8(int v) {
  if (v < 0) return 0;
  if (v > 100) return 100;
  return static_cast<uint8_t>(v);
}

static float getBatVoltage() {
  static bool adcInit = false;
  static esp_adc_cal_characteristics_t adcChars;
  if (!adcInit) {
    analogSetPinAttenuation(35, ADC_11db);
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12,
                             3600, &adcChars);
    adcInit = true;
  }

  uint16_t adcValue = analogRead(35);
  uint32_t batVolMv = esp_adc_cal_raw_to_voltage(adcValue, &adcChars);
  return float(batVolMv) * 25.1f / 5.1f / 1000.0f;
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

static bool getRtcEpochInternal(uint32_t &outEpoch) {
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  M5.Rtc.GetTime(&t);
  M5.Rtc.GetDate(&d);

  uint16_t year = d.Year;
  if (year < 100) year += 2000;
  if (year < 2024) return false;

  outEpoch = toEpoch(year, d.Month, d.Date, t.Hours, t.Minutes, t.Seconds);
  return true;
}

/** @copydoc getCurrentEpoch */
bool getCurrentEpoch(uint32_t &outEpoch) { return getRtcEpochInternal(outEpoch); }

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
  int hour = 0;
  int minute = 0;
  int second = 0;

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

static uint32_t bestKnownEpoch() {
  uint32_t nowEpoch = 0;
  if (getCurrentEpoch(nowEpoch)) return nowEpoch;
  return gState.lastEpoch;
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

static uint32_t randBetween(uint32_t minInclusive, uint32_t maxInclusive) {
  if (maxInclusive <= minInclusive) return minInclusive;
  uint32_t range = maxInclusive - minInclusive + 1;
  return minInclusive + (esp_random() % range);
}

static uint32_t randomTantrumOffsetSeconds() {
  return randBetween(TANTRUM_MIN_SECONDS, TANTRUM_MAX_SECONDS);
}

static uint8_t countBits(uint8_t mask) {
  uint8_t count = 0;
  while (mask) {
    count += (mask & 1U);
    mask >>= 1U;
  }
  return count;
}

static Stage stageForAgeMinutes(uint32_t ageMinutes) {
  if (ageMinutes < 15) return STAGE_EGG;
  if (ageMinutes < 24 * 60) return STAGE_BABY;
  if (ageMinutes < 72 * 60) return STAGE_CHILD;
  if (ageMinutes < 144 * 60) return STAGE_TEEN;
  if (ageMinutes < 288 * 60) return STAGE_ADULT;
  return STAGE_ELDER;
}

static void applyCareClassModifier(uint16_t mistakesInStage) {
  int happinessDelta = 0;
  uint16_t sicknessMultiplier = 1000;

  if (mistakesInStage <= 1) {
    happinessDelta = 10;
    sicknessMultiplier = 800;
  } else if (mistakesInStage <= 3) {
    happinessDelta = 0;
    sicknessMultiplier = 1000;
  } else if (mistakesInStage <= 6) {
    happinessDelta = -10;
    sicknessMultiplier = 1200;
  } else {
    happinessDelta = -20;
    sicknessMultiplier = 1400;
  }

  gState.happiness = clampU8((int)gState.happiness + happinessDelta);
  gState.sicknessRiskPermille = sicknessMultiplier;
}

static void evolveIfNeeded() {
  Stage current = static_cast<Stage>(gState.stage);
  Stage next = stageForAgeMinutes(gState.ageMinutes);
  if (next == current) return;

  uint16_t mistakesInStage =
      (gState.careMistakes >= gState.stageStartMistakes)
          ? (gState.careMistakes - gState.stageStartMistakes)
          : gState.careMistakes;

  applyCareClassModifier(mistakesInStage);
  gState.stage = static_cast<uint8_t>(next);
  gState.stageStartMistakes = gState.careMistakes;
}

static bool sleepWindowForStage(Stage stage, uint16_t &sleepMinute,
                                uint16_t &wakeMinute) {
  switch (stage) {
    case STAGE_EGG:
      return false;
    case STAGE_BABY:
      sleepMinute = 20 * 60;
      wakeMinute = 7 * 60;
      return true;
    case STAGE_CHILD:
      sleepMinute = 21 * 60;
      wakeMinute = 7 * 60;
      return true;
    case STAGE_TEEN:
      sleepMinute = 22 * 60;
      wakeMinute = 8 * 60;
      return true;
    case STAGE_ADULT:
      sleepMinute = 23 * 60;
      wakeMinute = 8 * 60;
      return true;
    case STAGE_ELDER:
      sleepMinute = 21 * 60 + 30;
      wakeMinute = 7 * 60 + 30;
      return true;
    default:
      return false;
  }
}

static bool isInSleepWindow(uint16_t minuteOfDay, uint16_t sleepMinute,
                            uint16_t wakeMinute) {
  if (sleepMinute == wakeMinute) return true;

  if (sleepMinute < wakeMinute) {
    return minuteOfDay >= sleepMinute && minuteOfDay < wakeMinute;
  }

  return minuteOfDay >= sleepMinute || minuteOfDay < wakeMinute;
}

static uint16_t minuteOfDayFromEpoch(uint32_t epoch) {
  if (epoch == 0) return 0;
  return static_cast<uint16_t>((epoch / SECONDS_PER_MINUTE) % MINUTES_PER_DAY);
}

static void syncSleepSchedule(uint32_t epoch) {
  Stage stage = static_cast<Stage>(gState.stage);
  uint16_t sleepMinute = 0;
  uint16_t wakeMinute = 0;

  if (!sleepWindowForStage(stage, sleepMinute, wakeMinute)) {
    gState.asleep = false;
    return;
  }

  bool shouldSleep = isInSleepWindow(minuteOfDayFromEpoch(epoch), sleepMinute,
                                     wakeMinute);

  if (shouldSleep && !gState.asleep) {
    gState.asleep = true;
  }

  if (!shouldSleep && gState.asleep) {
    gState.asleep = false;
    gState.lightsOn = true;
  }
}

static void scheduleNextTantrum(uint32_t nowEpoch) {
  if (nowEpoch == 0) return;
  gState.nextTantrumEpoch = nowEpoch + randomTantrumOffsetSeconds();
}

static void addCareMistake() {
  if (gState.careMistakes < USHRT_MAX) {
    ++gState.careMistakes;
  }
}

static bool isReasonActive(AttentionReason reason, uint32_t nowEpoch) {
  switch (reason) {
    case ATTN_HUNGER:
      return gState.hunger <= 20;
    case ATTN_HAPPINESS:
      return gState.happiness <= 20;
    case ATTN_POOP:
      return gState.poop >= 2;
    case ATTN_SICK:
      return gState.sick;
    case ATTN_LIGHTS:
      return gState.asleep && gState.lightsOn;
    case ATTN_TANTRUM:
      return gState.tantrumUntilEpoch != 0 &&
             (nowEpoch == 0 || nowEpoch < gState.tantrumUntilEpoch);
    default:
      return false;
  }
}

static uint8_t computeAlertMask(uint32_t nowEpoch) {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < ATTN_COUNT; ++i) {
    if (isReasonActive(static_cast<AttentionReason>(i), nowEpoch)) {
      mask |= static_cast<uint8_t>(1U << i);
    }
  }
  return mask;
}

static void applySignedRate(uint8_t &stat, int16_t &acc, int ratePerHour) {
  acc += ratePerHour;

  while (acc >= 60) {
    stat = clampU8((int)stat + 1);
    acc -= 60;
  }

  while (acc <= -60) {
    stat = clampU8((int)stat - 1);
    acc += 60;
  }
}

static void applyDrainRate(uint8_t &stat, int16_t &acc, int ratePerHour) {
  applySignedRate(stat, acc, -ratePerHour);
}

static void applyPassiveDrift() {
  const bool awake = !gState.asleep;

  if (awake) {
    applyDrainRate(gState.hunger, gState.hungerAcc, 12);
    applyDrainRate(gState.happiness, gState.happinessAcc, 8);
    applyDrainRate(gState.discipline, gState.disciplineAcc, 2);

    ++gState.poopMinuteAcc;
    while (gState.poopMinuteAcc >= 50) {
      gState.poopMinuteAcc -= 50;
      if (gState.poop < 99) {
        ++gState.poop;
      }
      gState.cleanliness = clampU8((int)gState.cleanliness - 12);
    }
  } else {
    applyDrainRate(gState.hunger, gState.hungerAcc, 3);
    applyDrainRate(gState.happiness, gState.happinessAcc, 2);
  }

  if (gState.poop > 0) {
    int cleanRate = 2 * (int)gState.poop;
    applyDrainRate(gState.cleanliness, gState.cleanlinessAcc, cleanRate);
  }
}

static void updateLowStatTimers() {
  if (gState.hunger <= 20) {
    if (gState.lowHungerMinutes < USHRT_MAX) ++gState.lowHungerMinutes;
  } else {
    gState.lowHungerMinutes = 0;
  }

  if (gState.happiness <= 20) {
    if (gState.lowHappinessMinutes < USHRT_MAX) ++gState.lowHappinessMinutes;
  } else {
    gState.lowHappinessMinutes = 0;
  }
}

static void maybeApplySicknessChance() {
  if (gState.asleep || gState.sick) return;

  int chancePerHourPct = 0;
  if (gState.poop >= 3) chancePerHourPct += 15;
  if (gState.lowHungerMinutes >= 30) chancePerHourPct += 10;
  if (gState.lowHappinessMinutes >= 60) chancePerHourPct += 10;
  if (chancePerHourPct > 35) chancePerHourPct = 35;

  int chancePerHourPermille = chancePerHourPct * 10;
  chancePerHourPermille =
      (chancePerHourPermille * (int)gState.sicknessRiskPermille + 500) / 1000;
  if (chancePerHourPermille > 950) chancePerHourPermille = 950;

  uint32_t thresholdPerMinutePpm =
      (uint32_t)chancePerHourPermille * 1000U / 60U;
  uint32_t roll = esp_random() % 1000000U;
  if (roll < thresholdPerMinutePpm) {
    gState.sick = true;
  }
}

static void updateAttentionTracking(uint32_t nowEpoch) {
  for (uint8_t i = 0; i < ATTN_COUNT; ++i) {
    bool active = isReasonActive(static_cast<AttentionReason>(i), nowEpoch);

    if (!active) {
      gState.attentionSinceEpoch[i] = 0;
      continue;
    }

    if (gState.attentionSinceEpoch[i] == 0) {
      gState.attentionSinceEpoch[i] = nowEpoch;
    }

    bool overdue =
        nowEpoch >= gState.attentionSinceEpoch[i] + ATTENTION_DELAY_SECONDS;
    bool cooldownDone =
        nowEpoch >= gState.attentionCooldownUntilEpoch[i];

    if (overdue && cooldownDone) {
      addCareMistake();
      gState.attentionCooldownUntilEpoch[i] =
          nowEpoch + ATTENTION_COOLDOWN_SECONDS;
    }
  }
}

static void applyHealthRules(uint32_t nowEpoch) {
  uint8_t alertMask = computeAlertMask(nowEpoch);
  uint8_t alertCount = countBits(alertMask);

  bool sickWithOtherAlert =
      gState.sick && ((alertMask & ~(1U << ATTN_SICK)) != 0);

  int netRatePerHour = 0;
  if (sickWithOtherAlert) {
    netRatePerHour = -20;
  } else if (alertCount >= 2) {
    netRatePerHour = -12;
  } else if (!gState.sick && gState.hunger > 60 && gState.happiness > 60 &&
             gState.poop == 0) {
    netRatePerHour = 4;
  }

  applySignedRate(gState.health, gState.healthAcc, netRatePerHour);
}

static void processTantrum(uint32_t nowEpoch, bool allowPopup) {
  if (gState.nextTantrumEpoch == 0) {
    scheduleNextTantrum(nowEpoch);
  }

  if (gState.tantrumUntilEpoch != 0 && nowEpoch >= gState.tantrumUntilEpoch) {
    gState.tantrumUntilEpoch = 0;
    gState.happiness = clampU8((int)gState.happiness - 10);
    addCareMistake();
    gState.tantrumCooldownUntilEpoch = nowEpoch + ATTENTION_COOLDOWN_SECONDS;
    scheduleNextTantrum(nowEpoch);
    if (allowPopup) {
      showMessage("Tantrum ignored", 1200);
    }
    return;
  }

  if (gState.tantrumUntilEpoch == 0 && !gState.asleep &&
      nowEpoch >= gState.nextTantrumEpoch &&
      nowEpoch >= gState.tantrumCooldownUntilEpoch) {
    gState.tantrumUntilEpoch = nowEpoch + TANTRUM_DURATION_SECONDS;
    if (allowPopup) {
      showMessage("Tantrum!", 1200);
    }
  }
}

static void stepOneMinute(uint32_t nowEpoch, bool allowPopup) {
  syncSleepSchedule(nowEpoch);
  processTantrum(nowEpoch, allowPopup);
  applyPassiveDrift();
  updateLowStatTimers();
  maybeApplySicknessChance();
  updateAttentionTracking(nowEpoch);
  applyHealthRules(nowEpoch);

  ++gState.ageMinutes;
  ++gState.coinMinuteAcc;
  while (gState.coinMinuteAcc >= 10) {
    gState.coinMinuteAcc -= 10;
    if (gState.coins < 999) ++gState.coins;
  }

  evolveIfNeeded();
}

static void simulateMinutes(uint32_t startEpoch, uint32_t minutes,
                            bool allowPopup) {
  if (minutes > MAX_OFFLINE_MINUTES) minutes = MAX_OFFLINE_MINUTES;
  uint32_t epoch = startEpoch;

  bool popupAvailable = allowPopup;
  for (uint32_t i = 0; i < minutes; ++i) {
    epoch += SECONDS_PER_MINUTE;
    stepOneMinute(epoch, popupAvailable);

    if (gRun.screen == SCREEN_MESSAGE) {
      popupAvailable = false;
    }
  }
}

static void applyClamp() {
  gState.hunger = clampU8(gState.hunger);
  gState.happiness = clampU8(gState.happiness);
  gState.cleanliness = clampU8(gState.cleanliness);
  gState.energy = clampU8(gState.energy);
  gState.health = clampU8(gState.health);
  gState.discipline = clampU8(gState.discipline);
  gState.weight = clampU8(gState.weight);
  if (gState.coins > 999) gState.coins = 999;
  if (gState.sicknessRiskPermille == 0) gState.sicknessRiskPermille = 1000;
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
  gState.lightsOn = true;
  gState.medGuaranteePending = false;

  gState.invFood = 3;
  gState.invSnack = 2;
  gState.invMed = 1;
  gState.invToy = 1;

  gState.careMistakes = 0;
  gState.stageStartMistakes = 0;
  gState.sicknessRiskPermille = 1000;
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
  applyClamp();
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
  if (getCurrentEpoch(nowEpoch)) {
    tmp.lastEpoch = nowEpoch;
  }

  tmp.crc = crc16(reinterpret_cast<uint8_t *>(&tmp), sizeof(PetState));

  prefs.begin("tama", false);
  prefs.putBytes("state", &tmp, sizeof(tmp));
  prefs.end();
}

/** @copydoc resolveTantrumByScold */
bool resolveTantrumByScold() {
  if (gState.tantrumUntilEpoch == 0) return false;

  uint32_t nowEpoch = bestKnownEpoch();
  if (nowEpoch != 0 && nowEpoch >= gState.tantrumUntilEpoch) {
    return false;
  }

  gState.tantrumUntilEpoch = 0;
  gState.tantrumCooldownUntilEpoch = 0;
  gState.attentionSinceEpoch[ATTN_TANTRUM] = 0;
  gState.attentionCooldownUntilEpoch[ATTN_TANTRUM] = 0;
  if (nowEpoch != 0) {
    scheduleNextTantrum(nowEpoch);
  }
  return true;
}

/** @copydoc isTantrumActive */
bool isTantrumActive() {
  uint32_t nowEpoch = bestKnownEpoch();
  if (gState.tantrumUntilEpoch == 0) return false;
  if (nowEpoch == 0) return true;
  return nowEpoch < gState.tantrumUntilEpoch;
}

/** @copydoc getActiveAlertMask */
uint8_t getActiveAlertMask() {
  return computeAlertMask(bestKnownEpoch());
}

/** @copydoc getActiveAlertCount */
uint8_t getActiveAlertCount() {
  return countBits(getActiveAlertMask());
}

/** @copydoc currentMood */
Mood currentMood() {
  if (gState.sick || gState.health < 35) return MOOD_SICK;
  if (gState.asleep) return MOOD_SLEEPY;

  int avg =
      (gState.hunger + gState.happiness + gState.cleanliness + gState.health +
       gState.discipline) /
      5;

  if (avg > 70) return MOOD_HAPPY;
  if (avg > 45) return MOOD_OK;
  return MOOD_SAD;
}

/** @copydoc advanceTime */
void advanceTime() {
  uint32_t nowMs = millis();
  if (nowMs - gRun.lastTickMs < TICK_INTERVAL_MS) {
    return;
  }

  uint32_t elapsedMinutes = (nowMs - gRun.lastTickMs) / TICK_INTERVAL_MS;
  gRun.lastTickMs += elapsedMinutes * TICK_INTERVAL_MS;

  uint32_t startEpoch = gState.lastEpoch;
  if (startEpoch == 0) {
    if (!getCurrentEpoch(startEpoch)) {
      setRtcToBuildTime();
      getCurrentEpoch(startEpoch);
    }
  }

  if (startEpoch == 0) {
    return;
  }

  simulateMinutes(startEpoch, elapsedMinutes, true);
  gState.lastEpoch = startEpoch + elapsedMinutes * SECONDS_PER_MINUTE;

  applyClamp();
  markDirty();
  saveState(false);
}

/** @copydoc applyOfflineProgress */
void applyOfflineProgress() {
  uint32_t nowEpoch = 0;
  if (!getCurrentEpoch(nowEpoch)) {
    setRtcToBuildTime();
    if (!getCurrentEpoch(nowEpoch)) {
      return;
    }
  }

  if (gState.lastEpoch == 0) {
    gState.lastEpoch = nowEpoch;
    if (gState.nextTantrumEpoch == 0) {
      scheduleNextTantrum(nowEpoch);
    }
    return;
  }

  if (nowEpoch <= gState.lastEpoch) {
    gState.lastEpoch = nowEpoch;
    return;
  }

  uint32_t elapsedMinutes = (nowEpoch - gState.lastEpoch) / SECONDS_PER_MINUTE;
  if (elapsedMinutes > 0) {
    simulateMinutes(gState.lastEpoch, elapsedMinutes, false);
  }

  gState.lastEpoch = nowEpoch;
  applyClamp();
}
