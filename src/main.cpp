#include <Arduino.h>
#include <FastLED.h>

// On utilise le GPIO 2 pour envoyer les données au panneau LED
#define LED_DATA_PIN 2

// Définition des broches pour les boutons
#define BTN_GREEN_PIN 0
#define BTN_RED_PIN 1
#define BUZZER_PIN 3 // Changement de broche pour éviter le conflit JTAG de la broche 4
#define BUZZER_CHANNEL 0 // Canal PWM matériel utilisé par l'ESP32

// Ton bandeau fait 8x32, soit 256 LEDs au total
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
#define NUM_LEDS (MATRIX_WIDTH * MATRIX_HEIGHT)

// Le tableau qui va contenir l'état de chaque LED
CRGB leds[NUM_LEDS];

// Position sur notre circuit de 64 LEDs (Ligne 4 puis Ligne 5)
int posX = 16;      // On démarre au milieu de la ligne 4 (Vert engage en premier)
const int TRACK_LENGTH = 64; // 32 LEDs sur ligne 4 + 32 LEDs sur ligne 5

// Variables pour le jeu PONG
int direction = 1;  // La balle part vers la droite (1) ou la gauche (-1)
unsigned long timerMouvement = 0; // Pour gérer la vitesse sans bloquer le programme avec delay
int vitesseJeu = 80; // Temps en ms entre chaque case (plus c'est bas, plus c'est rapide)
const int vitesseMax = 10; // Vitesse maximale (temps minimum en ms entre chaque case)
bool enAttenteEngagement = true; // Indique si on attend qu'un joueur engage
int joueurEngagement = -1; // -1 = Joueur Vert (Gauche), 1 = Joueur Rouge (Droite)
bool etatPrecedentVert = HIGH; // Mémorise l'état précédent du bouton pour détecter le clic
bool etatPrecedentRouge = HIGH;
int viesVert = 3; // Vies du joueur de gauche (Vert)
int viesRouge = 3; // Vies du joueur de droite (Rouge)
bool partieTerminee = false; // Indique si la partie est finie
int tailleRaquette = 5; // Taille de la zone de frappe (diminue à haute vitesse)
int compteurEchangesMax = 0; // Compte les échanges à vitesse maximale
bool pouvoirVert = false; // Indique si le joueur Vert a obtenu son pouvoir (1 max)
bool pouvoirRouge = false; // Indique si le joueur Rouge a obtenu son pouvoir (1 max)

// Variables pour gérer le buzzer de manière non-bloquante
unsigned long timerBuzzer = 0;
bool buzzerActif = false;
bool effetPouvoirActif = false;
unsigned long debutEffetPouvoir = 0;

// Fonction pour déclencher un bip sans le paramètre de durée (qui bug souvent sur ESP32)
void declencherBip(unsigned int frequence, unsigned long duree) {
  effetPouvoirActif = false; // Stoppe l'effet pouvoir si un bip normal prioritaire doit jouer
  ledcWriteTone(BUZZER_CHANNEL, frequence); // Utilisation de l'API PWM native
  ledcWrite(BUZZER_CHANNEL, 127); // Force le rapport cyclique à 50% (sur 255) pour créer le son
  timerBuzzer = millis() + duree;
  buzzerActif = true;
}

// Fonction pour déclencher l'effet sonore spécial du pouvoir (Grave -> Aigu -> Grave)
void declencherEffetPouvoir() {
  effetPouvoirActif = true;
  buzzerActif = false; // Stoppe tout bip standard en cours
  debutEffetPouvoir = millis();
}

// Fonction magique pour convertir des coordonnées (X, Y) en index 1D physique
uint16_t XY(uint8_t x, uint8_t y) {
  uint16_t i;
  
  // On repasse en base 0 pour les calculs mathématiques internes
  uint8_t internalX = x - 1;
  uint8_t internalY = y - 1;

  // Si la colonne est paire (0, 2, 4...), le câblage descend
  if (internalX % 2 == 0) {
    i = (internalX * MATRIX_HEIGHT) + internalY;
  } 
  // Si la colonne est impaire (1, 3, 5...), le câblage remonte
  else {
    i = (internalX * MATRIX_HEIGHT) + (MATRIX_HEIGHT - 1 - internalY);
  }
  return i;
}

