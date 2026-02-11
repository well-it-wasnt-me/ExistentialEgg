#pragma once

#include "pet.h"
#include <M5GFX.h>

/**
 * @file ui.h
 * @brief Rendering entry points and compatibility wrappers for sprite APIs.
 */

/**
 * @brief Render the currently active screen when runtime state is marked dirty.
 */
void renderScreen();

/**
 * @brief Draw a string using whichever `drawString` signature the platform exposes.
 * @tparam T Sprite/display type.
 */
template <typename T>
static auto drawStringCompat(T &sprite, const char *text, int x, int y, int)
    -> decltype(sprite.drawString(text, x, y), void()) {
  sprite.drawString(text, x, y);
}

/** @copydoc drawStringCompat */
template <typename T>
static auto drawStringCompat(T &sprite, const char *text, int x, int y, long)
    -> decltype(sprite.drawString(x, y, text), void()) {
  sprite.drawString(x, y, text);
}

/**
 * @brief Create a sprite buffer using legacy M5GFX API (`creatSprite` typo included).
 * @tparam T Sprite type.
 */
template <typename T>
static auto createSpriteCompat(T &sprite, int x, int y, int w, int h, bool layer, int)
    -> decltype(sprite.creatSprite(x, y, w, h, layer), void()) {
  sprite.creatSprite(x, y, w, h, layer);
}

/** @copydoc createSpriteCompat */
template <typename T>
static auto createSpriteCompat(T &sprite, int x, int y, int w, int h, bool layer, long)
    -> decltype(sprite.createSprite(w, h), void()) {
  (void)x;
  (void)y;
  (void)layer;
  sprite.createSprite(w, h);
}

/**
 * @brief Push the sprite to display using whichever API signature exists.
 * @tparam T Sprite type.
 */
template <typename T>
static auto pushSpriteCompat(T &sprite, int) -> decltype(sprite.pushSprite(), void()) {
  sprite.pushSprite();
}

/** @copydoc pushSpriteCompat */
template <typename T>
static auto pushSpriteCompat(T &sprite, long)
    -> decltype(sprite.pushSprite(0, 0), void()) {
  sprite.pushSprite(0, 0);
}

/**
 * @brief Clear the sprite framebuffer before drawing the next frame.
 * @tparam T Sprite type.
 */
template <typename T>
static auto clearSpriteCompat(T &sprite, int)
    -> decltype(sprite.fillScreen(TFT_BLACK), void()) {
  sprite.fillScreen(TFT_BLACK);
}

/** @copydoc clearSpriteCompat */
template <typename T>
static auto clearSpriteCompat(T &sprite, long)
    -> decltype(sprite.fillScreen(TFT_BLACK), void()) {
  sprite.fillScreen(TFT_BLACK);
}
