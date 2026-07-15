#include "buzzer.h"

// Variables pour gerer le buzzer de maniere non-bloquante
static unsigned long timerBuzzer = 0;
static bool buzzerActif = false;
static bool effetPouvoirActif = false;
static unsigned long debutEffetPouvoir = 0;

// Variables pour le bip double
static unsigned int freqBip2 = 0;
static unsigned long dureeBip2 = 0;
static bool attenteBip2 = false;

// Fonction pour declencher un bip sans le parametre de duree (qui bug souvent sur ESP32)
void declencherBip(unsigned int frequence, unsigned long duree) {
  effetPouvoirActif = false; // Stoppe l'effet pouvoir si un bip normal prioritaire doit jouer
  attenteBip2 = false; // Annule un bip double en attente
  ledcWriteTone(BUZZER_CHANNEL, frequence); // Utilisation de l'API PWM native
  ledcWrite(BUZZER_CHANNEL, 127); // Force le rapport cyclique a 50% (sur 255) pour creer le son
  timerBuzzer = millis() + duree;
  buzzerActif = true;
}

// Fonction pour declencher un son en deux temps
void declencherBipDouble(unsigned int f1, unsigned int f2, unsigned long d1, unsigned long d2) {
  effetPouvoirActif = false;
  freqBip2 = f2;
  dureeBip2 = d2;
  attenteBip2 = true;
  ledcWriteTone(BUZZER_CHANNEL, f1);
  ledcWrite(BUZZER_CHANNEL, 127);
  timerBuzzer = millis() + d1;
  buzzerActif = true;
}

// Fonction pour declencher l'effet sonore special du pouvoir (Grave -> Aigu -> Grave)
void declencherEffetPouvoir() {
  effetPouvoirActif = true;
  buzzerActif = false; // Stoppe tout bip standard en cours
  attenteBip2 = false; // Stoppe un bip double en attente
  debutEffetPouvoir = millis();
}

void gererBuzzer() {
  if (buzzerActif && millis() >= timerBuzzer) {
    if (attenteBip2) {
      // Declenche la deuxieme partie du son
      ledcWriteTone(BUZZER_CHANNEL, freqBip2);
      ledcWrite(BUZZER_CHANNEL, 127);
      timerBuzzer = millis() + dureeBip2;
      attenteBip2 = false;
    } else {
      ledcWriteTone(BUZZER_CHANNEL, 0); // Stoppe la frequence PWM
      ledcWrite(BUZZER_CHANNEL, 0);     // Coupe totalement le courant (0%) pour proteger le transistor
      buzzerActif = false;
    }
  }

  // --- GESTION DE L'EFFET SONORE DU POUVOIR ---
  if (effetPouvoirActif) {
    unsigned long tempsEcoule = millis() - debutEffetPouvoir;
    if (tempsEcoule <= 1000) { // L'effet dure exactement 1 seconde (1000 ms)
      unsigned int frequenceActuelle;
      if (tempsEcoule <= 500) {
        // Premiere moitie (0 a 500ms) : On monte de 100Hz a 2000Hz
        frequenceActuelle = map(tempsEcoule, 0, 500, 100, 2000);
      } else {
        // Deuxieme moitie (501 a 1000ms) : On redescend de 2000Hz a 100Hz
        frequenceActuelle = map(tempsEcoule, 500, 1000, 2000, 100);
      }
      // On met a jour la frequence du buzzer en temps reel !
      ledcWriteTone(BUZZER_CHANNEL, frequenceActuelle);
      ledcWrite(BUZZER_CHANNEL, 127);
    } else {
      // L'effet est termine
      effetPouvoirActif = false;
      ledcWriteTone(BUZZER_CHANNEL, 0);
      ledcWrite(BUZZER_CHANNEL, 0);
    }
  }
}
