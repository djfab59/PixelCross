#include <Arduino.h>
#include <FastLED.h>
#include <Preferences.h>
#include "shared.h"
#include "pong.h"
#include "settings.h"
#include "test.h"

// Le tableau qui va contenir l'etat de chaque LED
CRGB leds[NUM_LEDS];
Preferences preferences; // Objet pour gerer la sauvegarde en memoire
uint8_t brightness = 32; // Variable globale pour la luminosité (valeur par défaut)

AppState currentState = STATE_MENU; // L'application demarre sur le Menu

// Variables pour le système de Menu
int menuIndex = 0;
const int NUM_MENU_ITEMS = 3; // Pong, Réglages, Test
bool prevGreen1 = HIGH;
bool prevGreen2 = HIGH;
bool prevGreen = HIGH;

// Variables pour gerer le buzzer de maniere non-bloquante
unsigned long timerBuzzer = 0;
bool buzzerActif = false;
bool effetPouvoirActif = false;
unsigned long debutEffetPouvoir = 0;

// Variables pour le bip double
unsigned int freqBip2 = 0;
unsigned long dureeBip2 = 0;
bool attenteBip2 = false;

// Fonction pour declencher un bip sans le parametre de duree (qui bug souvent sur ESP32)
void declencherBip(unsigned int frequence, unsigned long duree) {
  effetPouvoirActif = false; // Stoppe l'effet pouvoir si un bip normal prioritaire doit jouer
  attenteBip2 = false; // Annule un bip double en attente
  ledcWriteTone(BUZZER_CHANNEL, frequence); // Utilisation de l'API PWM native
  ledcWrite(BUZZER_CHANNEL, 127); // Force le rapport cyclique a 50% (sur 255) pour creer le son
  timerBuzzer = millis() + duree;
  buzzerActif = true;
}

// Fonction pour declencher un son en deux temps
void declencherBipDouble(unsigned int f1, unsigned int f2, unsigned long d1, unsigned long d2) {
  effetPouvoirActif = false;
  freqBip2 = f2;
  dureeBip2 = d2;
  attenteBip2 = true;
  ledcWriteTone(BUZZER_CHANNEL, f1);
  ledcWrite(BUZZER_CHANNEL, 127);
  timerBuzzer = millis() + d1;
  buzzerActif = true;
}

// Fonction pour declencher l'effet sonore special du pouvoir (Grave -> Aigu -> Grave)
void declencherEffetPouvoir() {
  effetPouvoirActif = true;
  buzzerActif = false; // Stoppe tout bip standard en cours
  attenteBip2 = false; // Stoppe un bip double en attente
  debutEffetPouvoir = millis();
}

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

// --- POLICE ET AFFICHAGE TEXTE ---
bool invertText = false; // Permet d'ecrire a l'envers pour le joueur 2
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

// Dessine un caractere a des coordonnees (x,y)
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

// --- GESTION DES AFFICHEURS 7 SEGMENTS (74HC595) ---
// Dictionnaire standard pour allumer les segments (A a G) des chiffres 0 a 9
// Les afficheurs sont a "anode commune", la logique est donc inversee (0 = Allume, 1 = Eteint)
const uint8_t table7Seg[11] = {
  0b11000000, // 0
  0b11111001, // 1
  0b10100100, // 2
  0b10110000, // 3
  0b10011001, // 4
  0b10010010, // 5
  0b10000010, // 6
  0b11111000, // 7
  0b10000000, // 8
  0b10010000, // 9
  0b11111111  // 10 (Espace / Eteint)
};

void afficherScore7Seg(int scoreVert, int scoreRouge) {
  digitalWrite(PIN_LOAD, LOW);
  
  // Envoi dynamique selon le nombre de digits par module (DIGITS_PER_MODULE)
  // Ordre : Joueur Rouge d'abord (car c'est le dernier dans la chaine Daisy Chain)
  int divRouge = 1;
  for (int i = 0; i < DIGITS_PER_MODULE; i++) {
    uint8_t digit = (scoreRouge >= 0) ? table7Seg[(scoreRouge / divRouge) % 10] : table7Seg[10];
    shiftOut(PIN_SDI, PIN_SCLK, MSBFIRST, digit);
    divRouge *= 10;
  }

  // Ordre : Joueur Vert ensuite
  int divVert = 1;
  for (int i = 0; i < DIGITS_PER_MODULE; i++) {
    uint8_t digit = (scoreVert >= 0) ? table7Seg[(scoreVert / divVert) % 10] : table7Seg[10];
    shiftOut(PIN_SDI, PIN_SCLK, MSBFIRST, digit);
    divVert *= 10;
  }

  digitalWrite(PIN_LOAD, HIGH);
}

