#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "game.h"
#include "snake.h"
#include <Preferences.h>

// --- ETATS DU JEU SNAKE ---
enum SnakeState {
  SNAKE_SUBMENU,
  SNAKE_ATTENTE,    // Verrous
  SNAKE_PLAYING,
  SNAKE_FREEZE,     // Freeze apres collision (2s)
  SNAKE_FIN
};
static SnakeState snakeState = SNAKE_SUBMENU;

// --- SOUS-MENU ---
static int subMenuIndex = 0;
static bool subPrevG = HIGH;
static bool subPrevG1 = HIGH;
static bool subPrevG2 = HIGH;
static bool subPrevExitCombo = HIGH;

// --- MODE ---
static bool modeSolo = false;

// --- CONSTANTES ---
static const int MAX_SNAKE_LEN = 128;
static const unsigned long TEMPO = 500; // 2 cases/seconde = 500ms
static const unsigned long FREEZE_DUREE = 2000;

// --- STRUCTURE SERPENT ---
struct Serpent {
  uint8_t corpsX[MAX_SNAKE_LEN];
  uint8_t corpsY[MAX_SNAKE_LEN];
  int longueur;
  int tete; // Index de la tete dans le tableau circulaire
  int dirX; // -1, 0, +1
  int dirY; // -1, 0, +1
  int vies;
  int score;
  bool mort; // Plus de vies
  bool freeze;
  unsigned long freezeDebut;
  int prochainVirage; // 0=rien, -1=gauche, +1=droite (derniere demande entre deux ticks)
};

static Serpent vert;
static Serpent rouge;

// --- POMME ---
static uint8_t pommeX = 0;
static uint8_t pommeY = 0;

// --- VERROUS ---
static bool verrouVert = true;
static bool verrouRouge = true;

// --- BOUTONS ---
static bool prevBtnVert = HIGH;
static bool prevBtnRouge = HIGH;
static bool prevBtnG1 = HIGH;
static bool prevBtnG2 = HIGH;
static bool prevBtnR1 = HIGH;
static bool prevBtnR2 = HIGH;
static bool prevExitCombo = HIGH;

// --- TIMING ---
static unsigned long dernierTick = 0;

// --- HIGHSCORES ---
static int highscoreSolo = 0;
static int highscoreDuo = 0;

// --- FIN ---
static bool dernierMatchGagne = false;

// --- FONCTIONS UTILITAIRES ---

static void chargerHighscores() {
  Preferences prefs;
  prefs.begin("pixelcross", true);
  highscoreSolo = prefs.getInt("hs_snake_s", 0);
  highscoreDuo = prefs.getInt("hs_snake_d", 0);
  prefs.end();
}

static void sauvegarderHighscore(bool solo) {
  Preferences prefs;
  prefs.begin("pixelcross", false);
  if (solo) prefs.putInt("hs_snake_s", highscoreSolo);
  else prefs.putInt("hs_snake_d", highscoreDuo);
  prefs.end();
}

// Retourne l'index de la queue (element le plus ancien)
static int queueIndex(Serpent& s) {
  return (s.tete - s.longueur + 1 + MAX_SNAKE_LEN) % MAX_SNAKE_LEN;
}

// Verifie si une case est occupee par un serpent (avec option d'exclure la queue)
static bool caseOccupeeParSerpent(Serpent& s, int x, int y, bool exclureQueue = false) {
  if (s.longueur == 0) return false;
  int idx = queueIndex(s);
  int debut = exclureQueue ? 1 : 0; // Si on exclut la queue, on commence a l'index 1
  for (int i = debut; i < s.longueur; i++) {
    int ci = (idx + i) % MAX_SNAKE_LEN;
    if (s.corpsX[ci] == x && s.corpsY[ci] == y) return true;
  }
  return false;
}

// Verifie si une case est libre (pas de serpent, pas de pomme)
static bool caseLibre(int x, int y) {
  if (caseOccupeeParSerpent(vert, x, y)) return false;
  if (!modeSolo && caseOccupeeParSerpent(rouge, x, y)) return false;
  if (x == pommeX && y == pommeY) return false;
  return true;
}

