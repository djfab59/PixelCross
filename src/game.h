#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include <FastLED.h>
#include "shared.h"
#include "display.h"
#include "buzzer.h"
#include "score7seg.h"

// --- ANIMATIONS PARTAGEES ---

// Animation de fin de set : colonnes qui se remplissent de la couleur du gagnant
// vertGagne = true : remplissage de gauche a droite (vert), false : droite a gauche (rouge)
void animationSetGagnant(bool vertGagne);

// Animation nouveau record : clignotement dore/bleu + score qui clignote sur 7 segments
void animationNouveauRecord(int score);

// Fanfare de victoire (melodie)
void jouerFanfare();

// --- SYSTEME DE VERROU ---

// Dessine la LED de verrou (jaune) pour un joueur
// joueurVert = true : LED apres les vies du vert, false : apres les vies du rouge
void dessinerVerrou(bool joueurVert, int nbVies);

// --- AFFICHAGE DES VIES ---

// Dessine les vies d'un joueur sur la matrice
void dessinerViesVert(int vies);
void dessinerViesRouge(int vies);

// --- ANIMATION PERTE DE VIE ---

// Animation de clignotement quand un joueur perd une vie (avec piste coloree pour le pong)
// couleurPiste = couleur de la piste affichee pendant le clignotement (CRGB::Black pour ne pas afficher de piste)
void animationPerteVie(int viesVert, int viesRouge, bool vertAPerdu, CRGB couleurPiste = CRGB::Black);

// Animation de clignotement perte de vie (SOLO, sans affichage rouge)
void animationPerteVieSolo(int viesVert);

#endif
