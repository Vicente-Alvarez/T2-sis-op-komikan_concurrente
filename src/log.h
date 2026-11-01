#ifndef LOG_H
#define LOG_H

#include "tablero.h"

// Imprime el tablero de forma limpia sin mezclar mensajes
void imprimir_tablero_consola(const Tablero* t, int id_mesa);

// Formatea eventos con marca de tiempo y mesa
void registrar_evento(int id_mesa, const char* fmt, ...) __attribute__((format(printf, 2, 3)));

#endif