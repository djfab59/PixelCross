#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "game.h"
#include "goal.h"
#include <Preferences.h>

// --- ETATS DU JEU GOAL ---
enum GoalState {
  GOAL_ATTENTE,     // Verrous + attente de demarrage
  GOAL_PLAYING,     // En jeu (chrono actif)
  GOAL_FIN          // Fin de partie (clignotement)
};
static GoalState goalState = GOAL_ATTENTE;

// --- VARIABLES DE JEU ---
static int posVert = 16;        // Position X du centre du joueur vert (ligne 8)
static int posRouge = 16;       // Position X du centre du joueur rouge (ligne 1)
static int ballonVertX = 0;     // Position X du ballon vert en vol (0 = pas en vol)
static int ballonVertY = 0;     // Position Y du ballon vert en vol
static int ballonRougeX = 0;    // Position X du ballon rouge en vol
static int ballonRougeY = 0;    // Position Y du ballon rouge en vol
static bool ballonVertEnVol = false;
static bool ballonRougeEnVol = false;

// --- MUR ---
static int trouPos = 2;         // Position X du debut du trou (3 LEDs de large, commence a 2)
static int trouDirection = 1;   // 1 = vers la droite, -1 = vers la gauche

// --- SCORE ET CHRONO ---
static int scoreVert = 0;
static int scoreRouge = 0;
static int premierButJoueur = 0; // -1=vert, 1=rouge, 0=personne
static unsigned long chronoDepart = 0;
static const int DUREE_PARTIE = 60000; // 60 secondes en ms

// --- TEMPO ---
static unsigned long tempoBase = 500;  // ms (debut)
static const unsigned long tempoMin = 150; // ms (fin)
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

// --- HIGHSCORE ---
static int highscore = 0;

// --- FIN DE PARTIE ---
static bool dernierMatchGagne = false; // Pour clignotement

static void chargerHighscore() {
  Preferences prefs;
  prefs.begin("pixelcross", true);
  highscore = prefs.getInt("hs_goal", 0);
  prefs.end();
}

static void sauvegarderHighscore() {
  Preferences prefs;
  prefs.begin("pixelcross", false);
  prefs.putInt("hs_goal", highscore);
  prefs.end();
}

// --- CALCUL DU TEMPO COURANT ---
static unsigned long tempoActuel() {
  if (goalState != GOAL_PLAYING) return tempoBase;
  unsigned long elapsed = millis() - chronoDepart;
  if (elapsed >= DUREE_PARTIE) return tempoMin;
  // Interpolation lineaire de tempoBase vers tempoMin
  return tempoBase - (unsigned long)((tempoBase - tempoMin) * elapsed / DUREE_PARTIE);
}

// --- AFFICHAGE 7 SEGMENTS : MM.SS format chrono + score ---
static void afficher7Seg() {
  unsigned long elapsed = millis() - chronoDepart;
  int restant = (DUREE_PARTIE - (int)elapsed) / 1000;
  if (restant < 0) restant = 0;

  // Format : les 2 premiers digits = secondes restantes, point, 2 derniers = score
  // Ex: "45.02" = 45 secondes restantes, 2 buts
  float valVert = restant + scoreVert / 100.0;
  float valRouge = restant + scoreRouge / 100.0;
  afficherScoreDecimal7Seg(valVert, valRouge, 2);
}

