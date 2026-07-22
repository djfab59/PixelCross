#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "game.h"
#include "goal.h"
#include <Preferences.h>

// --- ETATS DU JEU GOAL ---
enum GoalState {
  GOAL_SUBMENU,     // Sous-menu SOLO / DUO
  GOAL_ATTENTE,     // Verrous + attente de demarrage
  GOAL_PLAYING,     // En jeu (chrono actif)
  GOAL_FIN          // Fin de partie (clignotement)
};
static GoalState goalState = GOAL_SUBMENU;

// --- SOUS-MENU ---
static int subMenuIndex = 0; // 0=SOLO, 1=DUO
static bool subPrevG = HIGH;
static bool subPrevG1 = HIGH;
static bool subPrevG2 = HIGH;
static bool subPrevExitCombo = HIGH;

// --- MODE ---
static bool modeSolo = false;

// --- VARIABLES DE JEU ---
static int posVert = 16;
static int posRouge = 16;
static int ballonVertX = 0;
static int ballonVertY = 0;
static int ballonRougeX = 0;
static int ballonRougeY = 0;
static bool ballonVertEnVol = false;
static bool ballonRougeEnVol = false;

// --- MUR ---
static int trouPos = 2;
static int trouDirection = 1;

// --- SCORE ET CHRONO ---
static int scoreVert = 0;
static int scoreRouge = 0;
static int premierButJoueur = 0;
static unsigned long chronoDepart = 0;
static const unsigned long DUREE_PARTIE = 120000; // 120 secondes

// --- TEMPO ---
static unsigned long tempoBase = 500;
static const unsigned long tempoMin = 50; // Plus rapide en fin de partie pour mieux sentir l'acceleration
static unsigned long dernierTickMur = 0;
static unsigned long dernierTickBalle = 0;
static unsigned long dernierTickJoueur = 0;

// --- VERROUS ---
static bool verrouVert = true;
static bool verrouRouge = true;
static bool prevBtnVert = HIGH;
static bool prevBtnRouge = HIGH;
static bool prevBtnG1 = HIGH;
static bool prevBtnG2 = HIGH;
static bool prevBtnR1 = HIGH;
static bool prevBtnR2 = HIGH;
static bool prevExitCombo = HIGH;

// --- HIGHSCORES ---
static int highscoreSolo = 0;
static int highscoreDuo = 0;

// --- FIN DE PARTIE ---
static bool dernierMatchGagne = false;

static void chargerHighscores() {
  Preferences prefs;
  prefs.begin("pixelcross", true);
  highscoreSolo = prefs.getInt("hs_goal_s", 0);
  highscoreDuo = prefs.getInt("hs_goal_d", 0);
  prefs.end();
}

static void sauvegarderHighscore(bool solo) {
  Preferences prefs;
  prefs.begin("pixelcross", false);
  if (solo) prefs.putInt("hs_goal_s", highscoreSolo);
  else prefs.putInt("hs_goal_d", highscoreDuo);
  prefs.end();
}

// --- CALCUL DU TEMPO COURANT ---
static unsigned long tempoActuel() {
  if (goalState != GOAL_PLAYING) return tempoBase;
  unsigned long elapsed = millis() - chronoDepart;
  if (elapsed >= DUREE_PARTIE) return tempoMin;
  return tempoBase - (unsigned long)((tempoBase - tempoMin) * elapsed / DUREE_PARTIE);
}

// --- AFFICHAGE 7 SEGMENTS ---
static void afficher7Seg() {
  unsigned long elapsed = millis() - chronoDepart;
  int restant = (DUREE_PARTIE - (int)elapsed) / 1000;
  if (restant < 0) restant = 0;
  float valVert = restant + scoreVert / 100.0;
  float valRouge = restant + scoreRouge / 100.0;
  if (modeSolo) {
    afficherScoreDecimal7Seg(valVert, valVert, 2);
  } else {
    afficherScoreDecimal7Seg(valVert, valRouge, 2);
  }
}

// --- INITIALISATION ---
void initGoal() {
  chargerHighscores();
  goalState = GOAL_SUBMENU;
  subMenuIndex = 0;
  subPrevG = digitalRead(BTN_GREEN_PIN);
  subPrevG1 = digitalRead(BTN_GREEN1_PIN);
  subPrevG2 = digitalRead(BTN_GREEN2_PIN);
  subPrevExitCombo = HIGH;
  afficherScore7Seg(highscoreSolo, highscoreSolo);
}

