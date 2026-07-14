# Contexte du Projet et Règles de Développement (PixelCross)

Ce fichier sert de référence pour les assistants IA travaillant sur ce projet afin de maintenir la cohérence des choix techniques et architecturaux.

## Matériel et Contraintes
- **Microcontrôleur** : ESP32-C3 SuperMini.
- **Affichage** : Matrice LED WS2812B 8x32 (256 LEDs au total).
- **Score** : 2x Modules 7 Segments (via 74HC595 en série). SDI sur GPIO 8, SCLK sur GPIO 9, LOAD sur GPIO 10.
- **Alimentation** : 5V 3A externe. **Règle stricte** : Le logiciel (FastLED) doit toujours limiter la consommation à 2500mA maximum pour des raisons de sécurité.
- **Boutons** : 
  - Joueur Vert : GPIO 0 (Action/Validation), GPIO 1 (Menu Gauche), GPIO 2 (Menu Droite)
  - Joueur Rouge : GPIO 5 (Action), GPIO 6 (Futur Gauche), GPIO 7 (Futur Droite)
- **Audio** : Buzzer passif sur GPIO 4 (contrôlé via PWM matériel natif `ledc` pour être non-bloquant).
- **WiFi** : Le module WiFi de l'ESP32-C3 est sensible. Une réduction de la puissance d'émission (`setTxPower`) est **obligatoire** pour un fonctionnement stable, surtout en mode Point d'Accès.

## Architecture et Choix Techniques
- **Framework** : Arduino (via PlatformIO).
- **Bibliothèque LED** : FastLED.
- **Topologie Matrice** : Le terrain de jeu utilise une fonction `XY(x, y)` sur mesure car le câblage de la matrice physique serpente (en zigzag). 
- **Espace de Jeu (Track)** : La piste fait 64 LEDs de long, utilisant la ligne 4 (camp gauche) et la ligne 5 (camp droit).
- **Interface (HUD)** : Les vies du joueur Vert (Gauche) sont affichées sur la ligne 1. Celles du joueur Rouge (Droit) sur la ligne 8.
- **Score Externe** : Le nombre de victoires (Tournoi) est conservé sur les modules 7 segments (configurable via `DIGITS_PER_MODULE`).
- **Table de partition** : Le projet utilise un fichier `default_ota.csv` pour définir une structure de mémoire compatible avec les mises à jour OTA.
- **Gestion WiFi** : La bibliothèque `tzapu/WiFiManager` est utilisée pour créer un portail de configuration captif.
- **Mises à jour OTA** : Le système peut se mettre à jour via WiFi en interrogeant l'API GitHub pour la "dernière release". Il télécharge le binaire, vérifie son intégrité avec une empreinte MD5, et l'installe. La version et le MD5 sont gérés par un script Python (`copy_firmware.py`) qui s'exécute après la compilation.
- **Machine d'états** : Le projet utilise une machine d'états simple (`enum AppState`) pour gérer les différents modes : `STATE_MENU`, `STATE_PONG`, `STATE_SETTINGS`, `STATE_TEST`, `STATE_WIFI_CONFIG`, `STATE_OTA_UPDATE`.
- **Menu Réglages** : Permet de configurer la luminosité (sauvegardée en mémoire NVS), de lancer le portail de configuration WiFi, et de lancer le processus de mise à jour OTA.

## Règles de Code (Coding Guidelines)
1. **Non-bloquant** : L'utilisation de la fonction `delay()` est **strictement interdite** dans la boucle de jeu principale (`loopPong` pendant les échanges) afin de ne pas rater les inputs. Les `delay()` sont tolérés pour des animations courtes et bloquantes (ex: fin de partie, messages temporaires) où la réactivité n'est pas critique.
2. **Anti-rebond / Inputs** : Les boutons utilisent les pull-ups internes (`INPUT_PULLUP`). La détection se fait sur le front descendant (passage de `HIGH` à `LOW`) en comparant l'état actuel et précédent, pour éviter la triche (maintien du bouton).
3. **Langue** : Les noms de variables, fonctions et les commentaires doivent être en **Français**. Le code doit être clair et organisé par blocs logiques.
4. **Rendu FastLED** : Le cycle d'affichage doit toujours respecter ce pattern : calcul de la logique -> `FastLED.clear()` -> application des couleurs -> `FastLED.show()`.
5. **Navigation** : Pour quitter un jeu ou un menu, la norme est d'appuyer simultanément sur les deux boutons de navigation gauches (`Vert 1 + Vert 2`).

## Mécaniques de jeu (à ne pas casser)
- **Accélération** : La vitesse de la balle augmente (le délai en ms diminue) à chaque échange réussi.
- **Mort subite** : Une fois la vitesse maximale atteinte, la taille de la zone de renvoi (raquette) diminue toutes les deux frappes.
- **Système de Fautes** : Appuyer sur le bouton avant que la balle ne soit dans la zone de la raquette entraîne une perte immédiate de la manche (anti-spam).
- **Verrou de Service** : Après un point, le joueur en défense doit déverrouiller l'engagement (LED orange) en appuyant sur son bouton avant que le serveur puisse engager.
- **Engagement** : Le joueur qui a perdu le point (ou la partie précédente) a toujours l'avantage de l'engagement suivant.
- **Pouvoirs Aléatoires** : Frapper sur la dernière LED octroie un pouvoir au hasard (1=Dash, 2=Invisibilité, 3=Bouclier passif). Frapper sur la première LED annule le pouvoir adverse.
- **Sons dynamiques** : Le pitch du buzzer monte de +50Hz à chaque accélération de la balle.

## Idées et Améliorations futures
- Ajout d'une animation d'attente (Idle) lorsque le jeu n'est pas utilisé.
- Variations de couleurs dynamiques en fonction de la vitesse de la balle.