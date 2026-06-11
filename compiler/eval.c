/*
 * eval.c — NAT Language v3.0 Expression Evaluator
 *
 * Walks an expression subtree and writes the result into `out`.
 * Supports: literals, variables, constants, array indexing,
 *           arithmetic (+−×÷%), string concat (and),
 *           comparisons, and function calls.
 */

#include "nat.h"

/* forward: executor needed for function calls inside expressions */
void execute_block(int start, int end);

/* ─────────────────────────────────────────────────────────────────
   VARIABLE HELPERS
   ───────────────────────────────────────────────────────────────── */

Variable *find_var(const char *name) {
    for (int i = g_var_count - 1; i >= 0; i--)
        if (strcmp(g_vars[i].name, name) == 0)
            return &g_vars[i];
    return NULL;
}

Variable *set_var(const char *name, const char *value) {
    /* update existing */
    for (int i = g_var_count - 1; i >= 0; i--)
        if (strcmp(g_vars[i].name, name) == 0) {
            strncpy(g_vars[i].value, value, MAX_STR-1);
            g_vars[i].is_array = 0;
            return &g_vars[i];
        }
    /* create new */
    if (g_var_count >= MAX_VARS) {
        fprintf(stderr, "NAT: variable table overflow\n");
        return NULL;
    }
    Variable *v = &g_vars[g_var_count++];
    strncpy(v->name,  name,  63);
    strncpy(v->value, value, MAX_STR-1);
    v->is_array = 0;
    v->arr_len  = 0;
    return v;
}

/* ─────────────────────────────────────────────────────────────────
   CONSTANT HELPERS
   ───────────────────────────────────────────────────────────────── */

Constant *find_const(const char *name) {
    for (int i = 0; i < g_const_count; i++)
        if (strcmp(g_consts[i].name, name) == 0)
            return &g_consts[i];
    return NULL;
}

/* ─────────────────────────────────────────────────────────────────
   INTERNAL HELPERS
   ───────────────────────────────────────────────────────────────── */

/* Is string a valid number? */
static int is_number(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    int dots = 0;
    for (; *p; p++) {
        if (*p == '.') { if (++dots > 1) return 0; }
        else if (!isdigit((unsigned char)*p)) return 0;
    }
    return 1;
}

/* Format double — integer if no fractional part, else %g */
static void fmt_num(char *out, int out_size, double v) {
    long long iv = (long long)v;
    if ((double)iv == v)
        snprintf(out, out_size, "%lld", iv);
    else
        snprintf(out, out_size, "%g", v);
}

/* ─────────────────────────────────────────────────────────────────
   FUNCTION CALL HELPER  (used for CALL_EXPR nodes)
   ───────────────────────────────────────────────────────────────── */
static void call_function(const char *fname,
                          Node **call_args, int call_argc,
                          char *out, int out_size) {
    FuncDef *fn = NULL;
    for (int i = 0; i < g_func_count; i++)
        if (strcmp(g_funcs[i].name, fname) == 0) { fn = &g_funcs[i]; break; }

    if (!fn) {
        fprintf(stderr, "NAT error: unknown function '%s'\n", fname);
        strncpy(out, "", out_size);
        return;
    }

    /* save scope */
    int  saved_vc  = g_var_count;
    int  saved_ret = g_has_return;
    char saved_rv[MAX_STR];
    strncpy(saved_rv, g_return_val, MAX_STR-1);

    /* bind arguments to parameters */
    for (int j = 0; j < fn->param_count; j++) {
        char val[MAX_STR] = {0};
        if (j < call_argc && call_args[j])
            eval(call_args[j], val, MAX_STR);
        if (g_var_count < MAX_VARS) {
            strncpy(g_vars[g_var_count].name,  fn->params[j], 63);
            strncpy(g_vars[g_var_count].value, val,           MAX_STR-1);
            g_vars[g_var_count].is_array = 0;
            g_var_count++;
        }
    }

    g_has_return    = 0;
    g_return_val[0] = '\0';

    execute_block(fn->body_start, fn->body_end);

    strncpy(out, g_return_val, out_size-1);

    /* restore scope */
    g_var_count  = saved_vc;
    g_has_return = saved_ret;
    strncpy(g_return_val, saved_rv, MAX_STR-1);
}

/* ─────────────────────────────────────────────────────────────────
   eval — main evaluator
   ───────────────────────────────────────────────────────────────── */
