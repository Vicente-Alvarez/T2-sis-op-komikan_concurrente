#ifndef BITACORA_H
#define BITACORA_H

int bitacora_iniciar(const char *ruta_archivo);
void bitacora_cerrar(void);
void bitacora_evento(int mesa_id, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#endif
