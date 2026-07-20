# Contexte du Projet et Règles de Développement (PixelCross)

Ce fichier sert de référence pour les assistants IA travaillant sur ce projet afin de maintenir la cohérence des choix techniques et architecturaux.

## Matériel et Contraintes
- **Microcontrôleur** : ESP32-C3 SuperMini.
- **Affichage** : Matrice LED WS2812B 8x32 (256 LEDs au total).
- **Score** : 2x Modules 4x7 Segments (via 74HC595 en série). SDI sur GPIO 8, SCLK sur GPIO 9, LOAD sur GPIO 10. `DIGITS_PER_MODULE = 4`.
- **Alimentation** : 5V 3A externe. **Règle stricte** : Le logiciel (FastLED) doit toujours limiter la consommation à 2500mA maximum pour des raisons de sécurité.
- **Boutons** : 
  - Joueur Vert : GPIO 0 (Action/Validation), GPIO 1 (Menu Gauche), GPIO 2 (Menu Droite)
  - Joueur Rouge : GPIO 5 (Action), GPIO 6 (Futur Gauche), GPIO 7 (Futur Droite)
- **Audio** : Buzzer passif sur GPIO 4 (contrôlé via PWM matériel natif `ledc` pour être non-bloquant).
- **WiFi** : Le module WiFi de l'ESP32-C3 est sensible. Une réduction de la puissance d'émission (`setTxPower`) est **obligatoire** pour un fonctionnement stable, surtout en mode Point d'Accès.

## Architecture et Choix Techniques
- **Framework** : Arduino (via PlatformIO).
- **Bibliothèque LED** : FastLED.
- **Topologie Matrice** : Le terrain de jeu utilise une fonction `XY(x, y)` sur mesure car le câblage de la matrice physique serpente (en zigzag). Coordonnées 1-indexées.
- **Espace de Jeu Pong (Track)** : La piste fait 64 LEDs de long, utilisant la ligne 4 (camp gauche) et la ligne 5 (camp droit).
- **Interface (HUD)** : Les vies du joueur Vert (Gauche) sont affichées sur la ligne 1. Celles du joueur Rouge (Droit) sur la ligne 8.
- **Score Externe** : Les afficheurs 7 segments affichent les highscores, compteurs d'échanges, temps, etc. selon le contexte du jeu.
- **Table de partition** : Le projet utilise un fichier `default_ota.csv` pour définir une structure de mémoire compatible avec les mises à jour OTA.
- **Gestion WiFi** : La bibliothèque `tzapu/WiFiManager` est utilisée pour créer un portail de configuration captif.
- **Mises à jour OTA** : Le système peut se mettre à jour via WiFi en interrogeant l'API GitHub pour la "dernière release". Il télécharge le binaire, vérifie son intégrité avec une empreinte MD5, et l'installe. Le script `post_build.py` peut créer automatiquement la release GitHub si un token est fourni.
- **Version firmware** : Format string "X.Y" (comparaison numérique composant par composant). Le `version.json` contient le MD5 indexé par plateforme (`md5["esp32-c3-devkitm-1"]`).
- **Machine d'états** : `enum AppState` dans `shared.h` : `STATE_MENU`, `STATE_PONG`, `STATE_CHRONO`, `STATE_GOAL`, `STATE_SETTINGS`, `STATE_TEST`, `STATE_WIFI_CONFIG`, `STATE_OTA_UPDATE`.
- **Menu principal** : PONG, CHRONO, GOAL, REGLAGES.
- **Sous-menu Réglages** : LIGHT, TEST, UPDATE, WIFI.

## Organisation du Code
- **`shared.h`** : Defines hardware, AppState, variables globales (leds, brightness), fonction XY().
- **`display.h/cpp`** : Police 3x5, drawChar, drawString, drawCenteredString, invertText.
- **`buzzer.h/cpp`** : Bips non-bloquants, bip double, effet pouvoir.
- **`score7seg.h/cpp`** : Affichage sur les modules 7 segments (entiers et décimaux).
- **`game.h/cpp`** : Code partagé entre les jeux (animations set gagnant, nouveau record, fanfare, verrou, vies).
- **`pong.h/cpp`** : Jeu Pong avec sous-menu SOLO/DUO, bot IA, compteur d'échanges, highscores.
- **`chrono.h/cpp`** : Jeu Chrono avec sous-menu SOLO/DUO, système de verrous, highscores.
- **`goal.h/cpp`** : Jeu Goal (football 2D, chrono 60s, mur mobile, highscore).
- **`settings.h/cpp`** : Menu réglages (luminosité, test, OTA, WiFi).
- **`test.h/cpp`** : Mode test hardware.
- **`main.cpp`** : Setup, loop, menu principal, écran d'accueil.
- **`post_build.py`** : Script post-compilation (packaging firmware, release GitHub automatique).

