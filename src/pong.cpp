#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "pong.h"
#include <Preferences.h>

// --- ETATS DU PONG ---
enum PongState {
  PONG_SUBMENU,   // Sous-menu SOLO / DUO
  PONG_PLAYING    // En jeu
};
static PongState pongState = PONG_SUBMENU;

// --- SOUS-MENU ---
static int subMenuIndex = 0; // 0=SOLO, 1=DUO
static bool subPrevG = HIGH;
static bool subPrevG1 = HIGH;
static bool subPrevG2 = HIGH;
static bool subPrevExitCombo = HIGH;

// --- MODE DE JEU ---
static bool modeSolo = false;

// --- VARIABLES COMMUNES ---
static int posX = 16;
static const int TRACK_LENGTH = 64;
static int direction = 1;
static unsigned long timerMouvement = 0;
static int vitesseJeu = 80;
static const int vitesseMax = 10;
static bool enAttenteEngagement = true;
static int joueurEngagement = -1;
static bool etatPrecedentVert = HIGH;
static bool etatPrecedentRouge = HIGH;
static int viesVert = 3;
static int viesRouge = 3;
static bool partieTerminee = false;
static int tailleRaquette = 5;
static int compteurEchangesMax = 0;
static int pouvoirVert = 0;
static int pouvoirRouge = 0;
static int dashRestant = 0;
static int invisibiliteRestante = 0;
static bool verrouService = true;
static bool prevExitCombo = HIGH;
static int victoiresVert = 0;
static int victoiresRouge = 0;
static bool dernierMatchGagne = false; // Pour le clignotement fin de partie en solo

// Variables pour l'animation non-bloquante du verrou de service
static bool alerteVerrouActive = false;
static unsigned long timerAlerteVerrou = 0;
static int compteurAlerteVerrou = 0;
const int DUREE_CLIGNOTEMENT_VERROU = 150;

// --- COMPTEUR D'ECHANGES ET HIGHSCORE ---
static int compteurEchanges = 0;        // Score courant (renvois cumules)
static int highscoreSolo = 0;           // Meilleur score solo (sauvegarde NVS)
static int highscoreDuo = 0;            // Meilleur score duo (sauvegarde NVS)

// --- CHARGEMENT / SAUVEGARDE HIGHSCORES ---
static void chargerHighscores() {
  Preferences prefs;
  prefs.begin("pixelcross", true); // lecture seule
  highscoreSolo = prefs.getInt("hs_solo", 0);
  highscoreDuo = prefs.getInt("hs_duo", 0);
  prefs.end();
}

static void sauvegarderHighscore(bool solo) {
  Preferences prefs;
  prefs.begin("pixelcross", false);
  if (solo) prefs.putInt("hs_solo", highscoreSolo);
  else prefs.putInt("hs_duo", highscoreDuo);
  prefs.end();
}

