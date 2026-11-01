#ifndef PIEZA_H
#define PIEZA_H

#include "tablero.h"
#include <semaphore.h>

typedef struct {
int id_mesa;
int id_pieza; // 0 para Puma, 1 a 12 para Perros
Tablero* tablero;
sem_t* sem_turno;
sem_t* sem_siguiente;
bool* juego_activo;
} ArgPieza;

void* hilo_puma(void* arg);
void* hilo_perro(void* arg);

#endif // PIEZA_H