#define _POSIX_C_SOURCE 200809L
#include "bitacora.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>

#include "util.h"

static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static FILE *g_archivo = NULL;
static long g_t0 = 0;

int bitacora_iniciar(const char *ruta_archivo) {
    g_t0 = util_ms_monotonico();
    if (ruta_archivo) {
        g_archivo = fopen(ruta_archivo, "w");
        if (!g_archivo) return -1;
    }
    return 0;
}

void bitacora_cerrar(void) {
    pthread_mutex_lock(&g_mtx);
    fflush(stdout);
    if (g_archivo) {
        fclose(g_archivo);
        g_archivo = NULL;
    }
    pthread_mutex_unlock(&g_mtx);
}

void bitacora_evento(int mesa_id, const char *fmt, ...) {
    char cuerpo[400], linea[480];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cuerpo, sizeof cuerpo, fmt, ap);
    va_end(ap);

    long ms = util_ms_monotonico() - g_t0;
    if (mesa_id < 0) snprintf(linea, sizeof linea, "[%7ld ms][servidor] %s\n", ms, cuerpo);
    else             snprintf(linea, sizeof linea, "[%7ld ms][mesa %2d ] %s\n", ms, mesa_id, cuerpo);

    pthread_mutex_lock(&g_mtx);
    fputs(linea, stdout);
    if (g_archivo) fputs(linea, g_archivo);
    pthread_mutex_unlock(&g_mtx);
}
