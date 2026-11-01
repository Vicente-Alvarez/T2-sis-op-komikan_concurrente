#define _POSIX_C_SOURCE 200809L
#include "tablero.h"

#include <errno.h>
#include <string.h>
#include <time.h>

// Estructura para registrar los saltos del puma
typedef struct { int origen, medio, destino; } Salto;

#define MAX_SALTOS 128

static int g_ady[TOTAL_CASILLAS][MAX_VECINOS];
static int g_ady_n[TOTAL_CASILLAS];
static Salto g_saltos[MAX_SALTOS];
static int g_saltos_n;
static pthread_once_t g_tablas_once = PTHREAD_ONCE_INIT;

// Agrega vecino en ambas direcciones
static void agregar_arista(int a, int b) {
g_ady[a][g_ady_n[a]++] = b;
g_ady[b][g_ady_n[b]++] = a;
}

// Guarda un salto posible
static void agregar_salto(int o, int m, int d) {
g_saltos[g_saltos_n++] = (Salto){o, m, d};
}

// Arma las lineas continuas para adyacencias y saltos
static void agregar_recta(const int* r, int n) {
for (int i = 0; i + 1 < n; i++) agregar_arista(r[i], r[i + 1]);
for (int i = 0; i + 2 < n; i++) {
agregar_salto(r[i], r[i + 1], r[i + 2]);
agregar_salto(r[i + 2], r[i + 1], r[i]);
}
}

// Define la mapa de conexiones (filas, columnas y trono)
static void construir_tablas(void) {
for (int f = 0; f < GRILLA_FILAS; f++) {
int r[GRILLA_COLS];
for (int c = 0; c < GRILLA_COLS; c++) r[c] = f * GRILLA_COLS + c;
agregar_recta(r, GRILLA_COLS);
}
static const int col0[] = {0, 5, 10, 15, 20};
static const int col1[] = {CASILLA_TRONO_IZQ, 1, 6, 11, 16, 21};
static const int col2[] = {CASILLA_CUSPIDE, 2, 7, 12, 17, 22};
static const int col3[] = {CASILLA_TRONO_DER, 3, 8, 13, 18, 23};
static const int col4[] = {4, 9, 14, 19, 24};
static const int diag_izq[] = {0, CASILLA_TRONO_IZQ, CASILLA_CUSPIDE};
static const int diag_der[] = {4, CASILLA_TRONO_DER, CASILLA_CUSPIDE};
static const int base_trono[] = {CASILLA_TRONO_IZQ, CASILLA_TRONO_DER};
agregar_recta(col0, 5);
agregar_recta(col1, 6);
agregar_recta(col2, 6);
agregar_recta(col3, 6);
agregar_recta(col4, 5);
agregar_recta(diag_izq, 3);
agregar_recta(diag_der, 3);
agregar_recta(base_trono, 2);
}

// Corre la tabla de conexiones una sola vez
static void tablas(void) { pthread_once(&g_tablas_once, construir_tablas); }

static bool casilla_valida(int i) { return i >= 0 && i < TOTAL_CASILLAS; }

// Chequea si dos casillas se tocan
bool es_adyacente(int origen, int destino) {
if (!casilla_valida(origen) || !casilla_valida(destino)) return false;
tablas();
for (int k = 0; k < g_ady_n[origen]; k++)
if (g_ady[origen][k] == destino) return true;
return false;
}

// Valida si un perro se puede mover
bool movimiento_valido_perro(const Tablero* t, int origen, int destino) {
if (!casilla_valida(origen) || !casilla_valida(destino)) return false;
if (t->casillas[origen].tipo != CASILLA_PERRO) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;
return es_adyacente(origen, destino);
}

// Valida paso o salto del puma
bool movimiento_valido_puma(const Tablero* t, int origen, int destino, int* casilla_capturada) {
int dummy;
if (!casilla_capturada) casilla_capturada = &dummy;
*casilla_capturada = -1;
if (!casilla_valida(origen) || !casilla_valida(destino)) return false;
if (t->casillas[origen].tipo != CASILLA_PUMA) return false;
if (t->casillas[destino].tipo != CASILLA_VACIA) return false;
if (es_adyacente(origen, destino)) return true;
tablas();
for (int k = 0; k < g_saltos_n; k++) {
const Salto* s = &g_saltos[k];
if (s->origen == origen && s->destino == destino &&
t->casillas[s->medio].tipo == CASILLA_PERRO) {
*casilla_capturada = s->medio;
return true;
}
}
return false;
}

