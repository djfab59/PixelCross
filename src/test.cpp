#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "test.h"

static bool prevExitCombo = HIGH;
static int testScoreVert = 0;
static int testScoreRouge = 0;

static bool prevG  = HIGH;
static bool prevG1 = HIGH;
static bool prevG2 = HIGH;
static bool prevR  = HIGH;
static bool prevR1 = HIGH;
static bool prevR2 = HIGH;

void initTest() {
  prevExitCombo = HIGH;
  invertText = false; // S'assure que le texte est a l'endroit au lancement
  testScoreVert = 0;
  testScoreRouge = 0;

  // Memorisation de l'etat initial des boutons
  prevG  = digitalRead(BTN_GREEN_PIN);
  prevG1 = digitalRead(BTN_GREEN1_PIN);
  prevG2 = digitalRead(BTN_GREEN2_PIN);
  prevR  = digitalRead(BTN_RED_PIN);
  prevR1 = digitalRead(BTN_RED1_PIN);
  prevR2 = digitalRead(BTN_RED2_PIN);

  // On affiche les scores avec 1 decimale (format XX.X)
  afficherScoreDecimal7Seg((float)testScoreVert / 10.0, (float)testScoreRouge / 10.0, 1);
}

void loopTest() {
  bool actG  = digitalRead(BTN_GREEN_PIN);
  bool actG1 = digitalRead(BTN_GREEN1_PIN);
  bool actG2 = digitalRead(BTN_GREEN2_PIN);
  bool actR  = digitalRead(BTN_RED_PIN);
  bool actR1 = digitalRead(BTN_RED1_PIN);
  bool actR2 = digitalRead(BTN_RED2_PIN);

  bool btnG  = (actG == LOW);
  bool btnG1 = (actG1 == LOW);
  bool btnG2 = (actG2 == LOW);
  bool btnR  = (actR == LOW);
  bool btnR1 = (actR1 == LOW);
  bool btnR2 = (actR2 == LOW);
  
  // Detection des clics uniques (front descendant)
  bool clicG  = (actG == LOW && prevG == HIGH);
  bool clicG1 = (actG1 == LOW && prevG1 == HIGH);
  bool clicG2 = (actG2 == LOW && prevG2 == HIGH);
  bool clicR  = (actR == LOW && prevR == HIGH);
  bool clicR1 = (actR1 == LOW && prevR1 == HIGH);
  bool clicR2 = (actR2 == LOW && prevR2 == HIGH);
  
  // Pour quitter le menu test, on appuie sur Vert 1 + Vert 2 simultanement
  bool exitCombo = (btnG1 && btnG2);

  FastLED.clear();

  if (exitCombo) {
    drawCenteredString("QUIT", 2, CRGB::White);
    if (prevExitCombo == HIGH) {
      declencherBip(800, 50);
      afficherScore7Seg(-1, -1); // Eteint les afficheurs
      currentState = STATE_MENU;
    }
  } else {
    // --- LOGIQUE D'INCREMENTATION DES AFFICHEURS ---
    bool scoreUpdated = false;
    if (clicG)  { testScoreVert = (testScoreVert + 100) % 10000; scoreUpdated = true; } // +10.0
    if (clicG1) { testScoreVert = (testScoreVert + 1)   % 10000; scoreUpdated = true; } // +0.1
    if (clicG2) { testScoreVert = (testScoreVert + 10)  % 10000; scoreUpdated = true; } // +1.0
    if (clicR)  { testScoreRouge = (testScoreRouge + 100) % 10000; scoreUpdated = true; } // +10.0
    if (clicR1) { testScoreRouge = (testScoreRouge + 1)   % 10000; scoreUpdated = true; } // +0.1
    if (clicR2) { testScoreRouge = (testScoreRouge + 10)  % 10000; scoreUpdated = true; } // +1.0

    if (scoreUpdated) {
      afficherScoreDecimal7Seg((float)testScoreVert / 10.0, (float)testScoreRouge / 10.0, 1);
      declencherBip(1500, 20);
    }

    // --- AFFICHAGE SUR LA MATRICE LED ---
    // On affiche un message pour chaque bouton presse
    if (btnG) {
      drawCenteredString("VERT", 2, CRGB::Green);
    } else if (btnG1) {
      drawCenteredString("VERT 1", 2, CRGB::Green);
    } else if (btnG2) {
      drawCenteredString("VERT 2", 2, CRGB::Green);
    } else if (btnR) {
      invertText = true; drawCenteredString("ROUGE", 2, CRGB::Red); invertText = false;
    } else if (btnR1) {
      invertText = true; drawCenteredString("ROUGE 1", 2, CRGB::Red); invertText = false;
    } else if (btnR2) {
      invertText = true; drawCenteredString("ROUGE 2", 2, CRGB::Red); invertText = false;
    } else {
      // Mode attente (Idle) du menu test : allume les 4 coins
      leds[XY(1, 1)] = CRGB::Red;
      leds[XY(32, 1)] = CRGB::Green;
      leds[XY(1, 8)] = CRGB::Blue;
      leds[XY(32, 8)] = CRGB::Yellow;
    }
  }
  FastLED.show();

  prevExitCombo = exitCombo;
  prevG  = actG;
  prevG1 = actG1;
  prevG2 = actG2;
  prevR  = actR;
  prevR1 = actR1;
  prevR2 = actR2;
}