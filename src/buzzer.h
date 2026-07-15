#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "shared.h"

// Fonctions pour declencher des sons non-bloquants
void declencherBip(unsigned int frequence, unsigned long duree);
void declencherBipDouble(unsigned int f1, unsigned int f2, unsigned long d1, unsigned long d2);
void declencherEffetPouvoir();

// Fonction a appeler dans loop() pour gerer les timers du buzzer
void gererBuzzer();

#endif
