#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"
#include "game.h"
#include "chrono.h"
#include <Preferences.h>

// --- ETATS DU JEU CHRONO ---
enum ChronoState {
  CHRONO_SUBMENU,       // Sous-menu SOLO / DUO
  CHRONO_ATTENTE,       // Affichage cible + attente verrous
  CHRONO_COMPTE,        // Chrono visible (5 premieres secondes)
  CHRONO_SILENCE,       // Chrono invisible, joueurs comptent
  CHRONO_RESULTAT,      // Affichage des resultats du set
  CHRONO_FIN            // Fin de partie (clignotement)
};
static ChronoState chronoState = CHRONO_SUBMENU;

// --- SOUS-MENU ---
static int subMenuIndex = 0; // 0=SOLO, 1=DUO
static bool subPrevG = HIGH;
static bool subPrevG1 = HIGH;
static bool subPrevG2 = HIGH;
static bool subPrevExitCombo = HIGH;

// --- MODE ---
static bool modeSolo = false;

// --- VARIABLES DE JEU ---
static int dureeCible = 15;           // Duree a deviner (9-30 secondes)
static unsigned long chronoDepart = 0; // Timestamp du debut du chrono
static int viesVert = 3;
static int viesRouge = 3;

// --- VERROUS ---
static bool verrouVert = true;
static bool verrouRouge = true;
static bool prevBtnVert = HIGH;
static bool prevBtnRouge = HIGH;

// --- APPUIS JOUEURS ---
static bool vertAAppuye = false;
static bool rougeAAppuye = false;
static unsigned long tempsAppuiVert = 0;  // Temps en centièmes quand le vert a appuye
static unsigned long tempsAppuiRouge = 0; // Temps en centièmes quand le rouge a appuye

// --- INTERRUPTIONS POUR CAPTURER LE TEMPS EXACT D'APPUI ---
static volatile unsigned long isrTimestampVert = 0;
static volatile bool isrVertDeclenche = false;
static volatile unsigned long isrTimestampRouge = 0;
static volatile bool isrRougeDeclenche = false;

static void IRAM_ATTR isrBoutonVert() {
  if (!isrVertDeclenche) {
    isrTimestampVert = millis();
    isrVertDeclenche = true;
  }
}

static void IRAM_ATTR isrBoutonRouge() {
  if (!isrRougeDeclenche) {
    isrTimestampRouge = millis();
    isrRougeDeclenche = true;
  }
}

// --- SCORES ---
static int ecartCumuleVert = 0;   // Ecart cumule en centiemes (sur la partie)
static int ecartCumuleRouge = 0;  // Ecart cumule en centiemes
static int highscoreSolo = 0;     // Plus petit ecart cumule solo (centiemes)
static int highscoreDuo = 0;      // Plus petit ecart cumule duo (centiemes)

// --- GESTION FIN DE PARTIE ---
static bool partieTerminee = false;
static bool dernierMatchGagne = false; // Pour le clignotement solo
static bool prevExitCombo = HIGH;

// --- ALERTE VERROU (animation non-bloquante) ---
static bool alerteVerrouActive = false;
static unsigned long timerAlerteVerrou = 0;
static int compteurAlerteVerrou = 0;
const int DUREE_CLIGNO_VERROU = 150;

// --- TIMER POUR AFFICHAGE RESULTAT ---
static unsigned long timerResultat = 0;
static int derniereSecondeAffichee = -1; // Pour eviter de rafraichir les 7 segments a chaque frame
static bool interruptionsAttachees = false; // Pour attacher les ISR apres relachement des boutons

// --- HIGHSCORES ---
static void chargerHighscores() {
  Preferences prefs;
  prefs.begin("pixelcross", true);
  highscoreSolo = prefs.getInt("hs_chrono_s", 0);
  highscoreDuo = prefs.getInt("hs_chrono_d", 0);
  prefs.end();
}

