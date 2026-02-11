#include "ui.h"
#include "logic.h"

#include <M5GFX.h>
#include <stdio.h>
#include <string.h>

/**
 * @file ui.cpp
 * @brief Screen rendering for the monochrome life simulator.
 */

static const uint16_t UI_BG = TFT_BLACK;
static const uint16_t UI_FG = TFT_WHITE;

// drawing helpers
static int estimateTextWidth(const char *text, uint8_t size) {
  if (!text) return 0;
  return (int)strlen(text) * 6 * size;
}

static void drawText(int x, int y, const char *text, uint8_t size) {
  gSprite.setTextSize(size);
  drawStringCompat(gSprite, text, x, y, 0);
}

static inline void drawRectCompat(Ink_Sprite &sprite, int x, int y, int w, int h,
                                  uint16_t color) {
  sprite.drawRect(x, y, w, h, color);
}

static inline void fillRectCompat(Ink_Sprite &sprite, int x, int y, int w, int h,
                                  uint16_t color) {
  sprite.fillRect(x, y, w, h, color);
}

static inline void drawCircleCompat(Ink_Sprite &sprite, int x, int y, int r,
                                    uint16_t color) {
  sprite.drawCircle(x, y, r, color);
}

static inline void fillCircleCompat(Ink_Sprite &sprite, int x, int y, int r,
                                    uint16_t color) {
  sprite.fillCircle(x, y, r, color);
}

static inline void drawLineCompat(Ink_Sprite &sprite, int x1, int y1, int x2,
                                  int y2, uint16_t color) {
  sprite.drawLine(x1, y1, x2, y2, color);
}

static void drawTextCentered(int y, const char *text, uint8_t size) {
  int w = estimateTextWidth(text, size);
  int x = (SCREEN_W - w) / 2;
  drawText(x, y, text, size);
}

static void drawTextRight(int xRight, int y, const char *text, uint8_t size) {
  int w = estimateTextWidth(text, size);
  drawText(xRight - w, y, text, size);
}

static void setTextColorMono(bool inverted) {
  gSprite.setTextColor(inverted ? UI_BG : UI_FG);
}

static void drawBar(int x, int y, int w, int h, uint8_t value) {
  drawRectCompat(gSprite, x, y, w, h, UI_FG);
  int fill = (w - 2) * value / 100;
  if (fill < 0) fill = 0;
  fillRectCompat(gSprite, x + 1, y + 1, fill, h - 2, UI_FG);
}

static void drawDivider(int y) { drawLineCompat(gSprite, 0, y, SCREEN_W, y, UI_FG); }

static void drawSoftkeys(const char *left, const char *mid, const char *right) {
  drawDivider(176);
  drawText(4, 182, left, 1);
  drawTextCentered(182, mid, 1);
  drawTextRight(SCREEN_W - 4, 182, right, 1);
}

static void drawStatMini(int x, int y, const char *label, uint8_t value) {
  drawText(x, y, label, 1);
  drawBar(x + 16, y + 1, 66, 8, value);
}

