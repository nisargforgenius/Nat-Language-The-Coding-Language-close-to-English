/*
 * main.c — NAT Language v3.0 Entry Point
 *
 * Usage:  nat <script.nat>
 *
 * Pre-pass: before executing, scan all lines for 'fix' declarations
 * and register them as constants.  'fix' lines are then skipped by
 * execute_block so they are never re-processed.
 *
 * fix pi 3.14
 * fix MAX 100
 * fix GREETING Hello
 */

#include "nat.h"
#include <time.h>

/* ─────────────────────────────────────────────────────────────────
   GLOBAL STATE
   ───────────────────────────────────────────────────────────────── */
Token    g_tokens[MAX_TOKENS];
int      g_tok_count = 0;
int      g_tok_pos   = 0;

Variable g_vars[MAX_VARS];
int      g_var_count = 0;

Constant g_consts[MAX_CONSTS];
int      g_const_count = 0;

FuncDef  g_funcs[MAX_FUNCS];
int      g_func_count = 0;

char     g_lines[MAX_LINES][MAX_LINE_LEN];
int      g_line_count = 0;

char     g_return_val[MAX_STR] = {0};
int      g_has_return          = 0;
int      g_current_line        = 0;

/* v3.5 module system */
char     g_imported[MAX_IMPORTS][MAX_PATH_LEN] = {{0}};
int      g_import_count = 0;
char     g_nat_exe_dir[MAX_PATH_LEN] = {0};

/* v3.6 — runtime injection table */
Inject   g_injects[MAX_INJECTS] = {{{0}}};
int      g_inject_count = 0;

/* ─────────────────────────────────────────────────────────────────
   strip_trailing — remove \r \n and trailing spaces
   ───────────────────────────────────────────────────────────────── */
static void strip_trailing(char *s) {
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' ||
                       s[len-1] == ' '  || s[len-1] == '\t'))
        s[--len] = '\0';
}

/* ─────────────────────────────────────────────────────────────────
   pre_pass_fix — scan all lines for 'fix name value' declarations
   and register them in g_consts before execution starts.
   ───────────────────────────────────────────────────────────────── */
void pre_pass_fix(void) {
    for (int i = 0; i < g_line_count; i++) {
        /* quick check: does the stripped line start with "fix "? */
        const char *p = g_lines[i];
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "fix ", 4) != 0) continue;

        /* tokenize the line and extract name + value */
        tokenize(g_lines[i]);
        if (g_tok_count < 3) continue;
        /* tokens: [0]=FIX  [1]=IDENT(name)  [2]=value */
        if (strcmp(g_tokens[0].type, T_FIX) != 0) continue;
        if (strcmp(g_tokens[1].type, T_IDENT) != 0) continue;

        if (g_const_count >= MAX_CONSTS) {
            fprintf(stderr, "NAT: constant table overflow\n");
            break;
        }

        Constant *c = &g_consts[g_const_count++];
        strncpy(c->name,  g_tokens[1].value, 63);

        /* value = everything from token[2] onwards, joined with spaces.
           skip an optional 'be' keyword: fix PI be 3.14159. */
        int start = 2;
        if (start < g_tok_count && strcmp(g_tokens[start].type, T_BE) == 0)
            start++;

        char val[MAX_STR] = {0};
        for (int j = start; j < g_tok_count; j++) {
            /* skip statement-terminator dot */
            if (strcmp(g_tokens[j].type, T_DOT) == 0) break;
            if (j > start) strncat(val, " ", MAX_STR - (int)strlen(val) - 1);
            strncat(val, g_tokens[j].value, MAX_STR - (int)strlen(val) - 1);
        }
        strncpy(c->value, val, MAX_STR-1);
    }
}

/* ─────────────────────────────────────────────────────────────────
   print_usage
   ───────────────────────────────────────────────────────────────── */
static void print_usage(const char *prog) {
    fprintf(stderr,
        "NAT Language Compiler v3.6.3\n"
        "Founder: Nisarg | Co-Founder: Claude Sonnet\n"
        "\n"
        "Usage: %s <script.nat>\n"
        "\n"
        "Quick syntax:\n"
        "  let x be 42.\n"
        "  let name be \"Nisarg\".\n"
        "  fix PI be 3.14.\n"
        "  show x.\n"
        "  show \"Hello \" and name.\n"
        "  x = x + 10.\n"
        "  repeat i from 1 to 10:\n"
        "      show i.\n"
        "  end.\n"
        "\n"
        "File I/O:\n"
        "  write \"text\" to \"file.txt\".\n"
        "  read \"file.txt\".\n"
        "  each line in file \"file.txt\":\n"
        "      show line.\n"
        "  end.\n",
        prog);
}

/* ─────────────────────────────────────────────────────────────────
   main
   ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {

    srand((unsigned int)time(NULL));   /* seed random number generator */

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    /* extract directory from .nat file path for tree search */
    strncpy(g_nat_exe_dir, argv[1], MAX_PATH_LEN-1);
    char *last_sep = strrchr(g_nat_exe_dir, '/');
    if (!last_sep) last_sep = strrchr(g_nat_exe_dir, '\\');
    if (last_sep) {
        *(last_sep + 1) = '\0';  /* keep trailing slash */
    } else {
        g_nat_exe_dir[0] = '\0'; /* same directory — no prefix needed */
    }

    FILE *fp = fopen(argv[1], "r");
    if (!fp) {
        fprintf(stderr, "NAT error: cannot open file '%s'\n", argv[1]);
        return 1;
    }

    /* read all lines */
    g_line_count = 0;
    while (g_line_count < MAX_LINES &&
           fgets(g_lines[g_line_count], MAX_LINE_LEN, fp)) {
        strip_trailing(g_lines[g_line_count]);
        g_line_count++;
    }
    fclose(fp);

    /* strip multi-line block comments, blank out lines inside them */
    {
        int in_block = 0;
        for (int i = 0; i < g_line_count; i++) {
            char *p = g_lines[i];
            char  out[MAX_LINE_LEN]; int oi = 0;
            int   j = 0, ln = (int)strlen(p);
            while (j < ln) {
                if (!in_block && p[j]=='/' && j+1<ln && p[j+1]=='*') {
                    in_block = 1; j += 2; continue;
                }
                if (in_block && p[j]=='*' && j+1<ln && p[j+1]=='/') {
                    in_block = 0; j += 2; continue;
                }
                if (!in_block) out[oi++] = p[j];
                j++;
            }
            out[oi] = '\0';
            strncpy(g_lines[i], out, MAX_LINE_LEN-1);
        }
    }

    /* pre-pass: register all 'fix' constants before execution */
    pre_pass_fix();

    /* v3.6 — apply injected variables before execution */
    for (int i = 0; i < g_inject_count; i++)
        set_var(g_injects[i].name, g_injects[i].value);

    /* execute */
    execute_block(0, g_line_count - 1);

    return 0;
}
