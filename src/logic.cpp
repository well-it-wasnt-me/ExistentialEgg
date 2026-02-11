#include "logic.h"

#include <esp_system.h>

/**
 * @file logic.cpp
 * @brief Input processing and gameplay action routing.
 */

template <typename T>
static auto btnAImpl(T &m5, int) -> decltype((m5.BtnUP)) { return m5.BtnUP; }
template <typename T>
static auto btnAImpl(T &m5, long) -> decltype((m5.BtnA)) { return m5.BtnA; }
template <typename T>
static auto btnBImpl(T &m5, int) -> decltype((m5.BtnMID)) { return m5.BtnMID; }
template <typename T>
static auto btnBImpl(T &m5, long) -> decltype((m5.BtnB)) { return m5.BtnB; }
template <typename T>
static auto btnCImpl(T &m5, int) -> decltype((m5.BtnDOWN)) { return m5.BtnDOWN; }
template <typename T>
static auto btnCImpl(T &m5, long) -> decltype((m5.BtnC)) { return m5.BtnC; }

static auto btnA() -> decltype(btnAImpl(M5, 0)) { return btnAImpl(M5, 0); }
static auto btnB() -> decltype(btnBImpl(M5, 0)) { return btnBImpl(M5, 0); }
static auto btnC() -> decltype(btnCImpl(M5, 0)) { return btnCImpl(M5, 0); }

static const uint8_t GPIO_TOP_HOME = 5;    // top hardware button
static const uint8_t GPIO_SIDE_QUICK = 27; // side hardware button

static bool gGpioButtonsInit = false;
static bool gTopWasDown = false;
static bool gSideWasDown = false;

static void initGpioButtons() {
  if (gGpioButtonsInit) return;

  pinMode(GPIO_TOP_HOME, INPUT_PULLUP);
  pinMode(GPIO_SIDE_QUICK, INPUT_PULLUP);
  gTopWasDown = (digitalRead(GPIO_TOP_HOME) == LOW);
  gSideWasDown = (digitalRead(GPIO_SIDE_QUICK) == LOW);
  gGpioButtonsInit = true;
}

static bool readGpioPressed(uint8_t pin, bool &wasDown) {
  bool isDown = (digitalRead(pin) == LOW);
  bool pressed = isDown && !wasDown;
  wasDown = isDown;
  return pressed;
}

static void goHomeShortcut() {
  if (gRun.screen == SCREEN_MINIGAME) {
    gRun.mgActive = false;
  }
  gRun.screen = SCREEN_HOME;
  markDirty();
}

static void sideQuickAction() {
  if (gRun.screen == SCREEN_MINIGAME) {
    gRun.mgActive = false;
  }
  gRun.screen = (gRun.screen == SCREEN_STATUS) ? SCREEN_HOME : SCREEN_STATUS;
  markDirty();
}

static void doFeed(bool isSnack) {
  if (isSnack) {
    gState.hunger = clampU8(gState.hunger + 15);
    gState.happiness = clampU8(gState.happiness + 10);
    gState.weight = clampU8(gState.weight + 2);
  } else {
    gState.hunger = clampU8(gState.hunger + 25);
    gState.happiness = clampU8(gState.happiness + 4);
    gState.weight = clampU8(gState.weight + 1);
  }
  showMessage(isSnack ? "Snack time!" : "Fed!", 1200);
}

static void doPlay() {
  gState.happiness = clampU8(gState.happiness + 20);
  gState.energy = clampU8(gState.energy - 12);
  gState.hunger = clampU8(gState.hunger - 6);
  showMessage("Play time!", 1200);
}

static void doClean() {
  gState.cleanliness = 100;
  gState.poop = 0;
  showMessage("All clean!", 1200);
}

static void doMedicine() {
  if (!gState.sick && gState.health > 85) {
    showMessage("No medicine needed", 1400);
    return;
  }
  gState.sick = false;
  gState.health = clampU8(gState.health + 25);
  gState.happiness = clampU8(gState.happiness - 5);
  showMessage("Feeling better", 1400);
}

static void doTrain() {
  gState.discipline = clampU8(gState.discipline + 12);
  gState.happiness = clampU8(gState.happiness - 4);
  gState.energy = clampU8(gState.energy - 6);
  showMessage("Training done", 1200);
}

