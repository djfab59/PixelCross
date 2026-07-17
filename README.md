# 🏓 PixelCross - Console de jeux LED

PixelCross est une console de jeux rétro jouable sur une matrice de LEDs WS2812B (8x32). Développée sur un microcontrôleur **ESP32-C3 SuperMini**, elle transforme une simple matrice en un véritable terrain d'affrontement compétitif avec plusieurs jeux, un système de highscore, et des mises à jour OTA automatiques.

## ✨ Fonctionnalités

- **Multi-jeux** : Pong 1D (Solo/Duo) et Chrono (Solo/Duo), avec un système de menu et sous-menus.
- **Écran d'accueil** : Animation "PXLCROSS" qui descend avec changement de couleur et mélodie au démarrage.
- **Highscores** : Sauvegardés en mémoire NVS, affichés sur les 7 segments dans les sous-menus.
- **Mises à jour OTA** : Le firmware peut être mis à jour sans fil via WiFi directement depuis les "releases" du dépôt GitHub.
- **Menu de Réglages** : Luminosité (avec correction gamma), mise à jour OTA, configuration WiFi.
- **Audio Dynamique** : Effets sonores non-bloquants avec tension croissante et fanfares.
- **Gestion de l'alimentation** : Consommation plafonnée à 2.5A grâce au gestionnaire d'énergie de FastLED.

---

## 🎮 Jeux disponibles

### 🏓 PONG (Solo & Duo)

Un Pong 1D XXL sur 64 LEDs avec physique dynamique, pouvoirs aléatoires et mort subite.

**Mode SOLO :**
- Bot imbattable (renvoie toujours)
- 3 vies, seul le pouvoir Bouclier est disponible
- Score = nombre d'échanges cumulé sur les 3 vies
- Highscore sauvegardé en mémoire

**Mode DUO :**
- 2 joueurs, 3 vies chacun, tous les pouvoirs actifs
- Compteur d'échanges cumulé sur la partie
- Highscore duo (meilleur nombre d'échanges) sauvegardé
- Compteur de victoires (non sauvegardé, reset quand on quitte)

**Mécaniques :**
- Terrain de 64 LEDs (ligne 4 + ligne 5)
- Accélération à chaque échange
- Mort subite : raquettes rétrécissent au-delà de la vitesse max
- Verrou de service (LED orange)
- Pouvoirs : Dash, Invisibilité, Bouclier
- Contre : frapper sur la LED mur détruit le pouvoir adverse

### ⏱️ CHRONO (Solo & Duo)

Jeu de précision inspiré de Fort Boyard : deviner une durée dans sa tête.

**Déroulement :**
1. Une durée aléatoire (9-30 secondes) s'affiche sur la matrice
2. Le chrono s'affiche sur les 7 segments pendant 5 secondes puis s'éteint
3. Les joueurs comptent dans leur tête et appuient quand ils pensent que le temps est écoulé
4. L'écart par rapport à la cible détermine le résultat

**Mode SOLO :**
- 3 manches (perte d'une vie à chaque manche)
- Score = écart cumulé en centièmes de seconde (plus c'est petit, mieux c'est)
- Highscore sauvegardé

**Mode DUO :**
- 3 vies chacun, celui avec le plus gros écart perd une vie
- Highscore = plus petit écart cumulé des deux joueurs ensemble
- Animation spéciale si nouveau record

---

## 🛠️ Matériel Requis

- 1x **ESP32-C3 SuperMini** (ou équivalent)
- 1x **Matrice LED WS2812B 8x32** (256 LEDs)
- 2x Boutons poussoirs d'arcade (Vert et Rouge) + 4 boutons de navigation
- 2x Modules 4x7 Segments (via 74HC595 en série)
- 1x Buzzer passif + Transistor (ex: 2N2222) et résistance (1kΩ)
- 1x Alimentation externe 5V 3A

## 🔌 Câblage

⚠️ **ATTENTION :** Ne jamais alimenter la matrice LED 8x32 uniquement via l'ESP32. Utilisez une alimentation 5V externe et **reliez toutes les masses (GND) ensemble**.

| Composant | Broche | Connexion |
| :--- | :--- | :--- |
| **Matrice LED** | `DIN` | GPIO **3** |
| **Bouton Vert (Action)** | | GPIO **0** |
| **Bouton Vert 1 (Gauche)** | | GPIO **1** |
| **Bouton Vert 2 (Droite)** | | GPIO **2** |
| **Bouton Rouge (Action)** | | GPIO **5** |
| **Bouton Rouge 1** | | GPIO **6** |
| **Bouton Rouge 2** | | GPIO **7** |
| **Afficheurs 7 Seg** | `SDI` | GPIO **8** |
| | `SCLK` | GPIO **9** |
| | `LOAD` | GPIO **10** |
| **Buzzer** | Signal | GPIO **4** (via transistor) |

## 🚀 Installation & Compilation

Ce projet est conçu avec **PlatformIO** (VS Code).

1. Clonez ce dépôt.
2. Ouvrez le dossier dans VS Code avec PlatformIO.
3. Branchez l'ESP32-C3 via USB.
4. Cliquez sur **Upload and Monitor**.

### Dépendances (téléchargées automatiquement)
* `fastled/FastLED`
* `tzapu/WiFiManager`
* `bblanchon/ArduinoJson`

## 🧭 Navigation

- **Menu principal** : Vert 1 / Vert 2 pour naviguer, Vert pour valider.
- **Retour** : Appuyez simultanément sur Vert 1 + Vert 2 pour revenir en arrière.
- **Sous-menus jeux** : Même navigation pour choisir SOLO / DUO.

## 🧑‍💻 Processus de mise à jour OTA

1. Modifiez `FIRMWARE_VERSION` dans `src/shared.h` (format "X.Y").
2. Compilez avec `pio run`.
3. Le script `post_build.py` package le firmware et crée automatiquement la release GitHub (si `.github_token` est configuré).
4. Les appareils existants trouvent et installent la mise à jour via le menu "UPDATE".

### Publication automatique
Créez un fichier `.github_token` à la racine contenant votre Personal Access Token GitHub (permission `Contents: Read and write`).

---
*Créé avec passion (et quelques LEDs).*
