#include "sound.h"

#include <M5CoreInk.h>

/**
 * @file sound.cpp
 * @brief Startup audio playback.
 */

/** @brief A single note (or rest) in the startup melody. */
struct Note {
  /** @brief Frequency in Hz. Use 0 for silence. */
  uint16_t freq;
  /** @brief Note duration in milliseconds. */
  uint16_t durMs;
};

/** @copydoc playStartupTune */
void playStartupTune() {
  static const Note melody[] = {
      {659, 125}, {659, 125}, {0, 125},  {659, 125}, {0, 167},
      {523, 125}, {659, 125}, {0, 167},  {784, 125}, {0, 375},
      {392, 125}};

  M5.Speaker.setVolume(180);
  for (auto &n : melody) {
    if (n.freq == 0) {
      delay(n.durMs);
    } else {
      M5.Speaker.tone(n.freq, n.durMs);
      delay((uint32_t)(n.durMs * 1.25f));
    }
  }
  M5.Speaker.mute();
}