static void initPartie() {
  posVert = 16;
  posRouge = 16;
  ballonVertEnVol = false;
  ballonRougeEnVol = false;
  scoreVert = 0;
  scoreRouge = 0;
  premierButJoueur = 0;
  trouPos = 2;
  trouDirection = 1;
  verrouVert = true;
  verrouRouge = modeSolo ? false : true;
  prevBtnVert = digitalRead(BTN_GREEN_PIN);
  prevBtnRouge = digitalRead(BTN_RED_PIN);
  prevBtnG1 = digitalRead(BTN_GREEN1_PIN);
  prevBtnG2 = digitalRead(BTN_GREEN2_PIN);
  prevBtnR1 = digitalRead(BTN_RED1_PIN);
  prevBtnR2 = digitalRead(BTN_RED2_PIN);
  prevExitCombo = HIGH;
  dernierTickMur = 0;
  dernierTickBalle = 0;
  dernierTickJoueur = 0;
  goalState = GOAL_ATTENTE;
}

// --- SOUS-MENU SOLO / DUO ---
static void loopSubMenu() {
  bool actG = digitalRead(BTN_GREEN_PIN);
  bool actG1 = digitalRead(BTN_GREEN1_PIN);
  bool actG2 = digitalRead(BTN_GREEN2_PIN);

  bool clicG = (actG == LOW && subPrevG == HIGH);
  bool clicG1 = (actG1 == LOW && subPrevG1 == HIGH);
  bool clicG2 = (actG2 == LOW && subPrevG2 == HIGH);
  bool exitCombo = (actG1 == LOW && actG2 == LOW);

  if (exitCombo && subPrevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    currentState = STATE_MENU;
    subPrevExitCombo = exitCombo;
    subPrevG = actG; subPrevG1 = actG1; subPrevG2 = actG2;
    return;
  }
  subPrevExitCombo = exitCombo;

  if (clicG1 || clicG2) {
    subMenuIndex = (subMenuIndex == 0) ? 1 : 0;
    declencherBip(500 + subMenuIndex * 100, 30);
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    afficherScore7Seg(hs, hs);
  }

  if (clicG) {
    declencherBip(1000, 100);
    modeSolo = (subMenuIndex == 0);
    initPartie();
  }

  FastLED.clear();
  if (subMenuIndex == 0) drawCenteredString("SOLO", 2, CRGB::Green);
  else drawCenteredString("DUO", 2, CRGB::Green);
  FastLED.show();

  subPrevG = actG; subPrevG1 = actG1; subPrevG2 = actG2;
}

// --- DESSIN DU TERRAIN ---
static void dessinerTerrain() {
  FastLED.clear();

  // Mur (lignes 4 et 5) en bleu, sauf le trou
  for (int x = 1; x <= MATRIX_WIDTH; x++) {
    bool dansTrou = (x >= trouPos && x < trouPos + 3);
    if (!dansTrou) {
      leds[XY(x, 4)] = CRGB(0, 0, 80);
      leds[XY(x, 5)] = CRGB(0, 0, 80);
    }
  }

  // Joueur vert (ligne 8, 3 LEDs)
  for (int i = -1; i <= 1; i++) {
    leds[XY(posVert + i, 8)] = CRGB(0, 80, 0);
  }

  // Joueur rouge (ligne 1, 3 LEDs) — DUO uniquement
  if (!modeSolo) {
    for (int i = -1; i <= 1; i++) {
      leds[XY(posRouge + i, 1)] = CRGB(80, 0, 0);
    }
  }

  // Ballon vert (au pied ou en vol)
  if (!ballonVertEnVol) {
    if (goalState == GOAL_ATTENTE && verrouVert) {
      leds[XY(posVert, 7)] = CRGB::Yellow;
    } else {
      leds[XY(posVert, 7)] = CRGB::Green;
    }
  } else {
    leds[XY(ballonVertX, ballonVertY)] = CRGB::Green;
  }

  // Ballon rouge (au pied ou en vol) — DUO uniquement
  if (!modeSolo) {
    if (!ballonRougeEnVol) {
      if (goalState == GOAL_ATTENTE && verrouRouge) {
        leds[XY(posRouge, 2)] = CRGB::Yellow;
      } else {
        leds[XY(posRouge, 2)] = CRGB::Red;
      }
    } else {
      leds[XY(ballonRougeX, ballonRougeY)] = CRGB::Red;
    }
  }
}

