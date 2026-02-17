#pragma once

#include <M5CoreInk.h>
#include <Preferences.h>
#include <stdint.h>

/**
 * @file pet.h
 * @brief Shared game state and persistence contracts.
 *
 * This is the single source of truth for the pet, the runtime, and every
 * questionable decision that must survive a reboot.
 */

/** @brief Screen size for M5 Core Ink. */
static const int SCREEN_W = 200;
static const int SCREEN_H = 200;

/**
 * @brief Save metadata and timing constants.
 *
 * Yes, these are magic values. No, they are not self-healing.
 */
static const uint32_t MAGIC = 0x54414D41; // "TAMA"
static const uint16_t STATE_VERSION = 2;
static const uint32_t SAVE_INTERVAL_MS = 2 * 60 * 1000;
static const uint32_t TICK_INTERVAL_MS = 60 * 1000;
static const uint32_t MAX_OFFLINE_MINUTES = 7 * 24 * 60; // one week

/** @brief UI screens the player can navigate through before returning to home anyway. */
enum Screen {
  SCREEN_HOME,
  SCREEN_MENU,
  SCREEN_STATUS,
  SCREEN_INVENTORY,
  SCREEN_MINIGAME,
  SCREEN_MESSAGE,
  SCREEN_HELP,
  SCREEN_RESET_CONFIRM
};

/** @brief High-level mood buckets derived from the stat apocalypse. */
enum Mood {
  MOOD_HAPPY,
  MOOD_OK,
  MOOD_SAD,
  MOOD_SLEEPY,
  MOOD_SICK
};

/** @brief Growth stages as time and care quality do their thing. */
enum Stage {
  STAGE_EGG,
  STAGE_BABY,
  STAGE_CHILD,
  STAGE_TEEN,
  STAGE_ADULT,
  STAGE_ELDER
};

/** @brief Inventory item types available for buying or consuming. */
enum ItemType {
  ITEM_FOOD,
  ITEM_SNACK,
  ITEM_MED,
  ITEM_TOY,
  ITEM_COUNT
};

/** @brief Attention reasons that can generate care mistakes. */
enum AttentionReason {
  ATTN_HUNGER,
  ATTN_HAPPINESS,
  ATTN_POOP,
  ATTN_SICK,
  ATTN_LIGHTS,
  ATTN_TANTRUM,
  ATTN_COUNT
};

/** @brief Shop/inventory definition for a single item type. */
struct ItemDef {
  /** @brief Display name shown in inventory and shop UI. */
  const char *name;
  /** @brief Coin cost per purchase. */
  uint8_t cost;
};

/** @brief Human-readable labels for each pet stage. */
extern const char *const kStageNames[];
/** @brief Human-readable labels for each mood state. */
extern const char *const kMoodNames[];
/** @brief Static catalog used by the inventory/shop screen. */
extern const ItemDef kItems[ITEM_COUNT];
/** @brief Main menu labels in visual order. */
extern const char *const kMenuItems[];
/** @brief Number of entries in `kMenuItems`. */
extern const uint8_t kMenuCount;

/**
 * @brief Serialized persistent pet state.
 *
 * If this changes, old saves may panic and politely stop loading.
 */
struct PetState {
  /** @brief Save signature. */
  uint32_t magic;
  /** @brief Save format version. */
  uint16_t version;
  /** @brief CRC16 checksum over the struct with this field zeroed. */
  uint16_t crc;

  /** @brief Last known RTC epoch used for offline progression. */
  uint32_t lastEpoch;
  /** @brief Total lifetime in in-game minutes. */
  uint32_t ageMinutes;

  /** @brief Currency used for purchases. */
  uint16_t coins;
  /** @brief Current growth stage encoded as `Stage`. */
  uint8_t stage;

  uint8_t hunger;      // 0-100
  uint8_t happiness;   // 0-100
  uint8_t cleanliness; // 0-100
  uint8_t energy;      // hidden/internal (legacy)
  uint8_t health;      // 0-100
  uint8_t discipline;  // 0-100
  uint8_t weight;      // 0-100

  uint8_t poop;
  bool asleep;
  bool sick;
  bool lightsOn;
  bool medGuaranteePending;

  uint8_t invFood;
  uint8_t invSnack;
  uint8_t invMed;
  uint8_t invToy;

  /** @brief Care mistakes accumulated over the pet lifetime. */
  uint16_t careMistakes;
  /** @brief Snapshot of `careMistakes` at stage start for stage scoring. */
  uint16_t stageStartMistakes;
  /** @brief Sickness chance multiplier in permille (1000 = 1.0x). */
  uint16_t sicknessRiskPermille;

  /** @brief Minutes spent with low hunger for sickness checks. */
  uint16_t lowHungerMinutes;
  /** @brief Minutes spent with low happiness for sickness checks. */
  uint16_t lowHappinessMinutes;

  /** @brief Per-minute drift accumulators to avoid truncation dead zones. */
  int16_t hungerAcc;
  int16_t happinessAcc;
  int16_t disciplineAcc;
  int16_t cleanlinessAcc;
  int16_t healthAcc;

  /** @brief Time accumulators for periodic events. */
  uint16_t poopMinuteAcc;
  uint16_t coinMinuteAcc;