static void drawHomeIconRow(int y, bool topRow) {
  const int cellW = 38;
  const int cellH = 14;
  const int gap = 8;
  const int startX = 12;

  for (int i = 0; i < 4; ++i) {
    int x = startX + i * (cellW + gap);
    int cx = x + cellW / 2;
    int cy = y + cellH / 2;

    drawRectCompat(gSprite, x, y, cellW, cellH, UI_FG);

    if (topRow) {
      switch (i) {
        case 0: // food
          drawLineCompat(gSprite, cx - 9, cy - 4, cx - 9, cy + 4, UI_FG);
          drawLineCompat(gSprite, cx - 10, cy - 4, cx - 8, cy - 4, UI_FG);
          drawLineCompat(gSprite, cx + 5, cy - 1, cx + 5, cy + 4, UI_FG);
          drawCircleCompat(gSprite, cx + 5, cy - 3, 2, UI_FG);
          break;
        case 1: // light
          drawCircleCompat(gSprite, cx, cy - 1, 3, UI_FG);
          drawRectCompat(gSprite, cx - 1, cy + 3, 3, 2, UI_FG);
          drawLineCompat(gSprite, cx - 5, cy - 1, cx - 3, cy - 1, UI_FG);
          drawLineCompat(gSprite, cx + 3, cy - 1, cx + 5, cy - 1, UI_FG);
          break;
        case 2: // medicine
          drawRectCompat(gSprite, cx - 4, cy - 4, 9, 9, UI_FG);
          drawLineCompat(gSprite, cx - 2, cy, cx + 2, cy, UI_FG);
          drawLineCompat(gSprite, cx, cy - 2, cx, cy + 2, UI_FG);
          break;
        case 3: // clean
          drawRectCompat(gSprite, cx - 5, cy - 2, 10, 7, UI_FG);
          drawLineCompat(gSprite, cx - 3, cy - 4, cx + 3, cy - 4, UI_FG);
          drawLineCompat(gSprite, cx + 4, cy - 4, cx + 6, cy - 2, UI_FG);
          break;
      }
    } else {
      switch (i) {
        case 0: // game
          drawCircleCompat(gSprite, cx, cy, 4, UI_FG);
          drawLineCompat(gSprite, cx - 2, cy - 2, cx + 2, cy + 2, UI_FG);
          break;
        case 1: // train
          drawRectCompat(gSprite, cx - 6, cy - 4, 5, 8, UI_FG);
          drawRectCompat(gSprite, cx + 1, cy - 4, 5, 8, UI_FG);
          drawLineCompat(gSprite, cx, cy - 3, cx, cy + 3, UI_FG);
          break;
        case 2: // call
          drawLineCompat(gSprite, cx - 3, cy - 3, cx - 1, cy - 5, UI_FG);
          drawLineCompat(gSprite, cx + 3, cy - 3, cx + 1, cy - 5, UI_FG);
          drawCircleCompat(gSprite, cx, cy, 4, UI_FG);
          break;
        case 3: // status
          drawCircleCompat(gSprite, cx - 3, cy, 3, UI_FG);
          drawCircleCompat(gSprite, cx + 3, cy, 3, UI_FG);
          drawLineCompat(gSprite, cx - 1, cy, cx + 1, cy, UI_FG);
          break;
      }
    }
  }
}

static int scaleSignedRounded(int value, int numer, int denom) {
  int mag = value >= 0 ? value : -value;
  int scaled = (mag * numer + denom / 2) / denom;
  return value >= 0 ? scaled : -scaled;
}

static int scaleDimRounded(int value, int numer, int denom) {
  int scaled = (value * numer + denom / 2) / denom;
  return scaled < 1 ? 1 : scaled;
}

static const int FACE_DRAW_BASE = 72;
static const int FACE_DRAW_TARGET = FACE_DRAW_BASE - 15;
static const int STAGE_DRAW_BASE = 24;
static const int STAGE_DRAW_TARGET = STAGE_DRAW_BASE - 5;

static int faceOffset(int value) {
  return scaleSignedRounded(value, FACE_DRAW_TARGET, FACE_DRAW_BASE);
}

static int faceRadius(int value) {
  return scaleDimRounded(value, FACE_DRAW_TARGET, FACE_DRAW_BASE);
}

static int stageOffset(int value) {
  return scaleSignedRounded(value, STAGE_DRAW_TARGET, STAGE_DRAW_BASE);
}

static int stageRadius(int value) {
  return scaleDimRounded(value, STAGE_DRAW_TARGET, STAGE_DRAW_BASE);
}

static int stageDim(int value) {
  return scaleDimRounded(value, STAGE_DRAW_TARGET, STAGE_DRAW_BASE);
}

