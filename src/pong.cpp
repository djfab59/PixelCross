#include "shared.h"
#include "pong.h"

// Position sur notre circuit de 64 LEDs (Ligne 4 puis Ligne 5)
static int posX = 16;      
static const int TRACK_LENGTH = 64; 

// Variables pour le jeu PONG
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

// Variables pour l'animation non-bloquante du verrou de service
static bool alerteVerrouActive = false;
static unsigned long timerAlerteVerrou = 0;
static int compteurAlerteVerrou = 0;
const int DUREE_CLIGNOTEMENT_VERROU = 150; // 150ms

void initPong() {
  posX = (joueurEngagement == -1) ? 16 : 48;
  direction = (joueurEngagement == -1) ? 1 : -1;
  timerMouvement = 0;
  vitesseJeu = 80;
  enAttenteEngagement = true;
  viesVert = 3;
  viesRouge = 3;
  partieTerminee = false;
  tailleRaquette = 5;
  compteurEchangesMax = 0;
  pouvoirVert = 0;
  pouvoirRouge = 0;
  dashRestant = 0;
  invisibiliteRestante = 0;
  verrouService = true;
  prevExitCombo = HIGH;
  alerteVerrouActive = false; // On s'assure que l'animation est stoppée
  timerAlerteVerrou = 0;
  compteurAlerteVerrou = 0;
  
  // On mémorise l'état actuel des boutons pour éviter un déclenchement instantané
  etatPrecedentVert = digitalRead(BTN_GREEN_PIN);
  etatPrecedentRouge = digitalRead(BTN_RED_PIN);

  afficherScore7Seg(victoiresVert, victoiresRouge);
}

