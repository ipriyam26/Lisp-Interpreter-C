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

long eval_op(long x, char *op, long y)
{
    if (strcmp(op, "+") == 0)
    {
        return x + y;
    }
    if (strcmp(op, "-") == 0)
    {
        return x - y;
    }
    if (strcmp(op, "*") == 0)
    {
        return x * y;
    }
    if (strcmp(op, "/") == 0)
    {
        return x / y;
    }
    printf("Found thi");
    return 0;
}

long eval(mpc_ast_t *tree)
{
    // If it was a number

    if (strstr(tree->tag, "number"))
    {
        return atoi(tree->contents);
    }

    // If not then it means its an operator

    char *op = tree->children[1]->contents;

    long x = eval(tree->children[2]);

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
    mpc_parser_t *Operator = mpc_new("operator");
    mpc_parser_t *Expr = mpc_new("expr");
    mpc_parser_t *Lispy = mpc_new("lispy");
    mpca_lang(
        MPCA_LANG_DEFAULT,
        "                                                     \
    number   : /-?[0-9]+/ ;                             \
    operator : '+' | '-' | '*' | '/' ;                  \
    expr     : <number> | '(' <operator> <expr>+ ')' ;  \
    lispy    : /^/ <operator> <expr>+ /$/ ;             \
  ",
        Number, Operator, Expr, Lispy);

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
            long result = eval(r.output);
            printf("%li\n", result);
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
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}