// Retorna todas las jugadas posibles del puma
int listar_movimientos_puma(const Tablero* t, Movimiento out[MAX_MOVIMIENTOS]) {
tablas();
int n = 0, o = t->pos_puma;
if (!casilla_valida(o)) return 0;
for (int k = 0; k < g_ady_n[o]; k++) {
int d = g_ady[o][k];
if (t->casillas[d].tipo == CASILLA_VACIA && n < MAX_MOVIMIENTOS) out[n++] = (Movimiento){d, -1};
}
for (int k = 0; k < g_saltos_n; k++) {
const Salto* s = &g_saltos[k];
if (s->origen == o && t->casillas[s->destino].tipo == CASILLA_VACIA &&
t->casillas[s->medio].tipo == CASILLA_PERRO && n < MAX_MOVIMIENTOS)
out[n++] = (Movimiento){s->destino, s->medio};
}
return n;
}

// Retorna los pasos que puede dar un perro
int listar_movimientos_perro(const Tablero* t, int id_perro, int out_destinos[MAX_VECINOS]) {
tablas();
if (id_perro < 0 || id_perro >= MAX_PERROS) return 0;
int o = t->pos_perro[id_perro], n = 0;
if (!casilla_valida(o)) return 0;
for (int k = 0; k < g_ady_n[o]; k++) {
int d = g_ady[o][k];
if (t->casillas[d].tipo == CASILLA_VACIA) out_destinos[n++] = d;
}
return n;
}

// Chequea si el puma no tiene movimientos
bool puma_esta_acorralado(const Tablero* t) {
Movimiento m[MAX_MOVIMIENTOS];
return listar_movimientos_puma(t, m) == 0;
}

// Cierra la partida internamente y avisa a los hilos
static void terminar_locked(Tablero* t, estado_juego_t e) {
if (t->estado == JUEGO_EN_CURSO) {
t->estado = e;
pthread_cond_broadcast(&t->cond_fin);
}
}

// Revisa si alguien ya gano
static void evaluar_fin_locked(Tablero* t) {
if (t->perros_capturados >= t->umbral_victoria) terminar_locked(t, JUEGO_GANA_PUMA);
else if (puma_esta_acorralado(t)) terminar_locked(t, JUEGO_GANA_PERROS);
}

// Aplica la jugada del puma en el tablero
bool aplicar_movimiento_puma(Tablero* t, int destino, int* captura_out) {
int captura = -1;
if (captura_out) *captura_out = -1;
if (t->estado != JUEGO_EN_CURSO) return false;
if (!movimiento_valido_puma(t, t->pos_puma, destino, &captura)) return false;

t->casillas[t->pos_puma].tipo = CASILLA_VACIA;
t->casillas[destino].tipo = CASILLA_PUMA;
t->pos_puma = destino;
if (captura >= 0) {
t->casillas[captura].tipo = CASILLA_VACIA;
for (int i = 0; i < MAX_PERROS; i++)
if (t->pos_perro[i] == captura) t->pos_perro[i] = -1;
t->perros_capturados++;
}
t->movimientos++;
if (captura_out) *captura_out = captura;
evaluar_fin_locked(t);
return true;
}

// Aplica el paso de un perro en el tablero
bool aplicar_movimiento_perro(Tablero* t, int id_perro, int destino) {
if (t->estado != JUEGO_EN_CURSO) return false;
if (id_perro < 0 || id_perro >= MAX_PERROS) return false;
int origen = t->pos_perro[id_perro];
if (!movimiento_valido_perro(t, origen, destino)) return false;

t->casillas[origen].tipo = CASILLA_VACIA;
t->casillas[destino].tipo = CASILLA_PERRO;
t->pos_perro[id_perro] = destino;
t->movimientos++;
evaluar_fin_locked(t);
return true;
}

// Consulta si la partida acabo
bool tablero_terminado(Tablero* t) {
pthread_mutex_lock(&t->lock_tablero);
bool fin = t->estado != JUEGO_EN_CURSO;
pthread_mutex_unlock(&t->lock_tablero);
return fin;
}

// Lee el estado con mutex
estado_juego_t tablero_estado(Tablero* t) {
pthread_mutex_lock(&t->lock_tablero);
estado_juego_t e = t->estado;
pthread_mutex_unlock(&t->lock_tablero);
return e;
}

// Fuerza el cierre de la mesa
void tablero_terminar(Tablero* t, estado_juego_t estado) {
pthread_mutex_lock(&t->lock_tablero);
terminar_locked(t, estado);
pthread_mutex_unlock(&t->lock_tablero);
}

// Hace esperar a los hilos de forma eficiente
bool tablero_dormir(Tablero* t, int ms) {
struct timespec dl;
clock_gettime(CLOCK_MONOTONIC, &dl);
dl.tv_sec += ms / 1000;
dl.tv_nsec += (long)(ms % 1000) * 1000000L;
if (dl.tv_nsec >= 1000000000L) { dl.tv_sec++; dl.tv_nsec -= 1000000000L; }

pthread_mutex_lock(&t->lock_tablero);
while (t->estado == JUEGO_EN_CURSO && ms > 0)
if (pthread_cond_timedwait(&t->cond_fin, &t->lock_tablero, &dl) == ETIMEDOUT) break;
bool sigue = t->estado == JUEGO_EN_CURSO;
pthread_mutex_unlock(&t->lock_tablero);
return sigue;
}

