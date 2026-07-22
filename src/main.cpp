#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "pong.h"
#include "chrono.h"
#include "goal.h"
#include "snake.h"
#include "settings.h"
#include "test.h"

// Le tableau qui va contenir l'etat de chaque LED
CRGB leds[NUM_LEDS];
Preferences preferences; // Objet pour gerer la sauvegarde en memoire
uint8_t brightness = 32; // Variable globale pour la luminosite (valeur par defaut)

AppState currentState = STATE_MENU; // L'application demarre sur le Menu

// Variables pour le systeme de Menu
int menuIndex = 0;
const int NUM_MENU_ITEMS = 5; // Pong, Chrono, Goal, Snake, Reglages
bool prevGreen1 = HIGH;
bool prevGreen2 = HIGH;
bool prevGreen = HIGH;

// Fonction magique pour convertir des coordonnees (X, Y) en index 1D physique
uint16_t XY(uint8_t x, uint8_t y) {
  uint16_t i;
  
  // On repasse en base 0 pour les calculs mathematiques internes
  uint8_t internalX = x - 1;
  uint8_t internalY = y - 1;

  // Si la colonne est paire (0, 2, 4...), le cablage descend
  if (internalX % 2 == 0) {
    i = (internalX * MATRIX_HEIGHT) + internalY;
  } 
  // Si la colonne est impaire (1, 3, 5...), le cablage remonte
  else {
    i = (internalX * MATRIX_HEIGHT) + (MATRIX_HEIGHT - 1 - internalY);
  }
  return i;
}

void setup() {
  Serial.begin(115200);
  
  // --- CHARGEMENT DES PREFERENCES UTILISATEUR ---
  preferences.begin("pixelcross", false);
  brightness = preferences.getUChar("brightness", 32);
  preferences.end();

  randomSeed(esp_random());
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  
  pinMode(BTN_GREEN_PIN, INPUT_PULLUP);
  pinMode(BTN_GREEN1_PIN, INPUT_PULLUP);
  pinMode(BTN_GREEN2_PIN, INPUT_PULLUP);
  pinMode(BTN_RED_PIN, INPUT_PULLUP);
  pinMode(BTN_RED1_PIN, INPUT_PULLUP);
  pinMode(BTN_RED2_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);

  pinMode(PIN_SDI, OUTPUT);
  pinMode(PIN_SCLK, OUTPUT);
  pinMode(PIN_LOAD, OUTPUT);
  afficherScore7Seg(-1, -1);

  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);
  
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500); 
  
  FastLED.setBrightness(brightness); 

  // --- ECRAN D'ACCUEIL ---
  // "PXLCROSS" descend du haut vers le centre avec changement de couleur progressif
  // et un son de demarrage melodique
  {
    const int yFinal = 2; // Position Y centree
    const int yDepart = -4; // Commence hors ecran (au-dessus)
    const int dureeDescente = 2000; // 2 secondes
    const int nbEtapes = yFinal - yDepart; // 6 etapes
    const int delaiParEtape = dureeDescente / nbEtapes;
    
    // Notes melodiques pour l'intro (une par etape de descente)
    int notesIntro[] = {262, 330, 392, 523, 659, 784}; // Do Mi Sol Do' Mi' Sol'
    
    for (int y = yDepart; y <= yFinal; y++) {
      FastLED.clear();
      
      // Couleur qui evolue du cyan vers le violet pendant la descente
      uint8_t progression = map(y, yDepart, yFinal, 0, 255);
      CRGB couleur = CRGB(progression, 255 - progression / 2, 255);
      
      drawCenteredString("PXLCROSS", y, couleur);
      FastLED.show();
      
      // Son melodique pour cette etape
      int noteIdx = y - yDepart;
      if (noteIdx < 6) {
        ledcWriteTone(BUZZER_CHANNEL, notesIntro[noteIdx]);
        ledcWrite(BUZZER_CHANNEL, 127);
      }
      
      delay(delaiParEtape);
    }
    
    // Coupe le son
    ledcWriteTone(BUZZER_CHANNEL, 0);
    ledcWrite(BUZZER_CHANNEL, 0);
    
    // Pause au centre (500ms)
    delay(500);
    
    // Fade out progressif (disparition en moins d'une seconde)
    for (int b = brightness; b >= 0; b -= 8) {
      if (b < 0) b = 0;
      FastLED.setBrightness(b);
      FastLED.show();
      delay(30);
      if (b == 0) break;
    }
    
    FastLED.clear();
    FastLED.show();
    
    // Remet la luminosite normale
    FastLED.setBrightness(brightness);
    delay(200);
  }
}

void drawMenu() {
  FastLED.clear();
  if (menuIndex == 0) {
    drawCenteredString("PONG", 2, CRGB::Green);
  } else if (menuIndex == 1) {
    drawCenteredString("CHRONO", 2, CRGB::Green);
  } else if (menuIndex == 2) {
    drawCenteredString("GOAL", 2, CRGB::Green);
  } else if (menuIndex == 3) {
    drawCenteredString("SNAKE", 2, CRGB::Green);
  } else if (menuIndex == 4) {
    drawCenteredString("REGLAGES", 2, CRGB::Green);
  }
  FastLED.show();
}

void loopMenu() {
  bool actGreen1 = digitalRead(BTN_GREEN1_PIN);
  bool actGreen2 = digitalRead(BTN_GREEN2_PIN);
  bool actGreen = digitalRead(BTN_GREEN_PIN);

  if (actGreen1 == LOW && prevGreen1 == HIGH) {
    menuIndex--;
    if (menuIndex < 0) menuIndex = NUM_MENU_ITEMS - 1;
    declencherBip(500, 30);
  }
  if (actGreen2 == LOW && prevGreen2 == HIGH) {
    menuIndex++;
    if (menuIndex >= NUM_MENU_ITEMS) menuIndex = 0;
    declencherBip(600, 30);
  }
  if (actGreen == LOW && prevGreen == HIGH) {
    declencherBip(1000, 100);
    if (menuIndex == 0) {
      initPong();
      currentState = STATE_PONG;
    } else if (menuIndex == 1) {
      initChrono();
      currentState = STATE_CHRONO;
    } else if (menuIndex == 2) {
      initGoal();
      currentState = STATE_GOAL;
    } else if (menuIndex == 3) {
      initSnake();
      currentState = STATE_SNAKE;
    } else if (menuIndex == 4) {
      initSettings();
      currentState = STATE_SETTINGS;
    }
  }

  prevGreen1 = actGreen1;
  prevGreen2 = actGreen2;
  prevGreen = actGreen;

  drawMenu();
}

void loop() {
  gererBuzzer();

  if (currentState == STATE_MENU) {
    loopMenu();
  } else if (currentState == STATE_PONG) {
    loopPong();
  } else if (currentState == STATE_CHRONO) {
    loopChrono();
  } else if (currentState == STATE_GOAL) {
    loopGoal();
  } else if (currentState == STATE_SNAKE) {
    loopSnake();
  } else if (currentState == STATE_SETTINGS) {
    loopSettings();
  } else if (currentState == STATE_TEST) {
    loopTest();
  } else if (currentState == STATE_WIFI_CONFIG) {
    // Etat dedie a la configuration WiFi.
    // On ne fait RIEN d'autre pour donner toutes les ressources au portail WiFi.
    // La fonction est bloquante et changera l'etat elle-meme a la fin.
    startWifiConfigPortal();
  } else if (currentState == STATE_OTA_UPDATE) {
    loopOtaUpdate();
  }
}
