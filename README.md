# 🏓 PixelCross - Le Pong LED 1D XXL

PixelCross est une réinvention intense et dynamique du célèbre jeu Pong, jouable sur une matrice de LEDs WS2812B (8x32). Développé sur un microcontrôleur **ESP32-C3 SuperMini**, ce jeu transforme une simple matrice en un véritable terrain d'affrontement compétitif avec gestion de la physique, des fautes et de la mort subite !

## ✨ Fonctionnalités

- **Terrain XXL replié :** Le terrain de jeu fait 64 LEDs de long ! Il part de la ligne 4 (camp gauche) et se prolonge sans coupure sur la ligne 5 (camp droit) pour créer un effet de profondeur.
- **Physique dynamique :** La balle accélère à chaque échange réussi.
- **Mécanique de Faute (Anti-Spam) :** Appuyer avant que la balle n'entre dans la zone de votre raquette vous fait instantanément perdre la manche !
- **Mode "Mort Subite" :** Une fois la vitesse maximale de la balle atteinte, la taille des raquettes se réduit toutes les deux frappes (passant de 5 LEDs à 2 LEDs) pour forcer l'erreur.
- **Vies et Score :** 3 vies par joueur, affichées en rose (en haut pour le joueur gauche, en bas pour le joueur droit).
- **Verrou de Sécurité :** L'engagement est bloqué tant que le joueur en défense n'a pas signalé qu'il est prêt.
- **Pouvoirs Aléatoires :** Frappez la balle sur la dernière LED pour obtenir un "Dash", une "Invisibilité" ou un "Bouclier" !
- **Animations :** Clignotement de point, balayage coloré (wipe) pour le grand gagnant de la partie.
- **Audio Dynamique :** Effets sonores non-bloquants avec tension croissante et fanfare de victoire.
- **Gestion de l'alimentation :** Consommation plafonnée intelligemment à 2.5A grâce au gestionnaire d'énergie de FastLED.

## 🛠️ Matériel Requis

- 1x **ESP32-C3 SuperMini** (ou équivalent)
- 1x **Matrice LED WS2812B 8x32** (256 LEDs)
- 2x Boutons poussoirs d'arcade (ex: Vert et Rouge)
- 1x Buzzer passif + Transistor (ex: 2N2222) et résistance (1kΩ)
- 1x Alimentation externe 5V 3A (Recommandé pour ne pas griller le port USB)
- Des câbles Dupont

## 🔌 Câblage

⚠️ **ATTENTION :** Ne jamais alimenter la matrice LED 8x32 uniquement via l'ESP32 au risque de détruire votre carte ou votre port USB. Utilisez une alimentation 5V externe et **reliez toutes les masses (GND) ensemble**.

| Composant | Broche | Connexion ESP32-C3 / Alim |
| :--- | :--- | :--- |
| **Matrice LED** | `5V` (VCC) | 5V Alimentation Externe |
| | `GND` | GND (Alim) + GND (ESP32) |
| | `DIN` (Data) | GPIO **2** |
| **Bouton Vert (Gauche)** | Patte 1 | GPIO **0** |
| | Patte 2 | GND |
| **Bouton Rouge (Droit)** | Patte 1 | GPIO **1** |
| | Patte 2 | GND |
| **Buzzer** | Signal | GPIO **3** (via transistor) |

*Note : Les boutons utilisent le mode `INPUT_PULLUP` interne de l'ESP32, il n'y a donc pas besoin de résistances externes.*

## 🚀 Installation & Compilation

Ce projet est conçu avec **PlatformIO** (VS Code).

1. Clonez ce dépôt.
2. Ouvrez le dossier du projet dans **VS Code** avec l'extension **PlatformIO** installée.
3. Branchez votre ESP32-C3 SuperMini via USB.
4. Cliquez sur le bouton **Upload and Monitor** (la flèche en bas de l'écran) pour compiler le projet et le flasher sur la carte.

### Dépendances
La librairie FastLED sera téléchargée automatiquement par PlatformIO selon la configuration du fichier `platformio.ini` :
* `fastled/FastLED`

## 🎮 Comment Jouer ?

**1. Démarrage et Engagement**
* Le joueur qui vient de gagner le point obtient l'engagement. La balle prend sa couleur.
* Le joueur en défense possède un **verrou (LED Orange)**. Il doit appuyer sur son bouton pour débloquer le service.
* L'attaquant appuie ensuite sur son bouton pour engager. S'il le fait trop tôt, une erreur sonore retentit !

**2. Les Échanges**
* La balle devient jaune.
* Le joueur adverse (Rouge) doit appuyer sur son bouton **uniquement** quand la balle survole sa zone de raquette (les LEDs lumineuses rouges en fin de piste).
* À chaque renvoi parfait, la vitesse de la balle augmente.

**3. Fautes et Pénalités**
* Taper dans le vide = Manche perdue (perte d'une vie).
* Rater la balle = Manche perdue.
* Le perdant obtient l'engagement pour la manche suivante (la balle prend sa couleur).

**4. Pouvoirs (Nouveau !)**
* Si vous frappez la balle exactement sur la **toute dernière LED** de votre camp, vous obtenez un pouvoir (LED Bleue).
* *Dash (LED 1)* : La balle fonce à une vitesse fulgurante sur 15 LEDs lors du renvoi.
* *Invisibilité (LED 2)* : La balle disparaît pendant 16 LEDs.
* *Bouclier (LED 3)* : Pouvoir passif. Si vous ratez la balle, elle rebondit automatiquement.
* *Contre* : Si vous frappez la balle sur la **première LED** de votre raquette, vous détruisez le pouvoir adverse.

**5. Mort Subite**
* Si les joueurs sont très bons et atteignent la vitesse max, la raquette de 5 LEDs commencera à se réduire (4, 3, puis 2 LEDs) rendant le renvoi extrêmement difficile !

**6. Fin de partie**
* Le premier joueur qui perd ses 3 vies (les points roses) perd la partie.
* Une grande animation aux couleurs du vainqueur balaie l'écran.
* Appuyez sur n'importe quel bouton pour relancer une toute nouvelle partie !

---
*Créé avec passion (et quelques LEDs).*