// --- BOUCLE ATTENTE (verrous) ---
static void loopAttente() {
  bool actVert = digitalRead(BTN_GREEN_PIN);
  bool actRouge = digitalRead(BTN_RED_PIN);
  bool clicVert = (actVert == LOW && prevBtnVert == HIGH);
  bool clicRouge = (actRouge == LOW && prevBtnRouge == HIGH);
  prevBtnVert = actVert;
  prevBtnRouge = actRouge;

  // Gestion du retour au sous-menu
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    goalState = GOAL_SUBMENU;
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    afficherScore7Seg(hs, hs);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Deverrouillage
  if (verrouVert && clicVert) {
    verrouVert = false;
    declencherBip(600, 30);
  }
  if (!modeSolo && verrouRouge && clicRouge) {
    verrouRouge = false;
    declencherBip(600, 30);
  }

  // Des que les verrous necessaires sont retires, la partie demarre
  if (!verrouVert && !verrouRouge) {
    declencherBip(1000, 50);
    chronoDepart = millis();
    dernierTickMur = millis();
    dernierTickBalle = millis();
    dernierTickJoueur = millis();
    goalState = GOAL_PLAYING;
  }

  dessinerTerrain();
  FastLED.show();
}

// --- BOUCLE DE JEU PRINCIPALE ---
static void loopPlaying() {
  unsigned long now = millis();
  unsigned long tempo = tempoActuel();

  // --- VERIFIER FIN DU CHRONO ---
  if (now - chronoDepart >= DUREE_PARTIE) {
    if (modeSolo) {
      // SOLO : comparer au highscore
      bool nouveauRecord = (scoreVert > highscoreSolo);
      if (nouveauRecord) {
        highscoreSolo = scoreVert;
        sauvegarderHighscore(true);
        animationNouveauRecord(highscoreSolo);
        dernierMatchGagne = true;
      } else {
        dernierMatchGagne = false;
      }
      animationSetGagnant(dernierMatchGagne);
    } else {
      // DUO : total des buts pour le highscore
      int totalButs = scoreVert + scoreRouge;
      bool nouveauRecord = (totalButs > highscoreDuo);
      if (nouveauRecord) {
        highscoreDuo = totalButs;
        sauvegarderHighscore(false);
        animationNouveauRecord(highscoreDuo);
      }

      bool vertGagne;
      if (scoreVert != scoreRouge) {
        vertGagne = (scoreVert > scoreRouge);
      } else {
        vertGagne = (premierButJoueur == -1);
      }
      animationSetGagnant(vertGagne);
      dernierMatchGagne = vertGagne;
    }

    jouerFanfare();
    if (modeSolo) {
      afficherScore7Seg(scoreVert, scoreVert);
    } else {
      afficherScore7Seg(scoreVert, scoreRouge);
    }
    goalState = GOAL_FIN;
    return;
  }

  // --- GESTION DU RETOUR ---
  bool btnG1state = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2state = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1state && btnG2state);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    goalState = GOAL_SUBMENU;
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    afficherScore7Seg(hs, hs);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // --- LECTURE DES BOUTONS ---
  bool actVert = digitalRead(BTN_GREEN_PIN);
  bool actRouge = digitalRead(BTN_RED_PIN);
  bool actG1 = digitalRead(BTN_GREEN1_PIN);
  bool actG2 = digitalRead(BTN_GREEN2_PIN);
  bool actR1 = digitalRead(BTN_RED1_PIN);
  bool actR2 = digitalRead(BTN_RED2_PIN);

  bool clicVert = (actVert == LOW && prevBtnVert == HIGH);
  bool clicRouge = (actRouge == LOW && prevBtnRouge == HIGH);

  prevBtnVert = actVert;
  prevBtnRouge = actRouge;
  prevBtnG1 = actG1;
  prevBtnG2 = actG2;
  prevBtnR1 = actR1;
  prevBtnR2 = actR2;

  // --- DEPLACEMENT JOUEURS (1.5x plus rapide que le mur/balle) ---
  unsigned long tempoJoueur = tempo * 2 / 3; // tempo / 1.5
  if (now - dernierTickJoueur >= tempoJoueur) {
    bool bouge = false;
    if (actG1 == LOW && posVert > 2) { posVert--; bouge = true; }
    if (actG2 == LOW && posVert < 31) { posVert++; bouge = true; }
    if (!modeSolo) {
      if (actR1 == LOW && posRouge > 2) { posRouge--; bouge = true; }
      if (actR2 == LOW && posRouge < 31) { posRouge++; bouge = true; }
    }
    if (bouge) dernierTickJoueur = now;
  }

  // --- TIR ---
  if (clicVert && !ballonVertEnVol) {
    ballonVertEnVol = true;
    ballonVertX = posVert;
    ballonVertY = 6;
    sonTir();
  }
  if (!modeSolo && clicRouge && !ballonRougeEnVol) {
    ballonRougeEnVol = true;
    ballonRougeX = posRouge;
    ballonRougeY = 3;
    sonTir();
  }

  // --- DEPLACEMENT DU MUR ---
  if (now - dernierTickMur >= tempo) {
    dernierTickMur = now;
    trouPos += trouDirection;
    if (trouPos + 2 > MATRIX_WIDTH - 1) { trouPos = MATRIX_WIDTH - 3; trouDirection = -1; }
    if (trouPos < 2) { trouPos = 2; trouDirection = 1; }
  }

  // --- DEPLACEMENT DES BALLONS ---
  if (now - dernierTickBalle >= tempo) {
    dernierTickBalle = now;

    // Ballon vert (monte vers ligne 1)
    if (ballonVertEnVol) {
      ballonVertY--;

      if (ballonVertY == 5 || ballonVertY == 4) {
        bool dansTrou = (ballonVertX >= trouPos && ballonVertX < trouPos + 3);
        if (!dansTrou) {
          ballonVertEnVol = false;
          sonInterception();
        }
      }

      // Collision avec le gardien rouge (ligne 1) — DUO uniquement
      if (!modeSolo && ballonVertEnVol && ballonVertY == 1) {
        bool bloque = (ballonVertX >= posRouge - 1 && ballonVertX <= posRouge + 1);
        if (bloque) {
          ballonVertEnVol = false;
          sonInterception();
        }
      }

      // BUT pour le vert (ballon sorti au-dela de la ligne 1)
      if (ballonVertEnVol && ballonVertY < 1) {
        ballonVertEnVol = false;
        scoreVert++;
        if (premierButJoueur == 0) premierButJoueur = -1;
        sonBut();
      }
    }

    // Ballon rouge (descend vers ligne 8) — DUO uniquement
    if (!modeSolo && ballonRougeEnVol) {
      ballonRougeY++;

      if (ballonRougeY == 4 || ballonRougeY == 5) {
        bool dansTrou = (ballonRougeX >= trouPos && ballonRougeX < trouPos + 3);
        if (!dansTrou) {
          ballonRougeEnVol = false;
          sonInterception();
        }
      }

      if (ballonRougeEnVol && ballonRougeY == 8) {
        bool bloque = (ballonRougeX >= posVert - 1 && ballonRougeX <= posVert + 1);
        if (bloque) {
          ballonRougeEnVol = false;
          sonInterception();
        }
      }

      // BUT pour le rouge (ballon sorti au-dela de la ligne 8)
      if (ballonRougeEnVol && ballonRougeY > 8) {
        ballonRougeEnVol = false;
        scoreRouge++;
        if (premierButJoueur == 0) premierButJoueur = 1;
        sonBut();
      }
    }

    // Collision ballon vs ballon — DUO uniquement
    if (!modeSolo && ballonVertEnVol && ballonRougeEnVol) {
      if (ballonVertX == ballonRougeX && ballonVertY == ballonRougeY) {
        ballonVertEnVol = false;
        ballonRougeEnVol = false;
        sonInterception();
      }
      else if (ballonVertX == ballonRougeX && abs(ballonVertY - ballonRougeY) == 1) {
        ballonVertEnVol = false;
        ballonRougeEnVol = false;
        sonInterception();
      }
    }
  }

  // --- AFFICHAGE ---
  dessinerTerrain();
  FastLED.show();
  afficher7Seg();
}