// Place la pomme sur une case libre aleatoire
static void placerPomme() {
  // Compter les cases libres
  int casesLibres = 0;
  for (int x = 1; x <= MATRIX_WIDTH; x++) {
    for (int y = 1; y <= MATRIX_HEIGHT; y++) {
      if (caseLibre(x, y)) casesLibres++;
    }
  }
  if (casesLibres == 0) {
    // Plus de place : la partie est terminee
    pommeX = 0;
    pommeY = 0;
    return;
  }
  int choix = random(0, casesLibres);
  int compteur = 0;
  for (int x = 1; x <= MATRIX_WIDTH; x++) {
    for (int y = 1; y <= MATRIX_HEIGHT; y++) {
      if (caseLibre(x, y)) {
        if (compteur == choix) {
          pommeX = x;
          pommeY = y;
          return;
        }
        compteur++;
      }
    }
  }
}

// Affichage 7 segments : format V.SSS
static void afficher7SegSnake() {
  float valVert = vert.vies + vert.score / 1000.0;
  float valRouge;
  if (modeSolo) {
    valRouge = valVert;
  } else {
    valRouge = rouge.vies + rouge.score / 1000.0;
  }
  afficherScoreDecimal7Seg(valVert, valRouge, 3);
}

// Initialise un serpent a une position donnee
static void initSerpent(Serpent& s, int startX, int startY, int dX, int dY) {
  s.longueur = 4;
  s.tete = 3;
  s.dirX = dX;
  s.dirY = dY;
  s.vies = 3;
  s.score = 0;
  s.mort = false;
  s.freeze = false;
  s.freezeDebut = 0;
  s.prochainVirage = 0;
  // La queue est derriere la tete
  for (int i = 0; i < 4; i++) {
    s.corpsX[i] = startX - dX * (3 - i); // queue[0] est le plus loin, tete[3] est devant
    s.corpsY[i] = startY - dY * (3 - i);
  }
}

// --- GESTION DE LA DIRECTION (tourner a gauche/droite) ---
static void tournerGauche(Serpent& s) {
  // Rotation 90 degres anti-horaire
  int oldDX = s.dirX;
  s.dirX = s.dirY;
  s.dirY = -oldDX;
}

static void tournerDroite(Serpent& s) {
  // Rotation 90 degres horaire
  int oldDX = s.dirX;
  s.dirX = -s.dirY;
  s.dirY = oldDX;
}

// --- TROUVER UNE DIRECTION LIBRE APRES FREEZE ---
static bool trouverDirectionLibre(Serpent& s) {
  int teteX = s.corpsX[s.tete];
  int teteY = s.corpsY[s.tete];

  // Directions possibles (gauche, droite par rapport a la direction actuelle)
  // mais jamais en arriere
  int dirs[3][2]; // max 3 directions (avant, gauche, droite)
  int nbDirs = 0;

  // Avant (direction actuelle)
  int avX = teteX + s.dirX;
  int avY = teteY + s.dirY;
  // Teleportation
  if (avX < 1) avX = MATRIX_WIDTH;
  else if (avX > MATRIX_WIDTH) avX = 1;
  if (avY < 1) avY = MATRIX_HEIGHT;
  else if (avY > MATRIX_HEIGHT) avY = 1;
  if (caseLibre(avX, avY)) { dirs[nbDirs][0] = s.dirX; dirs[nbDirs][1] = s.dirY; nbDirs++; }

  // Gauche
  int gDX = s.dirY;
  int gDY = -s.dirX;
  int gX = teteX + gDX;
  int gY = teteY + gDY;
  if (gX < 1) gX = MATRIX_WIDTH;
  else if (gX > MATRIX_WIDTH) gX = 1;
  if (gY < 1) gY = MATRIX_HEIGHT;
  else if (gY > MATRIX_HEIGHT) gY = 1;
  if (caseLibre(gX, gY)) { dirs[nbDirs][0] = gDX; dirs[nbDirs][1] = gDY; nbDirs++; }

  // Droite
  int dDX = -s.dirY;
  int dDY = s.dirX;
  int dX = teteX + dDX;
  int dY = teteY + dDY;
  if (dX < 1) dX = MATRIX_WIDTH;
  else if (dX > MATRIX_WIDTH) dX = 1;
  if (dY < 1) dY = MATRIX_HEIGHT;
  else if (dY > MATRIX_HEIGHT) dY = 1;
  if (caseLibre(dX, dY)) { dirs[nbDirs][0] = dDX; dirs[nbDirs][1] = dDY; nbDirs++; }

  if (nbDirs == 0) return false; // Aucune direction libre = mort

  int choix = random(0, nbDirs);
  s.dirX = dirs[choix][0];
  s.dirY = dirs[choix][1];
  return true;
}