## Règles de Code (Coding Guidelines)
1. **Non-bloquant** : `delay()` interdit dans les boucles de jeu pendant les échanges. Tolérés pour les animations courtes (fin de partie, messages).
2. **Anti-rebond / Inputs** : Pull-ups internes (`INPUT_PULLUP`). Détection sur front descendant (HIGH → LOW).
3. **Langue** : Variables, fonctions et commentaires en **Français**.
4. **Rendu FastLED** : Logique → `FastLED.clear()` → couleurs → `FastLED.show()`.
5. **Navigation** : Vert 1 + Vert 2 simultanément = retour au menu/sous-menu parent.
6. **Highscores** : Sauvegardés en NVS (Preferences) uniquement quand un nouveau record est battu.
7. **Sous-menus jeux** : Chaque jeu a un sous-menu SOLO/DUO avec affichage du highscore correspondant sur les 7 segments.
8. **Code partagé** : Les animations et mécaniques communes (verrou, vies, fanfare, record) sont dans `game.h/cpp`.

## Mécaniques de jeu Pong
- **Accélération** : La vitesse augmente à chaque échange réussi.
- **Mort subite** : Raquettes rétrécissent au-delà de la vitesse max.
- **Système de Fautes** : Frapper dans le vide = perte de manche.
- **Verrou de Service** : LED orange, le défenseur doit déverrouiller avant le service.
- **Engagement** : Le perdant du point a l'avantage de l'engagement suivant.
- **Pouvoirs** : LED mur (la plus éloignée du centre) = obtenir un pouvoir. LED centre (la plus proche) = détruire le pouvoir adverse.
  - Pouvoir 1 = Dash (15 LEDs à vitesse fulgurante)
  - Pouvoir 2 = Invisibilité (16 LEDs)
  - Pouvoir 3 = Bouclier (passif, rebond auto)
- **Sons dynamiques** : Pitch monte de +50Hz à chaque accélération.
- **Mode SOLO** : Bot imbattable, seul pouvoir 3 disponible, score = échanges cumulés.

## Mécaniques de jeu Chrono
- **Durée cible** : Aléatoire entre 9 et 30 secondes (entières).
- **Chrono visible** : Affiché par incrément de 1s pendant les 5 premières secondes, puis éteint.
- **Écart** : Valeur absolue entre le temps d'appui et la cible, en centièmes de seconde.
- **Mode SOLO** : 3 manches systématiques, score = écart cumulé (plus petit = meilleur).
- **Mode DUO** : Le plus gros écart perd une vie. Highscore = cumul des deux joueurs.
- **Affichage** : Temps au format SS.MM (4 digits) sur les 7 segments.
- **Verrou** : Identique au Pong, LED jaune après les vies.

## Mécaniques de jeu Goal
- **Terrain** : Ligne 1 = Rouge, Ligne 2 = Ballon rouge, Lignes 4-5 = Mur bleu avec trou, Ligne 7 = Ballon vert, Ligne 8 = Vert. Lignes 3 et 6 = transit.
- **Joueurs** : 3 LEDs (gardien) déplaçables latéralement avec G1/G2 ou R1/R2. Position limitée de 2 à 31.
- **Ballon** : Part du centre du joueur, avance d'une ligne par tempo. Couleur du joueur (jaune pendant le lock).
- **Mur** : Bleu, trou de 3 LEDs qui rebondit de gauche à droite. Même tempo que la balle.
- **Tir** : Un seul ballon en vol par joueur. Nouveau ballon uniquement après interception ou but.
- **Interception** : Mur (ballon sur ligne 4/5 hors trou) ou gardien adverse (ballon sur ligne 1/8 dans les 3 LEDs).
- **Collision ballon-ballon** : Si même case, les deux s'annulent.
- **Tempo** : Unique pour tout (balle, mur, joueur). 500ms → 150ms linéaire sur 60 secondes.
- **Score** : Chrono 60s affiché sur 7 segments (format SS.BB). Highscore = total buts des deux joueurs.
- **Fin de partie** : Celui avec le plus de buts gagne. Égalité = premier buteur gagne.
- **Verrou** : LED jaune à la place du ballon. Les deux joueurs doivent déverrouiller pour démarrer.

## Idées et Améliorations futures
- Ajout d'autres jeux (Simon, Reaction, Runner...).
- Animation d'attente (Idle) lorsque le jeu n'est pas utilisé.
- Épinglage du certificat HTTPS pour l'OTA.