// Nouvelle fonction pour gerer les nombres a virgule
void afficherScoreDecimal7Seg(float scoreVert, float scoreRouge, int decimales) {
  // Multiplicateur pour transformer le float en int (ex: 9.99 avec 2 decimales -> 9.99 * 100 = 999)
  int multiplicateur = pow(10, decimales);
  int intScoreVert = (scoreVert >= 0) ? round(scoreVert * multiplicateur) : -1;
  int intScoreRouge = (scoreRouge >= 0) ? round(scoreRouge * multiplicateur) : -1;

  digitalWrite(PIN_LOAD, LOW);

  // --- JOUEUR ROUGE ---
  int divRouge = 1;
  for (int i = 0; i < DIGITS_PER_MODULE; i++) {
    uint8_t digit = (intScoreRouge >= 0) ? table7Seg[(intScoreRouge / divRouge) % 10] : table7Seg[10];
    // Si on est sur le digit qui doit avoir le point, on force son bit a 0
    if (intScoreRouge >= 0 && i == decimales) {
      digit &= 0b01111111; // Met le bit du point a 0 (allume) sans toucher aux autres
    }
    shiftOut(PIN_SDI, PIN_SCLK, MSBFIRST, digit);
    divRouge *= 10;
  }

  // --- JOUEUR VERT ---
  int divVert = 1;
  for (int i = 0; i < DIGITS_PER_MODULE; i++) {
    uint8_t digit = (intScoreVert >= 0) ? table7Seg[(intScoreVert / divVert) % 10] : table7Seg[10];
    if (intScoreVert >= 0 && i == decimales) {
      digit &= 0b01111111;
    }
    shiftOut(PIN_SDI, PIN_SCLK, MSBFIRST, digit);
    divVert *= 10;
  }

  digitalWrite(PIN_LOAD, HIGH);
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

  // Bip de demarrage
  declencherBipDouble(800, 1200, 80, 120);
}

void drawMenu() {
  FastLED.clear();
  if (menuIndex == 0) {
    drawCenteredString("PONG", 2, CRGB::Green);
  } else if (menuIndex == 1) {
    drawCenteredString("REGLAGES", 2, CRGB::Green);
  } else if (menuIndex == 2) {
    drawCenteredString("TEST", 2, CRGB::Green);
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
      initSettings();
      currentState = STATE_SETTINGS;
    } else if (menuIndex == 2) {
      initTest();
      currentState = STATE_TEST;
    }
  }

  prevGreen1 = actGreen1;
  prevGreen2 = actGreen2;
  prevGreen = actGreen;

  drawMenu();
}

void gererBuzzer() {
  if (buzzerActif && millis() >= timerBuzzer) {
    if (attenteBip2) {
      // Declenche la deuxieme partie du son
      ledcWriteTone(BUZZER_CHANNEL, freqBip2);
      ledcWrite(BUZZER_CHANNEL, 127);
      timerBuzzer = millis() + dureeBip2;
      attenteBip2 = false;
    } else {
      ledcWriteTone(BUZZER_CHANNEL, 0); // Stoppe la fréquence PWM
      ledcWrite(BUZZER_CHANNEL, 0);     // Coupe totalement le courant (0%) pour proteger le transistor
      buzzerActif = false;
    }
  }

  // --- GESTION DE L'EFFET SONORE DU POUVOIR ---
  if (effetPouvoirActif) {
    unsigned long tempsEcoule = millis() - debutEffetPouvoir;
    if (tempsEcoule <= 1000) { // L'effet dure exactement 1 seconde (1000 ms)
      unsigned int frequenceActuelle;
      if (tempsEcoule <= 500) {
        // Premiere moitie (0 a 500ms) : On monte de 100Hz a 2000Hz
        frequenceActuelle = map(tempsEcoule, 0, 500, 100, 2000);
      } else {
        // Deuxieme moitie (501 a 1000ms) : On redescend de 2000Hz a 100Hz
        frequenceActuelle = map(tempsEcoule, 500, 1000, 2000, 100);
      }
      // On met a jour la frequence du buzzer en temps reel !
      ledcWriteTone(BUZZER_CHANNEL, frequenceActuelle);
      ledcWrite(BUZZER_CHANNEL, 127);
    } else {
      // L'effet est termine
      effetPouvoirActif = false;
      ledcWriteTone(BUZZER_CHANNEL, 0);
      ledcWrite(BUZZER_CHANNEL, 0);
    }
  }
}

void loop() {
  gererBuzzer();

  if (currentState == STATE_MENU) {
    loopMenu();
  } else if (currentState == STATE_PONG) {
    loopPong();
  } else if (currentState == STATE_SETTINGS) {
    loopSettings();
  } else if (currentState == STATE_TEST) {
    loopTest();
  } else if (currentState == STATE_WIFI_CONFIG) {
    // Etat dedie a la configuration WiFi.
    // On ne fait RIEN d'autre pour donner toutes les ressources au portail WiFi.
    // La fonction est bloquante et changera l'etat elle-meme a la fin.
    startWifiConfigPortal();
  }
}