void eval(Node *n, char *out, int out_size) {
    if (!n || !out) return;
    out[0] = '\0';

    switch (n->kind) {

    /* ── literal value ── */
    case NODE_VAL:
        strncpy(out, n->value, out_size-1);
        return;

    /* ── variable lookup (also checks constants) ── */
    case NODE_VAR: {
        /* check constants first (fix values are immutable) */
        Constant *c = find_const(n->name);
        if (c) { strncpy(out, c->value, out_size-1); return; }

        Variable *v = find_var(n->name);
        if (!v) {
            fprintf(stderr, "NAT error: unknown variable '%s'\n", n->name);
            return;
        }
        if (v->is_array) {
            /* print all elements space-separated when used without index */
            int rem = out_size - 1;
            for (int i = 0; i < v->arr_len && rem > 0; i++) {
                strncat(out, v->arr[i], rem);
                rem -= (int)strlen(v->arr[i]);
                if (i < v->arr_len - 1 && rem > 1) { strcat(out, " "); rem--; }
            }
        } else {
            strncpy(out, v->value, out_size-1);
        }
        return;
    }

    /* ── array index: name[i] ── */
    case NODE_VAR_IDX: {
        Variable *v = find_var(n->name);
        if (!v || !v->is_array) {
            fprintf(stderr, "NAT error: '%s' is not an array\n", n->name);
            return;
        }
        char idxstr[MAX_STR] = {0};
        eval(n->left, idxstr, MAX_STR);
        int idx = atoi(idxstr);
        if (idx < 0 || idx >= v->arr_len) {
            fprintf(stderr, "NAT error: index %d out of range for '%s'\n",
                    idx, n->name);
            return;
        }
        strncpy(out, v->arr[idx], out_size-1);
        return;
    }

    /* ── string concatenation: left and right ── */
    case NODE_CONCAT: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);
        snprintf(out, out_size, "%s%s", l, r);
        return;
    }

    /* ── arithmetic ── */
    case NODE_ADD_EXPR:
    case NODE_SUB_EXPR:
    case NODE_MUL_EXPR:
    case NODE_DIV_EXPR:
    case NODE_MOD_EXPR: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);

        if (is_number(l) && is_number(r)) {
            double a = atof(l), b = atof(r);
            double res = 0;
            switch (n->kind) {
                case NODE_ADD_EXPR: res = a + b; break;
                case NODE_SUB_EXPR: res = a - b; break;
                case NODE_MUL_EXPR: res = a * b; break;
                case NODE_MOD_EXPR:
                    if ((long long)b == 0) {
                        fprintf(stderr, "NAT error: modulo by zero\n");
                        strncpy(out, "0", out_size-1); return;
                    }
                    res = (double)((long long)a % (long long)b);
                    break;
                default: /* NODE_DIV_EXPR */
                    if (b == 0.0) {
                        fprintf(stderr, "NAT error: division by zero\n");
                        strncpy(out, "0", out_size-1); return;
                    }
                    res = a / b;
                    break;
            }
            fmt_num(out, out_size, res);
        } else {
            /* string + string falls back to concatenation */
            if (n->kind == NODE_ADD_EXPR)
                snprintf(out, out_size, "%s%s", l, r);
            else
                strncpy(out, l, out_size-1);
        }
        return;
    }

    /* ── comparisons → "1" or "0" ── */
    case NODE_CMP_EQ:
    case NODE_CMP_NEQ:
    case NODE_CMP_GT:
    case NODE_CMP_LT:
    case NODE_CMP_GTE:
    case NODE_CMP_LTE: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);
        int cmp;
        if (is_number(l) && is_number(r)) {
            double a = atof(l), b = atof(r);
            switch (n->kind) {
                case NODE_CMP_EQ:  cmp = (a == b); break;
                case NODE_CMP_NEQ: cmp = (a != b); break;
                case NODE_CMP_GT:  cmp = (a >  b); break;
                case NODE_CMP_LT:  cmp = (a <  b); break;
                case NODE_CMP_GTE: cmp = (a >= b); break;
                default:           cmp = (a <= b); break;
            }
        } else {
            int sc = strcmp(l, r);
            switch (n->kind) {
                case NODE_CMP_EQ:  cmp = (sc == 0); break;
                case NODE_CMP_NEQ: cmp = (sc != 0); break;
                case NODE_CMP_GT:  cmp = (sc >  0); break;
                case NODE_CMP_LT:  cmp = (sc <  0); break;
                case NODE_CMP_GTE: cmp = (sc >= 0); break;
                default:           cmp = (sc <= 0); break;
            }
        }
        strncpy(out, cmp ? "1" : "0", out_size-1);
        return;
    }

    /* ── function call inside expression ── */
    case NODE_CALL_EXPR:
        call_function(n->name, n->args, n->arg_count, out, out_size);
        return;

    case NODE_NOOP:
    default:
        return;
    }
}