void loopPong() {
  // --- GESTION DU RETOUR AU MENU ---
  bool btnG1 = (digitalRead(BTN_GREEN1_PIN) == LOW);
  bool btnG2 = (digitalRead(BTN_GREEN2_PIN) == LOW);
  bool exitCombo = (btnG1 && btnG2);

  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1); // Éteint les 7 segments
    victoiresVert = 0; // Réinitialise les scores du tournoi en quittant
    victoiresRouge = 0;
    joueurEngagement = -1; // Vert commencera de nouveau si on relance depuis le menu
    currentState = STATE_MENU;
    prevExitCombo = exitCombo;
    return; // On quitte immédiatement et retourne au menu
  }
  prevExitCombo = exitCombo;

  // 1. Lecture ultra-rapide des boutons (sans bloquer avec delay)
  bool etatActuelVert = digitalRead(BTN_GREEN_PIN);
  bool etatActuelRouge = digitalRead(BTN_RED_PIN);

  // Détection du front descendant (moment précis de l'appui) pour éviter la triche du bouton maintenu
  bool clicVert = (etatActuelVert == LOW && etatPrecedentVert == HIGH);
  bool clicRouge = (etatActuelRouge == LOW && etatPrecedentRouge == HIGH);

  etatPrecedentVert = etatActuelVert;
  etatPrecedentRouge = etatActuelRouge;

  // --- GESTION DE LA FIN DE PARTIE ---
  if (partieTerminee) {
    CRGB couleurGagnant = (viesVert <= 0) ? CRGB::Red : CRGB::Green;
    CRGB couleurDouce = (viesVert <= 0) ? CRGB(40, 0, 0) : CRGB(0, 40, 0); // Couleur assombrie
    
    // Clignotement doux de l'écran en attendant la nouvelle partie
    if (millis() % 1000 < 500) {
      for (int i = 0; i < NUM_LEDS; i++) leds[i] = couleurDouce;
    } else {
      FastLED.clear();
    }
    FastLED.show();

    // Si un joueur appuie sur un bouton d'action, on relance une partie
    if (clicVert || clicRouge) {
      declencherBip(2000, 30); 
      initPong();
    }

    return; // On stoppe l'exécution de la boucle ici tant que le jeu est fini
  }

  if (enAttenteEngagement) {
    // Logique d'engagement
    if (joueurEngagement == -1) {
      if (clicRouge) verrouService = false;
      if (clicVert && !alerteVerrouActive) { // On ne peut pas déclencher si l'alerte est déjà en cours
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

    // --- GESTION DE L'ANIMATION D'ALERTE (NON-BLOQUANTE) ---
    if (alerteVerrouActive) {
      if (millis() - timerAlerteVerrou > DUREE_CLIGNOTEMENT_VERROU) {
        timerAlerteVerrou = millis();
        compteurAlerteVerrou++;

        if (compteurAlerteVerrou % 2 != 0) { // Phase 1: Bip + LED éteinte
          ledcWriteTone(BUZZER_CHANNEL, 150);
          ledcWrite(BUZZER_CHANNEL, 127);
        } else { // Phase 2: Pas de bip + LED allumée
          ledcWriteTone(BUZZER_CHANNEL, 0);
          ledcWrite(BUZZER_CHANNEL, 0);
        }

        if (compteurAlerteVerrou >= 6) { // 3 clignotements complets (6 phases)
          alerteVerrouActive = false;
          // On s'assure que le son est coupé et que la LED est visible à la fin
          ledcWriteTone(BUZZER_CHANNEL, 0);
          ledcWrite(BUZZER_CHANNEL, 0);
        }
      }
    }


  } else {
    // 2. Logique de frappe (Renvoi)
    // Si la balle se dirige vers la GAUCHE (c'est au Vert de jouer)
    if (direction == -1 && clicVert) {
      bool frappeValide = (posX <= tailleRaquette);
      bool pouvoirObtenu = false;
      bool pouvoirDetruit = false;

      if (frappeValide) {
        if (posX == 1) pouvoirObtenu = true; 
        if (posX == tailleRaquette && pouvoirRouge > 0) pouvoirDetruit = true;
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
        if (pouvoirObtenu) pouvoirVert = random(1, 4); 
        if (pouvoirDetruit) pouvoirRouge = 0;

        direction = 1; 
        if (vitesseJeu > vitesseMax) {
          vitesseJeu -= 5; 
        } else {
          compteurEchangesMax++; 
          if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
        }
      } else {
        posX = 0; 
        timerMouvement = 0; 
      }
      clicVert = false;
    }
    
    // Si la balle se dirige vers la DROITE (c'est au Rouge de jouer)
    if (direction == 1 && clicRouge) {
      bool frappeValide = (posX >= (TRACK_LENGTH - tailleRaquette + 1));
      bool pouvoirObtenu = false;
      bool pouvoirDetruit = false;

      if (frappeValide) {
        if (posX == TRACK_LENGTH) pouvoirObtenu = true; 
        if (posX == (TRACK_LENGTH - tailleRaquette + 1) && pouvoirVert > 0) pouvoirDetruit = true;
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
        if (pouvoirObtenu) pouvoirRouge = random(1, 4); 
        if (pouvoirDetruit) pouvoirVert = 0;

        direction = -1; 
        if (vitesseJeu > vitesseMax) {
          vitesseJeu -= 5;
        } else {
          compteurEchangesMax++; 
          if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
        }
      } else {
        posX = TRACK_LENGTH + 1; 
        timerMouvement = 0; 
      }
      clicRouge = false;
    }

    // --- UTILISATION DES POUVOIRS ACTIFS ---
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

    // 3. Déplacement de la balle géré par le temps (millis)
    unsigned long delaiMouvement = (dashRestant > 0) ? 5 : vitesseJeu;
    if (millis() - timerMouvement > delaiMouvement) {
      timerMouvement = millis();
      posX += direction;
      if (dashRestant > 0) dashRestant--;
      if (invisibiliteRestante > 0) invisibiliteRestante--;

      if (posX < 1 || posX > TRACK_LENGTH) {
        bool vertARate = (posX < 1); 
        
        if (vertARate && pouvoirVert == 3) {
          pouvoirVert = 0; 
          posX = 1; 
          direction = 1; 
          declencherEffetPouvoir(); 
        } 
        else if (!vertARate && pouvoirRouge == 3) {
          pouvoirRouge = 0; 
          posX = TRACK_LENGTH; 
          direction = -1; 
          declencherEffetPouvoir(); 
        } 
        else {
          // On réduit ENCORE PLUS la luminosité du trait. La réduction précédente à 150
          // n'était pas suffisante pour empêcher la chute de tension sur la matrice.
          // Une valeur de 80 devrait être beaucoup plus stable.
          CRGB couleurPoint = vertARate ? CRGB(80, 0, 0) : CRGB(0, 80, 0);
          joueurEngagement = vertARate ? -1 : 1;
          
          if (vertARate) viesVert--;
          else viesRouge--;

          if (viesVert <= 0 || viesRouge <= 0) {
            // Fin de partie : on incrémente le compteur de matchs gagnés
            if (viesVert <= 0) victoiresRouge++;
            else victoiresVert++;
            
            if (victoiresVert > 999) victoiresVert = 999; // Plafond à 999 victoires
            if (victoiresRouge > 999) victoiresRouge = 999;
            
            afficherScore7Seg(victoiresVert, victoiresRouge);

            ledcWriteTone(BUZZER_CHANNEL, 150); 
            ledcWrite(BUZZER_CHANNEL, 127);
            
            CRGB couleurGagnant = (viesVert <= 0) ? CRGB::Red : CRGB::Green;
            FastLED.clear();
            for (int x = 1; x <= MATRIX_WIDTH; x++) {
              int colonne = (viesVert <= 0) ? (MATRIX_WIDTH - x + 1) : x; 
              for (int y = 1; y <= MATRIX_HEIGHT; y++) leds[XY(colonne, y)] = couleurGagnant;
              FastLED.show();
              delay(30); 
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
            partieTerminee = true;
          } else {
            declencherBip(300, 600); 
            CRGB couleurVie = CRGB(127, 10, 73); 
            
            for (int i = 0; i < 3; i++) {
              FastLED.clear(); 
              for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
              for (int v = 0; v < viesRouge; v++) leds[XY(32 - v, 8)] = couleurVie;
              FastLED.show(); 
              delay(250); 
              
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
              FastLED.show(); 
              delay(350); 
            }
          }
          
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

  // 4. Dessin de l'écran
  FastLED.clear();
  // Augmentons la luminosité des raquettes pour voir si le scintillement disparaît
  // Une valeur comme 80 est assez lumineuse pour être stable.
  for(int x = 1; x <= tailleRaquette; x++) leds[XY(x, 4)] = CRGB(0, 80, 0);   
  for(int x = 32 - tailleRaquette + 1; x <= 32; x++) leds[XY(x, 5)] = CRGB(80, 0, 0); 

  CRGB couleurVie = CRGB(127, 10, 73); 
  for (int i = 0; i < viesVert; i++) leds[XY(1 + i, 1)] = couleurVie; 
  for (int i = 0; i < viesRouge; i++) leds[XY(32 - i, 8)] = couleurVie; 

  if (pouvoirVert == 1) leds[XY(1, 2)] = CRGB::Blue; 
  else if (pouvoirVert == 2) leds[XY(2, 2)] = CRGB::Blue; 
  else if (pouvoirVert == 3) leds[XY(3, 2)] = CRGB::Blue; 

  if (pouvoirRouge == 1) leds[XY(32, 7)] = CRGB::Blue; 
  else if (pouvoirRouge == 2) leds[XY(31, 7)] = CRGB::Blue; 
  else if (pouvoirRouge == 3) leds[XY(30, 7)] = CRGB::Blue; 

  if (enAttenteEngagement && verrouService) {
    // On gère l'affichage du verrou en fonction de l'animation d'alerte
    bool afficherVerrou = true;
    if (alerteVerrouActive && compteurAlerteVerrou % 2 != 0) {
      afficherVerrou = false; // On n'affiche pas le verrou pendant la phase "éteinte" du clignotement
    }

    if (afficherVerrou) {
      if (joueurEngagement == -1) leds[XY(28, 8)] = CRGB::Orange; 
      else leds[XY(5, 1)] = CRGB::Orange; 
    }
  }

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