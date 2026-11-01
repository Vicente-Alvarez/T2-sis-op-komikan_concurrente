#include "log.h"

#include <stdarg.h>
#include <stdio.h>

#include "bitacora.h"

// Traduce el entero a un caracter representativo (. P d)
static char simbolo(const Tablero* t, int casilla) {
switch (t->casillas[casilla].tipo) {
case CASILLA_PUMA: return 'P';
case CASILLA_PERRO: return 'd';
default: return '.';
}
}

// Arma el texto del tablero y lo manda a la bitacora de un golpe
void imprimir_tablero_consola(const Tablero* t, int id_mesa) {
char buf[320];
int n = snprintf(buf, sizeof buf,
"TABLERO (jugada #%lu | capturados %d/%d | perros en juego %d)\n"
" %c\n"
" %c %c\n",
t->movimientos, t->perros_capturados, t->umbral_victoria,
t->perros_iniciales - t->perros_capturados,
simbolo(t, CASILLA_CUSPIDE), simbolo(t, CASILLA_TRONO_IZQ), simbolo(t, CASILLA_TRONO_DER));
for (int f = 0; f < GRILLA_FILAS && n > 0 && n < (int)sizeof buf - 14; f++) {
n += snprintf(buf + n, sizeof buf - (size_t)n, " ");
for (int c = 0; c < GRILLA_COLS; c++)
n += snprintf(buf + n, sizeof buf - (size_t)n, "%c ", simbolo(t, f * GRILLA_COLS + c));
if (f + 1 < GRILLA_FILAS) n += snprintf(buf + n, sizeof buf - (size_t)n, "\n");
}
bitacora_evento(id_mesa, "%s", buf);
}

// Registra eventos generales en el log
void registrar_evento(int id_mesa, const char* fmt, ...) {
char buf[300];
va_list ap;
va_start(ap, fmt);
vsnprintf(buf, sizeof buf, fmt, ap);
va_end(ap);
bitacora_evento(id_mesa, "%s", buf);
}