static void doSleepToggle() {
  gState.asleep = !gState.asleep;
  showMessage(gState.asleep ? "Sleeping" : "Awake", 1200);
}

static void doGameReset() {
  defaultState();
  gRun.menuIndex = 0;
  gRun.inventoryIndex = 0;
  gRun.helpScroll = 0;
  gRun.mgActive = false;
  gRun.mgTarget = 0;
  gRun.mgDeadlineMs = 0;
  gRun.screen = SCREEN_HOME;
  showMessage("Game reset", 1600);
  saveState(true);
}

static void applyInventoryUse(ItemType item) {
  switch (item) {
    case ITEM_FOOD:
      doFeed(false);
      gState.invFood--;
      break;
    case ITEM_SNACK:
      doFeed(true);
      gState.invSnack--;
      break;
    case ITEM_MED:
      doMedicine();
      gState.invMed--;
      break;
    case ITEM_TOY:
      doPlay();
      gState.invToy--;
      break;
    default:
      break;
  }
}

/** @copydoc inventoryCount */
uint8_t inventoryCount(ItemType item) {
  switch (item) {
    case ITEM_FOOD:
      return gState.invFood;
    case ITEM_SNACK:
      return gState.invSnack;
    case ITEM_MED:
      return gState.invMed;
    case ITEM_TOY:
      return gState.invToy;
    default:
      return 0;
  }
}

static void setInventoryCount(ItemType item, uint8_t value) {
  switch (item) {
    case ITEM_FOOD:
      gState.invFood = value;
      break;
    case ITEM_SNACK:
      gState.invSnack = value;
      break;
    case ITEM_MED:
      gState.invMed = value;
      break;
    case ITEM_TOY:
      gState.invToy = value;
      break;
    default:
      break;
  }
}

static void buyItem(ItemType item) {
  if (gState.coins < kItems[item].cost) {
    showMessage("Not enough coins", 1400);
    return;
  }
  gState.coins -= kItems[item].cost;
  uint8_t count = inventoryCount(item);
  if (count < 99) {
    setInventoryCount(item, count + 1);
  }
  showMessage("Bought!", 900);
}

static void handleInventorySelect() {
  ItemType item = static_cast<ItemType>(gRun.inventoryIndex);
  uint8_t count = inventoryCount(item);
  if (count > 0) {
    applyInventoryUse(item);
  } else {
    buyItem(item);
  }
  markDirty();
  saveState(true);
}

static void startMiniGame() {
  gRun.mgActive = true;
  gRun.mgTarget = esp_random() % 3;
  gRun.mgDeadlineMs = millis() + 5000;
}

static void resolveMiniGame(bool success) {
  gRun.mgActive = false;
  if (success) {
    gState.coins = (gState.coins + 5 > 999) ? 999 : gState.coins + 5;
    gState.happiness = clampU8(gState.happiness + 8);
    showMessage("Nice! +5 coins", 1500);
  } else {
    gState.happiness = clampU8(gState.happiness - 5);
    showMessage("Missed it", 1200);
  }
  saveState(true);
}

static void handleMenuSelect() {
  switch (gRun.menuIndex) {
    case 0: // Feed
      if (gState.invFood > 0) {
        gState.invFood--;
        doFeed(false);
      } else {
        showMessage("No food - buy in Inv", 1500);
      }
      break;
    case 1: // Play
      doPlay();
      break;
    case 2: // Clean
      doClean();
      break;
    case 3: // Sleep/Wake
      doSleepToggle();
      break;
    case 4: // Medicine
      if (gState.invMed > 0) {
        gState.invMed--;
        doMedicine();
      } else {
        showMessage("No medicine", 1200);
      }
      break;
    case 5: // Train
      doTrain();
      break;
    case 6: // Inventory
      gRun.screen = SCREEN_INVENTORY;
      break;
    case 7: // Mini-game
      gRun.screen = SCREEN_MINIGAME;
      startMiniGame();
      break;
    case 8: // Status
      gRun.screen = SCREEN_STATUS;
      break;
    case 9: // Helper
      gRun.helpScroll = 0;
      gRun.screen = SCREEN_HELP;
      break;
    default:
      break;
  }
  markDirty();
  saveState(true);
}