static void sauvegarderHighscore(bool solo) {
  Preferences prefs;
  prefs.begin("pixelcross", false);
  if (solo) prefs.putInt("hs_chrono_s", highscoreSolo);
  else prefs.putInt("hs_chrono_d", highscoreDuo);
  prefs.end();
}

// --- AFFICHAGE DU NOMBRE CIBLE SUR LA MATRICE ---
static void dessinerCible() {
  // Affiche le nombre cible en Y=3 cote vert
  char buf[4];
  snprintf(buf, sizeof(buf), "%d", dureeCible);
  drawString(buf, 1, 3, CRGB::White);

  // Affiche le nombre cible inverse cote rouge
  invertText = true;
  drawString(buf, 1, 3, CRGB::White);
  invertText = false;
}

// --- AFFICHAGE TEMPS AU FORMAT SS.MM SUR LES 7 SEGMENTS ---
// tempsCs = temps en centiemes de seconde
static void afficherTempsCentiemes(int tempsCs, bool deuxCotes) {
  // Format SS.MM (ex: 16.56 pour 1656 centiemes)
  float tempsF = tempsCs / 100.0;
  if (deuxCotes) {
    afficherScoreDecimal7Seg(tempsF, tempsF, 2);
  } else {
    // On ne devrait pas en avoir besoin ici mais au cas ou
    afficherScoreDecimal7Seg(tempsF, tempsF, 2);
  }
}

// Affiche un ecart par joueur (vert a gauche, rouge a droite)
static void afficherEcartsJoueurs(int ecartVert, int ecartRouge) {
  float fVert = ecartVert / 100.0;
  float fRouge = ecartRouge / 100.0;
  afficherScoreDecimal7Seg(fVert, fRouge, 2);
}

// --- INITIALISATION ---
void initChrono() {
  chargerHighscores();
  chronoState = CHRONO_SUBMENU;
  subMenuIndex = 0;
  subPrevG = digitalRead(BTN_GREEN_PIN);
  subPrevG1 = digitalRead(BTN_GREEN1_PIN);
  subPrevG2 = digitalRead(BTN_GREEN2_PIN);
  subPrevExitCombo = HIGH;
  // Affiche le highscore solo par defaut
  if (highscoreSolo > 0) afficherTempsCentiemes(highscoreSolo, true);
  else afficherScore7Seg(0, 0);
}

static void initSet() {
  // Nouvelle duree aleatoire entre 9 et 30
  dureeCible = random(9, 31);
  chronoDepart = 0;
  vertAAppuye = false;
  rougeAAppuye = false;
  tempsAppuiVert = 0;
  tempsAppuiRouge = 0;
  verrouVert = true;
  verrouRouge = modeSolo ? false : true; // En solo pas de verrou rouge
  alerteVerrouActive = false;
  derniereSecondeAffichee = -1;
  interruptionsAttachees = false;
  prevBtnVert = digitalRead(BTN_GREEN_PIN);
  prevBtnRouge = digitalRead(BTN_RED_PIN);
  chronoState = CHRONO_ATTENTE;
}

static void initPartie() {
  viesVert = 3;
  viesRouge = modeSolo ? 0 : 3;
  ecartCumuleVert = 0;
  ecartCumuleRouge = 0;
  partieTerminee = false;
  dernierMatchGagne = false;
  prevExitCombo = HIGH;
  initSet();
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
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    if (hs > 0) afficherTempsCentiemes(hs, true);
    else afficherScore7Seg(0, 0);
  }

  // Validation
  if (clicG) {
    declencherBip(1000, 100);
    modeSolo = (subMenuIndex == 0);
    initPartie();
  }

  // Affichage
  FastLED.clear();
  if (subMenuIndex == 0) drawCenteredString("SOLO", 2, CRGB::Green);
  else drawCenteredString("DUO", 2, CRGB::Green);
  FastLED.show();

  subPrevG = actG; subPrevG1 = actG1; subPrevG2 = actG2;
}

