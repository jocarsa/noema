#ifndef NOEMA_DIAG_H
#define NOEMA_DIAG_H

#ifdef __cplusplus
extern "C" {
#endif

// Escribe un error estilo: path:line:col: <kind>: <msg>
void diag_format(char *out, int cap,
                 const char *path, int line, int col,
                 const char *kind, const char *msg);

#ifdef __cplusplus
}
#endif

#endif

