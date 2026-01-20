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

static int value_truthy(const Value *v) {
    if (!v) return 0;
    if (v->kind == VAL_NULL) return 0;
    if (v->kind == VAL_BOOL) return v->int_value ? 1 : 0;
    if (v->kind == VAL_INT) return v->int_value != 0;
    if (v->kind == VAL_STRING) return (v->string_value && v->string_value[0]) ? 1 : 0;
    return 0;
}

static void runtime_type_error(char *err, int cap, const char *path, int line, int col, const char *msg) {
    diag_format(err, cap, path, line, col, "runtime error", msg);
}

static int values_equal(const Value *a, const Value *b) {
    if (a->kind != b->kind) return 0;
    switch (a->kind) {
        case VAL_NULL: return 1;
        case VAL_INT:  return a->int_value == b->int_value;
        case VAL_BOOL: return a->int_value == b->int_value;
        case VAL_STRING:
            if (!a->string_value && !b->string_value) return 1;
            if (!a->string_value || !b->string_value) return 0;
            return strcmp(a->string_value, b->string_value) == 0;
        default:
            return 0;
    }
}

static Value eval_expr(Runtime *rt, const Expr *e, int line, int col, const char *path, char *err, int cap);

static Value eval_unary(Runtime *rt, const Expr *e, const char *path, char *err, int cap) {
    Value rhs = eval_expr(rt, e->as.unary.rhs, e->line, e->col, path, err, cap);
    if (err[0]) return rhs;

    Value out;
    memset(&out, 0, sizeof(out));
    out.kind = VAL_NULL;

    if (e->as.unary.op == OP_NOT) {
        out.kind = VAL_BOOL;
        out.int_value = value_truthy(&rhs) ? 0 : 1;
        value_free(&rhs);
        return out;
    }

    if (e->as.unary.op == OP_NEG) {
        if (rhs.kind != VAL_INT) {
            runtime_type_error(err, cap, path, e->line, e->col, "unary '-' expects integer");
            value_free(&rhs);
            return out;
        }
        out.kind = VAL_INT;
        out.int_value = -rhs.int_value;
        value_free(&rhs);
        return out;
    }

    runtime_type_error(err, cap, path, e->line, e->col, "unsupported unary operator");
    value_free(&rhs);
    return out;
}

static Value make_bool(int b) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_BOOL;
    v.int_value = b ? 1 : 0;
    return v;
}

static Value make_int(int x) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_INT;
    v.int_value = x;
    return v;
}

static Value make_null(void) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_NULL;
    return v;
}

static Value make_string_owned(char *s) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_STRING;
    v.string_value = s ? s : xstrdup("");
    if (!v.string_value) v.string_value = xstrdup("");
    return v;
}

