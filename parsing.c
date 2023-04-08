#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}
void add_history(char *unused) {}

#else
#include <editline/history.h>
#include <editline/readline.h>
#endif

static char input[2048];

enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };
typedef struct lval {
    int type;
    long num;

    // Error and Symbol have string data
    char *error;
    char *sym;

    // Count of pointers and a list of pointers to lval
    int count;
    struct lval **cell;  // This is the pointer to the cell that holds the
                         // pointers i.e it points to the list of pointers that
                         // are pointing to the lval struct
} lval;

void lval_print(lval *v);
lval *lval_err(char *m) {
    lval *a = malloc(sizeof(lval));
    a->type = LVAL_ERR;
    a->error = malloc(strlen(m) + 1);
    strcpy(a->error, m);
    return a;
}

lval *lval_num(long num) {
    lval *a = malloc(sizeof(lval));
    a->type = LVAL_NUM;
    a->num = num;
    return a;
}

lval *lval_sym(char *s) {
    lval *a = malloc(sizeof(lval));
    a->type = LVAL_SYM;
    a->sym = malloc(strlen(s) + 1);
    strcpy(a->sym, s);
    return a;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v) {
    switch (v->type) {
        case LVAL_NUM:
            break;
        case LVAL_ERR:
            free(v->error);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;

        case LVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                lval_del(v->cell[i]);
            }
            free(v->cell);
            break;
    }
    free(v);
}

lval *lval_add(lval *v, lval *x) {
    v->count++;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

void lval_expr_print(lval *v, char open, char close) {
    putchar(open);

    for (int i = 0; i < v->count; i++) {
        lval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }
    putchar(close);
}

void lval_print(lval *v) {
    switch (v->type) {
        /* In the case the type is a number print it */
        /* Then 'break' out of the switch. */
        case LVAL_NUM:
            printf("%li", v->num);
            break;

        case LVAL_ERR:
            printf("Error: %s", v->error);
            break;
        case LVAL_SYM:
            printf("%s", v->sym);
            break;
        case LVAL_SEXPR:
            lval_expr_print(v, '(', ')');
            break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval *v)

{
    lval_print(v);
    putchar('\n');
}

lval *lval_read_num(mpc_ast_t *t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval *lval_read(mpc_ast_t *t) {
    if (strstr(t->tag, "number")) {
        return lval_read_num(t);
    }
    if (strstr(t->tag, "symbol")) {
        return lval_sym(t->contents);
    }

    lval *x = NULL;
    if (strcmp(t->tag, ">") == 0) {
        x = lval_sexpr();
    }
    if (strstr(t->tag, "sexpr")) {
        x = lval_sexpr();
    }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}
lval *builtin_op(lval *a, char *op);
lval *lval_eval(lval *v);
lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);

lval *lval_sexpr_eval(lval *v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == LVAL_ERR) {
            return lval_take(v, i);
        }
    }

    /* Empty Expression */
    if (v->count == 0) {
        return v;
    }

    /* Single Expression */
    if (v->count == 1) {
        return lval_take(v, 0);
    }

    lval *f = lval_pop(v, 0);

    if (f->type != LVAL_SYM) {
        lval_del(f);
        lval_del(v);
        return lval_err("S-expression Does not start with symbol!");
    }

    lval *result = builtin_op(v, f->sym);
    lval_del(f);
    return result;
}

lval *lval_eval(lval *v) {
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_sexpr_eval(v);
    }
    return v;
}

lval *lval_pop(lval *v, int i) {
    // NOTE - Using the second lval_pop without '&' in memmove will result in a
    // segmentation fault or undefined behavior as it is trying to move a
    // pointer without referencing the address of the pointer. The '&' operator
    // is needed to pass the address of the pointer to the memmove function.

    lval *x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i + 1], sizeof(lval *) * (v->count - i - 1));
    v->count--;
    v->cell = realloc(v->cell, sizeof(lval *) * v->count);
    return x;
}

lval *lval_take(lval *v, int i) {
    lval *x = lval_pop(v, i);
    lval_del(v);
    return x;
}

lval *builtin_op(lval *a, char *op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != LVAL_NUM) {
            lval_del(a);
            return lval_err("Cannot operate on non-number!");
        }
    }

    lval *x = lval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        /* Pop the next element */
        lval *y = lval_pop(a, 0);

        if (strcmp(op, "+") == 0) {
            x->num += y->num;
        }
        if (strcmp(op, "-") == 0) {
            x->num -= y->num;
        }
        if (strcmp(op, "*") == 0) {
            x->num *= y->num;
        }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                lval_del(x);
                lval_del(y);
                x = lval_err("Division By Zero!");
                break;
            }
            x->num /= y->num;
        }

        lval_del(y);
    }
    lval_del(a);
    return x;
}

int main(int argc, char **argv) {
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                    \
    number   : /-?[0-9]+/ ;                           \
    symbol : '+' | '-' | '*' | '/' ;                  \
    sexpr   : '(' <expr>* ')' ;                       \
    expr     : <number> | <symbol> | <sexpr> ;        \
    lispy    : /^/  <expr>* /$/ ;                     \
  ",
              Number, Symbol, Sexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.3");
    puts("Press CTRL+C to exit\n");
    while (1) {
        mpc_result_t r;

        /* Output our prompt and get input */
        char *input = readline("lispy> ");
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *x = lval_eval(lval_read(r.output));
            lval_println(x);
            lval_del(x);

        } else {
            printf("erro");
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Add input to history */
        add_history(input);
        free(input);
    }
    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}