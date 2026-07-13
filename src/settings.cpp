#include "shared.h"
#include "settings.h"
#include <Preferences.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Update.h>

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


// --- LOGIQUE DE MISE A JOUR OTA ---

// Parametres pour l'API GitHub
const char* API_HOST = "api.github.com";
const char* GITHUB_USER_REPO = "djfab59/PixelCross";
const String API_URL = String("/repos/") + GITHUB_USER_REPO + "/releases/latest";

// Differentes etapes de la mise a jour
enum OtaState {
  OTA_INIT,
  OTA_CONNECTING_WIFI,
  OTA_CHECK_VERSION,
  OTA_DOWNLOADING,
  OTA_SUCCESS,
  OTA_FAIL,
  OTA_UP_TO_DATE
};
static OtaState otaCurrentState;

void initOtaUpdate() {
  otaCurrentState = OTA_INIT;
  FastLED.clear();
  drawCenteredString("UPDATE", 2, CRGB::Blue);
  FastLED.show();
}

void performOtaUpdate(String firmwareUrl) {
  Serial.println("Debut du telechargement de la mise a jour depuis " + firmwareUrl);

  FastLED.clear();
  drawCenteredString("DL", 2, CRGB::Orange); // DL pour Download
  FastLED.show();

  WiFiClientSecure client;
  // NOTE: setInsecure() contourne la validation du certificat. C'est pratique pour les
  // projets personnels mais deconseille pour des produits commerciaux sensibles.
  client.setInsecure();

  HTTPClient http;
  // On utilise la surcharge qui prend une URL complete et on active le suivi des redirections
  http.begin(client, firmwareUrl);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  
  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    int contentLength = http.getSize();
    if (contentLength <= 0) {
      Serial.println("Erreur: Taille de contenu invalide.");
      otaCurrentState = OTA_FAIL;
      return;
    }

    bool canBegin = Update.begin(contentLength);
    if (!canBegin) {
      Serial.println("Erreur: Espace insuffisant pour la mise a jour.");
      Update.printError(Serial);
      otaCurrentState = OTA_FAIL;
      return;
    }

    WiFiClient* stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    if (written == contentLength) {
      Serial.println("Telechargement complet.");
    } else {
      Serial.println("Erreur: Telechargement incomplet.");
      otaCurrentState = OTA_FAIL;
      return;
    }

    if (Update.end()) {
      Serial.println("Mise a jour terminee avec succes !");
      otaCurrentState = OTA_SUCCESS;
    } else {
      Serial.println("Erreur lors de la finalisation de la mise a jour.");
      Update.printError(Serial);
      otaCurrentState = OTA_FAIL;
    }

  } else {
    Serial.printf("Erreur HTTP lors du telechargement du firmware: %d\n", httpCode);
    otaCurrentState = OTA_FAIL;
  }
  http.end();
}

void loopOtaUpdate() {
  static unsigned long stateTimer = 0;

  switch (otaCurrentState) {
    case OTA_INIT:
      if (WiFi.status() == WL_CONNECTED) {
        otaCurrentState = OTA_CHECK_VERSION;
      } else {
        otaCurrentState = OTA_CONNECTING_WIFI;
        // On s'assure que le correctif de puissance est applique
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_5dBm);
        WiFi.begin();
        stateTimer = millis();
        Serial.println("OTA: Connexion au WiFi...");
      }
      break;

    case OTA_CONNECTING_WIFI:
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("OTA: Connecte !");
        otaCurrentState = OTA_CHECK_VERSION;
      } else if (millis() - stateTimer > 15000) { // Timeout de 15s
        Serial.println("OTA: Echec de la connexion WiFi.");
        otaCurrentState = OTA_FAIL;
      }
      break;

    case OTA_CHECK_VERSION:
      {
        Serial.println("Verification de la version en ligne...");
        Serial.println("Interrogation de l'API GitHub pour la derniere release...");
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        
        http.begin(client, API_HOST, 443, API_URL);
        // L'API GitHub requiert un User-Agent
        http.addHeader("User-Agent", "ESP32-OTA-Updater");

        int httpCode = http.GET();
        if (httpCode == HTTP_CODE_OK) {
          // L'API peut renvoyer beaucoup de donnees, on utilise un document dynamique
          DynamicJsonDocument doc(4096);
          DeserializationError error = deserializeJson(doc, http.getStream());
          http.end();

          if (error) {
            Serial.printf("Erreur de deserialisation JSON: %s\n", error.c_str());
            otaCurrentState = OTA_FAIL;
            break;
          }

          // On cherche les URL de nos fichiers dans la liste des "assets" de la release
          String version_url;
          String firmware_url;
          String firmware_filename = String("firmware-") + PIO_ENV_NAME + ".zip";

          for (JsonObject asset : doc["assets"].as<JsonArray>()) {
            if (String(asset["name"]) == "version.json") {
              version_url = asset["browser_download_url"].as<String>();
            }
            if (String(asset["name"]) == firmware_filename) {
              firmware_url = asset["browser_download_url"].as<String>();
            }
          }

          if (version_url.isEmpty() || firmware_url.isEmpty()) {
            Serial.println("Erreur: Impossible de trouver version.json ou le fichier firmware dans la derniere release.");
            otaCurrentState = OTA_FAIL;
            break;
          }

          // Maintenant qu'on a l'URL du version.json, on le telecharge
          http.begin(client, version_url);
          httpCode = http.GET();
          if (httpCode != HTTP_CODE_OK) {
             Serial.printf("Erreur au telechargement de version.json: HTTP %d\n", httpCode);
             otaCurrentState = OTA_FAIL;
             break;
          }

          DynamicJsonDocument version_doc(256);
          deserializeJson(version_doc, http.getStream());
          int remoteVersion = version_doc["version"];
          Serial.printf("Version actuelle: %d, Version en ligne: %d\n", FIRMWARE_VERSION, remoteVersion);

          if (remoteVersion > FIRMWARE_VERSION) {
            performOtaUpdate(firmware_url);
          } else { otaCurrentState = OTA_UP_TO_DATE; }

        } else {
          Serial.printf("Erreur d'interrogation de l'API GitHub: HTTP %d\n", httpCode);
          http.end();
          otaCurrentState = OTA_FAIL;
        }
      }
      break;

    case OTA_DOWNLOADING:
      // La fonction performOtaUpdate est bloquante, donc on ne passe jamais vraiment ici.
      // L'etat est mis a jour a la fin de performOtaUpdate.
      break;

    case OTA_SUCCESS:
      FastLED.clear();
      drawCenteredString("OK", 2, CRGB::Green);
      FastLED.show();
      Serial.println("Redemarrage dans 3 secondes...");
      delay(3000);
      ESP.restart();
      break;

    case OTA_FAIL:
    case OTA_UP_TO_DATE:
      if (stateTimer == 0) { // Premiere entree dans cet etat
        FastLED.clear();
        if (otaCurrentState == OTA_FAIL) drawCenteredString("FAIL", 2, CRGB::Red);
        else drawCenteredString("A JOUR", 2, CRGB::Green);
        FastLED.show();
        stateTimer = millis();
      }
      if (millis() - stateTimer > 3000) { // Apres 3 secondes
        stateTimer = 0; // On reinitialise le timer pour la prochaine fois
        currentState = STATE_SETTINGS;
      }
      break;
  }
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
        initOtaUpdate();
        currentState = STATE_OTA_UPDATE;
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