// Lee la configuracion del mapa inicial
static int leer_disposicion(const config_t* cfg, char celdas[TOTAL_CASILLAS], bool silencioso) {
char seq[CFG_MAX_FILAS * CFG_MAX_COLS + 1];
int n = 0;
for (int f = 0; f < cfg->disp_filas; f++)
for (const char* s = cfg->disposicion[f]; *s; s++) {
if (*s == ' ' || *s == '\t') continue;
if (*s != 'P' && *s != 'D' && *s != '.') {
if (!silencioso) fprintf(stderr, "tablero: simbolo '%c' invalido en la disposicion (use P, D, .)\n", *s);
return -1;
}
if (n < (int)sizeof seq - 1) seq[n] = *s;
n++;
}
if (n != NUM_CASILLAS_GRILLA && n != TOTAL_CASILLAS) {
if (!silencioso)
fprintf(stderr, "tablero: la disposicion debe tener 25 (grilla) o 28 (trono + grilla) simbolos, hay %d\n", n);
return -1;
}
memset(celdas, '.', TOTAL_CASILLAS);
if (n == TOTAL_CASILLAS) {
celdas[CASILLA_CUSPIDE] = seq[0];
celdas[CASILLA_TRONO_IZQ] = seq[1];
celdas[CASILLA_TRONO_DER] = seq[2];
memcpy(celdas, seq + 3, NUM_CASILLAS_GRILLA);
} else {
memcpy(celdas, seq, NUM_CASILLAS_GRILLA);
}
int p = 0, d = 0;
for (int i = 0; i < TOTAL_CASILLAS; i++) { p += celdas[i] == 'P'; d += celdas[i] == 'D'; }
if (p != 1 || d < 1 || d > MAX_PERROS) {
if (!silencioso)
fprintf(stderr, "tablero: la disposicion necesita exactamente 1 'P' y entre 1 y %d 'D' (hay P=%d, D=%d)\n",
MAX_PERROS, p, d);
return -1;
}
return 0;
}

// Revisa que la disposicion de inicio este bien
int tablero_validar_disposicion(const config_t* cfg) {
if (!cfg || !cfg->usa_disposicion) return 0;
char celdas[TOTAL_CASILLAS];
return leer_disposicion(cfg, celdas, false);
}

// Coloca las piezas por defecto si no hay config
static void disposicion_por_defecto(char celdas[TOTAL_CASILLAS]) {
memset(celdas, '.', TOTAL_CASILLAS);
celdas[12] = 'P';
for (int i = 0; i < MAX_PERROS; i++)
celdas[i] = 'D';
}

// Inicia el tablero completo y sus locks
int inicializar_tablero(Tablero* t, const config_t* cfg) {
char celdas[TOTAL_CASILLAS];
tablas();
if (cfg && cfg->usa_disposicion) {
if (leer_disposicion(cfg, celdas, false) != 0) return -1;
} else {
disposicion_por_defecto(celdas);
}

memset(t, 0, sizeof *t);
pthread_condattr_t ca;
if (pthread_mutex_init(&t->lock_tablero, NULL) != 0) return -1;
if (pthread_condattr_init(&ca) != 0) { pthread_mutex_destroy(&t->lock_tablero); return -1; }
pthread_condattr_setclock(&ca, CLOCK_MONOTONIC);
int rc = pthread_cond_init(&t->cond_fin, &ca);
pthread_condattr_destroy(&ca);
if (rc != 0) { pthread_mutex_destroy(&t->lock_tablero); return -1; }
t->listo = true;

for (int i = 0; i < MAX_PERROS; i++) t->pos_perro[i] = -1;
t->pos_puma = -1;
for (int i = 0; i < TOTAL_CASILLAS; i++) {
t->casillas[i].id = i;
t->casillas[i].tipo = CASILLA_VACIA;
if (celdas[i] == 'P') {
t->casillas[i].tipo = CASILLA_PUMA;
t->pos_puma = i;
} else if (celdas[i] == 'D') {
t->casillas[i].tipo = CASILLA_PERRO;
t->pos_perro[t->perros_iniciales++] = i;
}
}
t->umbral_victoria = t->perros_iniciales < CAPTURAS_PARA_GANAR ? t->perros_iniciales : CAPTURAS_PARA_GANAR;
t->estado = JUEGO_EN_CURSO;

pthread_mutex_lock(&t->lock_tablero);
evaluar_fin_locked(t);
pthread_mutex_unlock(&t->lock_tablero);
return 0;
}

// Libera los recursos de sincronizacion
void destruir_tablero(Tablero* t) {
if (!t->listo) return;
pthread_cond_destroy(&t->cond_fin);
pthread_mutex_destroy(&t->lock_tablero);
t->listo = false;
}