static void drawStageIcon(int cx, int cy, Stage stage) {
  switch (stage) {
    case STAGE_EGG:
      drawCircleCompat(gSprite, cx, cy + stageOffset(2), stageRadius(10), UI_FG);
      drawCircleCompat(gSprite, cx, cy + stageOffset(-4), stageRadius(8), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-4), cy + stageOffset(2),
                     cx + stageOffset(-1), cy + stageOffset(5), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-1), cy + stageOffset(5),
                     cx + stageOffset(2), cy + stageOffset(2), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(2), cy + stageOffset(2),
                     cx + stageOffset(5), cy + stageOffset(5), UI_FG);
      break;
    case STAGE_BABY:
      drawCircleCompat(gSprite, cx, cy + stageOffset(-4), stageRadius(7), UI_FG);
      drawCircleCompat(gSprite, cx, cy + stageOffset(8), stageRadius(6), UI_FG);
      drawCircleCompat(gSprite, cx, cy + stageOffset(2), stageRadius(2), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-3), cy + stageOffset(4),
                     cx + stageOffset(3), cy + stageOffset(4), UI_FG);
      break;
    case STAGE_CHILD:
      drawCircleCompat(gSprite, cx, cy + stageOffset(-4), stageRadius(8), UI_FG);
      drawRectCompat(gSprite, cx + stageOffset(-6), cy + stageOffset(4),
                     stageDim(12), stageDim(10), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-10), cy + stageOffset(6),
                     cx + stageOffset(-6), cy + stageOffset(8), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(6), cy + stageOffset(8),
                     cx + stageOffset(10), cy + stageOffset(6), UI_FG);
      break;
    case STAGE_TEEN:
      drawCircleCompat(gSprite, cx, cy + stageOffset(-4), stageRadius(8), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-6), cy + stageOffset(-12),
                     cx + stageOffset(-2), cy + stageOffset(-8), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(0), cy + stageOffset(-12),
                     cx + stageOffset(2), cy + stageOffset(-8), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(6), cy + stageOffset(-12),
                     cx + stageOffset(2), cy + stageOffset(-8), UI_FG);
      drawRectCompat(gSprite, cx + stageOffset(-5), cy + stageOffset(4),
                     stageDim(10), stageDim(12), UI_FG);
      break;
    case STAGE_ADULT:
      drawCircleCompat(gSprite, cx, cy + stageOffset(-5), stageRadius(9), UI_FG);
      drawRectCompat(gSprite, cx + stageOffset(-8), cy + stageOffset(4),
                     stageDim(16), stageDim(12), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-12), cy + stageOffset(6),
                     cx + stageOffset(-8), cy + stageOffset(10), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(8), cy + stageOffset(10),
                     cx + stageOffset(12), cy + stageOffset(6), UI_FG);
      break;
    case STAGE_ELDER:
      drawCircleCompat(gSprite, cx, cy + stageOffset(-5), stageRadius(8), UI_FG);
      drawCircleCompat(gSprite, cx + stageOffset(-4), cy + stageOffset(-5),
                       stageRadius(2), UI_FG);
      drawCircleCompat(gSprite, cx + stageOffset(4), cy + stageOffset(-5),
                       stageRadius(2), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(-2), cy + stageOffset(-5),
                     cx + stageOffset(2), cy + stageOffset(-5), UI_FG);
      drawRectCompat(gSprite, cx + stageOffset(-6), cy + stageOffset(4),
                     stageDim(12), stageDim(10), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(10), cy + stageOffset(2),
                     cx + stageOffset(10), cy + stageOffset(14), UI_FG);
      drawLineCompat(gSprite, cx + stageOffset(8), cy + stageOffset(14),
                     cx + stageOffset(12), cy + stageOffset(14), UI_FG);
      break;
    default:
      break;
  }
}