/** @copydoc handleButtons */
void handleButtons() {
  initGpioButtons();

  bool a = btnA().wasPressed();
  bool b = btnB().wasPressed();
  bool c = btnC().wasPressed();
  bool top = readGpioPressed(GPIO_TOP_HOME, gTopWasDown);
  bool side = readGpioPressed(GPIO_SIDE_QUICK, gSideWasDown);

  if (top) {
    goHomeShortcut();
    return;
  }
  if (side) {
    sideQuickAction();
    return;
  }

  if (gRun.screen == SCREEN_MINIGAME && gRun.mgActive) {
    if (a) {
      resolveMiniGame(gRun.mgTarget == 0);
      markDirty();
    } else if (b) {
      resolveMiniGame(gRun.mgTarget == 1);
      markDirty();
    } else if (c) {
      resolveMiniGame(gRun.mgTarget == 2);
      markDirty();
    } else if (millis() > gRun.mgDeadlineMs) {
      resolveMiniGame(false);
      markDirty();
    }
    return;
  }

  if (a) {
    switch (gRun.screen) {
      case SCREEN_HOME:
        gRun.screen = SCREEN_MENU;
        break;
      case SCREEN_MENU:
        gRun.screen = SCREEN_HOME;
        break;
      case SCREEN_STATUS:
        gRun.screen = SCREEN_HOME;
        break;
      case SCREEN_INVENTORY:
        gRun.screen = SCREEN_MENU;
        break;
      case SCREEN_MINIGAME:
        gRun.screen = SCREEN_MENU;
        gRun.mgActive = false;
        break;
      case SCREEN_MESSAGE:
        gRun.screen = gRun.lastScreen;
        break;
      case SCREEN_HELP:
        if (gRun.helpScroll > 0) gRun.helpScroll--;
        break;
      case SCREEN_RESET_CONFIRM:
        gRun.screen = SCREEN_STATUS;
        break;
      default:
        gRun.screen = SCREEN_HOME;
        break;
    }
    markDirty();
  }

  if (b) {
    switch (gRun.screen) {
      case SCREEN_HOME:
        if (gState.asleep)
          doSleepToggle();
        else
          doPlay();
        break;
      case SCREEN_MENU:
        handleMenuSelect();
        break;
      case SCREEN_STATUS:
        gRun.screen = SCREEN_INVENTORY;
        break;
      case SCREEN_INVENTORY:
        handleInventorySelect();
        break;
      case SCREEN_MINIGAME:
        if (!gRun.mgActive) {
          startMiniGame();
        }
        break;
      case SCREEN_MESSAGE:
        gRun.screen = gRun.lastScreen;
        break;
      case SCREEN_HELP:
        gRun.screen = SCREEN_MENU;
        break;
      case SCREEN_RESET_CONFIRM:
        doGameReset();
        break;
      default:
        break;
    }
    markDirty();
  }

  if (c) {
    switch (gRun.screen) {
      case SCREEN_HOME:
        gRun.screen = SCREEN_STATUS;
        break;
      case SCREEN_MENU:
        gRun.menuIndex = (gRun.menuIndex + 1) % kMenuCount;
        break;
      case SCREEN_STATUS:
        gRun.screen = SCREEN_RESET_CONFIRM;
        break;
      case SCREEN_INVENTORY:
        gRun.inventoryIndex = (gRun.inventoryIndex + 1) % ITEM_COUNT;
        break;
      case SCREEN_MINIGAME:
        gRun.screen = SCREEN_MENU;
        gRun.mgActive = false;
        break;
      case SCREEN_MESSAGE:
        gRun.screen = gRun.lastScreen;
        break;
      case SCREEN_HELP:
        if (gRun.helpScroll < 255) gRun.helpScroll++;
        break;
      case SCREEN_RESET_CONFIRM:
        gRun.screen = SCREEN_STATUS;
        break;
      default:
        break;
    }
    markDirty();
  }
}

/** @copydoc handleMessageTimeout */
void handleMessageTimeout() {
  if (gRun.screen != SCREEN_MESSAGE) return;
  if (millis() > gRun.messageUntilMs) {
    gRun.screen = gRun.lastScreen;
    markDirty();
  }
}

/** @copydoc handleIdle */
void handleIdle() {
  if (gRun.screen == SCREEN_MESSAGE) return;
  if (millis() - gRun.lastUiActionMs > UI_IDLE_SLEEP_MS) {
    if (!gState.asleep) {
      gState.asleep = true;
      showMessage("Auto sleep", 1200);
    }
  }
}