// --- ANIMATION NOUVEAU RECORD ---
static void animationNouveauRecord() {
  // Clignotement bleu/dore rapide de toute la matrice + clignotement du score sur 7 segments
  int score = modeSolo ? highscoreSolo : highscoreDuo;
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

// --- FONCTION FACTORISEE POUR GERER LA FRAPPE D'UN JOUEUR ---
// posMin/posMax : bornes de la zone de raquette
// ledMur : position de la LED la plus eloignee du centre (ou on obtient le pouvoir)
// ledCentre : position de la LED la plus proche du centre (ou on detruit le pouvoir adverse)
static void gererFrappe(int posMin, int posMax, int ledMur, int ledCentre, int& pouvoirJoueur, int& pouvoirAdverse, int nouvelleDirection) {
  bool frappeValide = (posX >= posMin && posX <= posMax);
  bool pouvoirObtenu = false;
  bool pouvoirDetruit = false;

  if (frappeValide) {
    if (posX == ledMur) pouvoirObtenu = true;
    if (posX == ledCentre && pouvoirAdverse > 0) pouvoirDetruit = true;
  }

  int vitesseFuture = vitesseJeu;
  int compteurFuture = compteurEchangesMax;
  if (frappeValide) {
    if (vitesseFuture > vitesseMax) vitesseFuture -= 5;
    else compteurFuture++;
  }
  unsigned int freqRebond = 1000 + (((80 - vitesseFuture) / 5) + compteurFuture) * 50;

  if (pouvoirDetruit) {
    declencherBipDouble(2500, 1500, 60, 80);
  } else if (pouvoirObtenu) {
    declencherBipDouble(1500, 2500, 60, 80);
  } else {
    declencherBip(freqRebond, 30);
  }

  if (frappeValide) {
    if (pouvoirObtenu) pouvoirJoueur = random(1, 4);
    if (pouvoirDetruit) pouvoirAdverse = 0;

    direction = nouvelleDirection;
    if (vitesseJeu > vitesseMax) {
      vitesseJeu -= 5;
    } else {
      compteurEchangesMax++;
      if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
    }
    // Incrementer le compteur d'echanges
    compteurEchanges++;
    afficherScore7Seg(compteurEchanges, compteurEchanges);
  } else {
    posX = (nouvelleDirection == 1) ? 0 : TRACK_LENGTH + 1;
    timerMouvement = 0;
  }
}

// --- FRAPPE DU BOT (SOLO) ---
// Le bot renvoie toujours quand la balle atteint la derniere LED de son cote
static void gererFrappeBot() {
  // Le bot frappe toujours au bon moment, pas de pouvoir
  direction = -1; // Renvoie vers le vert
  // On incremente le compteur et on accelere comme un renvoi normal
  if (vitesseJeu > vitesseMax) {
    vitesseJeu -= 5;
  } else {
    compteurEchangesMax++;
    if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
  }
  compteurEchanges++;
  afficherScore7Seg(compteurEchanges, compteurEchanges);
  
  unsigned int freqRebond = 1000 + (((80 - vitesseJeu) / 5) + compteurEchangesMax) * 50;
  declencherBip(freqRebond, 30);
}

// --- INITIALISATION ---
void initPong() {
  chargerHighscores();
  pongState = PONG_SUBMENU;
  subMenuIndex = 0;
  subPrevG = digitalRead(BTN_GREEN_PIN);
  subPrevG1 = digitalRead(BTN_GREEN1_PIN);
  subPrevG2 = digitalRead(BTN_GREEN2_PIN);
  subPrevExitCombo = HIGH;
  // Affiche le highscore du mode selectionne par defaut (SOLO)
  afficherScore7Seg(highscoreSolo, highscoreSolo);
}

static void initPartie() {
  posX = (joueurEngagement == -1) ? 16 : 48;
  direction = (joueurEngagement == -1) ? 1 : -1;
  timerMouvement = 0;
  vitesseJeu = 80;
  enAttenteEngagement = true;
  viesVert = 3;
  viesRouge = modeSolo ? 0 : 3; // Pas de vies pour le bot
  partieTerminee = false;
  tailleRaquette = 5;
  compteurEchangesMax = 0;
  pouvoirVert = 0;
  pouvoirRouge = 0;
  dashRestant = 0;
  invisibiliteRestante = 0;
  verrouService = true;
  prevExitCombo = HIGH;
  alerteVerrouActive = false;
  timerAlerteVerrou = 0;
  compteurAlerteVerrou = 0;
  compteurEchanges = 0;

  etatPrecedentVert = digitalRead(BTN_GREEN_PIN);
  etatPrecedentRouge = digitalRead(BTN_RED_PIN);

  if (modeSolo) {
    joueurEngagement = -1; // Toujours le vert qui sert en solo
    afficherScore7Seg(0, 0);
  } else {
    afficherScore7Seg(compteurEchanges, compteurEchanges);
  }
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

  // Retour au menu principal
  if (exitCombo && subPrevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    currentState = STATE_MENU;
    subPrevExitCombo = exitCombo;
    subPrevG = actG; subPrevG1 = actG1; subPrevG2 = actG2;
    return;
  }
  subPrevExitCombo = exitCombo;

  // Navigation
  if (clicG1 || clicG2) {
    subMenuIndex = (subMenuIndex == 0) ? 1 : 0;
    declencherBip(500 + subMenuIndex * 100, 30);
    // Affiche le highscore du mode survole
    if (subMenuIndex == 0) afficherScore7Seg(highscoreSolo, highscoreSolo);
    else afficherScore7Seg(highscoreDuo, highscoreDuo);
  }

  // Validation
  if (clicG) {
    declencherBip(1000, 100);
    modeSolo = (subMenuIndex == 0);
    victoiresVert = 0;
    victoiresRouge = 0;
    joueurEngagement = -1;
    initPartie();
    pongState = PONG_PLAYING;
  }

  // Affichage
  FastLED.clear();
  if (subMenuIndex == 0) drawCenteredString("SOLO", 2, CRGB::Green);
  else drawCenteredString("DUO", 2, CRGB::Green);
  FastLED.show();

  subPrevG = actG; subPrevG1 = actG1; subPrevG2 = actG2;
}

// --- BOUCLE DE JEU PRINCIPALE ---
static void loopPlaying() {
  // --- GESTION DU RETOUR AU SOUS-MENU ---
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);

  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    victoiresVert = 0;
    victoiresRouge = 0;
    joueurEngagement = -1;
    pongState = PONG_SUBMENU;
    // Reaffiche le highscore du mode actuel
    if (subMenuIndex == 0) afficherScore7Seg(highscoreSolo, highscoreSolo);
    else afficherScore7Seg(highscoreDuo, highscoreDuo);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Lecture des boutons
  bool etatActuelVert = digitalRead(BTN_GREEN_PIN);
  bool etatActuelRouge = digitalRead(BTN_RED_PIN);
  bool clicVert = (etatActuelVert == LOW && etatPrecedentVert == HIGH);
  bool clicRouge = (etatActuelRouge == LOW && etatPrecedentRouge == HIGH);
  etatPrecedentVert = etatActuelVert;
  etatPrecedentRouge = etatActuelRouge;

  // --- FIN DE PARTIE ---
  if (partieTerminee) {
    CRGB couleurDouce;
    if (modeSolo) {
      couleurDouce = dernierMatchGagne ? CRGB(0, 40, 0) : CRGB(40, 0, 0);
    } else {
      couleurDouce = (viesVert <= 0) ? CRGB(40, 0, 0) : CRGB(0, 40, 0);
    }
    if (millis() % 1000 < 500) {
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = couleurDouce;
    } else {
      FastLED.clear();
    }
    FastLED.show();

    if (clicVert || clicRouge) {
      declencherBip(2000, 30);
      if (modeSolo) {
        joueurEngagement = -1;
        initPartie();
      } else {
        initPartie();
      }
    }
    return;
  }

  // --- ATTENTE D'ENGAGEMENT ---
  if (enAttenteEngagement) {
    if (modeSolo) {
      // En solo, seul le vert sert (pas de verrou car pas d'adversaire humain)
      if (clicVert) {
        declencherBip(1000, 50);
        enAttenteEngagement = false;
        direction = 1;
        timerMouvement = millis();
      }
    } else {
      // Mode DUO : logique d'engagement avec verrou
      if (joueurEngagement == -1) {
        if (clicRouge) verrouService = false;
        if (clicVert && !alerteVerrouActive) {
          if (verrouService) {
            alerteVerrouActive = true;
            timerAlerteVerrou = millis();
            compteurAlerteVerrou = 0;
          } else {
            declencherBip(1000, 50);
            enAttenteEngagement = false;
            direction = 1;
            timerMouvement = millis();
          }
        }
      } else if (joueurEngagement == 1) {
        if (clicVert) verrouService = false;
        if (clicRouge && !alerteVerrouActive) {
          if (verrouService) {
            alerteVerrouActive = true;
            timerAlerteVerrou = millis();
            compteurAlerteVerrou = 0;
          } else {
            declencherBip(1000, 50);
            enAttenteEngagement = false;
            direction = -1;
            timerMouvement = millis();
          }
        }
      }

      // Animation d'alerte verrou
      if (alerteVerrouActive) {
        if (millis() - timerAlerteVerrou > DUREE_CLIGNOTEMENT_VERROU) {
          timerAlerteVerrou = millis();
          compteurAlerteVerrou++;
          if (compteurAlerteVerrou % 2 != 0) {
            ledcWriteTone(BUZZER_CHANNEL, 150);
            ledcWrite(BUZZER_CHANNEL, 127);
          } else {
            ledcWriteTone(BUZZER_CHANNEL, 0);
            ledcWrite(BUZZER_CHANNEL, 0);
          }
          if (compteurAlerteVerrou >= 6) {
            alerteVerrouActive = false;
            ledcWriteTone(BUZZER_CHANNEL, 0);
            ledcWrite(BUZZER_CHANNEL, 0);
          }
        }
      }
    }

  } else {
    // --- LOGIQUE DE FRAPPE ---
    if (modeSolo) {
      // SOLO : le vert frappe normalement, seul pouvoir 3 est possible
      if (direction == -1 && clicVert) {
        // Frappe du vert (identique a DUO mais pouvoir limite a 3)
        bool frappeValide = (posX >= 1 && posX <= tailleRaquette);
        bool pouvoirObtenu = false;

        if (frappeValide && posX == 1) pouvoirObtenu = true; // LED mur (la plus eloignee du centre)

        int vitesseFuture = vitesseJeu;
        int compteurFuture = compteurEchangesMax;
        if (frappeValide) {
          if (vitesseFuture > vitesseMax) vitesseFuture -= 5;
          else compteurFuture++;
        }
        unsigned int freqRebond = 1000 + (((80 - vitesseFuture) / 5) + compteurFuture) * 50;

        if (pouvoirObtenu) declencherBipDouble(1500, 2500, 60, 80);
        else declencherBip(freqRebond, 30);

        if (frappeValide) {
          if (pouvoirObtenu) pouvoirVert = 3; // Toujours pouvoir 3
          direction = 1;
          if (vitesseJeu > vitesseMax) vitesseJeu -= 5;
          else {
            compteurEchangesMax++;
            if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
          }
          compteurEchanges++;
          afficherScore7Seg(compteurEchanges, compteurEchanges);
        } else {
          posX = 0;
          timerMouvement = 0;
        }
        clicVert = false;
      }
    } else {
      // DUO : logique standard
      if (direction == -1 && clicVert) {
        // Vert : ledMur=1 (la plus loin du centre), ledCentre=tailleRaquette (la plus proche du centre)
        gererFrappe(1, tailleRaquette, 1, tailleRaquette, pouvoirVert, pouvoirRouge, 1);
        clicVert = false;
      }
      if (direction == 1 && clicRouge) {
        // Rouge : ledMur=TRACK_LENGTH (la plus loin du centre), ledCentre=TRACK_LENGTH-tailleRaquette+1 (la plus proche)
        gererFrappe(TRACK_LENGTH - tailleRaquette + 1, TRACK_LENGTH, TRACK_LENGTH, TRACK_LENGTH - tailleRaquette + 1, pouvoirRouge, pouvoirVert, -1);
        clicRouge = false;
      }
    }

    // --- UTILISATION DES POUVOIRS (SOLO : seul pouvoir 3 = bouclier) ---
    if (!modeSolo) {
      // DUO : pouvoirs actifs classiques
      if (direction == 1 && clicVert && pouvoirVert > 0 && pouvoirVert != 3 && posX > 5 && posX <= 32) {
        if (pouvoirVert == 1) dashRestant = 15;
        else if (pouvoirVert == 2) invisibiliteRestante = 16;
        pouvoirVert = 0;
        declencherEffetPouvoir();
      }
      if (direction == -1 && clicRouge && pouvoirRouge > 0 && pouvoirRouge != 3 && posX < 60 && posX >= 33) {
        if (pouvoirRouge == 1) dashRestant = 15;
        else if (pouvoirRouge == 2) invisibiliteRestante = 16;
        pouvoirRouge = 0;
        declencherEffetPouvoir();
      }
    }
    // En solo, le pouvoir 3 est passif (s'active automatiquement quand la balle sort)

    // --- DEPLACEMENT DE LA BALLE ---
    unsigned long delaiMouvement = (dashRestant > 0) ? 5 : vitesseJeu;
    if (millis() - timerMouvement > delaiMouvement) {
      timerMouvement = millis();
      posX += direction;
      if (dashRestant > 0) dashRestant--;
      if (invisibiliteRestante > 0) invisibiliteRestante--;

      // --- GESTION BALLE HORS LIMITES ---
      // En SOLO : le bot renvoie quand la balle atteint la LED mur (TRACK_LENGTH)
      if (modeSolo && posX == TRACK_LENGTH && direction == 1) {
        gererFrappeBot();
      }
      else if (posX < 1 || posX > TRACK_LENGTH) {
        bool vertARate = (posX < 1);

        // Pouvoir 3 (bouclier) du vert
        if (vertARate && pouvoirVert == 3) {
          pouvoirVert = 0;
          posX = 1;
          direction = 1;
          declencherEffetPouvoir();
        }
        // Pouvoir 3 du rouge (DUO uniquement)
        else if (!vertARate && !modeSolo && pouvoirRouge == 3) {
          pouvoirRouge = 0;
          posX = TRACK_LENGTH;
          direction = -1;
          declencherEffetPouvoir();
        }
        else {
          // --- QUELQU'UN A PERDU ---
          if (modeSolo) {
            // SOLO : le vert perd une vie
            viesVert--;
            joueurEngagement = -1;

            if (viesVert <= 0) {
              // Fin de partie SOLO : comparer au highscore
              bool nouveauRecord = (compteurEchanges > highscoreSolo);
              if (nouveauRecord) {
                highscoreSolo = compteurEchanges;
                sauvegarderHighscore(true);
                animationNouveauRecord();
                dernierMatchGagne = true;
                // Sequence vert gagne
                CRGB couleurGagnant = CRGB::Green;
                FastLED.clear();
                for (int x = 1; x <= MATRIX_WIDTH; x++) {
                  for (int y = 1; y <= MATRIX_HEIGHT; y++) leds[XY(x, y)] = couleurGagnant;
                  FastLED.show(); delay(30);
                }
              } else {
                dernierMatchGagne = false;
                // Sequence rouge gagne
                CRGB couleurGagnant = CRGB::Red;
                FastLED.clear();
                for (int x = 1; x <= MATRIX_WIDTH; x++) {
                  int colonne = MATRIX_WIDTH - x + 1;
                  for (int y = 1; y <= MATRIX_HEIGHT; y++) leds[XY(colonne, y)] = couleurGagnant;
                  FastLED.show(); delay(30);
                }
              }

              ledcWriteTone(BUZZER_CHANNEL, 0);
              ledcWrite(BUZZER_CHANNEL, 0);
              delay(200);
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
              afficherScore7Seg(compteurEchanges, compteurEchanges);
              partieTerminee = true;

            } else {
              // SOLO : perte d'une vie (pas la derniere)
              declencherBip(300, 600);
              CRGB couleurVie = CRGB(127, 10, 73);
              for (int i = 0; i < 3; i++) {
                FastLED.clear();
                for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
                FastLED.show(); delay(250);
                FastLED.clear();
                for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
                leds[XY(1 + viesVert, 1)] = couleurVie;
                FastLED.show(); delay(350);
              }
              // On ne reset pas le compteur d'echanges (cumule)
              posX = 16;
              enAttenteEngagement = true;
              vitesseJeu = 80;
              tailleRaquette = 5;
              compteurEchangesMax = 0;
              pouvoirVert = 0;
              dashRestant = 0;
              invisibiliteRestante = 0;
            }

          } else {
            // DUO : logique de perte standard
            CRGB couleurPoint = vertARate ? CRGB(80, 0, 0) : CRGB(0, 80, 0);
            joueurEngagement = vertARate ? -1 : 1;
            if (vertARate) viesVert--;
            else viesRouge--;

            if (viesVert <= 0 || viesRouge <= 0) {
              // Fin de partie DUO
              if (viesVert <= 0) victoiresRouge++;
              else victoiresVert++;
              if (victoiresVert > 999) victoiresVert = 999;
              if (victoiresRouge > 999) victoiresRouge = 999;

              // Affiche le compteur d'echanges final avant la sequence
              afficherScore7Seg(compteurEchanges, compteurEchanges);

              // Verifier highscore duo
              bool nouveauRecord = (compteurEchanges > highscoreDuo);
              if (nouveauRecord) {
                highscoreDuo = compteurEchanges;
                sauvegarderHighscore(false);
                animationNouveauRecord();
              }

              // Sequence vainqueur
              ledcWriteTone(BUZZER_CHANNEL, 150);
              ledcWrite(BUZZER_CHANNEL, 127);
              CRGB couleurGagnant = (viesVert <= 0) ? CRGB::Red : CRGB::Green;
              FastLED.clear();
              for (int x = 1; x <= MATRIX_WIDTH; x++) {
                int colonne = (viesVert <= 0) ? (MATRIX_WIDTH - x + 1) : x;
                for (int y = 1; y <= MATRIX_HEIGHT; y++) leds[XY(colonne, y)] = couleurGagnant;
                FastLED.show(); delay(30);
              }
              ledcWriteTone(BUZZER_CHANNEL, 0);
              ledcWrite(BUZZER_CHANNEL, 0);
              delay(200);

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
              // Apres la sequence, affiche les victoires
              afficherScore7Seg(victoiresVert, victoiresRouge);
              partieTerminee = true;
            } else {
              // DUO : perte d'une vie (pas la derniere)
              declencherBip(300, 600);
              CRGB couleurVie = CRGB(127, 10, 73);
              for (int i = 0; i < 3; i++) {
                FastLED.clear();
                for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
                for (int v = 0; v < viesRouge; v++) leds[XY(32 - v, 8)] = couleurVie;
                FastLED.show(); delay(250);
                FastLED.clear();
                for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
                for (int v = 0; v < viesRouge; v++) leds[XY(32 - v, 8)] = couleurVie;
                if (vertARate) leds[XY(1 + viesVert, 1)] = couleurVie;
                else leds[XY(32 - viesRouge, 8)] = couleurVie;
                for (int p = 1; p <= TRACK_LENGTH; p++) {
                  int bx = (p <= 32) ? p : p - 32;
                  int by = (p <= 32) ? 4 : 5;
                  leds[XY(bx, by)] = couleurPoint;
                }
                FastLED.show(); delay(350);
              }
              // Reset set mais pas compteur echanges (cumule)
              posX = (joueurEngagement == -1) ? 16 : 48;
              enAttenteEngagement = true;
              vitesseJeu = 80;
              tailleRaquette = 5;
              compteurEchangesMax = 0;
              pouvoirVert = 0;
              pouvoirRouge = 0;
              dashRestant = 0;
              invisibiliteRestante = 0;
              verrouService = true;
            }
          }
        }
      }
    }
  }

  // --- DESSIN DE L'ECRAN ---
  FastLED.clear();

  // Raquette verte (toujours)
  for (int x = 1; x <= tailleRaquette; x++) leds[XY(x, 4)] = CRGB(0, 80, 0);

  // Raquette rouge (toujours affichee, meme en solo pour l'esthetique)
  for (int x = 32 - tailleRaquette + 1; x <= 32; x++) leds[XY(x, 5)] = CRGB(80, 0, 0);

  // Vies vertes
  CRGB couleurVie = CRGB(127, 10, 73);
  for (int i = 0; i < viesVert; i++) leds[XY(1 + i, 1)] = couleurVie;

  // Vies rouges (DUO uniquement)
  if (!modeSolo) {
    for (int i = 0; i < viesRouge; i++) leds[XY(32 - i, 8)] = couleurVie;
  }

  // Pouvoirs vert (SOLO : seul pouvoir 3 affiche)
  if (modeSolo) {
    if (pouvoirVert == 3) leds[XY(3, 2)] = CRGB::Blue;
  } else {
    if (pouvoirVert == 1) leds[XY(1, 2)] = CRGB::Blue;
    else if (pouvoirVert == 2) leds[XY(2, 2)] = CRGB::Blue;
    else if (pouvoirVert == 3) leds[XY(3, 2)] = CRGB::Blue;
  }

  // Pouvoirs rouge (DUO uniquement)
  if (!modeSolo) {
    if (pouvoirRouge == 1) leds[XY(32, 7)] = CRGB::Blue;
    else if (pouvoirRouge == 2) leds[XY(31, 7)] = CRGB::Blue;
    else if (pouvoirRouge == 3) leds[XY(30, 7)] = CRGB::Blue;
  }

  // Verrou de service (DUO uniquement)
  if (!modeSolo && enAttenteEngagement && verrouService) {
    bool afficherVerrou = true;
    if (alerteVerrouActive && compteurAlerteVerrou % 2 != 0) afficherVerrou = false;
    if (afficherVerrou) {
      if (joueurEngagement == -1) leds[XY(28, 8)] = CRGB::Orange;
      else leds[XY(5, 1)] = CRGB::Orange;
    }
  }

  // Balle
  if (posX >= 1 && posX <= TRACK_LENGTH) {
    int balleX = (posX <= 32) ? posX : posX - 32;
    int balleY = (posX <= 32) ? 4 : 5;

    if (enAttenteEngagement) {
      leds[XY(balleX, balleY)] = (joueurEngagement == -1) ? CRGB::Green : CRGB::Red;
    } else {
      if (invisibiliteRestante <= 0) leds[XY(balleX, balleY)] = CRGB::Yellow;
    }
  }

  FastLED.show();
}

// --- POINT D'ENTREE ---
void loopPong() {
  if (pongState == PONG_SUBMENU) {
    loopSubMenu();
  } else {
    loopPlaying();
  }
}
