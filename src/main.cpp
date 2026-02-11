#include <M5CoreInk.h>
#include <esp_system.h>

#include "logic.h"
#include "pet.h"
#include "sound.h"
#include "ui.h"

/**
 * @brief Firmware initialization entry point.
 *
 * Boots hardware, restores state, and gently informs the pet that time is real.
 */
void setup() {
  M5.begin();

  if (!M5.M5Ink.isInit()) {
    while (1) {
      delay(100);
    }
  }

  createSpriteCompat(gSprite, 0, 0, SCREEN_W, SCREEN_H, true, 0);
  clearSpriteCompat(gSprite, 0);
  pushSpriteCompat(gSprite, 0);

  playStartupTune();

  randomSeed(esp_random());

  bool loaded = loadState();
  if (!loaded) {
    defaultState();
  }

  applyOfflineProgress();
  gRun.screen = SCREEN_HOME;
  gRun.lastScreen = SCREEN_HOME;
  gRun.lastUiActionMs = millis();
  gRun.lastSaveMs = millis();
  gRun.lastTickMs = millis();
  gRun.menuIndex = 0;
  gRun.inventoryIndex = 0;
  gRun.helpScroll = 0;
  gRun.mgActive = false;
  gRun.dirty = true;

  saveState(true);
}

/**
 * @brief Main firmware loop.
 *
 * Poll input, run simulation, render if needed, repeat forever because deadlines.
 */
void loop() {
  M5.update();

  handleButtons();
  handleMessageTimeout();
  advanceTime();
  handleIdle();
  renderScreen();

  delay(10);
}