// --- BOUCLE ATTENTE (verrous + affichage cible) ---
static void loopAttente() {
  bool actVert = digitalRead(BTN_GREEN_PIN);
  bool actRouge = digitalRead(BTN_RED_PIN);
  bool clicVert = (actVert == LOW && prevBtnVert == HIGH);
  bool clicRouge = (actRouge == LOW && prevBtnRouge == HIGH);
  prevBtnVert = actVert;
  prevBtnRouge = actRouge;

  // Gestion du retour
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    chronoState = CHRONO_SUBMENU;
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    if (hs > 0) afficherTempsCentiemes(hs, true);
    else afficherScore7Seg(0, 0);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Deverrouillage
  if (modeSolo) {
    // En solo : le vert deverrouille, le chrono demarre immediatement
    if (verrouVert && clicVert) {
      verrouVert = false;
      declencherBip(1000, 50);
      chronoDepart = millis();
      chronoState = CHRONO_COMPTE;
      return;
    }
  } else {
    // DUO : chaque joueur deverrouille son cote
    if (verrouVert && clicVert) {
      verrouVert = false;
      declencherBip(600, 30);
    }
    if (verrouRouge && clicRouge) {
      verrouRouge = false;
      declencherBip(600, 30);
    }
    // Des que les deux sont deverrouilles, le chrono demarre automatiquement
    if (!verrouVert && !verrouRouge) {
      declencherBip(1000, 50);
      chronoDepart = millis();
      chronoState = CHRONO_COMPTE;
      return;
    }
  }

  // Dessin
  FastLED.clear();
  dessinerViesVert(viesVert);
  if (!modeSolo) dessinerViesRouge(viesRouge);
  dessinerCible();

  // Affichage des verrous
  if (verrouVert) dessinerVerrou(true, viesVert);
  if (!modeSolo && verrouRouge) dessinerVerrou(false, viesRouge);

  FastLED.show();
  afficherScore7Seg(-1, -1); // 7 segments eteints en attente
}

// --- BOUCLE CHRONO VISIBLE (5 premieres secondes) ---

static void loopCompte() {
  unsigned long elapsed = millis() - chronoDepart;
  int secondes = elapsed / 1000;

  // Attache les interruptions uniquement quand les boutons sont relaches
  // pour eviter un declenchement parasite du au clic de deverrouillage
  if (!interruptionsAttachees) {
    bool btnVertRelache = (digitalRead(BTN_GREEN_PIN) == HIGH);
    bool btnRougeRelache = modeSolo || (digitalRead(BTN_RED_PIN) == HIGH);
    if (btnVertRelache && btnRougeRelache) {
      isrVertDeclenche = false;
      isrRougeDeclenche = false;
      attachInterrupt(digitalPinToInterrupt(BTN_GREEN_PIN), isrBoutonVert, FALLING);
      if (!modeSolo) attachInterrupt(digitalPinToInterrupt(BTN_RED_PIN), isrBoutonRouge, FALLING);
      interruptionsAttachees = true;
    }
  }

  // Gestion du retour
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    // Desactive les interruptions si encore actives
    if (!vertAAppuye) detachInterrupt(digitalPinToInterrupt(BTN_GREEN_PIN));
    if (!modeSolo && !rougeAAppuye) detachInterrupt(digitalPinToInterrupt(BTN_RED_PIN));
    chronoState = CHRONO_SUBMENU;
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Affichage du chrono par increment de 1 seconde (00.00, 01.00, etc.)
  // On ne met a jour que quand la seconde change pour eviter l'overhead
  if (secondes != derniereSecondeAffichee && secondes < 6) {
    int affichageCs = secondes * 100;
    afficherTempsCentiemes(affichageCs, true);
    derniereSecondeAffichee = secondes;
  }

  // Apres 5 secondes reelles ecoulees, on passe en mode silence
  if (elapsed >= 6000) {
    afficherScore7Seg(-1, -1); // Eteint les 7 segments
    chronoState = CHRONO_SILENCE;
  }

  // Lecture des appuis via les interruptions (timestamp exact)
  if (isrVertDeclenche && !vertAAppuye) {
    vertAAppuye = true;
    tempsAppuiVert = (isrTimestampVert - chronoDepart) / 10;
    detachInterrupt(digitalPinToInterrupt(BTN_GREEN_PIN));
    declencherBip(800, 30);
  }
  if (!modeSolo && isrRougeDeclenche && !rougeAAppuye) {
    rougeAAppuye = true;
    tempsAppuiRouge = (isrTimestampRouge - chronoDepart) / 10;
    detachInterrupt(digitalPinToInterrupt(BTN_RED_PIN));
    declencherBip(800, 30);
  }

  // Verifier si tout le monde a appuye
  bool tousAppuye = modeSolo ? vertAAppuye : (vertAAppuye && rougeAAppuye);
  if (tousAppuye) {
    chronoState = CHRONO_RESULTAT;
    timerResultat = millis();
    return;
  }

  // Dessin
  FastLED.clear();
  dessinerViesVert(viesVert);
  if (!modeSolo) dessinerViesRouge(viesRouge);
  dessinerCible();
  // Indicateur d'appui (LED jaune = a appuye)
  if (vertAAppuye) dessinerVerrou(true, viesVert);
  if (!modeSolo && rougeAAppuye) dessinerVerrou(false, viesRouge);
  FastLED.show();
}