// --- INITIALISATION ---
void initSnake() {
  chargerHighscores();
  snakeState = SNAKE_SUBMENU;
  subMenuIndex = 0;
  subPrevG = digitalRead(BTN_GREEN_PIN);
  subPrevG1 = digitalRead(BTN_GREEN1_PIN);
  subPrevG2 = digitalRead(BTN_GREEN2_PIN);
  subPrevExitCombo = HIGH;
  afficherScore7Seg(highscoreSolo, highscoreSolo);
}

static void initPartie() {
  initSerpent(vert, 1, 1, 1, 0); // Haut gauche, direction droite
  if (!modeSolo) {
    initSerpent(rouge, 32, 8, -1, 0); // Bas droite, direction gauche
  } else {
    rouge.longueur = 0;
    rouge.mort = true;
    rouge.vies = 0;
    rouge.score = 0;
  }
  verrouVert = true;
  verrouRouge = modeSolo ? false : true;
  prevBtnVert = digitalRead(BTN_GREEN_PIN);
  prevBtnRouge = digitalRead(BTN_RED_PIN);
  prevBtnG1 = digitalRead(BTN_GREEN1_PIN);
  prevBtnG2 = digitalRead(BTN_GREEN2_PIN);
  prevBtnR1 = digitalRead(BTN_RED1_PIN);
  prevBtnR2 = digitalRead(BTN_RED2_PIN);
  prevExitCombo = HIGH;
  dernierTick = 0;
  pommeX = 0; pommeY = 0;
  placerPomme();
  snakeState = SNAKE_ATTENTE;
}

// --- SOUS-MENU ---
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

// --- BOUCLE ATTENTE (verrous) ---
static void loopAttente() {
  bool actVert = digitalRead(BTN_GREEN_PIN);
  bool actRouge = digitalRead(BTN_RED_PIN);
  bool clicVert = (actVert == LOW && prevBtnVert == HIGH);
  bool clicRouge = (actRouge == LOW && prevBtnRouge == HIGH);
  prevBtnVert = actVert;
  prevBtnRouge = actRouge;

  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    snakeState = SNAKE_SUBMENU;
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

  if (verrouVert && clicVert) { verrouVert = false; declencherBip(600, 30); }
  if (!modeSolo && verrouRouge && clicRouge) { verrouRouge = false; declencherBip(600, 30); }

  if (!verrouVert && !verrouRouge) {
    declencherBip(1000, 50);
    dernierTick = millis();
    snakeState = SNAKE_PLAYING;
  }

  // Dessin
  FastLED.clear();
  // Dessiner les serpents
  int idxV = queueIndex(vert);
  for (int i = 0; i < vert.longueur; i++) {
    int ci = (idxV + i) % MAX_SNAKE_LEN;
    bool estTete = (ci == vert.tete);
    if (estTete) {
      leds[XY(vert.corpsX[ci], vert.corpsY[ci])] = verrouVert ? CRGB::Yellow : CRGB(0, 200, 0);
    } else {
      leds[XY(vert.corpsX[ci], vert.corpsY[ci])] = CRGB(0, 60, 0);
    }
  }
  if (!modeSolo) {
    int idxR = queueIndex(rouge);
    for (int i = 0; i < rouge.longueur; i++) {
      int ci = (idxR + i) % MAX_SNAKE_LEN;
      bool estTete = (ci == rouge.tete);
      if (estTete) {
        leds[XY(rouge.corpsX[ci], rouge.corpsY[ci])] = verrouRouge ? CRGB::Yellow : CRGB(200, 0, 0);
      } else {
        leds[XY(rouge.corpsX[ci], rouge.corpsY[ci])] = CRGB(60, 0, 0);
      }
    }
  }
  // Pomme
  if (pommeX > 0) leds[XY(pommeX, pommeY)] = CRGB::Yellow;
  FastLED.show();
}

