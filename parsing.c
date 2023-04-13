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

#define LASSERT(args, cond, err) \
    if (!(cond)) {               \
        lval_del(args);          \
        return lval_err(err);    \
    }

static char input[2048];
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;
enum { LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_FUNC, LVAL_SEXPR, LVAL_QEXPR };

lval *lenv_get(lenv *env, lval *a);

lval *lval_copy(lval *a);
lval *lval_err(char *m);
void lenv_del(lenv *e);
lval *builtin_op(lenv *env, lval *a, char *op);
lval *lval_eval(lenv *env, lval *v);
lval *lval_take(lval *v, int i);
lval *lval_pop(lval *v, int i);
void lval_del(lval *v);
void lval_print(lval *v);
typedef lval *(*lbuiltin)(lenv *, lval *);

enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM };

struct lval {
    int type;
    long num;

    // Error and Symbol have string data
    char *error;
    char *sym;
    lbuiltin func;

    // Count of pointers and a list of pointers to lval
    int count;
    lval **cell;  // This is the pointer to the cell that holds the
                  // pointers i.e it points to the list of pointers that
                  // are pointing to the lval struct
};

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

lval *lval_func(lbuiltin func) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_FUNC;
    v->func = func;
    return v;
}

lval *lval_sexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

lval *lval_qexpr(void) {
    lval *v = malloc(sizeof(lval));
    v->type = LVAL_QEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void lval_del(lval *v) {
    switch (v->type) {
        case LVAL_NUM:
            break;
        case LVAL_FUNC:
            break;
        case LVAL_ERR:
            free(v->error);
            break;
        case LVAL_SYM:
            free(v->sym);
            break;

        case LVAL_QEXPR:
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

lval *lval_join(lval *x, lval *y) {
    /* For each cell in 'y' add it to 'x' */
    for (int i = 0; i < y->count; i++) {
        x = lval_add(x, y->cell[i]);
    }
    free(y->cell);
    free(y);

    return x;
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
        case LVAL_QEXPR:
            lval_expr_print(v, '{', '}');
            break;
        case LVAL_FUNC:
            printf("<function>");
            break;
    }
}

lval *lval_copy(lval *v) {
    lval *x = malloc(sizeof(lval));
    x->type = v->type;

    switch (v->type) {
        case LVAL_FUNC:
            x->func = v->func;
            break;
        case LVAL_NUM:
            x->num = v->num;
            break;

        case LVAL_ERR:
            x->error = malloc(sizeof(v->error) + 1);
            strcpy(x->error, v->error);
            break;
        case LVAL_SYM:
            x->sym = malloc(sizeof(x->sym) + 1);
            strcpy(x->sym, v->sym);
            break;
        case LVAL_SEXPR:
        case LVAL_QEXPR:
            x->count = v->count;
            x->cell = malloc(sizeof(lval *) * x->count);
            for (int i = 0; i < x->count; i++) {
                x->cell[i] = lval_copy(v->cell[i]);
            }
            break;
    }
    return x;
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
    if (strstr(t->tag, "qexpr")) {
        x = lval_qexpr();
    }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, ")") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, "{") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->contents, "}") == 0) {
            continue;
        }
        if (strcmp(t->children[i]->tag, "regex") == 0) {
            continue;
        }
        x = lval_add(x, lval_read(t->children[i]));
    }

    return x;
}

lval *lval_sexpr_eval(lenv *env, lval *v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = lval_eval(env, v->cell[i]);
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

    if (f->type != LVAL_FUNC) {
        lval_del(f);
        lval_del(v);
        return lval_err("First element is not a function!");
    }

    lval *result = f->func(env, v);
    lval_del(f);
    return result;
}

lval *lval_eval(lenv *env, lval *v) {
    if (v->type == LVAL_SYM) {
        lval *x = lenv_get(env, v);
        lval_del(x);
        return x;
    }
    /* Evaluate Sexpressions */
    if (v->type == LVAL_SEXPR) {
        return lval_sexpr_eval(env, v);
    }
    return v;
}

struct lenv {
    int count;
    char **syms;
    lval **vals;
};

lenv *lenv_new(void) {
    lenv *env = malloc(sizeof(lenv));
    env->count = 0;
    env->syms = NULL;
    env->vals = NULL;
    return env;
}

lval *lenv_get(lenv *env, lval *a) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->syms[i], a->sym) == 0) {
            return lval_copy(env->vals[i]);
        }
    }
    return lval_err("symbol not found!");
}

void lenv_put(lenv *env, lval *k, lval *v) {
    /* Iterate over all items in environment */
    /* This is to see if variable already exists */
    for (int i = 0; i < env->count; i++) {
        /* If variable is found delete item at that position */
        /* And replace with variable supplied by user */
        if (strcmp(env->syms[i], k->sym) == 0) {
            lval_del(env->vals[i]);
            env->vals[i] = lval_copy(v);
            return;
        }
    }

    /* If no existing entry found allocate space for new entry */
    env->count++;
    env->vals = realloc(env->vals, sizeof(lval *) * env->count);
    env->syms = realloc(env->syms, sizeof(char *) * env->count);

    /* Copy contents of lval and symbol string into new location */
    env->vals[env->count - 1] = lval_copy(v);
    env->syms[env->count - 1] = malloc(strlen(k->sym) + 1);
    strcpy(env->syms[env->count - 1], k->sym);
}