void setup() {
  Serial.begin(115200);
  
  // Configuration des boutons avec résistance interne de tirage (PULLUP)
  pinMode(BTN_GREEN_PIN, INPUT_PULLUP);
  pinMode(BTN_RED_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  // Configuration native du PWM de l'ESP32 (Canal 0, 2000Hz, Résolution 8 bits)
  ledcSetup(BUZZER_CHANNEL, 2000, 8);
  ledcAttachPin(BUZZER_PIN, BUZZER_CHANNEL);

  // Configuration de FastLED (Modèle de puce, broche, ordre des couleurs)
  FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(leds, NUM_LEDS);
  
  // Gestion dynamique de l'alimentation intégrée à FastLED !
  // Ton alimentation fait 3000mA (3A). On limite le logiciel à 2500mA
  // pour garder une marge de sécurité.
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 2500); 
  
  // On peut maintenant régler la luminosité de base à 100% (255) !
  FastLED.setBrightness(32); 
  Serial.println("Démarrage du test avec l'alimentation externe de 3A !");
}

void loop() {
  // --- GESTION DU BUZZER (Non-bloquant) ---
  if (buzzerActif && millis() >= timerBuzzer) {
    ledcWriteTone(BUZZER_CHANNEL, 0); // Stoppe la fréquence PWM
    ledcWrite(BUZZER_CHANNEL, 0);     // Coupe totalement le courant (0%) pour protéger le transistor
    buzzerActif = false;
  }

  // --- GESTION DE L'EFFET SONORE DU POUVOIR ---
  if (effetPouvoirActif) {
    unsigned long tempsEcoule = millis() - debutEffetPouvoir;
    if (tempsEcoule <= 1000) { // L'effet dure exactement 1 seconde (1000 ms)
      unsigned int frequenceActuelle;
      if (tempsEcoule <= 500) {
        // Première moitié (0 à 500ms) : On monte de 100Hz à 2000Hz
        frequenceActuelle = map(tempsEcoule, 0, 500, 100, 2000);
      } else {
        // Deuxième moitié (501 à 1000ms) : On redescend de 2000Hz à 100Hz
        frequenceActuelle = map(tempsEcoule, 500, 1000, 2000, 100);
      }
      // On met à jour la fréquence du buzzer en temps réel !
      ledcWriteTone(BUZZER_CHANNEL, frequenceActuelle);
      ledcWrite(BUZZER_CHANNEL, 127);
    } else {
      // L'effet est terminé
      effetPouvoirActif = false;
      ledcWriteTone(BUZZER_CHANNEL, 0);
      ledcWrite(BUZZER_CHANNEL, 0);
    }
  }

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

    // Si un joueur appuie, on relance une partie à 0
    if (clicVert || clicRouge) {
      declencherBip(2000, 30); // Bip de redémarrage plus aigu (et plus fort physiquement)
      partieTerminee = false;
      viesVert = 3;
      viesRouge = 3;
      pouvoirVert = false;
      pouvoirRouge = false;
      joueurEngagement = -1; // Vert engage la nouvelle partie
      tailleRaquette = 5;
      compteurEchangesMax = 0;
      posX = 16; // Vert commence au milieu de sa ligne
      enAttenteEngagement = true;
    }
    return; // On stoppe l'exécution de la boucle ici tant que le jeu est fini
  }

  if (enAttenteEngagement) {
    // Logique d'engagement
    if (joueurEngagement == -1 && clicVert) {
      declencherBip(2000, 30); // Bip d'engagement Vert
      enAttenteEngagement = false;
      direction = 1; // Le joueur de gauche (vert) engage vers la droite
      timerMouvement = millis();
    } 
    else if (joueurEngagement == 1 && clicRouge) {
      declencherBip(2000, 30); // Bip d'engagement Rouge
      enAttenteEngagement = false;
      direction = -1; // Le joueur de droite (rouge) engage vers la gauche
      timerMouvement = millis();
    }
  } else {
    // 2. Logique de frappe (Renvoi)
    // Si la balle se dirige vers la GAUCHE (c'est au Vert de jouer)
    if (direction == -1 && clicVert) {
      declencherBip(2000, 30); // Bip de renvoi (ou de faute) Vert
      if (posX <= tailleRaquette) {
        // Frappe parfaite sur la toute dernière LED disponible !
        if (posX == 1) pouvoirVert = true; 
        
        direction = 1; // Renvoi valide vers la droite
        if (vitesseJeu > vitesseMax) {
          vitesseJeu -= 5; // On accélère la balle à chaque échange !
        } else {
          compteurEchangesMax++; // On compte les échanges à vitesse max
          // Tous les 2 échanges à vitesse max, on réduit la raquette (jusqu'à 2 min)
          if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
        }
      } else {
        // Faute : Appui trop tôt ! Vert perd la manche.
        posX = 0; // On force la balle hors limite à gauche
        timerMouvement = 0; // On force le déclenchement immédiat de la perte de point
      }
      
      // On "consomme" le clic pour ne pas déclencher le pouvoir par erreur dans la même boucle !
      clicVert = false;
    }
    
    // Si la balle se dirige vers la DROITE (c'est au Rouge de jouer)
    if (direction == 1 && clicRouge) {
      declencherBip(2000, 30); // Bip de renvoi (ou de faute) Rouge
      if (posX >= (TRACK_LENGTH - tailleRaquette + 1)) {
        // Frappe parfaite sur la toute dernière LED disponible !
        if (posX == TRACK_LENGTH) pouvoirRouge = true; 
         
        direction = -1; // Renvoi valide vers la gauche
        if (vitesseJeu > vitesseMax) {
          vitesseJeu -= 5;
        } else {
          compteurEchangesMax++; // On compte les échanges à vitesse max
          // Tous les 2 échanges à vitesse max, on réduit la raquette (jusqu'à 2 min)
          if (compteurEchangesMax % 2 == 0 && tailleRaquette > 2) tailleRaquette--;
        }
      } else {
        // Faute : Appui trop tôt ! Rouge perd la manche.
        posX = TRACK_LENGTH + 1; // On force la balle hors limite à droite
        timerMouvement = 0; // On force le déclenchement immédiat de la perte de point
      }
      
      // On "consomme" le clic pour ne pas déclencher le pouvoir par erreur dans la même boucle !
      clicRouge = false;
    }

    // --- UTILISATION DES POUVOIRS (Frappe Smash : +15 LEDs et Explosion) ---
    // Joueur Vert : La balle s'éloigne (direction == 1), on est sur la ligne 4 (posX <= 32)
    if (direction == 1 && clicVert && pouvoirVert && posX <= 32) {
      pouvoirVert = false;
      posX += 15; // Bond en avant
      declencherEffetPouvoir(); // Déclenche le sweep Grave -> Aigu -> Grave
    }

    // Joueur Rouge : La balle s'éloigne (direction == -1), on est sur la ligne 5 (posX >= 33)
    if (direction == -1 && clicRouge && pouvoirRouge && posX >= 33) {
      pouvoirRouge = false;
      posX -= 15; // Bond en avant
      declencherEffetPouvoir(); // Déclenche le sweep Grave -> Aigu -> Grave
    }

    // 3. Déplacement de la balle géré par le temps (millis)
    if (millis() - timerMouvement > vitesseJeu) {
      timerMouvement = millis();
      posX += direction;

      // Si la balle sort complètement de l'écran : un joueur a raté
      if (posX < 1 || posX > TRACK_LENGTH) {
        bool vertARate = (posX < 1); // Si elle sort à gauche, c'est Vert qui a raté
        CRGB couleurPoint = vertARate ? CRGB::Red : CRGB::Green; 
        
        // Le perdant obtient l'engagement à la prochaine manche
        joueurEngagement = vertARate ? -1 : 1;
        
        // On retire une vie au perdant
        if (vertARate) viesVert--;
        else viesRouge--;

        if (viesVert <= 0 || viesRouge <= 0) {
          // 1. Lancement du "Klaxon" de but (Goal Horn de hockey)
          ledcWriteTone(BUZZER_CHANNEL, 150); // Fréquence très grave
          ledcWrite(BUZZER_CHANNEL, 127);
          
          // Animation de victoire : balayage de l'écran pendant que le klaxon sonne
          CRGB couleurGagnant = (viesVert <= 0) ? CRGB::Red : CRGB::Green;
          FastLED.clear();
          for (int x = 1; x <= MATRIX_WIDTH; x++) {
            // Le balayage part du camp du gagnant
            int colonne = (viesVert <= 0) ? (MATRIX_WIDTH - x + 1) : x; 
            for (int y = 1; y <= MATRIX_HEIGHT; y++) {
              leds[XY(colonne, y)] = couleurGagnant;
            }
            FastLED.show();
            delay(30); // 32 colonnes * 30ms = ~960ms de klaxon
          }
          
          // Arrêt du klaxon et courte pause dramatique
          ledcWriteTone(BUZZER_CHANNEL, 0);
          ledcWrite(BUZZER_CHANNEL, 0);
          delay(200); 

          // 2. Fanfare "Charge!" typique des stades (G4, C5, E5, G5, E5, G5)
          int notes[] = {392, 523, 659, 784, 659, 784}; 
          int durees[] = {150, 150, 150, 300, 150, 500};
          for (int i = 0; i < 6; i++) {
            ledcWriteTone(BUZZER_CHANNEL, notes[i]);
            ledcWrite(BUZZER_CHANNEL, 127);
            delay(durees[i]);
            ledcWriteTone(BUZZER_CHANNEL, 0);
            ledcWrite(BUZZER_CHANNEL, 0);
            delay(50); // Silence entre les notes
          }
          
          delay(1000); // Petite pause avant d'autoriser la relance du jeu
          partieTerminee = true;
        } else {
          declencherBip(300, 600); // Bip long pour la perte d'une vie
          CRGB couleurVie = CRGB(127, 10, 73); // Rose foncé
          
          // Animation de point marqué (manche perdue) avec clignotement de la vie
          for (int i = 0; i < 3; i++) {
            // 1. ÉTAT ÉTEINT : La piste s'éteint, la vie perdue disparait
            FastLED.clear(); 
            for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
            for (int v = 0; v < viesRouge; v++) leds[XY(32 - v, 8)] = couleurVie;
            FastLED.show(); 
            delay(250); // Temps éteint (assez lent)
            
            // 2. ÉTAT ALLUMÉ : La piste s'allume, la vie perdue réapparait
            FastLED.clear();
            for (int v = 0; v < viesVert; v++) leds[XY(1 + v, 1)] = couleurVie;
            for (int v = 0; v < viesRouge; v++) leds[XY(32 - v, 8)] = couleurVie;
            
            // On rajoute la vie qui est en train d'être perdue pour la faire clignoter
            if (vertARate) leds[XY(1 + viesVert, 1)] = couleurVie; 
            else leds[XY(32 - viesRouge, 8)] = couleurVie;         
            
            // On allume la piste aux couleurs du gagnant
            for (int p = 1; p <= TRACK_LENGTH; p++) {
              int bx = (p <= 32) ? p : p - 32;
              int by = (p <= 32) ? 4 : 5;
              leds[XY(bx, by)] = couleurPoint;
            }
            FastLED.show(); 
            delay(350); // Temps allumé
          }
        }
        
        // Remise à zéro pour l'engagement suivant
        posX = (joueurEngagement == -1) ? 16 : 48; // Milieu de la ligne 4 (16) ou 5 (48)
        enAttenteEngagement = true; // On bloque en attente
        vitesseJeu = 80; 
        tailleRaquette = 5; // On réinitialise la taille des raquettes
        compteurEchangesMax = 0; // On réinitialise le compteur
        pouvoirVert = false; // Les pouvoirs sont perdus à chaque fin de manche
        pouvoirRouge = false;
      }
    }
  }

  // 4. Dessin de l'écran
  FastLED.clear();

  // Zones de renvoi (Taille dynamique)
  for(int x = 1; x <= tailleRaquette; x++) leds[XY(x, 4)] = CRGB(0, 20, 0);   // Raquette verte (gauche)
  for(int x = 32 - tailleRaquette + 1; x <= 32; x++) leds[XY(x, 5)] = CRGB(20, 0, 0); // Raquette rouge (droite)

  // Affichage des vies en rose foncé (Ligne 1 pour Vert, Ligne 8 pour Rouge)
  // On crée une couleur avec la moitié de la luminosité du DeepPink habituel (env 255, 20, 147)
  CRGB couleurVie = CRGB(127, 10, 73); 
  for (int i = 0; i < viesVert; i++) leds[XY(1 + i, 1)] = couleurVie; // Vies gauche (Ligne du haut)
  for (int i = 0; i < viesRouge; i++) leds[XY(32 - i, 8)] = couleurVie; // Vies droite (Ligne du bas)

  // Affichage des pouvoirs (LED bleue en dessous des vies)
  if (pouvoirVert) leds[XY(1, 2)] = CRGB::Blue;
  if (pouvoirRouge) leds[XY(32, 7)] = CRGB::Blue;

  // Dessin de la balle
  if (posX >= 1 && posX <= TRACK_LENGTH) {
    // Conversion de la position 1D (1 à 64) en coordonnées 2D réelles
    int balleX = (posX <= 32) ? posX : posX - 32;
    int balleY = (posX <= 32) ? 4 : 5;

    if (enAttenteEngagement) {
      // La balle prend la couleur du joueur qui doit engager
      leds[XY(balleX, balleY)] = (joueurEngagement == -1) ? CRGB::Green : CRGB::Red;
    } else {
      // La balle devient jaune en cours d'échange
      leds[XY(balleX, balleY)] = CRGB::Yellow;
    }
  }

  FastLED.show();
}