// --- BOUCLE SILENCE (chrono invisible, joueurs comptent) ---
static void loopSilence() {
  // Gestion du retour
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    // Desactive les interruptions si encore actives
    if (!vertAAppuye) detachInterrupt(digitalPinToInterrupt(BTN_GREEN_PIN));
    if (!modeSolo && !rougeAAppuye) detachInterrupt(digitalPinToInterrupt(BTN_RED_PIN));
    chronoState = CHRONO_SUBMENU;
    subPrevExitCombo = HIGH;
    prevExitCombo = exitCombo;
    return;
  }
  prevExitCombo = exitCombo;

  // Lecture des appuis via les interruptions (timestamp exact)
  if (isrVertDeclenche && !vertAAppuye) {
    vertAAppuye = true;
    tempsAppuiVert = (isrTimestampVert - chronoDepart) / 10;
    detachInterrupt(digitalPinToInterrupt(BTN_GREEN_PIN));
    declencherBip(800, 30);
  }
  if (!modeSolo && isrRougeDeclenche && !rougeAAppuye) {
    rougeAAppuye = true;
    tempsAppuiRouge = (isrTimestampRouge - chronoDepart) / 10;
    detachInterrupt(digitalPinToInterrupt(BTN_RED_PIN));
    declencherBip(800, 30);
  }

  // Verifier si tout le monde a appuye
  bool tousAppuye = modeSolo ? vertAAppuye : (vertAAppuye && rougeAAppuye);
  if (tousAppuye) {
    chronoState = CHRONO_RESULTAT;
    timerResultat = millis();
    return;
  }

  // Dessin
  FastLED.clear();
  dessinerViesVert(viesVert);
  if (!modeSolo) dessinerViesRouge(viesRouge);
  dessinerCible();
  // Indicateur d'appui (LED jaune = a appuye)
  if (vertAAppuye) dessinerVerrou(true, viesVert);
  if (!modeSolo && rougeAAppuye) dessinerVerrou(false, viesRouge);
  FastLED.show();
}