static void drawMoodFace(int cx, int cy, Mood mood, int eyeDx, int eyeDy, int eyeR,
                         int mouthHalf, int mouthDy) {
  if (mood == MOOD_SLEEPY) {
    drawLineCompat(gSprite, cx - eyeDx - 2, cy + eyeDy, cx - eyeDx + 2, cy + eyeDy,
                   UI_FG);
    drawLineCompat(gSprite, cx + eyeDx - 2, cy + eyeDy, cx + eyeDx + 2, cy + eyeDy,
                   UI_FG);
  } else if (mood == MOOD_SICK) {
    drawLineCompat(gSprite, cx - eyeDx - 2, cy + eyeDy - 2, cx - eyeDx + 2,
                   cy + eyeDy + 2, UI_FG);
    drawLineCompat(gSprite, cx - eyeDx - 2, cy + eyeDy + 2, cx - eyeDx + 2,
                   cy + eyeDy - 2, UI_FG);
    drawLineCompat(gSprite, cx + eyeDx - 2, cy + eyeDy - 2, cx + eyeDx + 2,
                   cy + eyeDy + 2, UI_FG);
    drawLineCompat(gSprite, cx + eyeDx - 2, cy + eyeDy + 2, cx + eyeDx + 2,
                   cy + eyeDy - 2, UI_FG);
  } else {
    drawCircleCompat(gSprite, cx - eyeDx, cy + eyeDy, eyeR, UI_FG);
    drawCircleCompat(gSprite, cx + eyeDx, cy + eyeDy, eyeR, UI_FG);
  }

  int mouthY = cy + mouthDy;
  if (mood == MOOD_HAPPY) {
    drawLineCompat(gSprite, cx - mouthHalf, mouthY - 2, cx, mouthY + 2, UI_FG);
    drawLineCompat(gSprite, cx, mouthY + 2, cx + mouthHalf, mouthY - 2, UI_FG);
  } else if (mood == MOOD_SAD || mood == MOOD_SICK) {
    drawLineCompat(gSprite, cx - mouthHalf, mouthY + 2, cx, mouthY - 2, UI_FG);
    drawLineCompat(gSprite, cx, mouthY - 2, cx + mouthHalf, mouthY + 2, UI_FG);
  } else {
    drawLineCompat(gSprite, cx - mouthHalf, mouthY, cx + mouthHalf, mouthY, UI_FG);
  }
}

static void drawPetAvatar(int cx, int cy, Stage stage, Mood mood) {
  switch (stage) {
    case STAGE_EGG:
      drawCircleCompat(gSprite, cx, cy + 5, 15, UI_FG);
      drawCircleCompat(gSprite, cx, cy - 4, 11, UI_FG);
      drawLineCompat(gSprite, cx - 7, cy + 4, cx - 3, cy + 8, UI_FG);
      drawLineCompat(gSprite, cx - 3, cy + 8, cx + 1, cy + 4, UI_FG);
      drawLineCompat(gSprite, cx + 1, cy + 4, cx + 6, cy + 8, UI_FG);
      drawMoodFace(cx, cy + 1, mood, 4, -1, 1, 3, 3);
      break;
    case STAGE_BABY:
      drawCircleCompat(gSprite, cx, cy - 6, 12, UI_FG);
      drawCircleCompat(gSprite, cx, cy + 10, 10, UI_FG);
      drawCircleCompat(gSprite, cx, cy + 2, 2, UI_FG);
      drawMoodFace(cx, cy - 6, mood, 4, -2, 1, 3, 4);
      break;
    case STAGE_CHILD:
      drawCircleCompat(gSprite, cx, cy - 8, 11, UI_FG);
      drawRectCompat(gSprite, cx - 10, cy + 2, 20, 16, UI_FG);
      drawLineCompat(gSprite, cx - 13, cy + 6, cx - 10, cy + 9, UI_FG);
      drawLineCompat(gSprite, cx + 10, cy + 9, cx + 13, cy + 6, UI_FG);
      drawMoodFace(cx, cy - 8, mood, 4, -2, 1, 4, 4);
      break;
    case STAGE_TEEN:
      drawCircleCompat(gSprite, cx, cy - 8, 11, UI_FG);
      drawLineCompat(gSprite, cx - 7, cy -18, cx - 3, cy -13, UI_FG);
      drawLineCompat(gSprite, cx, cy -18, cx + 2, cy -13, UI_FG);
      drawLineCompat(gSprite, cx + 7, cy -18, cx + 3, cy -13, UI_FG);
      drawRectCompat(gSprite, cx - 9, cy + 2, 18, 18, UI_FG);
      drawMoodFace(cx, cy - 8, mood, 4, -2, 1, 4, 4);
      break;
    case STAGE_ADULT:
      drawCircleCompat(gSprite, cx, cy - 8, 12, UI_FG);
      drawRectCompat(gSprite, cx - 11, cy + 2, 22, 20, UI_FG);
      drawLineCompat(gSprite, cx - 15, cy + 6, cx - 11, cy + 11, UI_FG);
      drawLineCompat(gSprite, cx + 11, cy + 11, cx + 15, cy + 6, UI_FG);
      drawMoodFace(cx, cy - 8, mood, 4, -2, 1, 4, 4);
      break;
    case STAGE_ELDER:
      drawCircleCompat(gSprite, cx, cy - 8, 11, UI_FG);
      drawRectCompat(gSprite, cx - 9, cy + 2, 18, 16, UI_FG);
      drawLineCompat(gSprite, cx + 13, cy + 2, cx + 13, cy + 18, UI_FG);
      drawLineCompat(gSprite, cx + 11, cy + 18, cx + 15, cy + 18, UI_FG);
      drawMoodFace(cx, cy - 8, mood, 4, -2, 1, 4, 4);
      break;
    default:
      drawCircleCompat(gSprite, cx, cy, faceRadius(20), UI_FG);
      drawMoodFace(cx, cy, mood, 4, -2, 1, 4, 4);
      break;
  }
}

