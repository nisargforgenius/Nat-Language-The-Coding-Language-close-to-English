/*
 * nat.h — NAT Language v3.2: Shared Types, Constants, Forward Declarations
 *
 * NAT is a simple, English-style interpreted language.
 * Architecture: tokenizer → parser → AST → executor/evaluator
 *
 * v3.2 additions:
 *   Logic   : and/or between conditions, English greater/less than with and/or
 *             chained compare (1 < x < 10), "x in middle of A and B"
 *   Strings : length of x, upper of x, lower of x, x contains "y"
 *   Math    : 2^8 power operator, abs of x, round of x
 *   Vars    : let x, b, c.  (declare without value)
 *             x be 10.      (assign after declaration)
 */

#ifndef NAT_H
#define NAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <math.h>

/* ─────────────────────────────────────────────
   LIMITS
   ───────────────────────────────────────────── */
#define MAX_TOKENS      1024
#define MAX_VARS        512
#define MAX_CONSTS      128
#define MAX_FUNCS       512
#define MAX_LINES       2048
#define MAX_LINE_LEN    512
#define MAX_STR         512
#define MAX_ARGS        16
#define MAX_PARAMS      16
#define MAX_ARRAY_ELEM  256
#define MAX_ASK_VARS    16
#define MAX_INJECTS     64      /* v3.6 runtime injection slots */

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
#define T_OR       "OR"
#define T_ARE      "ARE"
#define T_IF       "IF"
#define T_ELSE     "ELSE"
#define T_IS       "IS"
#define T_NOT      "NOT"
#define T_GREATER  "GREATER"
#define T_LESS     "LESS"
#define T_THAN     "THAN"
#define T_WHILE    "WHILE"
#define T_NUM      "NUM"
#define T_IN       "IN"
#define T_MIDDLE   "MIDDLE"
#define T_BETWEEN  "BETWEEN"
#define T_OF       "OF"
#define T_LENGTH   "LENGTH"
#define T_UPPER    "UPPER"
#define T_LOWER    "LOWER"
#define T_CONTAINS "CONTAINS"
#define T_ABS      "ABS"
#define T_ROUND    "ROUND"
#define T_LARGER   "LARGER"
#define T_SMALLEST "SMALLEST"

#define T_PLUS     "PLUS"
#define T_MINUS    "MINUS"
#define T_STAR     "STAR"
#define T_SLASH    "SLASH"
#define T_MOD      "MOD"
#define T_POW      "POW"
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
    NODE_LET_DECLARE,   /* let x, b, c.  — declare with no value */
    NODE_ASSIGN,        /* x be 10.       — assign to existing var */
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
    NODE_ADD_ASSIGN,
    NODE_VAL,
    NODE_VAR,
    NODE_VAR_IDX,
    NODE_ADD_EXPR,
    NODE_SUB_EXPR,
    NODE_MUL_EXPR,
    NODE_DIV_EXPR,
    NODE_MOD_EXPR,
    NODE_POW_EXPR,      /* 2^8 */
    NODE_CONCAT,
    NODE_AND,           /* logical and */
    NODE_OR,            /* logical or  */
    NODE_CMP_EQ,
    NODE_CMP_NEQ,
    NODE_CMP_GT,
    NODE_CMP_LT,
    NODE_CMP_GTE,
    NODE_CMP_LTE,
    NODE_CONTAINS,      /* str contains substr */
    NODE_LENGTH,        /* length of x  */
    NODE_UPPER,         /* upper of x   */
    NODE_LOWER,         /* lower of x   */
    NODE_ABS,           /* abs of x     */
    NODE_ROUND,         /* round of x   */
    NODE_LARGER,        /* larger(a,b,c,...) / larger of a,b,c */
    NODE_SMALLEST,      /* smallest(a,b,c,...) / smallest of a,b,c */
    NODE_TRIM,          /* trim of x    */
    NODE_REPLACE,       /* replace A with B in x */
    NODE_SPLIT,         /* split x by " " */
    NODE_MAX,           /* max of a and b */
    NODE_MIN,           /* min of a and b */
    NODE_FLOOR,         /* floor of x   */
    NODE_CEIL,          /* ceil of x    */
    NODE_IS_NUMBER,     /* x is number      */
    NODE_IS_TEXT,       /* x is text        */
    NODE_IS_EVEN,       /* x is even        */
    NODE_IS_ODD,        /* x is odd         */
    NODE_RANDOM,        /* random from A to B for x y z */
    NODE_STR_REPEAT,    /* repeat "ha" 3 times */
    NODE_REVERSE,       /* reverse of x / reverse arr */
    NODE_FIRST_N,       /* first N of x     */
    NODE_LAST_N,        /* last N of x      */
    NODE_FIRST_ELEM,    /* first of arr     */
    NODE_LAST_ELEM,     /* last of arr      */
    NODE_TEXT_OF,       /* text of 42       */
    NODE_ARR_INSERT,    /* add x to arr at N */
    NODE_ARR_REMOVE,    /* remove arr at N / remove last/first from arr */
    NODE_ARR_SWAP,      /* swap arr at 1 and 3 */
    NODE_ARR_DECLARE_N, /* let fruits are 10. — empty array of size N */
    NODE_EACH,          /* each item in arr/str: ... end. */
    NODE_SHOW_EACH,     /* show each item in arr. / online / with sep */
    NODE_CLAMP,         /* clamp x from A to B */
    NODE_USE,           /* use math.tree. */
    NODE_GRAPH,         /* create graph "title": ... end. */
    NODE_NEWLINE,       /* newline. / skip. — blank line output */
    /* v3.6 — file I/O */
    NODE_FILE_WRITE,    /* write "text" to "file.txt". */
    NODE_FILE_WRITE_LINE, /* write "text" to "file.txt" at line N. */
    NODE_FILE_APPEND,   /* append "text" to "file.txt". */
    NODE_FILE_INSERT,   /* insert "text" to "file.txt" at line N. */
    NODE_FILE_READ,     /* read "file.txt". */
    NODE_FILE_READ_LINE,/* let x be read "file.txt" at line N. */
    NODE_FILE_DELETE,   /* delete file "file.txt". */
    NODE_FILE_REMOVE_LINE, /* remove line N from "file.txt". */
    NODE_FILE_EXISTS,   /* if file "file.txt" exists: */
    NODE_FILE_EACH,     /* each line in file "file.txt": */
    NODE_WRITE_INSIDE,  /* write x inside "one.nat". */
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

    int       loop_to;

    char      ask_vars[MAX_ASK_VARS][64];
    int       ask_count;
};

