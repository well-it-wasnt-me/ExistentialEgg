#pragma once

#include "pet.h"

/**
 * @file logic.h
 * @brief Input handling and gameplay action orchestration.
 *
 * Buttons go in, consequences come out.
 */

/**
 * @brief Process hardware/input button events and trigger game actions.
 */
void handleButtons();

/**
 * @brief Dismiss message screen when its timeout expires.
 */
void handleMessageTimeout();

/**
 * @brief Apply idle behavior (auto-sleep) after UI inactivity.
 */
void handleIdle();

/**
 * @brief Read inventory quantity for a specific item type.
 * @param item Inventory item identifier.
 * @return Number of owned items.
 */
uint8_t inventoryCount(ItemType item);