static Value eval_binary(Runtime *rt, const Expr *e, const char *path, char *err, int cap) {
    // short-circuit for et/aut
    if (e->as.binary.op == OP_AND) {
        Value lhs = eval_expr(rt, e->as.binary.lhs, e->line, e->col, path, err, cap);
        if (err[0]) return lhs;
        if (!value_truthy(&lhs)) {
            // return falsy lhs (like many languages); but for simplicity, return bool
            value_free(&lhs);
            return make_bool(0);
        }
        value_free(&lhs);
        Value rhs = eval_expr(rt, e->as.binary.rhs, e->line, e->col, path, err, cap);
        if (err[0]) return rhs;
        int b = value_truthy(&rhs);
        value_free(&rhs);
        return make_bool(b);
    }

    if (e->as.binary.op == OP_OR) {
        Value lhs = eval_expr(rt, e->as.binary.lhs, e->line, e->col, path, err, cap);
        if (err[0]) return lhs;
        if (value_truthy(&lhs)) {
            value_free(&lhs);
            return make_bool(1);
        }
        value_free(&lhs);
        Value rhs = eval_expr(rt, e->as.binary.rhs, e->line, e->col, path, err, cap);
        if (err[0]) return rhs;
        int b = value_truthy(&rhs);
        value_free(&rhs);
        return make_bool(b);
    }

    Value lhs = eval_expr(rt, e->as.binary.lhs, e->line, e->col, path, err, cap);
    if (err[0]) return lhs;

    Value rhs = eval_expr(rt, e->as.binary.rhs, e->line, e->col, path, err, cap);
    if (err[0]) { value_free(&lhs); return rhs; }

    // arithmetic + string concat
    if (e->as.binary.op == OP_ADD) {
        if (lhs.kind == VAL_INT && rhs.kind == VAL_INT) {
            Value out = make_int(lhs.int_value + rhs.int_value);
            value_free(&lhs); value_free(&rhs);
            return out;
        }
        if (lhs.kind == VAL_STRING && rhs.kind == VAL_STRING) {
            const char *a = lhs.string_value ? lhs.string_value : "";
            const char *b = rhs.string_value ? rhs.string_value : "";
            size_t na = strlen(a), nb = strlen(b);
            char *buf = (char*)malloc(na + nb + 1);
            if (!buf) {
                runtime_type_error(err, cap, path, e->line, e->col, "out of memory in string concatenation");
                value_free(&lhs); value_free(&rhs);
                return make_null();
            }
            memcpy(buf, a, na);
            memcpy(buf + na, b, nb);
            buf[na + nb] = '\0';
            value_free(&lhs); value_free(&rhs);
            return make_string_owned(buf);
        }
        runtime_type_error(err, cap, path, e->line, e->col, "operator '+' expects int+int or string+string");
        value_free(&lhs); value_free(&rhs);
        return make_null();
    }

    if (e->as.binary.op == OP_SUB || e->as.binary.op == OP_MUL ||
        e->as.binary.op == OP_DIV || e->as.binary.op == OP_MOD) {

        if (lhs.kind != VAL_INT || rhs.kind != VAL_INT) {
            runtime_type_error(err, cap, path, e->line, e->col, "arithmetic operators expect integers");
            value_free(&lhs); value_free(&rhs);
            return make_null();
        }

        if (e->as.binary.op == OP_SUB) {
            Value out = make_int(lhs.int_value - rhs.int_value);
            value_free(&lhs); value_free(&rhs);
            return out;
        }
        if (e->as.binary.op == OP_MUL) {
            Value out = make_int(lhs.int_value * rhs.int_value);
            value_free(&lhs); value_free(&rhs);
            return out;
        }
        if (e->as.binary.op == OP_DIV) {
            if (rhs.int_value == 0) {
                runtime_type_error(err, cap, path, e->line, e->col, "division by zero");
                value_free(&lhs); value_free(&rhs);
                return make_null();
            }
            Value out = make_int(lhs.int_value / rhs.int_value);
            value_free(&lhs); value_free(&rhs);
            return out;
        }
        if (e->as.binary.op == OP_MOD) {
            if (rhs.int_value == 0) {
                runtime_type_error(err, cap, path, e->line, e->col, "modulo by zero");
                value_free(&lhs); value_free(&rhs);
                return make_null();
            }
            Value out = make_int(lhs.int_value % rhs.int_value);
            value_free(&lhs); value_free(&rhs);
            return out;
        }
    }

    // comparisons/equality
    if (e->as.binary.op == OP_EQ || e->as.binary.op == OP_NE) {
        int eq = values_equal(&lhs, &rhs);
        Value out = make_bool(e->as.binary.op == OP_EQ ? eq : !eq);
        value_free(&lhs); value_free(&rhs);
        return out;
    }

    if (e->as.binary.op == OP_LT || e->as.binary.op == OP_LE ||
        e->as.binary.op == OP_GT || e->as.binary.op == OP_GE) {

        if (lhs.kind != VAL_INT || rhs.kind != VAL_INT) {
            runtime_type_error(err, cap, path, e->line, e->col, "comparison operators expect integers");
            value_free(&lhs); value_free(&rhs);
            return make_null();
        }

        int ok = 0;
        if (e->as.binary.op == OP_LT) ok = (lhs.int_value < rhs.int_value);
        if (e->as.binary.op == OP_LE) ok = (lhs.int_value <= rhs.int_value);
        if (e->as.binary.op == OP_GT) ok = (lhs.int_value > rhs.int_value);
        if (e->as.binary.op == OP_GE) ok = (lhs.int_value >= rhs.int_value);

        Value out = make_bool(ok);
        value_free(&lhs); value_free(&rhs);
        return out;
    }

    runtime_type_error(err, cap, path, e->line, e->col, "unsupported binary operator");
    value_free(&lhs); value_free(&rhs);
    return make_null();
}

static Value eval_expr(Runtime *rt, const Expr *e, int line, int col, const char *path, char *err, int cap) {
    Value v;
    memset(&v, 0, sizeof(v));
    v.kind = VAL_NULL;

    if (!e) {
        runtime_type_error(err, cap, path, line, col, "null expression");
        return v;
    }

    switch (e->kind) {
        case EXPR_LITERAL:
            if (e->as.lit.lit_kind == LIT_INT) {
                v.kind = VAL_INT;
                v.int_value = e->as.lit.int_value;
                return v;
            }
            if (e->as.lit.lit_kind == LIT_BOOL) {
                v.kind = VAL_BOOL;
                v.int_value = e->as.lit.int_value ? 1 : 0;
                return v;
            }
            if (e->as.lit.lit_kind == LIT_NULL) {
                v.kind = VAL_NULL;
                return v;
            }
            if (e->as.lit.lit_kind == LIT_STRING) {
                v.kind = VAL_STRING;
                v.string_value = xstrdup(e->as.lit.text);
                if (!v.string_value) v.string_value = xstrdup("");
                return v;
            }
            runtime_type_error(err, cap, path, e->line, e->col, "unknown literal kind");
            return v;

        case EXPR_VAR: {
            Var *var = find_var(rt, e->as.var.name);
            if (!var) {
                char msg[320];
                snprintf(msg, sizeof(msg), "undefined variable '%s'", e->as.var.name);
                diag_format(err, cap, path, e->line, e->col, "runtime error", msg);
                return v;
            }
            return value_copy(&var->v);
        }

        case EXPR_UNARY:
            return eval_unary(rt, e, path, err, cap);

        case EXPR_BINARY:
            return eval_binary(rt, e, path, err, cap);

        default:
            runtime_type_error(err, cap, path, e->line, e->col, "unsupported expression kind");
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
            Value rhs = eval_expr(rt, s->value, s->line, s->col, path, tmp, (int)sizeof(tmp));
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
            Value v = eval_expr(rt, s->arg, s->line, s->col, path, tmp, (int)sizeof(tmp));
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

