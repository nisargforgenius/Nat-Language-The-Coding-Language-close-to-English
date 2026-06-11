/*
 * nat.h — NAT Language v3.0: Shared Types, Constants, Forward Declarations
 *
 * NAT is a simple, English-style interpreted language.
 * Architecture: tokenizer → parser → AST → executor/evaluator
 *
 * v3.0 changes:
 *  - num() replaces int()/float()/double() — one universal number type
 *  - 'be' replaces '=' for assignment
 *  - Plain string: let name be hello.   (no quotes needed for single words)
 *  - fix pi 3.14                        (constants, no dot)
 *  - add x with y to z.                 (English arithmetic assignment)
 *  - repeat i from 1 to 6 is INCLUSIVE  (loop goes 1..6)
 */

#ifndef NAT_H
#define NAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* ─────────────────────────────────────────────
   LIMITS
   ───────────────────────────────────────────── */
#define MAX_TOKENS      1024
#define MAX_VARS        512
#define MAX_CONSTS      128
#define MAX_FUNCS       128
#define MAX_LINES       2048
#define MAX_LINE_LEN    512
#define MAX_STR         512
#define MAX_ARGS        16
#define MAX_PARAMS      16
#define MAX_ARRAY_ELEM  256
#define MAX_ASK_VARS    16

/* ─────────────────────────────────────────────
   TOKEN TYPE STRINGS
   ───────────────────────────────────────────── */
#define T_LET      "LET"
#define T_BE       "BE"
#define T_FIX      "FIX"
#define T_ADD      "ADD"
#define T_SHOW     "SHOW"
#define T_MAKE     "MAKE"
#define T_WITH     "WITH"
#define T_INSIDE   "INSIDE"
#define T_END      "END"
#define T_GIVE     "GIVE"
#define T_REPEAT   "REPEAT"
#define T_TIMES    "TIMES"
#define T_FROM     "FROM"
#define T_TO       "TO"
#define T_STEP     "STEP"
#define T_ASK      "ASK"
#define T_FOR      "FOR"
#define T_AND      "AND"
#define T_ARE      "ARE"
#define T_IF       "IF"
#define T_ELSE     "ELSE"
#define T_IS       "IS"
#define T_NOT      "NOT"
#define T_GREATER  "GREATER"
#define T_LESS     "LESS"
#define T_THAN     "THAN"
#define T_WHILE    "WHILE"
#define T_NUM      "NUM"    /* num() — universal number wrapper */

#define T_PLUS     "PLUS"
#define T_MINUS    "MINUS"
#define T_STAR     "STAR"
#define T_SLASH    "SLASH"
#define T_MOD      "MOD"
#define T_EQ       "EQ"
#define T_NEQ      "NEQ"
#define T_GT       "GT"
#define T_LT       "LT"
#define T_GTE      "GTE"
#define T_LTE      "LTE"

#define T_LPAREN   "LPAREN"
#define T_RPAREN   "RPAREN"
#define T_LBRACK   "LBRACK"
#define T_RBRACK   "RBRACK"
#define T_COMMA    "COMMA"
#define T_DOT      "DOT"
#define T_COLON    "COLON"

#define T_IDENT    "IDENT"
#define T_NUMBER   "NUMBER"
#define T_STRING   "STRING"

/* ─────────────────────────────────────────────
   TOKEN
   ───────────────────────────────────────────── */
typedef struct {
    char type[20];
    char value[MAX_STR];
} Token;

/* ─────────────────────────────────────────────
   AST NODE TYPES
   ───────────────────────────────────────────── */
typedef enum {
    NODE_LET,
    NODE_ARRAY,
    NODE_SHOW,
    NODE_GIVE,
    NODE_FUNC_DEF,
    NODE_FUNC_CALL,
    NODE_CALL_EXPR,
    NODE_ASK,
    NODE_LOOP,
    NODE_FOR_LOOP,
    NODE_WHILE,
    NODE_IF,
    NODE_ADD_ASSIGN,   /* add x with y to z  */
    NODE_VAL,
    NODE_VAR,
    NODE_VAR_IDX,
    NODE_ADD_EXPR,
    NODE_SUB_EXPR,
    NODE_MUL_EXPR,
    NODE_DIV_EXPR,
    NODE_MOD_EXPR,
    NODE_CONCAT,
    NODE_CMP_EQ,
    NODE_CMP_NEQ,
    NODE_CMP_GT,
    NODE_CMP_LT,
    NODE_CMP_GTE,
    NODE_CMP_LTE,
    NODE_NOOP
} NodeType;

typedef struct Node Node;

struct Node {
    NodeType  kind;

    char      name[64];
    char      value[MAX_STR];

    Node     *left;
    Node     *right;
    Node     *else_branch;

    Node     *args[MAX_ARGS];
    int       arg_count;

    char      params[MAX_PARAMS][64];
    int       param_count;

    int       body_start;
    int       body_end;

    /* for-loop / while */
    int       loop_to;       /* also: end-line index for NODE_IF */

    /* ask variables */
    char      ask_vars[MAX_ASK_VARS][64];
    int       ask_count;
};

/* ─────────────────────────────────────────────
   VARIABLE
   ───────────────────────────────────────────── */
typedef struct {
    char name[64];
    char value[MAX_STR];
    int  is_array;
    char arr[MAX_ARRAY_ELEM][MAX_STR];
    int  arr_len;
} Variable;

/* ─────────────────────────────────────────────
   CONSTANT  (fix name value)
   ───────────────────────────────────────────── */
typedef struct {
    char name[64];
    char value[MAX_STR];
} Constant;

/* ─────────────────────────────────────────────
   FUNCTION RECORD
   ───────────────────────────────────────────── */
typedef struct {
    char name[64];
    char params[MAX_PARAMS][64];
    int  param_count;
    int  body_start;
    int  body_end;
} FuncDef;

/* ─────────────────────────────────────────────
   GLOBAL STATE
   ───────────────────────────────────────────── */
extern Token    g_tokens[MAX_TOKENS];
extern int      g_tok_count;
extern int      g_tok_pos;

extern Variable g_vars[MAX_VARS];
extern int      g_var_count;

extern Constant g_consts[MAX_CONSTS];
extern int      g_const_count;

extern FuncDef  g_funcs[MAX_FUNCS];
extern int      g_func_count;

extern char     g_lines[MAX_LINES][MAX_LINE_LEN];
extern int      g_line_count;

extern char     g_return_val[MAX_STR];
extern int      g_has_return;

/* ─────────────────────────────────────────────
   API
   ───────────────────────────────────────────── */
void      tokenize(const char *src);
Node     *parse_statement(int line_index);
Node     *parse_expression(void);
void      eval(Node *n, char *out, int out_size);
void      execute(Node *n);
void      execute_block(int start, int end);
Variable *find_var(const char *name);
Variable *set_var(const char *name, const char *value);
Constant *find_const(const char *name);
Node     *node_new(NodeType kind);
void      node_free(Node *n);

#endif /* NAT_H */
