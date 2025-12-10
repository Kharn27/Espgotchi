#pragma once

#include <stdint.h>

// Géométrie de l'écran TFT
static constexpr uint16_t SCREEN_W = 320;
static constexpr uint16_t SCREEN_H = 240;

// Hauteurs des barres
static constexpr uint16_t TOP_BAR_H    = 28;
// Mets ici la valeur que tu utilises déjà pour ta barre du bas
static constexpr uint16_t BOTTOM_BAR_H = 40; // par ex. 40

// Bouton SPD dans la top bar (à droite)
static constexpr uint16_t SPEED_BTN_W = 64;
static constexpr uint16_t SPEED_BTN_H = TOP_BAR_H;
static constexpr uint16_t SPEED_BTN_X = SCREEN_W - SPEED_BTN_W;
static constexpr uint16_t SPEED_BTN_Y = 0;

static constexpr uint32_t RENDER_FPS = 4;