static void drawTopBar() {
  RTC_TimeTypeDef t;
  RTC_DateTypeDef d;
  M5.Rtc.GetTime(&t);
  M5.Rtc.GetDate(&d);

  char timeBuf[6];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", t.Hours, t.Minutes);

  char leftBuf[24];
  uint32_t days = gState.ageMinutes / (24 * 60);
  snprintf(leftBuf, sizeof(leftBuf), "%s - D%lu", timeBuf, (unsigned long)days);
  drawText(4, 6, leftBuf, 1);

  char coinBuf[14];
  snprintf(coinBuf, sizeof(coinBuf), "C%u", gState.coins);
  drawTextCentered(6, coinBuf, 1);

  char batBuf[12];
  snprintf(batBuf, sizeof(batBuf), "B%u%%", getBatteryPercent());
  drawTextRight(SCREEN_W - 4, 6, batBuf, 1);
}

static void drawHeader(const char *title, uint8_t size = 1) {
  drawTopBar();
  drawDivider(20);
  if (title && title[0]) {
    drawTextCentered(24, title, size);
    drawDivider(44);
  }
}

static void renderHome() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawTopBar();
  drawDivider(20);

  Mood mood = currentMood();
  Stage stage = static_cast<Stage>(gState.stage);

  drawHomeIconRow(24, true);

  const int playX = 28;
  const int playY = 44;
  const int playW = 144;
  const int playH = 78;
  drawRectCompat(gSprite, playX, playY, playW, playH, UI_FG);
  drawRectCompat(gSprite, playX + 2, playY + 2, playW - 4, playH - 4, UI_FG);

  char ageBuf[20];
  uint32_t days = gState.ageMinutes / (24 * 60);
  snprintf(ageBuf, sizeof(ageBuf), "Age %lud", (unsigned long)days);
  drawText(playX + 6, playY + 4, kStageNames[stage], 1);
  drawTextRight(playX + playW - 6, playY + 4, ageBuf, 1);

  drawPetAvatar(SCREEN_W / 2, playY + 43, stage, mood);

  if (gState.asleep) {
    drawText(playX + 6, playY + 14, "Zzz", 1);
  }
  if (gState.sick) {
    drawTextRight(playX + playW - 6, playY + 14, "Sick", 1);
  }

  drawHomeIconRow(126, false);

  drawDivider(142);
  drawStatMini(8, 146, "HU", gState.hunger);
  drawStatMini(8, 160, "EN", gState.energy);
  drawStatMini(108, 146, "CL", gState.cleanliness);
  drawStatMini(108, 160, "HP", gState.happiness);

  const char *action = gState.asleep ? "B Wake" : "B Play";
  drawSoftkeys("A Menu", action, "C Status");
}