// --- BOUCLE FIN DE PARTIE ---
static void loopFin() {
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    goalState = GOAL_SUBMENU;
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    afficherScore7Seg(hs, hs);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  bool actVert = digitalRead(BTN_GREEN_PIN);
  bool actRouge = digitalRead(BTN_RED_PIN);
  bool clicVert = (actVert == LOW && prevBtnVert == HIGH);
  bool clicRouge = (actRouge == LOW && prevBtnRouge == HIGH);
  prevBtnVert = actVert;
  prevBtnRouge = actRouge;

  CRGB couleurDouce = dernierMatchGagne ? CRGB(0, 40, 0) : CRGB(40, 0, 0);
  if (millis() % 1000 < 500) {
    for (int i = 0; i < NUM_LEDS; i++) leds[i] = couleurDouce;
  } else {
    FastLED.clear();
  }
  FastLED.show();

  if (clicVert || clicRouge) {
    declencherBip(2000, 30);
    initPartie();
  }
}

// --- POINT D'ENTREE ---
void loopGoal() {
  switch (goalState) {
    case GOAL_SUBMENU: loopSubMenu(); break;
    case GOAL_ATTENTE: loopAttente(); break;
    case GOAL_PLAYING: loopPlaying(); break;
    case GOAL_FIN:     loopFin(); break;
  }
}
