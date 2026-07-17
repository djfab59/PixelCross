#ifndef SHARED_H
#define SHARED_H

// Version actuelle du firmware au format "major.minor"
#define FIRMWARE_VERSION "0.5"

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
#define DIGITS_PER_MODULE 4 // Nombre de chiffres par afficheur (a modifier si changement de materiel)

// Parametres de la matrice
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// Etats du programme global
enum AppState {
  STATE_MENU,
  STATE_PONG,
  STATE_CHRONO,
  STATE_SETTINGS,
  STATE_TEST,
  STATE_WIFI_CONFIG,
  STATE_OTA_UPDATE
};
extern AppState currentState;

// Declaration des variables globales partagees
extern CRGB leds[NUM_LEDS];
extern uint8_t brightness;

// Fonction de mapping des coordonnees (X, Y) vers l'index physique de la LED
uint16_t XY(uint8_t x, uint8_t y);

#endif
