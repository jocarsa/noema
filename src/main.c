#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noema.h"

static void print_usage(const char *prog) {
    fprintf(stderr,
        "Usage: %s <file.noema> [--tokens] [--ast] [--trace]\n"
        "\n"
        "Options:\n"
        "  --tokens   Tokenize only (debug)\n"
        "  --ast      Parse and print AST only (debug)\n"
        "  --trace    Trace execution (debug) (reserved)\n",
        prog
    );
}

static NoemaOptions parse_args(int argc, char **argv, const char **path_out) {
    NoemaOptions opt;
    memset(&opt, 0, sizeof(opt));

    *path_out = NULL;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (strcmp(a, "--help") == 0 || strcmp(a, "-h") == 0) {
            opt.show_help = 1;
            continue;
        }

        if (strcmp(a, "--tokens") == 0) {
            opt.dump_tokens = 1;
            continue;
        }

        if (strcmp(a, "--ast") == 0) {
            opt.dump_ast = 1;
            continue;
        }

        if (strcmp(a, "--trace") == 0) {
            opt.trace_exec = 1;
            continue;
        }

        if (a[0] != '-' && *path_out == NULL) {
            *path_out = a;
            continue;
        }

        opt.bad_args = 1;
    }

    return opt;
}

int main(int argc, char **argv) {
    const char *path = NULL;
    NoemaOptions opt = parse_args(argc, argv, &path);

    if (opt.show_help) {
        print_usage(argv[0]);
        return 0;
    }

    if (opt.bad_args || path == NULL) {
        print_usage(argv[0]);
        return 1;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return 1;
    }

    NoemaResult r = noema_run_file(f, path, &opt);

    fclose(f);

    if (!r.ok) {
        if (r.message[0]) fprintf(stderr, "%s\n", r.message);
        else fprintf(stderr, "Noema: failed.\n");
        return 1;
    }

    return 0;
}

