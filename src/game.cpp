#include "game.h"

// --- ANIMATION DE FIN DE SET ---
void animationSetGagnant(bool vertGagne) {
  ledcWriteTone(BUZZER_CHANNEL, 150);
  ledcWrite(BUZZER_CHANNEL, 127);

  CRGB couleurGagnant = vertGagne ? CRGB::Green : CRGB::Red;
  FastLED.clear();
  for (int x = 1; x <= MATRIX_WIDTH; x++) {
    int colonne = vertGagne ? x : (MATRIX_WIDTH - x + 1);
    for (int y = 1; y <= MATRIX_HEIGHT; y++) leds[XY(colonne, y)] = couleurGagnant;
    FastLED.show();
    delay(30);
  }

  ledcWriteTone(BUZZER_CHANNEL, 0);
  ledcWrite(BUZZER_CHANNEL, 0);
  delay(200);
}

// --- ANIMATION NOUVEAU RECORD ---
void animationNouveauRecord(int score) {
  for (int i = 0; i < 6; i++) {
    // Flash dore
    for (int j = 0; j < NUM_LEDS; j++) leds[j] = CRGB(255, 180, 0);
    afficherScore7Seg(score, score);
    FastLED.show();
    ledcWriteTone(BUZZER_CHANNEL, 1500 + i * 200);
    ledcWrite(BUZZER_CHANNEL, 127);
    delay(100);
    // Flash bleu
    for (int j = 0; j < NUM_LEDS; j++) leds[j] = CRGB(0, 50, 255);
    afficherScore7Seg(-1, -1);
    FastLED.show();
    ledcWriteTone(BUZZER_CHANNEL, 0);
    ledcWrite(BUZZER_CHANNEL, 0);
    delay(100);
  }
  // Final : affiche le score une derniere fois
  afficherScore7Seg(score, score);
  delay(500);
}

// --- FANFARE DE VICTOIRE ---
void jouerFanfare() {
  int notes[] = {392, 523, 659, 784, 659, 784};
  int durees[] = {150, 150, 150, 300, 150, 500};
  for (int i = 0; i < 6; i++) {
    ledcWriteTone(BUZZER_CHANNEL, notes[i]);
    ledcWrite(BUZZER_CHANNEL, 127);
    delay(durees[i]);
    ledcWriteTone(BUZZER_CHANNEL, 0);
    ledcWrite(BUZZER_CHANNEL, 0);
    delay(50);
  }
  delay(1000);
}

// --- DESSIN DU VERROU ---
void dessinerVerrou(bool joueurVert, int nbVies) {
  if (joueurVert) {
    // LED jaune juste apres les vies du vert (ligne 1, position vies+1)
    leds[XY(1 + nbVies, 1)] = CRGB::Orange;
  } else {
    // LED jaune juste apres les vies du rouge (ligne 8, symetrique)
    leds[XY(32 - nbVies, 8)] = CRGB::Orange;
  }
}

// --- AFFICHAGE DES VIES ---
void dessinerViesVert(int vies) {
  CRGB couleurVie = CRGB(127, 10, 73);
  for (int i = 0; i < vies; i++) leds[XY(1 + i, 1)] = couleurVie;
}

void dessinerViesRouge(int vies) {
  CRGB couleurVie = CRGB(127, 10, 73);
  for (int i = 0; i < vies; i++) leds[XY(32 - i, 8)] = couleurVie;
}

// --- ANIMATION PERTE DE VIE (DUO) ---
void animationPerteVie(int viesVert, int viesRouge, bool vertAPerdu, CRGB couleurPiste) {
  declencherBip(300, 600);
  CRGB couleurVie = CRGB(127, 10, 73);
  bool afficherPiste = (couleurPiste != CRGB(CRGB::Black));

  for (int i = 0; i < 3; i++) {
    FastLED.clear();
    dessinerViesVert(viesVert);
    dessinerViesRouge(viesRouge);
    if (afficherPiste) {
      for (int p = 1; p <= 64; p++) {
        int bx = (p <= 32) ? p : p - 32;
        int by = (p <= 32) ? 4 : 5;
        leds[XY(bx, by)] = couleurPiste;
      }
    }
    FastLED.show();
    delay(250);

    FastLED.clear();
    dessinerViesVert(viesVert);
    dessinerViesRouge(viesRouge);
    // LED fantome de la vie perdue
    if (vertAPerdu) leds[XY(1 + viesVert, 1)] = couleurVie;
    else leds[XY(32 - viesRouge, 8)] = couleurVie;
    if (afficherPiste) {
      for (int p = 1; p <= 64; p++) {
        int bx = (p <= 32) ? p : p - 32;
        int by = (p <= 32) ? 4 : 5;
        leds[XY(bx, by)] = couleurPiste;
      }
    }
    FastLED.show();
    delay(350);
  }
}

// --- ANIMATION PERTE DE VIE (SOLO) ---
void animationPerteVieSolo(int viesVert) {
  declencherBip(300, 600);
  CRGB couleurVie = CRGB(127, 10, 73);
  for (int i = 0; i < 3; i++) {
    FastLED.clear();
    dessinerViesVert(viesVert);
    FastLED.show();
    delay(250);

    FastLED.clear();
    dessinerViesVert(viesVert);
    leds[XY(1 + viesVert, 1)] = couleurVie;
    FastLED.show();
    delay(350);
  }
}

// --- SONS DE JEU PARTAGES ---

void sonTir() {
  declencherBipDouble(600, 1200, 40, 60);
}

void sonInterception() {
  declencherBipDouble(1000, 400, 60, 80);
}

void sonBut() {
  declencherBipDouble(1500, 2000, 80, 120);
}