// --- INITIALISATION ---
void initGoal() {
  chargerHighscore();
  goalState = GOAL_ATTENTE;
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
  verrouRouge = true;
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

  // Affiche le highscore
  if (highscore > 0) afficherScore7Seg(highscore, highscore);
  else afficherScore7Seg(0, 0);
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

  // Joueur rouge (ligne 1, 3 LEDs)
  for (int i = -1; i <= 1; i++) {
    leds[XY(posRouge + i, 1)] = CRGB(80, 0, 0);
  }

  // Ballon vert (au pied ou en vol)
  if (!ballonVertEnVol) {
    if (goalState == GOAL_ATTENTE && verrouVert) {
      leds[XY(posVert, 7)] = CRGB::Yellow; // Lock = jaune
    } else {
      leds[XY(posVert, 7)] = CRGB::Green;
    }
  } else {
    leds[XY(ballonVertX, ballonVertY)] = CRGB::Green;
  }

  // Ballon rouge (au pied ou en vol)
  if (!ballonRougeEnVol) {
    if (goalState == GOAL_ATTENTE && verrouRouge) {
      leds[XY(posRouge, 2)] = CRGB::Yellow; // Lock = jaune
    } else {
      leds[XY(posRouge, 2)] = CRGB::Red;
    }
  } else {
    leds[XY(ballonRougeX, ballonRougeY)] = CRGB::Red;
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

  // Gestion du retour au menu
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    currentState = STATE_MENU;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Deverrouillage
  if (verrouVert && clicVert) {
    verrouVert = false;
    declencherBip(600, 30);
  }
  if (verrouRouge && clicRouge) {
    verrouRouge = false;
    declencherBip(600, 30);
  }

  // Des que les deux sont deverrouilles, la partie demarre
  if (!verrouVert && !verrouRouge) {
    declencherBip(1000, 50);
    chronoDepart = millis();
    dernierTickMur = millis();
    dernierTickBalle = millis();
    dernierTickJoueur = millis();
    goalState = GOAL_PLAYING;
  }

  // Dessin
  dessinerTerrain();
  FastLED.show();
}

// --- BOUCLE DE JEU PRINCIPALE ---
static void loopPlaying() {
  unsigned long now = millis();
  unsigned long tempo = tempoActuel();

  // --- VERIFIER FIN DU CHRONO ---
  if (now - chronoDepart >= DUREE_PARTIE) {
    // Fin de partie
    int totalButs = scoreVert + scoreRouge;
    bool nouveauRecord = (totalButs > highscore);
    if (nouveauRecord) {
      highscore = totalButs;
      sauvegarderHighscore();
      animationNouveauRecord(highscore);
    }

    // Determiner le vainqueur
    bool vertGagne;
    if (scoreVert != scoreRouge) {
      vertGagne = (scoreVert > scoreRouge);
    } else {
      // Egalite : celui qui a marque le premier gagne
      vertGagne = (premierButJoueur == -1);
    }

    animationSetGagnant(vertGagne);
    jouerFanfare();
    afficherScore7Seg(scoreVert, scoreRouge);
    dernierMatchGagne = vertGagne;
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
    currentState = STATE_MENU;
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
  bool clicG1 = (actG1 == LOW && prevBtnG1 == HIGH);
  bool clicG2 = (actG2 == LOW && prevBtnG2 == HIGH);
  bool clicR1 = (actR1 == LOW && prevBtnR1 == HIGH);
  bool clicR2 = (actR2 == LOW && prevBtnR2 == HIGH);

  prevBtnVert = actVert;
  prevBtnRouge = actRouge;
  prevBtnG1 = actG1;
  prevBtnG2 = actG2;
  prevBtnR1 = actR1;
  prevBtnR2 = actR2;

  // --- DEPLACEMENT JOUEURS (sur front descendant, limite par le tempo) ---
  if (now - dernierTickJoueur >= tempo) {
    bool bouge = false;
    if (actG1 == LOW && posVert > 2) { posVert--; bouge = true; }
    if (actG2 == LOW && posVert < 31) { posVert++; bouge = true; }
    if (actR1 == LOW && posRouge > 2) { posRouge--; bouge = true; }
    if (actR2 == LOW && posRouge < 31) { posRouge++; bouge = true; }
    if (bouge) dernierTickJoueur = now;
  }

  // --- TIR ---
  if (clicVert && !ballonVertEnVol) {
    ballonVertEnVol = true;
    ballonVertX = posVert;
    ballonVertY = 6; // Premiere position en vol (au-dessus de la ligne 7)
    sonTir();
  }
  if (clicRouge && !ballonRougeEnVol) {
    ballonRougeEnVol = true;
    ballonRougeX = posRouge;
    ballonRougeY = 3; // Premiere position en vol (en-dessous de la ligne 2)
    sonTir();
  }

  // --- DEPLACEMENT DU MUR (au tempo) ---
  if (now - dernierTickMur >= tempo) {
    dernierTickMur = now;
    trouPos += trouDirection;
    // Rebond aux bords (le trou fait 3 LEDs, bornes: 2 a MATRIX_WIDTH-3)
    if (trouPos + 2 > MATRIX_WIDTH - 1) { trouPos = MATRIX_WIDTH - 3; trouDirection = -1; }
    if (trouPos < 2) { trouPos = 2; trouDirection = 1; }
  }

  // --- DEPLACEMENT DES BALLONS (au tempo) ---
  if (now - dernierTickBalle >= tempo) {
    dernierTickBalle = now;

    // Ballon vert (monte de ligne 6 vers ligne 1)
    if (ballonVertEnVol) {
      ballonVertY--;

      // Collision avec le mur (lignes 4 et 5)
      if (ballonVertY == 5 || ballonVertY == 4) {
        bool dansTrou = (ballonVertX >= trouPos && ballonVertX < trouPos + 3);
        if (!dansTrou) {
          // Intercepte par le mur
          ballonVertEnVol = false;
          sonInterception();
        }
      }

      // Collision avec le gardien rouge (ligne 1)
      if (ballonVertEnVol && ballonVertY == 1) {
        bool bloque = (ballonVertX >= posRouge - 1 && ballonVertX <= posRouge + 1);
        if (bloque) {
          ballonVertEnVol = false;
          sonInterception();
        } else {
          // BUT pour le vert !
          ballonVertEnVol = false;
          scoreVert++;
          if (premierButJoueur == 0) premierButJoueur = -1;
          sonBut();
        }
      }

      // Sorti de l'ecran (ne devrait pas arriver avec la logique ci-dessus)
      if (ballonVertEnVol && ballonVertY < 1) {
        ballonVertEnVol = false;
      }
    }

    // Ballon rouge (descend de ligne 3 vers ligne 8)
    if (ballonRougeEnVol) {
      ballonRougeY++;

      // Collision avec le mur (lignes 4 et 5)
      if (ballonRougeY == 4 || ballonRougeY == 5) {
        bool dansTrou = (ballonRougeX >= trouPos && ballonRougeX < trouPos + 3);
        if (!dansTrou) {
          // Intercepte par le mur
          ballonRougeEnVol = false;
          sonInterception();
        }
      }

      // Collision avec le gardien vert (ligne 8)
      if (ballonRougeEnVol && ballonRougeY == 8) {
        bool bloque = (ballonRougeX >= posVert - 1 && ballonRougeX <= posVert + 1);
        if (bloque) {
          ballonRougeEnVol = false;
          sonInterception();
        } else {
          // BUT pour le rouge !
          ballonRougeEnVol = false;
          scoreRouge++;
          if (premierButJoueur == 0) premierButJoueur = 1;
          sonBut();
        }
      }

      // Sorti de l'ecran
      if (ballonRougeEnVol && ballonRougeY > 8) {
        ballonRougeEnVol = false;
      }
    }

    // Collision ballon vs ballon (telescopage sur la meme case OU croisement)
    if (ballonVertEnVol && ballonRougeEnVol) {
      // Meme case
      if (ballonVertX == ballonRougeX && ballonVertY == ballonRougeY) {
        ballonVertEnVol = false;
        ballonRougeEnVol = false;
        sonInterception();
      }
      // Croisement : le vert etait en Y+1 et le rouge en Y-1 avant ce tick (meme colonne)
      // Apres ce tick le vert est en Y et le rouge en Y+1 (ou inversement)
      // Detection simple : meme colonne et distance verticale de 1 avec directions opposees
      else if (ballonVertX == ballonRougeX && abs(ballonVertY - ballonRougeY) == 1) {
        // Ils se sont croises si le vert monte (Y diminue) et le rouge descend (Y augmente)
        // Vert allait de ballonVertY+1 vers ballonVertY, Rouge allait de ballonRougeY-1 vers ballonRougeY
        // Ils se croisent si ballonVertY+1 == ballonRougeY-1+1 => toujours quand distance == 1 sur meme colonne
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
  // Gestion du retour au menu
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    currentState = STATE_MENU;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Clignotement en attendant un appui pour relancer
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
    // Relance une partie
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
    verrouRouge = true;
    goalState = GOAL_ATTENTE;
  }
}

// --- POINT D'ENTREE ---
void loopGoal() {
  switch (goalState) {
    case GOAL_ATTENTE: loopAttente(); break;
    case GOAL_PLAYING: loopPlaying(); break;
    case GOAL_FIN:     loopFin(); break;
  }
}
