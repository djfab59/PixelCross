#ifndef SCORE7SEG_H
#define SCORE7SEG_H

#include <Arduino.h>
#include "shared.h"

// Fonction globale pour l'affichage du score sur les modules 7 segments physiques
void afficherScore7Seg(int scoreVert, int scoreRouge);
void afficherScoreDecimal7Seg(float scoreVert, float scoreRouge, int decimales);

#endif