  /** @brief Medicine timing and guarantee tracking. */
  uint32_t lastMedicineEpoch;

  /** @brief Tantrum scheduler and state. */
  uint32_t nextTantrumEpoch;
  uint32_t tantrumUntilEpoch;         // 0 when inactive
  uint32_t tantrumCooldownUntilEpoch; // block retrigger after failure

  /** @brief Per-reason attention state for care-mistake timing. */
  uint32_t attentionSinceEpoch[ATTN_COUNT];
  uint32_t attentionCooldownUntilEpoch[ATTN_COUNT];
};

/**
 * @brief Ephemeral runtime/UI state.
 *
 * None of this belongs in permanent storage, unlike your regrets.
 */
struct RuntimeState {
  /** @brief Active screen currently rendered. */
  Screen screen;
  /** @brief Previous screen used when closing transient message overlays. */
  Screen lastScreen;
  /** @brief Last user interaction timestamp (`millis`). */
  uint32_t lastUiActionMs;
  /** @brief Last save timestamp (`millis`). */
  uint32_t lastSaveMs;
  /** @brief Last simulation tick timestamp (`millis`). */
  uint32_t lastTickMs;
  /** @brief Whether screen content needs redraw. */
  bool dirty;

  /** @brief Current menu selection index. */
  uint8_t menuIndex;

  /** @brief Current inventory selection index. */
  uint8_t inventoryIndex;

  /** @brief Scroll offset for helper page. */
  uint8_t helpScroll;

  /** @brief Short transient message text. */
  char message[64];
  /** @brief Message expiry timestamp (`millis`). */
  uint32_t messageUntilMs;

  /** @brief Whether the reaction mini-game is currently running. */
  bool mgActive;
  /** @brief Target button in mini-game: 0=A, 1=B, 2=C. */
  uint8_t mgTarget; // 0=A,1=B,2=C
  /** @brief Mini-game timeout deadline (`millis`). */
  uint32_t mgDeadlineMs;

  /** @brief Hidden developer mode unlocked via button sequence. */
  bool devModeUnlocked;
  /** @brief Runtime debug overlay visibility toggle. */
  bool debugOverlay;
  /** @brief Input ring buffer for the developer sequence. */
  uint8_t devSeqBuf[7];
  /** @brief Number of valid entries currently in `devSeqBuf`. */
  uint8_t devSeqLen;
  /** @brief Start time for the current developer sequence attempt (`millis`). */
  uint32_t devSeqStartedMs;
};

/** @brief Global persistent pet state instance. */
extern PetState gState;
/** @brief Global runtime/UI state instance. */
extern RuntimeState gRun;
/** @brief NVS preferences storage handle. */
extern Preferences prefs;
/** @brief Shared draw sprite bound to the e-ink display. */
extern Ink_Sprite gSprite;

/**
 * @brief Clamp a stat value into the only acceptable emotional range.
 * @param v Input value.
 * @return Value clamped to [0, 100].
 */
uint8_t clampU8(int v);
/**
 * @brief Estimate battery charge percent from the ADC reading.
 * @return Battery percentage clamped to [0, 100].
 */
uint8_t getBatteryPercent();
/**
 * @brief Read current RTC epoch if available.
 * @param outEpoch Output epoch seconds.
 * @return `true` when RTC time is valid.
 */
bool getCurrentEpoch(uint32_t &outEpoch);
/**
 * @brief Mark runtime state as needing a redraw and refresh idle timer.
 *
 * Because nothing says "responsive UI" like a dirty flag.
 */
void markDirty();
/**
 * @brief Show a temporary message screen.
 * @param msg Null-terminated text to display.
 * @param durationMs How long to show the message in milliseconds.
 */
void showMessage(const char *msg, uint32_t durationMs);
/**
 * @brief Resolve an active tantrum by scolding.
 * @return `true` when a tantrum was active and resolved.
 */
bool resolveTantrumByScold();
/**
 * @brief Whether a tantrum is currently active.
 * @return `true` if tantrum timer is running.
 */
bool isTantrumActive();
/**
 * @brief Compute currently active attention reasons as bitmask.
 * @return Bitmask using `AttentionReason` ordinals.
 */
uint8_t getActiveAlertMask();
/**
 * @brief Compute count of currently active attention reasons.
 * @return Number of active alerts.
 */
uint8_t getActiveAlertCount();
/**
 * @brief Compute the pet mood from current stats.
 * @return Derived mood bucket.
 */
Mood currentMood();
/**
 * @brief Reset persistent state to a fresh new life.
 *
 * Fresh meaning "minutes from existential decline."
 */
void defaultState();
/**
 * @brief Load state from NVS and validate checksum/version.
 * @return `true` if a valid save was loaded; otherwise `false`.
 */
bool loadState();
/**
 * @brief Persist current state to NVS.
 * @param force When `true`, bypasses save interval throttling.
 */
void saveState(bool force);
/**
 * @brief Apply elapsed RTC time to simulate offline progression.
 */
void applyOfflineProgress();
/**
 * @brief Advance simulation based on elapsed runtime ticks.
 */
void advanceTime();