void lenv_del(lenv *env) {
    for (int i = 0; i < env->count; i++) {
        free(env->syms[i]);
        lval_del(env->vals[i]);
    }
    free(env->syms);
    free(env->vals);
    free(env);
}

/*!
A little helper to check if lval is valid
*/
lval *head_tail_helper(lval *a) {
    LASSERT(a, a->count == 1, "Function 'head' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'head' passed incorrect types!");
    LASSERT(a, a->cell[0]->count != 0, "Function 'head' passed {}!");
    return lval_num(1);
}
lval *builtin_list(lenv *env, lval *a) {
    a->type = LVAL_QEXPR;
    return a;
}

lval *builtin_eval(lenv *env, lval *a) {
    LASSERT(a, a->count == 1, "Function 'eval' passed too many arguments!");
    LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
            "Function 'eval' passed incorrect type!");

    lval *x = lval_take(a, 0);
    x->type = LVAL_SEXPR;
    return lval_eval(env, a);
}
/*
Takes a Q-Expression and returns a Q-Expression with only the first element
 */
lval *builtin_head(lenv *env, lval *a) {
    lval *res = head_tail_helper(a);
    if (res->type == LVAL_ERR) {
        return res;
    }
    lval_del(res);

    lval *v = lval_take(a, 0);

    while (v->count > 1) {
        lval_del(lval_pop(v, 1));
    }
    return v;
}

/*
Takes a Q-Expression and returns a Q-Expression with the first element removed
*/
lval *builtin_tail(lenv *env, lval *a) {
    lval *res = head_tail_helper(a);
    if (res->type == LVAL_ERR) {
        return res;
    }
    lval_del(res);

    lval *v = lval_take(a, 0);
    lval_del(lval_pop(v, 0));
    return v;
}

lval *builtin_join(lenv *env, lval *a) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
                "Function 'join' passed incorrect type");
    }
    lval *x = lval_pop(a, 0);
    while (a->count) {
        x = lval_join(x, lval_pop(a, 0));
    }

    lval_del(a);
    return x;
}

lval *builtin_op(lenv *env, lval *a, char *op) {
    for (int i = 0; i < a->count; i++) {
        LASSERT(a, a->cell[i]->type == LVAL_NUM,
                "Cannot operate on non-number!");
        // if (a->cell[i]->type != LVAL_NUM) {
        //     // lval_del(a);
        //     return lval_err("Cannot operate on non-number!");
        // }
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
lval *builtin_add(lenv *env, lval *a) { return builtin_op(env, a, "+"); }

lval *builtin_sub(lenv *env, lval *a) { return builtin_op(env, a, "-"); }

lval *builtin_mul(lenv *env, lval *a) { return builtin_op(env, a, "*"); }

lval *builtin_div(lenv *env, lval *a) { return builtin_op(env, a, "/"); }

void lenv_add_builtin(lenv *env, char *name, lbuiltin func) {
    lval *k = lval_sym(name);
    lval *v = lval_func(func);
    lenv_put(env, k, v);
    lval_del(k);
    lval_del(v);
}

void lenv_add_builtins(lenv *env) {
    /* List Functions */
    lenv_add_builtin(env, "head", builtin_head);
    lenv_add_builtin(env, "list", builtin_list);
    lenv_add_builtin(env, "tail", builtin_tail);
    lenv_add_builtin(env, "eval", builtin_eval);
    lenv_add_builtin(env, "join", builtin_join);

    /* Mathematical Functions */
    lenv_add_builtin(env, "+", builtin_add);
    lenv_add_builtin(env, "-", builtin_sub);
    lenv_add_builtin(env, "*", builtin_mul);
    lenv_add_builtin(env, "/", builtin_div);
}

int main(int argc, char **argv) {
    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Qexpr = mpc_new("qexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");
    mpca_lang(MPCA_LANG_DEFAULT,
              "                                    \
    number   : /-?[0-9]+/ ;                           \
    symbol   :  /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;                  \
    sexpr    : '(' <expr>* ')' ;                       \
    qexpr    : '{' <expr>* '}' ;                      \
    expr     : <number> | <symbol> | <sexpr> | <qexpr> ;        \
    lispy    : /^/  <expr>* /$/ ;                     \
    ",
              Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    puts("Lispy Version 0.0.0.3");
    puts("Press CTRL+C to exit\n");
    lenv *env = lenv_new();
    lenv_add_builtins(env);
    while (1) {
        char *input = readline("lispy> ");

        mpc_result_t r;

        /* Output our prompt and get input */
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval *x = lval_eval(env, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);

        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Add input to history */
        add_history(input);
        free(input);
    }
    lenv_del(env);
    mpc_cleanup(6, Number, Symbol, Qexpr, Sexpr, Expr, Lispy);

    return 0;
}