// --- DESSIN DU TERRAIN ---
static void dessinerJeu() {
  FastLED.clear();

  // Serpent vert
  if (vert.longueur > 0) {
    int idxV = queueIndex(vert);
    for (int i = 0; i < vert.longueur; i++) {
      int ci = (idxV + i) % MAX_SNAKE_LEN;
      bool estTete = (ci == vert.tete);
      CRGB couleur;
      if (vert.mort) {
        couleur = estTete ? CRGB(0, 0, 150) : CRGB(0, 0, 50); // Bleu quand mort
      } else {
        couleur = estTete ? CRGB(0, 200, 0) : CRGB(0, 60, 0);
      }
      // Clignotement pendant le freeze
      if (vert.freeze && millis() % 400 < 200) couleur = CRGB::Black;
      leds[XY(vert.corpsX[ci], vert.corpsY[ci])] = couleur;
    }
  }

  // Serpent rouge
  if (!modeSolo && rouge.longueur > 0) {
    int idxR = queueIndex(rouge);
    for (int i = 0; i < rouge.longueur; i++) {
      int ci = (idxR + i) % MAX_SNAKE_LEN;
      bool estTete = (ci == rouge.tete);
      CRGB couleur;
      if (rouge.mort) {
        couleur = estTete ? CRGB(0, 0, 150) : CRGB(0, 0, 50); // Bleu quand mort
      } else {
        couleur = estTete ? CRGB(200, 0, 0) : CRGB(60, 0, 0);
      }
      if (rouge.freeze && millis() % 400 < 200) couleur = CRGB::Black;
      leds[XY(rouge.corpsX[ci], rouge.corpsY[ci])] = couleur;
    }
  }

  // Pomme
  if (pommeX > 0) leds[XY(pommeX, pommeY)] = CRGB::Yellow;

  FastLED.show();
}

// --- PERTE DE VIE ---
static void perdreVie(Serpent& s) {
  s.vies--;
  declencherBip(300, 600);
  if (s.vies <= 0) {
    s.mort = true;
    s.freeze = false;
  } else {
    s.freeze = true;
    s.freezeDebut = millis();
  }
  afficher7SegSnake();
}

// --- DEPLACER UN SERPENT ---
// Retourne true si le serpent a mange la pomme
static bool deplacerSerpent(Serpent& s, Serpent& autre) {
  if (s.mort || s.freeze) return false;

  int teteX = s.corpsX[s.tete];
  int teteY = s.corpsY[s.tete];

  // Nouvelle position
  int newX = teteX + s.dirX;
  int newY = teteY + s.dirY;

  // Teleportation
  if (newX < 1) newX = MATRIX_WIDTH;
  else if (newX > MATRIX_WIDTH) newX = 1;
  if (newY < 1) newY = MATRIX_HEIGHT;
  else if (newY > MATRIX_HEIGHT) newY = 1;

  // Verifier collision avec la case de destination (apres teleportation)
  // On exclut la queue du serpent car elle va avancer (liberer sa case) ce tick-ci
  bool collisionPropre = caseOccupeeParSerpent(s, newX, newY, true);
  // Pour l'adversaire : s'il est mort, son corps est un obstacle complet (pas d'exclusion de queue)
  bool collisionAutre = false;
  if (autre.longueur > 0) {
    collisionAutre = caseOccupeeParSerpent(autre, newX, newY, !autre.mort && !autre.freeze);
  }

  if (collisionPropre || collisionAutre) {
    // Le serpent ne peut pas aller la -> perte de vie
    perdreVie(s);
    return false;
  }

  // Verifier si c'est la pomme
  bool mangePomme = (newX == pommeX && newY == pommeY);

  // Avancer la tete
  s.tete = (s.tete + 1) % MAX_SNAKE_LEN;
  s.corpsX[s.tete] = newX;
  s.corpsY[s.tete] = newY;

  if (mangePomme) {
    // Le serpent grandit (on ne retire pas la queue)
    s.longueur++;
    if (s.longueur > MAX_SNAKE_LEN) s.longueur = MAX_SNAKE_LEN;
    s.score++;
    sonBut();
    return true;
  } else {
    // Deplacement normal : la longueur reste la meme (la queue avance)
    // Pas besoin de retirer explicitement car on utilise longueur fixe
    return false;
  }
}