static void renderMenu() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Menu");

  const int startY = 50;
  const int cellW = 88;
  const int cellH = 20;
  const int gapX = 8;
  const int gapY = 4;
  const int leftX = 8;
  const int rightX = leftX + cellW + gapX;

  for (uint8_t i = 0; i < kMenuCount; ++i) {
    int col = (i % 2);
    int row = (i / 2);
    int x = col == 0 ? leftX : rightX;
    int y = startY + row * (cellH + gapY);

    if (i == gRun.menuIndex) {
      gSprite.setColor(UI_FG);
      fillRectCompat(gSprite, x, y, cellW, cellH, UI_FG);
      drawRectCompat(gSprite, x, y, cellW, cellH, UI_FG);
      setTextColorMono(true);
    } else {
      drawRectCompat(gSprite, x, y, cellW, cellH, UI_FG);
      setTextColorMono(false);
    }

    int labelW = estimateTextWidth(kMenuItems[i], 1);
    int labelX = x + (cellW - labelW) / 2;
    drawText(labelX, y + 6, kMenuItems[i], 1);
  }
  setTextColorMono(false);

  drawSoftkeys("A Home", "B Select", "C Next");
}

static void renderStatus() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Status");

  int y = 52;
  drawStatMini(8, y, "HL", gState.health);
  drawStatMini(8, y + 14, "HU", gState.hunger);
  drawStatMini(8, y + 28, "HP", gState.happiness);
  drawStatMini(8, y + 42, "CL", gState.cleanliness);
  drawStatMini(8, y + 56, "EN", gState.energy);
  drawStatMini(8, y + 70, "DS", gState.discipline);

  char buf[24];
  uint32_t days = gState.ageMinutes / (24 * 60);
  drawText(110, 50, "Stage", 1);
  drawText(110, 60, kStageNames[gState.stage], 1);
  drawStageIcon(170, 66, static_cast<Stage>(gState.stage));
  snprintf(buf, sizeof(buf), "Age: %lud", (unsigned long)days);
  drawText(110, 82, buf, 1);
  snprintf(buf, sizeof(buf), "Wt: %u", gState.weight);
  drawText(110, 94, buf, 1);
  snprintf(buf, sizeof(buf), "Poop: %u", gState.poop);
  drawText(110, 106, buf, 1);
  snprintf(buf, sizeof(buf), "Sick: %s", gState.sick ? "Yes" : "No");
  drawText(110, 118, buf, 1);
  snprintf(buf, sizeof(buf), "Sleep: %s", gState.asleep ? "Yes" : "No");
  drawText(110, 130, buf, 1);
  snprintf(buf, sizeof(buf), "Bat: %u%%", getBatteryPercent());
  drawText(110, 142, buf, 1);

  drawSoftkeys("A Back", "B Inv", "C Reset");
}

static void renderResetConfirm() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Reset Game?");

  drawRectCompat(gSprite, 14, 56, 172, 86, UI_FG);
  drawTextCentered(74, "This will erase", 1);
  drawTextCentered(88, "all progress.", 1);
  drawTextCentered(108, "B: Confirm", 1);
  drawTextCentered(122, "A/C: Cancel", 1);

  drawSoftkeys("A No", "B Yes", "C No");
}

