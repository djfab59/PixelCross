#ifndef SHARED_H
#define SHARED_H

// Version actuelle du firmware. A incrementer a chaque nouvelle version.
#define FIRMWARE_VERSION 1

#include <Arduino.h>
#include <FastLED.h>

// Definition commune des broches materielles
#define LED_DATA_PIN 3
#define BUZZER_PIN 4
#define BUZZER_CHANNEL 0

// Nouveaux boutons selon le plan multi-jeux
#define BTN_GREEN_PIN 0   // Validation / Action Vert
#define BTN_GREEN1_PIN 1  // Menu Gauche
#define BTN_GREEN2_PIN 2  // Menu Droite
#define BTN_RED_PIN 5     // Action Rouge
#define BTN_RED1_PIN 6    // Futur Action Rouge 1
#define BTN_RED2_PIN 7    // Futur Action Rouge 2

// Broches pour les afficheurs 7 segments (74HC595)
#define PIN_SDI 8
#define PIN_SCLK 9
#define PIN_LOAD 10
#define DIGITS_PER_MODULE 3 // Nombre de chiffres par afficheur (a modifier si changement de materiel)

// Parametres de la matrice
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// Etats du programme global
enum AppState {
  STATE_MENU,
  STATE_PONG,
  STATE_SETTINGS,
  STATE_TEST,
  STATE_WIFI_CONFIG,
  STATE_OTA_UPDATE
  // On ajoutera les prochains jeux ici
};
extern AppState currentState;

// Declaration des variables et fonctions globales partagees
extern CRGB leds[NUM_LEDS];
extern uint8_t brightness;

uint16_t XY(uint8_t x, uint8_t y);
void declencherBip(unsigned int frequence, unsigned long duree);
void declencherBipDouble(unsigned int f1, unsigned int f2, unsigned long d1, unsigned long d2);
void declencherEffetPouvoir();
void gererBuzzer();

// Declaration des fonctions d'affichage de texte pour qu'elles soient utilisables partout
void drawChar(char c, int x, int y, CRGB color);
void drawString(const char* str, int x, int y, CRGB color);
void drawCenteredString(const char* str, int y, CRGB color);
extern bool invertText;

// Fonction globale pour l'affichage du score physique
void afficherScore7Seg(int scoreVert, int scoreRouge);
void afficherScoreDecimal7Seg(float scoreVert, float scoreRouge, int decimales);

#endif