#include "display.h"

// Variable globale pour ecrire a l'envers pour le joueur 2
bool invertText = false;

// Mini-police de caracteres 3x5 (A-Z, 0-9) optimisee bit par bit
const uint8_t police3x5[36][5] = {
  {0b010, 0b101, 0b111, 0b101, 0b101}, // A
  {0b110, 0b101, 0b110, 0b101, 0b110}, // B
  {0b011, 0b100, 0b100, 0b100, 0b011}, // C
  {0b110, 0b101, 0b101, 0b101, 0b110}, // D
  {0b111, 0b100, 0b110, 0b100, 0b111}, // E
  {0b111, 0b100, 0b110, 0b100, 0b100}, // F
  {0b011, 0b100, 0b101, 0b101, 0b011}, // G
  {0b101, 0b101, 0b111, 0b101, 0b101}, // H
  {0b111, 0b010, 0b010, 0b010, 0b111}, // I
  {0b001, 0b001, 0b001, 0b101, 0b011}, // J
  {0b101, 0b110, 0b100, 0b110, 0b101}, // K
  {0b100, 0b100, 0b100, 0b100, 0b111}, // L
  {0b101, 0b111, 0b111, 0b101, 0b101}, // M
  {0b110, 0b101, 0b101, 0b101, 0b101}, // N
  {0b010, 0b101, 0b101, 0b101, 0b010}, // O
  {0b110, 0b101, 0b110, 0b100, 0b100}, // P
  {0b010, 0b101, 0b101, 0b110, 0b011}, // Q
  {0b110, 0b101, 0b110, 0b101, 0b101}, // R
  {0b011, 0b100, 0b010, 0b001, 0b110}, // S
  {0b111, 0b010, 0b010, 0b010, 0b010}, // T
  {0b101, 0b101, 0b101, 0b101, 0b111}, // U
  {0b101, 0b101, 0b101, 0b101, 0b010}, // V
  {0b101, 0b101, 0b101, 0b111, 0b101}, // W
  {0b101, 0b101, 0b010, 0b101, 0b101}, // X
  {0b101, 0b101, 0b010, 0b010, 0b010}, // Y
  {0b111, 0b001, 0b010, 0b100, 0b111}, // Z
  {0b111, 0b101, 0b101, 0b101, 0b111}, // 0
  {0b010, 0b110, 0b010, 0b010, 0b111}, // 1
  {0b111, 0b001, 0b111, 0b100, 0b111}, // 2
  {0b111, 0b001, 0b111, 0b001, 0b111}, // 3
  {0b101, 0b101, 0b111, 0b001, 0b001}, // 4
  {0b111, 0b100, 0b111, 0b001, 0b111}, // 5
  {0b111, 0b100, 0b111, 0b101, 0b111}, // 6
  {0b111, 0b001, 0b010, 0b100, 0b100}, // 7
  {0b111, 0b101, 0b111, 0b101, 0b111}, // 8
  {0b111, 0b101, 0b111, 0b001, 0b111}  // 9
};

// Dessine un caractere a des coordonnees (x, y)
void drawChar(char c, int x, int y, CRGB color) {
  int idx = -1;
  if (c >= 'A' && c <= 'Z') idx = c - 'A';
  else if (c >= '0' && c <= '9') idx = 26 + (c - '0');
  if (idx == -1) return;

  for (int row = 0; row < 5; row++) {
    uint8_t line = police3x5[idx][row];
    for (int col = 0; col < 3; col++) {
      if (line & (1 << (2 - col))) {
        int px = x + col;
        int py = y + row;
        if (px >= 1 && px <= MATRIX_WIDTH && py >= 1 && py <= MATRIX_HEIGHT) {
          if (invertText) {
            leds[XY(MATRIX_WIDTH - px + 1, MATRIX_HEIGHT - py + 1)] = color;
          } else {
            leds[XY(px, py)] = color;
          }
        }
      }
    }
  }
}

void drawString(const char* str, int x, int y, CRGB color) {
  int i = 0;
  while (str[i] != '\0') {
    if (str[i] == ' ') x += 2; // Espace plus fin
    else { drawChar(str[i], x, y, color); x += 4; } // 3px lettre + 1px espacement
    i++;
  }
}

// Calcule la largeur du mot et le centre parfaitement sur l'ecran
void drawCenteredString(const char* str, int y, CRGB color) {
  int len = 0, i = 0;
  while(str[i] != '\0') { len++; i++; }
  int width = len * 4 - 1;
  int x = (MATRIX_WIDTH - width) / 2 + 1;
  drawString(str, x, y, color);
}
