#ifndef PIEZA_H
#define PIEZA_H

#include "tablero.h"
#include <semaphore.h>

typedef struct {
int id_mesa;
int id_pieza; // 0 para el Puma, 1 a 12 para los Perros
Tablero* tablero;
sem_t* sem_turno;      // semaforo que espera esta pieza para jugar
sem_t* sem_siguiente;  // semaforo que activa la pieza para darle la mano al otro
bool* juego_activo;    // bandera global para saber si la partida sigue corriendo
} ArgPieza;

void* hilo_puma(void* arg);
void* hilo_perro(void* arg);

#endif // PIEZA_H