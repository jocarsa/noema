// src/runtime.c
#include "runtime.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_VARS 1000
#define NAME_MAX NOEMA_TOKEN_VALUE_MAX

static char* xstrdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

typedef struct {
    char name[NAME_MAX];
    Value v;
    int in_use;
} Var;

struct Runtime {
    Var vars[MAX_VARS];
};

static void value_free(Value *v) {
    if (!v) return;
    if (v->kind == VAL_STRING && v->string_value) {
        free(v->string_value);
        v->string_value = NULL;
    }
    v->kind = VAL_NULL;
    v->int_value = 0;
}

static Value value_copy(const Value *src) {
    Value out;
    memset(&out, 0, sizeof(out));
    out.kind = src->kind;
    out.int_value = src->int_value;
    if (src->kind == VAL_STRING && src->string_value) {
        out.string_value = xstrdup(src->string_value);
        if (!out.string_value) out.string_value = xstrdup("");
    }
    return out;
}

static Var* find_var(Runtime *rt, const char *name) {
    for (int i = 0; i < MAX_VARS; i++) {
        if (rt->vars[i].in_use && strcmp(rt->vars[i].name, name) == 0) {
            return &rt->vars[i];
        }
    }
    return NULL;
}

static Var* upsert_var(Runtime *rt, const char *name) {
    Var *v = find_var(rt, name);
    if (v) return v;

    for (int i = 0; i < MAX_VARS; i++) {
        if (!rt->vars[i].in_use) {
            rt->vars[i].in_use = 1;
            strncpy(rt->vars[i].name, name, NAME_MAX - 1);
            rt->vars[i].name[NAME_MAX - 1] = '\0';
            rt->vars[i].v.kind = VAL_NULL;
            rt->vars[i].v.int_value = 0;
            rt->vars[i].v.string_value = NULL;
            return &rt->vars[i];
        }
    }
    return NULL;
}

static Value expr_to_value(Runtime *rt, const Expr *e, int line, int col, const char *path, char *err, int cap) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_NULL;

    switch (e->kind) {
        case EXPR_INT:
            v.kind = VAL_INT;
            v.int_value = e->int_value;
            return v;

        case EXPR_STRING:
            v.kind = VAL_STRING;
            v.string_value = xstrdup(e->text);
            if (!v.string_value) v.string_value = xstrdup("");
            return v;

        case EXPR_BOOL:
            v.kind = VAL_BOOL;
            v.int_value = e->int_value ? 1 : 0;
            return v;

        case EXPR_NULL:
            v.kind = VAL_NULL;
            return v;

        case EXPR_IDENT: {
            Var *var = find_var(rt, e->text);
            if (!var) {
                // Avoid truncation warnings by keeping this buffer larger than worst case.
                char msg[320];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", e->text);
                diag_format(err, cap, path, line, col, "runtime error", msg);
                return v;
            }
            return value_copy(&var->v);
        }

        default:
            diag_format(err, cap, path, line, col, "runtime error", "unsupported expression");
            return v;
    }
}

static void print_value(const Value *v) {
    switch (v->kind) {
        case VAL_STRING:
            printf("%s\n", v->string_value ? v->string_value : "");
            break;
        case VAL_INT:
            printf("%d\n", v->int_value);
            break;
        case VAL_BOOL:
            printf("%s\n", v->int_value ? "verum" : "falsum");
            break;
        case VAL_NULL:
        default:
            printf("nulla\n");
            break;
    }
}

Runtime* runtime_create(void) {
    Runtime *rt = (Runtime*)calloc(1, sizeof(Runtime));
    return rt;
}

void runtime_destroy(Runtime *rt) {
    if (!rt) return;
    for (int i = 0; i < MAX_VARS; i++) {
        if (rt->vars[i].in_use) {
            value_free(&rt->vars[i].v);
            rt->vars[i].in_use = 0;
        }
    }
    free(rt);
}

int runtime_exec(Runtime *rt, Stmt *program, const char *path, char *err_out, int err_cap) {
    // Permite programa vacío
    if (!rt) return 0;

    if (!err_out || err_cap <= 0) return 0;
    err_out[0] = '\0';

    if (!path || !path[0]) path = "<input>";

    for (Stmt *s = program; s; s = s->next) {
        if (s->kind == STMT_IMPORT) {
            // no-op: "sonus" built-in (por ahora)
            continue;
        }

        if (s->kind == STMT_ASSIGN) {
            Var *var = upsert_var(rt, s->target);
            if (!var) {
                diag_format(err_out, err_cap, path, s->line, s->col, "runtime error", "too many variables");
                return 0;
            }

            char tmp[512]; tmp[0] = '\0';
            Value rhs = expr_to_value(rt, &s->value, s->line, s->col, path, tmp, (int)sizeof(tmp));
            if (tmp[0]) {
                snprintf(err_out, err_cap, "%s", tmp);
                value_free(&rhs);
                return 0;
            }

            value_free(&var->v);
            var->v = rhs;
            continue;
        }

        if (s->kind == STMT_CALL_PRINT) {
            char tmp[512]; tmp[0] = '\0';
            Value v = expr_to_value(rt, &s->arg, s->line, s->col, path, tmp, (int)sizeof(tmp));
            if (tmp[0]) {
                snprintf(err_out, err_cap, "%s", tmp);
                value_free(&v);
                return 0;
            }
            print_value(&v);
            value_free(&v);
            continue;
        }

        diag_format(err_out, err_cap, path, s->line, s->col, "runtime error", "unknown statement kind");
        return 0;
    }

    return 1;
}

