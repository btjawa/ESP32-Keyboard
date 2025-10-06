#pragma once

#include <Arduino.h>

constexpr uint8_t K_ROWS[] = {21, 10, 14, 8};
constexpr uint8_t K_ROWS_LEN = sizeof(K_ROWS) / sizeof(K_ROWS[0]);
constexpr uint8_t K_COLS[] = {9, 11, 47, 48};
constexpr uint8_t K_COLS_LEN = sizeof(K_COLS) / sizeof(K_COLS[0]);

extern volatile bool gKeyPressed[K_ROWS_LEN][K_COLS_LEN];

void setupKeyboard();
