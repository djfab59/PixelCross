#include <Arduino.h>
#include <FastLED.h>

// On utilise le GPIO 2 pour envoyer les données au panneau LED
#define LED_DATA_PIN 2

// Définition des broches pour les boutons
#define BTN_GREEN_PIN 0
#define BTN_RED_PIN 1

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
const int vitesseMax = 20; // Vitesse maximale (temps minimum en ms entre chaque case)
bool enAttenteEngagement = true; // Indique si on attend qu'un joueur engage
int joueurEngagement = -1; // -1 = Joueur Vert (Gauche), 1 = Joueur Rouge (Droite)
bool etatPrecedentVert = HIGH; // Mémorise l'état précédent du bouton pour détecter le clic
bool etatPrecedentRouge = HIGH;
int viesVert = 3; // Vies du joueur de gauche (Vert)
int viesRouge = 3; // Vies du joueur de droite (Rouge)
bool partieTerminee = false; // Indique si la partie est finie
int tailleRaquette = 5; // Taille de la zone de frappe (diminue à haute vitesse)
int compteurEchangesMax = 0; // Compte les échanges à vitesse maximale

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
      partieTerminee = false;
      viesVert = 3;
      viesRouge = 3;
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
      enAttenteEngagement = false;
      direction = 1; // Le joueur de gauche (vert) engage vers la droite
      timerMouvement = millis();
    } 
    else if (joueurEngagement == 1 && clicRouge) {
      enAttenteEngagement = false;
      direction = -1; // Le joueur de droite (rouge) engage vers la gauche
      timerMouvement = millis();
    }
  } else {
    // 2. Logique de frappe (Renvoi)
    // Si la balle se dirige vers la GAUCHE (c'est au Vert de jouer)
    if (direction == -1 && clicVert) {
      if (posX <= tailleRaquette) {
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
    }
    
    // Si la balle se dirige vers la DROITE (c'est au Rouge de jouer)
    if (direction == 1 && clicRouge) {
      if (posX >= (TRACK_LENGTH - tailleRaquette + 1)) { 
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
          // Animation de victoire globale plus douce : balayage de l'écran
          CRGB couleurGagnant = (viesVert <= 0) ? CRGB::Red : CRGB::Green;
          FastLED.clear();
          for (int x = 1; x <= MATRIX_WIDTH; x++) {
            // Le balayage part du camp du gagnant
            int colonne = (viesVert <= 0) ? (MATRIX_WIDTH - x + 1) : x; 
            for (int y = 1; y <= MATRIX_HEIGHT; y++) {
              leds[XY(colonne, y)] = couleurGagnant;
            }
            FastLED.show();
            delay(30);
          }
          delay(1000); // Petite pause à la fin du balayage
          partieTerminee = true;
        } else {
          // Animation de point marqué (manche perdue)
          for (int i = 0; i < 3; i++) {
            FastLED.clear(); FastLED.show(); delay(100);
            for (int p = 1; p <= TRACK_LENGTH; p++) {
              int bx = (p <= 32) ? p : p - 32;
              int by = (p <= 32) ? 4 : 5;
              leds[XY(bx, by)] = couleurPoint;
            }
            FastLED.show(); delay(200);
          }
        }
        
        // Remise à zéro pour l'engagement suivant
        posX = (joueurEngagement == -1) ? 16 : 48; // Milieu de la ligne 4 (16) ou 5 (48)
        enAttenteEngagement = true; // On bloque en attente
        vitesseJeu = 80; 
        tailleRaquette = 5; // On réinitialise la taille des raquettes
        compteurEchangesMax = 0; // On réinitialise le compteur
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