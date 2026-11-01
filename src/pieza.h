#ifndef PIEZA_H
#define PIEZA_H

#include "config.h"
#include "tablero.h"

// Struct con los datos que recibe cada hilo de pieza
typedef struct {
int id_mesa;
int id_pieza; // 0 para el Puma, 1 a 12 para Perros
Tablero* tablero;
const config_t* cfg; // Tiempos de retardo y configs
unsigned int semilla; // Semilla propia para numeros al azar
} ArgPieza;

// Hilos independientes para cada tipo de pieza
void* hilo_puma(void* arg);
void* hilo_perro(void* arg);

#endif