static void renderInventory() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Inventory");

  ItemType item = static_cast<ItemType>(gRun.inventoryIndex);
  uint8_t count = inventoryCount(item);
  char buf[32];

  drawRectCompat(gSprite, 20, 54, 160, 76, UI_FG);

  snprintf(buf, sizeof(buf), "%s", kItems[item].name);
  drawTextCentered(62, buf, 3);

  snprintf(buf, sizeof(buf), "Count %u", count);
  drawTextCentered(92, buf, 2);

  snprintf(buf, sizeof(buf), "Cost %u", kItems[item].cost);
  drawTextCentered(112, buf, 2);

  drawTextCentered(136, count > 0 ? "Use" : "Buy", 2);

  drawSoftkeys("A Back", "B Use/Buy", "C Next");
}

static void renderMinigame() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Mini-game");

  if (!gRun.mgActive) {
    drawTextCentered(74, "Press B", 2);
    drawTextCentered(94, "to start", 2);
  } else {
    const char *target =
        gRun.mgTarget == 0 ? "A" : (gRun.mgTarget == 1 ? "B" : "C");
    char buf[32];
    snprintf(buf, sizeof(buf), "Press %s", target);
    drawTextCentered(76, buf, 3);

    uint32_t now = millis();
    uint32_t remaining =
        (gRun.mgDeadlineMs > now) ? (gRun.mgDeadlineMs - now) : 0;
    snprintf(buf, sizeof(buf), "%lus", (unsigned long)(remaining / 1000));
    drawTextCentered(112, buf, 2);
  }

  drawSoftkeys("A Back", "B Go", "C Back");
}

static void renderMessage() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawRectCompat(gSprite, 12, 54, 176, 84, UI_FG);
  drawTextCentered(86, gRun.message, 2);
  drawSoftkeys("A OK", "B OK", "C OK");
}

static void renderHelp() {
  clearSpriteCompat(gSprite, 0);
  setTextColorMono(false);
  drawHeader("Helper", 1);

  static const char *const kHelpLines[] = {
      "Controls",
      "A: Up in Helper",
      "B: Select / Back",
      "C: Down in Helper",
      "G5: Go Home",
      "G27: Toggle Status",
      "",
      "Home",
      "A Menu  B Play/Wake",
      "C Status",
      "",
      "Reset",
      "Status: C opens confirm",
      "B confirms reset",
      "A/C cancel reset",
      "",
      "Legend",
      "HL Health  HU Hunger",
      "EN Energy  CL Clean",
      "HP Happy   DS Discipline"};

  const int startY = 52;
  const int lineH = 12;
  const int visibleLines = 10;
  const int totalLines = (int)(sizeof(kHelpLines) / sizeof(kHelpLines[0]));
  const int maxScroll = totalLines > visibleLines ? (totalLines - visibleLines) : 0;
  int scroll = gRun.helpScroll;
  if (scroll > maxScroll) scroll = maxScroll;

  for (int i = 0; i < visibleLines; ++i) {
    int idx = scroll + i;
    if (idx >= totalLines) break;
    drawText(8, startY + i * lineH, kHelpLines[idx], 1);
  }

  char pageBuf[16];
  snprintf(pageBuf, sizeof(pageBuf), "%d/%d", scroll + 1, maxScroll + 1);
  drawTextRight(SCREEN_W - 4, 166, pageBuf, 1);

  drawSoftkeys("A Up", "B Back", "C Down");
}

/** @copydoc renderScreen */
void renderScreen() {
  if (!gRun.dirty) return;
  gRun.dirty = false;

  switch (gRun.screen) {
    case SCREEN_HOME:
      renderHome();
      break;
    case SCREEN_MENU:
      renderMenu();
      break;
    case SCREEN_STATUS:
      renderStatus();
      break;
    case SCREEN_INVENTORY:
      renderInventory();
      break;
    case SCREEN_MINIGAME:
      renderMinigame();
      break;
    case SCREEN_MESSAGE:
      renderMessage();
      break;
    case SCREEN_HELP:
      renderHelp();
      break;
    case SCREEN_RESET_CONFIRM:
      renderResetConfirm();
      break;
    default:
      renderHome();
      break;
  }

  pushSpriteCompat(gSprite, 0);
}
