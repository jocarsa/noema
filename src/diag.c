#include "diag.h"
#include <stdio.h>

void diag_format(char *out, int cap,
                 const char *path, int line, int col,
                 const char *kind, const char *msg)
{
    if (!out || cap <= 0) return;
    if (!path) path = "<stdin>";
    if (!kind) kind = "error";
    if (!msg) msg = "unknown";

    // line/col pueden ser 0 si no se conocen
    if (line > 0 && col > 0) {
        snprintf(out, cap, "%s:%d:%d: %s: %s", path, line, col, kind, msg);
    } else if (line > 0) {
        snprintf(out, cap, "%s:%d: %s: %s", path, line, kind, msg);
    } else {
        snprintf(out, cap, "%s: %s: %s", path, kind, msg);
    }
}

