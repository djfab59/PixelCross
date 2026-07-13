#include "shared.h"
#include "settings.h"
#include <Preferences.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>

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

// Table de correction Gamma pour une perception lineaire de la luminosite (0 a 100%)
const uint8_t gammaTable[101] = { // Table finale, calibree pour une montee douce et un plafond a 227 (101 valeurs)
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
static bool wifiConnected = false; // Pour memoriser l'etat de la connexion

void initSettings() {
  // On s'assure que tout est reinitialise en entrant dans le menu
  prevExitCombo = HIGH;
  editingMode = false;
  settingsMenuIndex = 0;
  prevG = digitalRead(BTN_GREEN_PIN);
  prevG1 = digitalRead(BTN_GREEN1_PIN);
  prevG2 = digitalRead(BTN_GREEN2_PIN);
  wifiConnected = (WiFi.status() == WL_CONNECTED); // On met a jour le statut au cas ou on etait deja connecte
}

void startWifiConfigPortal() {
  Serial.println("Starting WiFi Config Portal...");
  FastLED.clear();
  drawCenteredString("WIFI", 2, CRGB::Blue);
  FastLED.show();

  // --- INITIALISATION ROBUSTE DU WIFI ---
  // On passe en mode Point d'Acces avant de configurer la puissance
  WiFi.mode(WIFI_AP);

  // LA solution: Reduire la puissance d'emission pour eviter les instabilites
  Serial.println("Reduction de la puissance d'emission WiFi a 5dBm pour la stabilite...");
  WiFi.setTxPower(WIFI_POWER_5dBm);

  WiFiManager wm;
  wm.setDebugOutput(true);
  wm.setConfigPortalTimeout(180);

  bool success = wm.startConfigPortal("PixelCross-Setup");

  if (success) {
    Serial.println("WiFi Connected!");
    FastLED.clear();
    drawCenteredString("OK", 2, CRGB::Green);
  } else {
    Serial.println("Config Portal Timed Out or Failed.");
    FastLED.clear();
    drawCenteredString("FAIL", 2, CRGB::Red);
  }

  FastLED.show();
  messageDisplayTime = millis(); // Affiche le message de statut pendant 2 secondes
  wifiConnected = (WiFi.status() == WL_CONNECTED);

  currentState = STATE_SETTINGS;
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
      // On attend simplement que le temps soit ecoule, le message est deja sur l'ecran
      return;
    } else {
      // Le temps est ecoule, on reinitialise pour que la boucle puisse continuer normalement
      messageDisplayTime = 0;
    }
  }
  
  FastLED.clear();

  // --- GESTION DE LA LOGIQUE ---
  if (editingMode) {
    // --- MODE EDITION DE LA LUMINOSITE ---

    // 1. On met à jour la valeur de luminosite brute à partir du pourcentage
    // C'est la PREMIERE chose a faire pour que les changements soient visibles immediatement.
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

    // 3. On applique notre luminosite calculee manuellement a la couleur
    CRGB adjustedColor = CRGB::Blue;
    drawCenteredString("LIGHT", 2, adjustedColor.scale8(newBrightness));

    // Affichage du pourcentage sur l'afficheur 7 segments
    afficherScore7Seg(currentPercentage, -1);

    if (clicG) { // Valider et quitter le mode edition
      brightness = newBrightness; // On met a jour la variable globale seulement a la validation

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
    if (clicG1) { // Naviguer vers le haut
      settingsMenuIndex--;
      if (settingsMenuIndex < 0) settingsMenuIndex = 2; // 3 options: 0=LIGHT, 1=WIFI, 2=UPDATE
      declencherBip(500, 30);
    }
    if (clicG2) { // Naviguer vers le bas
      settingsMenuIndex++;
      if (settingsMenuIndex > 2) settingsMenuIndex = 0;
      declencherBip(600, 30);
    }

    if (settingsMenuIndex == 0) {
      drawCenteredString("LIGHT", 2, CRGB::Green);
    } else if (settingsMenuIndex == 1) {
      if (wifiConnected) drawCenteredString("WIFI OK", 2, CRGB::Green);
      else drawCenteredString("WIFI", 2, CRGB::Green);
    } else if (settingsMenuIndex == 2) {
      drawCenteredString("UPDATE", 2, CRGB::Green);
    }

    if (clicG) { // Entrer en mode edition
      if (settingsMenuIndex == 0) {
        // Pour retrouver le pourcentage a partir de la valeur brute, on cherche
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
      } else if (settingsMenuIndex == 1) {
        currentState = STATE_WIFI_CONFIG;
      } else if (settingsMenuIndex == 2) {
        // --- ACTION POUR "UPDATE" ---
        declencherBip(800, 50);

        // 1. Afficher "UPDATE" en bleu pendant le test
        FastLED.clear();
        drawCenteredString("UPDATE", 2, CRGB::Blue);
        FastLED.show();

        bool internetAccess = false;
        // On verifie si on est deja connecte
        if (WiFi.status() != WL_CONNECTED) {
          // On tente de recuperer le dernier SSID configure pour un log plus clair.
          String savedSSID = WiFi.SSID();
          if (savedSSID.length() > 0) {
            Serial.printf("WiFi non connecte. Tentative de connexion sur le SSID %s...\n", savedSSID.c_str());
          } else {
            Serial.println("WiFi non connecte. Tentative de connexion avec les identifiants sauvegardes...");
          }
          
          // On s'assure que le correctif de puissance est applique avant toute tentative de connexion
          // et que le mode est bien Station (au cas ou on sort du mode AP du portail)
          WiFi.mode(WIFI_STA);
          Serial.println("Reduction de la puissance d'emission WiFi a 5dBm pour la stabilite...");
          WiFi.setTxPower(WIFI_POWER_5dBm);

          WiFi.begin(); // Utilise les derniers identifiants connus

          // Attendre la connexion avec un timeout de 15 secondes
          unsigned long startAttemptTime = millis();
          unsigned long lastDotTime = 0;
          while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
            gererBuzzer(); // On continue de gerer le buzzer pour qu'il s'arrete
            if (millis() - lastDotTime > 500) {
              Serial.print(".");
              lastDotTime = millis();
            }
            delay(10); // On laisse un peu de temps aux autres process
          }
          Serial.println();
        }

        // Une fois (potentiellement) connecte, on teste l'acces a internet
        if (WiFi.status() == WL_CONNECTED) {
          Serial.println("Connecte au WiFi. Test de l'acces a Internet...");
          HTTPClient http;
          http.begin("http://detectportal.firefox.com/success.txt");
          http.setConnectTimeout(5000); // Timeout de 5 secondes
          int httpCode = http.GET();

          if (httpCode == HTTP_CODE_OK) {
            internetAccess = true;
            Serial.printf("[HTTP] Test reussi, code: %d\n", httpCode);
          } else {
            Serial.printf("[HTTP] Echec du test, code: %d, erreur: %s\n", httpCode, http.errorToString(httpCode).c_str());
          }
          http.end();
        } else {
          Serial.println("Echec de la connexion WiFi.");
        }

        // 2. Afficher le resultat (Vert ou Rouge) et le laisser a l'ecran un court instant
        FastLED.clear();
        drawCenteredString("UPDATE", 2, internetAccess ? CRGB::Green : CRGB::Red);
        if (internetAccess) declencherBip(1200, 80);
        else declencherBipDouble(400, 200, 100, 100);
        FastLED.show();
        messageDisplayTime = millis(); // Utilise le mecanisme de message temporise pour revenir au menu
      }
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