#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"

#ifdef _WIN32
#include <string.h>
static char buffer[2048];

char *readline(char *prompt)
{
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char *cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}
void add_history(char *unused) {}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

static char input[2048];
enum
{
    LVAL_NUM,
    LVAL_ERR
};
enum
{
    LERR_DIV_ZERO,
    LERR_BAD_OP,
    LERR_BAD_NUM
};

typedef struct
{
    int type;
    long num;
    int error;
} lval;

lval lval_err(int x)
{
    lval a;
    a.type = LVAL_ERR;
    a.error = x;
    return a;
}

lval lval_num(long num)
{

    lval a;
    a.type = LVAL_NUM;
    a.num = num;
    return a;
}

void lval_print(lval v)
{
    switch (v.type)
    {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM:
        printf("%li", v.num);
        break;

    /* In the case the type is an error */
    case LVAL_ERR:
        /* Check what type of error it is and print it */
        if (v.error == LERR_DIV_ZERO)
        {
            printf("Error: Division By Zero!");
        }
        if (v.error == LERR_BAD_OP)
        {
            printf("Error: Invalid Operator!");
        }
        if (v.error == LERR_BAD_NUM)
        {
            printf("Error: Invalid Number!");
        }
        break;
    }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v)
{
    lval_print(v);
    putchar('\n');
}

lval eval_op(lval x, char *op, lval y)

{
    if (x.type == LVAL_ERR)
    {
        return x;
    }
    if (y.type == LVAL_ERR)
    {
        return y;
    }
    if (strcmp(op, "+") == 0)
    {
        return lval_num(

            x.num + y.num);
    }
    if (strcmp(op, "-") == 0)
    {
        return lval_num(
            x.num - y.num);
    }
    if (strcmp(op, "*") == 0)
    {
        return lval_num(x.num * y.num);
    }
    if (strcmp(op, "/") == 0)
    {
        /* If second operand is zero return error */
        return y.num == 0
                   ? lval_err(LERR_DIV_ZERO)
                   : lval_num(x.num / y.num);
    }
    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *tree)
{
    // If it was a number

    if (strstr(tree->tag, "number"))
    {
        errno = 0;
        long x = strtol(tree->contents, NULL, 10);
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }

    // If not then it means its an operator

    char *op = tree->children[1]->contents;

    lval x = eval(tree->children[2]);

    int i = 3; // . first 2 have already been ignored

    while (strstr(tree->children[i]->tag, "expr"))
    {

        x = eval_op(x, op, eval(tree->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char **argv)
{

    mpc_parser_t *Number = mpc_new("number");
    mpc_parser_t *Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");
    mpca_lang(
        MPCA_LANG_DEFAULT,
        "                                    \
    number   : /-?[0-9]+/ ;                           \
    symbol : '+' | '-' | '*' | '/' ;                  \
    sexpr   : '(' <expr>* ')' ;                       \
    expr     : <number> | <symbol> | <sexpr> ;        \
    lispy    : /^/  <expr>* /$/ ;                     \
  ",
        Number, Symbol, Sexpr, Expr, Lispy);

    puts(
        "Lispy Version 0.0.0.3");
    puts(
        "Press CMD+C to exit\n");
    while (1)
    {
        mpc_result_t r;

        /* Output our prompt and get input */
        char *input = readline("lispy> ");
        if (mpc_parse("<stdin>", input, Lispy, &r))
        {
            lval result = eval(r.output);
            lval_println(result);
            // printf("%li\n", result);
            mpc_ast_delete(r.output);
        }
        else
        {
            printf("erro");
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        /* Add input to history */
        add_history(input);
        free(input);
    }
    mpc_cleanup(4, Number, Symbol, Sexpr, Expr, Lispy);

    return 0;
}