// --- BOUCLE RESULTAT ---
static void loopResultat() {
  // Calcul des ecarts (en centiemes)
  int cibleCs = dureeCible * 100; // ex: 20 secondes = 2000 centiemes
  int ecartVert = abs((int)tempsAppuiVert - cibleCs);
  int ecartRouge = modeSolo ? 0 : abs((int)tempsAppuiRouge - cibleCs);

  // Affiche le temps de chaque joueur sur les 7 segments
  float fVert = tempsAppuiVert / 100.0;
  float fRouge = modeSolo ? fVert : tempsAppuiRouge / 100.0;
  afficherScoreDecimal7Seg(fVert, fRouge, 2);

  delay(2000); // Laisse le temps de voir les resultats

  // Cumule les ecarts
  ecartCumuleVert += ecartVert;
  if (!modeSolo) ecartCumuleRouge += ecartRouge;

  if (modeSolo) {
    // SOLO : perte de vie systematique
    viesVert--;

    if (viesVert <= 0) {
      // Fin de partie solo
      bool nouveauRecord = (highscoreSolo == 0 || ecartCumuleVert < highscoreSolo);
      if (nouveauRecord) {
        highscoreSolo = ecartCumuleVert;
        sauvegarderHighscore(true);
        dernierMatchGagne = true;
        animationNouveauRecord(ecartCumuleVert);
        animationSetGagnant(true);
      } else {
        dernierMatchGagne = false;
        animationSetGagnant(false);
      }
      jouerFanfare();
      afficherTempsCentiemes(ecartCumuleVert, true);
      partieTerminee = true;
      chronoState = CHRONO_FIN;
    } else {
      // Perte d'une vie (pas la derniere)
      animationPerteVieSolo(viesVert);
      // Affiche l'ecart cumule actuel
      afficherTempsCentiemes(ecartCumuleVert, true);
      delay(1000);
      initSet();
    }
  } else {
    // DUO : celui avec le plus gros ecart perd une vie
    bool vertPerd = (ecartVert >= ecartRouge);

    if (vertPerd) viesVert--;
    else viesRouge--;

    // Petite animation pour montrer qui a gagne le set
    animationSetGagnant(!vertPerd);

    if (viesVert <= 0 || viesRouge <= 0) {
      // Fin de partie duo
      int ecartTotal = ecartCumuleVert + ecartCumuleRouge;
      bool nouveauRecord = (highscoreDuo == 0 || ecartTotal < highscoreDuo);
      if (nouveauRecord) {
        highscoreDuo = ecartTotal;
        sauvegarderHighscore(false);
        animationNouveauRecord(ecartTotal);
      }

      // Sequence vainqueur (celui qui a encore des vies)
      bool vertGagne = (viesRouge <= 0);
      animationSetGagnant(vertGagne);
      jouerFanfare();

      // Affiche les ecarts cumules de chaque joueur
      afficherEcartsJoueurs(ecartCumuleVert, ecartCumuleRouge);
      partieTerminee = true;
      chronoState = CHRONO_FIN;
    } else {
      // Perte d'une vie (pas la derniere)
      animationPerteVie(viesVert, viesRouge, vertPerd);
      // Affiche les ecarts cumules
      afficherEcartsJoueurs(ecartCumuleVert, ecartCumuleRouge);
      delay(1000);
      initSet();
    }
  }
}

// --- BOUCLE FIN DE PARTIE ---
static void loopFin() {
  // Gestion du retour
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);
  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1);
    chronoState = CHRONO_SUBMENU;
    int hs = (subMenuIndex == 0) ? highscoreSolo : highscoreDuo;
    if (hs > 0) afficherTempsCentiemes(hs, true);
    else afficherScore7Seg(0, 0);
    subPrevG = digitalRead(BTN_GREEN_PIN);
    subPrevG1 = digitalRead(BTN_GREEN1_PIN);
    subPrevG2 = digitalRead(BTN_GREEN2_PIN);
    subPrevExitCombo = HIGH;
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
    initPartie();
  }
}

// --- POINT D'ENTREE ---
void loopChrono() {
  switch (chronoState) {
    case CHRONO_SUBMENU: loopSubMenu(); break;
    case CHRONO_ATTENTE: loopAttente(); break;
    case CHRONO_COMPTE:  loopCompte(); break;
    case CHRONO_SILENCE: loopSilence(); break;
    case CHRONO_RESULTAT: loopResultat(); break;
    case CHRONO_FIN:     loopFin(); break;
  }
}
