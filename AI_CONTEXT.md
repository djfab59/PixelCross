# Contexte du Projet et Règles de Développement (PixelCross)

Ce fichier sert de référence pour les assistants IA travaillant sur ce projet afin de maintenir la cohérence des choix techniques et architecturaux.

## Matériel et Contraintes
- **Microcontrôleur** : ESP32-C3 SuperMini.
- **Affichage** : Matrice LED WS2812B 8x32 (256 LEDs au total).
- **Alimentation** : 5V 3A externe. **Règle stricte** : Le logiciel (FastLED) doit toujours limiter la consommation à 2500mA maximum pour des raisons de sécurité.
- **Boutons** : GPIO 0 (Vert/Gauche), GPIO 1 (Rouge/Droit).
- **Audio** : Buzzer passif sur GPIO 3 (contrôlé via PWM matériel natif `ledc` pour être non-bloquant).

## Architecture et Choix Techniques
- **Framework** : Arduino (via PlatformIO).
- **Bibliothèque LED** : FastLED.
- **Topologie Matrice** : Le terrain de jeu utilise une fonction `XY(x, y)` sur mesure car le câblage de la matrice physique serpente (en zigzag). 
- **Espace de Jeu (Track)** : La piste fait 64 LEDs de long, utilisant la ligne 4 (camp gauche) et la ligne 5 (camp droit).
- **Interface (HUD)** : Les vies du joueur Vert (Gauche) sont affichées sur la ligne 1. Celles du joueur Rouge (Droit) sur la ligne 8.

## Règles de Code (Coding Guidelines)
1. **Non-bloquant** : L'utilisation de la fonction `delay()` est **strictement interdite** dans la boucle principale (`loop()`) pendant le jeu, afin de ne pas rater les inputs ultra-rapides des boutons. Tout ce qui touche à la physique ou aux animations temporelles doit utiliser `millis()`.
2. **Anti-rebond / Inputs** : Les boutons utilisent les pull-ups internes (`INPUT_PULLUP`). La détection se fait sur le front descendant (passage de `HIGH` à `LOW`) en comparant l'état actuel et précédent, pour éviter la triche (maintien du bouton).
3. **Langue** : Les noms de variables, fonctions et les commentaires doivent être en **Français**. Le code doit être clair et organisé par blocs logiques.
4. **Rendu FastLED** : Le cycle d'affichage doit toujours respecter ce pattern : calcul de la logique -> `FastLED.clear()` -> application des couleurs -> `FastLED.show()`.

## Mécaniques de jeu (à ne pas casser)
- **Accélération** : La vitesse de la balle augmente (le délai en ms diminue) à chaque échange réussi.
- **Mort subite** : Une fois la vitesse maximale atteinte, la taille de la zone de renvoi (raquette) diminue toutes les deux frappes.
- **Système de Fautes** : Appuyer sur le bouton avant que la balle ne soit dans la zone de la raquette entraîne une perte immédiate de la manche (anti-spam).
- **Verrou de Service** : Après un point, le joueur en défense doit déverrouiller l'engagement (LED orange) en appuyant sur son bouton avant que le serveur puisse engager.
- **Pouvoirs Aléatoires** : Frapper sur la dernière LED octroie un pouvoir au hasard (1=Dash, 2=Invisibilité, 3=Bouclier passif). Frapper sur la première LED annule le pouvoir adverse.
- **Sons dynamiques** : Le pitch du buzzer monte de +50Hz à chaque accélération de la balle.

## Idées et Améliorations futures
- Ajout d'une animation d'attente (Idle) lorsque le jeu n'est pas utilisé.
- Variations de couleurs dynamiques en fonction de la vitesse de la balle.