#ifndef LOG_H
#define LOG_H

#include "tablero.h"

void imprimir_tablero_consola(Tablero* t, int id_mesa);
void registrar_evento(int id_mesa, const char* mensaje);

#endif // LOG_H