/* ─────────────────────────────────────────────
   VARIABLE  (v4.0 Phase 2 — typed storage)
   ───────────────────────────────────────────── */

/* value type tag */
typedef enum { VAL_STR = 0, VAL_NUM = 1 } ValType;

typedef struct {
    char    name[64];
    ValType type;               /* VAL_NUM or VAL_STR                    */
    double  num;                /* used when type == VAL_NUM              */
    char    str[MAX_STR];       /* used when type == VAL_STR              */
    int     is_array;
    ValType arr_type[MAX_ARRAY_ELEM]; /* type of each array element       */
    double  arr_num[MAX_ARRAY_ELEM];  /* numeric array elements           */
    char    arr[MAX_ARRAY_ELEM][MAX_STR]; /* string array elements        */
    int     arr_len;
} Variable;

/* ── helper macros ───────────────────────────── */

/* read variable as double — no atof on hot path for numeric vars */
#define VAR_NUM(v) ((v)->type == VAL_NUM ? (v)->num : atof((v)->str))

/* write variable as string into a buffer */
static inline void var_to_str(const Variable *v, char *out, int sz) {
    if (v->type == VAL_NUM) {
        /* inline the fmt_num logic to avoid forward decl issue */
        long long iv = (long long)v->num;
        if ((double)iv == v->num)
            snprintf(out, sz, "%lld", iv);
        else
            snprintf(out, sz, "%.10g", v->num);
    } else {
        strncpy(out, v->str, sz-1);
        out[sz-1] = '\0';
    }
}

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
    /* v3.5: store actual body lines for tree-loaded functions */
    char **body_lines;   /* heap-allocated array of line strings */
    int    body_line_count;
    int    is_tree_func; /* 1 if loaded from a .tree file */
} FuncDef;

/* ─────────────────────────────────────────────
   GRAPH DEFINITION (v3.5 p3)
   ───────────────────────────────────────────── */
#define MAX_POINTS 256

typedef struct {
    char title[MAX_STR];
    char x_label[64];
    char y_label[64];
    double x_min, x_max, x_step;  /* x_step=0 means auto */
    double y_min, y_max, y_step;  /* y_step=0 means auto */
    char graph_type[32];
    char color[32];
    double px[MAX_POINTS];
    double py[MAX_POINTS];
    int    point_count;
} GraphDef;

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

/* v4.0 Phase 2 — typed return value */
typedef struct { ValType type; double num; char str[MAX_STR]; } NatVal;
extern NatVal   g_return_val;
extern char     g_return_str[MAX_STR]; /* legacy compat — remove in Phase 3 */
extern int      g_has_return;

/* ── v3.5 module system ── */
#define MAX_IMPORTS     64
#define MAX_PATH_LEN    512
extern char     g_imported[MAX_IMPORTS][MAX_PATH_LEN]; /* already loaded trees */
extern int      g_import_count;
extern char     g_nat_exe_dir[MAX_PATH_LEN];           /* directory of .nat script */
extern char     g_nat_bin_dir[MAX_PATH_LEN];           /* directory of nat.exe itself */

/* v3.6 — runtime injection table (write x inside "file.nat") */
typedef struct { char name[64]; char value[MAX_STR]; } Inject;
extern Inject   g_injects[MAX_INJECTS];
extern int      g_inject_count;

/* ─────────────────────────────────────────────
   API
   ───────────────────────────────────────────── */
void      tokenize(const char *src);
Node     *parse_statement(int line_index);
Node     *parse_expression(void);
void      eval(Node *n, char *out, int out_size);
void      execute(Node *n);
void      execute_block(int start, int end);
void      execute_func_body(FuncDef *fn);
Variable *find_var(const char *name);
Variable *set_var(const char *name, const char *value);
void cache_numeric(Variable *v, const char *value);  /* v4.0 Phase 2 */
int  is_number(const char *s);                        /* numeric string check */
void fmt_num(char *out, int out_size, double v);      /* number formatter */
Constant *find_const(const char *name);
Node     *node_new(NodeType kind);
void      node_free(Node *n);

#endif /* NAT_H */

/* ─────────────────────────────────────────────────────────────────
   ERROR SYSTEM (v3.3)
   ───────────────────────────────────────────────────────────────── */
extern int g_current_line;   /* set by execute_block each iteration */
#include "errors.h"
