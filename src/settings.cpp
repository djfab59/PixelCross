#include "shared.h"
#include "settings.h"
#include <Preferences.h>

static bool prevExitCombo = HIGH;
static bool editingMode = false;
static int settingsMenuIndex = 0; // Pour naviguer dans les sous-menus (0=Light, 1=Volume...)
static int currentPercentage = 0; // Pour stocker le pourcentage pendant l'edition

// Variables pour la repetition automatique (appui long)
static unsigned long holdStartTime = 0;
static unsigned long lastRepeatTime = 0;
const unsigned long HOLD_TRIGGER_DELAY = 1000; // 1 seconde avant de declencher la repetition
const unsigned long REPEAT_RATE = 100;         // Repeter toutes les 100ms

// Variables pour l'affichage temporise des messages (non-bloquant)
static unsigned long messageDisplayTime = 0;
const unsigned long MESSAGE_DURATION = 2000; // 2 secondes

// Table de correction Gamma pour une perception lineaire de la luminosite (0 à 100%)
const uint8_t gammaTable[101] = { // Table finale, calibree pour une montee douce et un plafond à 227 (101 valeurs)
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,
   15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
   30,  32,  34,  36,  38,  40,  42,  44,  46,  48,  50,  52,  54,  56,  58,
   60,  62,  64,  66,  68,  70,  72,  74,  76,  78,  80,  82,  84,  86,  88,
   90,  93,  96,  99, 102, 105, 108, 112, 115, 118, 121, 124, 127, 130, 133,
  136, 139, 142, 145, 150, 155, 160, 165, 170, 175, 180, 185, 190, 195, 200,
  205, 210, 215, 220, 225, 230, 235, 240, 245, 250, 255
};

static bool prevG = HIGH;
static bool prevG1 = HIGH;
static bool prevG2 = HIGH;

void initSettings() {
  // On s'assure que tout est reinitialise en entrant dans le menu
  prevExitCombo = HIGH;
  editingMode = false;
  settingsMenuIndex = 0;
  prevG = digitalRead(BTN_GREEN_PIN);
  prevG1 = digitalRead(BTN_GREEN1_PIN);
  prevG2 = digitalRead(BTN_GREEN2_PIN);
}

void loopSettings() {
  // --- LECTURE DES BOUTONS ---
  bool actG = digitalRead(BTN_GREEN_PIN);
  bool actG1 = digitalRead(BTN_GREEN1_PIN);
  bool actG2 = digitalRead(BTN_GREEN2_PIN);

  bool clicG = (actG == LOW && prevG == HIGH);
  bool clicG1 = (actG1 == LOW && prevG1 == HIGH);
  bool clicG2 = (actG2 == LOW && prevG2 == HIGH);

  bool exitCombo = (actG1 == LOW && actG2 == LOW);

  // Si un message temporaire est en cours d'affichage, on bloque les interactions
  if (messageDisplayTime != 0) {
    if (millis() - messageDisplayTime < MESSAGE_DURATION) {
      // On attend simplement que le temps soit écoulé, le message est déjà sur l'écran
      return;
    } else {
      // Le temps est écoulé, on réinitialise pour que la boucle puisse continuer normalement
      messageDisplayTime = 0;
    }
  }
  
  FastLED.clear();

  // --- GESTION DE LA LOGIQUE ---
  if (editingMode) {
    // --- MODE EDITION DE LA LUMINOSITE ---

    // 1. On met à jour la valeur de luminosite brute à partir du pourcentage
    // C'est la PREMIERE chose à faire pour que les changements soient visibles immediatement.
    uint8_t newBrightness = gammaTable[currentPercentage];

    // --- GESTION DE L'APPUI LONG ---
    if (clicG1) { // Appui court pour diminuer
        if (currentPercentage > 1) { currentPercentage--; declencherBip(400, 20); }
        holdStartTime = millis(); // Demarre le chrono pour l'appui long
        lastRepeatTime = holdStartTime;
    }
    if (clicG2) { // Appui court pour augmenter
        if (currentPercentage < 100) { currentPercentage++; declencherBip(500, 20); }
        holdStartTime = millis();
        lastRepeatTime = holdStartTime;
    }

    // Si le bouton G1 (diminuer) est maintenu
    if (actG1 == LOW && millis() - holdStartTime > HOLD_TRIGGER_DELAY) {
        if (millis() - lastRepeatTime > REPEAT_RATE) {
            if (currentPercentage > 1) {
                currentPercentage--;
                declencherBip(450, 10); // Bip plus discret pour la repetition
            }
            lastRepeatTime = millis();
        }
    }

    // Si le bouton G2 (augmenter) est maintenu
    if (actG2 == LOW && millis() - holdStartTime > HOLD_TRIGGER_DELAY) {
        if (millis() - lastRepeatTime > REPEAT_RATE) {
            if (currentPercentage < 100) {
                currentPercentage++;
                declencherBip(550, 10);
            }
            lastRepeatTime = millis();
        }
    }

    // 2. On desactive la luminosite globale de FastLED pour eviter le "double-dip"
    FastLED.setBrightness(255);

    // 3. On applique notre luminosite calculee manuellement à la couleur
    CRGB adjustedColor = CRGB::Blue;
    drawCenteredString("LIGHT", 2, adjustedColor.scale8(newBrightness));

    // Affichage du pourcentage sur l'afficheur 7 segments
    afficherScore7Seg(currentPercentage, -1);

    if (clicG) { // Valider et quitter le mode edition
      brightness = newBrightness; // On met à jour la variable globale seulement à la validation

      Preferences prefs;
      prefs.begin("pixelcross", false);
      // On sauvegarde la valeur brute (0-255) calculee par la table gamma
      prefs.putUChar("brightness", brightness);
      prefs.end();

      // On quitte le mode edition et on retourne au menu des reglages
      editingMode = false;
      afficherScore7Seg(-1, -1); // Eteint les afficheurs
      declencherBip(1200, 80);

      // 4. TRES IMPORTANT : On reactive la luminosite globale pour le reste de l'application
      FastLED.setBrightness(brightness);
    }

  } else {
    // --- MODE NAVIGATION DANS LE MENU REGLAGES ---
    // Il n'y a qu'une seule option pour l'instant : "LIGHT"
    drawCenteredString("LIGHT", 2, CRGB::Green);

    if (clicG) { // Entrer en mode edition
      // Pour retrouver le pourcentage à partir de la valeur brute, on cherche
      // l'entree dans la table gamma qui est la plus proche de notre `brightness` actuelle.
      int closest_dist = 256;
      int best_percentage = 1; // On part de 1% par securite
      for (int i = 1; i <= 100; i++) { // On commence à 1 pour ignorer 0%
          int dist = abs(gammaTable[i] - brightness);
          if (dist < closest_dist) {
              closest_dist = dist;
              best_percentage = i;
          }
      }
      currentPercentage = best_percentage;
      editingMode = true;
      declencherBip(800, 50);
    }
  }

  // --- AFFICHAGE ET GESTION DE LA SORTIE ---
  FastLED.show();

  // On s'assure que la luminosite globale est correcte si on n'est pas en mode edition
  if (!editingMode) FastLED.setBrightness(brightness);

  if (exitCombo && prevExitCombo == HIGH) {
    declencherBip(800, 50);
    afficherScore7Seg(-1, -1); // S'assure d'eteindre en quittant
    currentState = STATE_MENU;
  }

  prevExitCombo = exitCombo;
  prevG = actG;
  prevG1 = actG1;
  prevG2 = actG2;
}