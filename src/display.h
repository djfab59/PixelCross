#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <FastLED.h>
#include "shared.h"

// Variable globale pour ecrire a l'envers (joueur 2)
extern bool invertText;

// Fonctions d'affichage de texte sur la matrice LED
void drawChar(char c, int x, int y, CRGB color);
void drawString(const char* str, int x, int y, CRGB color);
void drawCenteredString(const char* str, int y, CRGB color);

#endif