// --- BOUCLE DE JEU ---
static void loopPlaying() {
  unsigned long now = millis();

  // --- GESTION DU RETOUR ---
  bool btnG1state = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2state = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1state && btnG2state);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    snakeState = SNAKE_SUBMENU;
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

  // --- LECTURE DES BOUTONS (direction) ---
  bool actG1 = digitalRead(BTN_GREEN1_PIN);
  bool actG2 = digitalRead(BTN_GREEN2_PIN);
  bool actR1 = digitalRead(BTN_RED1_PIN);
  bool actR2 = digitalRead(BTN_RED2_PIN);
  bool clicG1 = (actG1 == LOW && prevBtnG1 == HIGH);
  bool clicG2 = (actG2 == LOW && prevBtnG2 == HIGH);
  bool clicR1 = (actR1 == LOW && prevBtnR1 == HIGH);
  bool clicR2 = (actR2 == LOW && prevBtnR2 == HIGH);
  prevBtnG1 = actG1;
  prevBtnG2 = actG2;
  prevBtnR1 = actR1;
  prevBtnR2 = actR2;

  // Stocker la derniere demande de virage (appliquee au prochain tick uniquement)
  if (!vert.mort && !vert.freeze) {
    if (clicG1) vert.prochainVirage = -1; // Gauche
    if (clicG2) vert.prochainVirage = 1;  // Droite
  }
  // Tourner le rouge
  if (!modeSolo && !rouge.mort && !rouge.freeze) {
    if (clicR1) rouge.prochainVirage = -1;
    if (clicR2) rouge.prochainVirage = 1;
  }

  // --- GESTION DU FREEZE ---
  if (vert.freeze && now - vert.freezeDebut >= FREEZE_DUREE) {
    vert.freeze = false;
    if (!trouverDirectionLibre(vert)) {
      // Pas de direction libre -> mort
      vert.vies = 0;
      vert.mort = true;
    }
  }
  if (!modeSolo && rouge.freeze && now - rouge.freezeDebut >= FREEZE_DUREE) {
    rouge.freeze = false;
    if (!trouverDirectionLibre(rouge)) {
      rouge.vies = 0;
      rouge.mort = true;
    }
  }

  // --- TICK DE DEPLACEMENT ---
  if (now - dernierTick >= TEMPO) {
    dernierTick = now;

    // Appliquer les virages demandes (un seul par tick, le dernier enregistre)
    if (vert.prochainVirage == -1) tournerGauche(vert);
    else if (vert.prochainVirage == 1) tournerDroite(vert);
    vert.prochainVirage = 0;

    if (!modeSolo) {
      if (rouge.prochainVirage == -1) tournerGauche(rouge);
      else if (rouge.prochainVirage == 1) tournerDroite(rouge);
      rouge.prochainVirage = 0;
    }

    // Calculer les positions futures AVANT de deplacer
    // pour detecter la collision tete-a-tete symetriquement
    int futureVertX = 0, futureVertY = 0;
    int futureRougeX = 0, futureRougeY = 0;
    bool vertPeutBouger = (!vert.mort && !vert.freeze);
    bool rougePeutBouger = (!modeSolo && !rouge.mort && !rouge.freeze);

    if (vertPeutBouger) {
      futureVertX = vert.corpsX[vert.tete] + vert.dirX;
      futureVertY = vert.corpsY[vert.tete] + vert.dirY;
      if (futureVertX < 1) futureVertX = MATRIX_WIDTH;
      else if (futureVertX > MATRIX_WIDTH) futureVertX = 1;
      if (futureVertY < 1) futureVertY = MATRIX_HEIGHT;
      else if (futureVertY > MATRIX_HEIGHT) futureVertY = 1;
    }
    if (rougePeutBouger) {
      futureRougeX = rouge.corpsX[rouge.tete] + rouge.dirX;
      futureRougeY = rouge.corpsY[rouge.tete] + rouge.dirY;
      if (futureRougeX < 1) futureRougeX = MATRIX_WIDTH;
      else if (futureRougeX > MATRIX_WIDTH) futureRougeX = 1;
      if (futureRougeY < 1) futureRougeY = MATRIX_HEIGHT;
      else if (futureRougeY > MATRIX_HEIGHT) futureRougeY = 1;
    }

    // Collision tete-a-tete : les deux vont sur la meme case
    if (vertPeutBouger && rougePeutBouger &&
        futureVertX == futureRougeX && futureVertY == futureRougeY) {
      perdreVie(vert);
      perdreVie(rouge);
      // Pas de deplacement ce tick
    } else {
      // Deplacer normalement
      bool vertMangePomme = deplacerSerpent(vert, rouge);
      bool rougeMangePomme = false;
      if (!modeSolo) {
        rougeMangePomme = deplacerSerpent(rouge, vert);
      }

      // Gestion de la pomme
      if (vertMangePomme && rougeMangePomme) {
        // Les deux arrivent en meme temps sur la pomme : personne ne gagne
        vert.longueur--;
        vert.score--;
        rouge.longueur--;
        rouge.score--;
        sonInterception();
        placerPomme();
      } else if (vertMangePomme || rougeMangePomme) {
        placerPomme();
      }
    }

    // Verifier si la pomme ne peut plus etre placee (toutes les cases occupees)
    if (pommeX == 0 && pommeY == 0) {
      // Le joueur restant gagne
      snakeState = SNAKE_FIN;
      dernierMatchGagne = !vert.mort;
    }

    afficher7SegSnake();

    // --- VERIFIER FIN DE PARTIE ---
    if (modeSolo && vert.mort) {
      // Fin solo
      bool nouveauRecord = (vert.score > highscoreSolo);
      if (nouveauRecord) {
        highscoreSolo = vert.score;
        sauvegarderHighscore(true);
        animationNouveauRecord(highscoreSolo);
        dernierMatchGagne = true;
      } else {
        dernierMatchGagne = false;
      }
      animationSetGagnant(dernierMatchGagne);
      jouerFanfare();
      afficher7SegSnake();
      snakeState = SNAKE_FIN;
      return;
    }

    if (!modeSolo && vert.mort && rouge.mort) {
      // Fin duo : les deux sont morts
      int total = vert.score + rouge.score;
      bool nouveauRecord = (total > highscoreDuo);
      if (nouveauRecord) {
        highscoreDuo = total;
        sauvegarderHighscore(false);
        animationNouveauRecord(highscoreDuo);
      }
      bool vertGagne = (vert.score > rouge.score);
      animationSetGagnant(vertGagne);
      jouerFanfare();
      afficher7SegSnake();
      dernierMatchGagne = vertGagne;
      snakeState = SNAKE_FIN;
      return;
    }
  }

  // --- AFFICHAGE ---
  dessinerJeu();
}

// --- BOUCLE FIN DE PARTIE ---
static void loopFin() {
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    snakeState = SNAKE_SUBMENU;
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
void loopSnake() {
  switch (snakeState) {
    case SNAKE_SUBMENU: loopSubMenu(); break;
    case SNAKE_ATTENTE: loopAttente(); break;
    case SNAKE_PLAYING: loopPlaying(); break;
    case SNAKE_FREEZE:  loopPlaying(); break; // Le freeze est gere dans loopPlaying
    case SNAKE_FIN:     loopFin(); break;
  }
}
