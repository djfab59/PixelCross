#include "score7seg.h"

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

// Fonction pour gerer les nombres a virgule
void afficherScoreDecimal7Seg(float scoreVert, float scoreRouge, int decimales) {
  // Multiplicateur entier pour transformer le float en int (ex: 9.99 avec 2 decimales -> 999)
  int multiplicateur = 1;
  for (int i = 0; i < decimales; i++) multiplicateur *= 10;

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
