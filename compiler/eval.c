/*
 * eval.c — NAT Language v3.2 Expression Evaluator
 *
 * v3.2 additions:
 *   NODE_AND, NODE_OR      — logical and/or
 *   NODE_CONTAINS          — string contains check
 *   NODE_POW_EXPR          — 2^8 power
 *   NODE_LENGTH            — length of x
 *   NODE_UPPER             — upper of x
 *   NODE_LOWER             — lower of x
 *   NODE_ABS               — abs of x
 *   NODE_ROUND             — round of x
 */

#include "nat.h"

void execute_block(int start, int end);
static int is_number(const char *s);     /* forward decl — defined later in this file */
static double eval_num(Node *n);         /* forward decl — defined later in this file */

/* ─────────────────────────────────────────────────────────────────
   VARIABLE HELPERS
   ───────────────────────────────────────────────────────────────── */

/* v4.0 Phase 1a — detect if a string is purely numeric (using the
   same rules as is_number()) and cache the parsed value. */
void cache_numeric(Variable *v, const char *value) {
    if (is_number(value)) {
        v->is_num    = 1;
        v->num_value = atof(value);
    } else {
        v->is_num = 0;
    }
}

Variable *find_var(const char *name) {
    for (int i = g_var_count - 1; i >= 0; i--)
        if (strcmp(g_vars[i].name, name) == 0)
            return &g_vars[i];
    return NULL;
}

Variable *set_var(const char *name, const char *value) {
    for (int i = g_var_count - 1; i >= 0; i--)
        if (strcmp(g_vars[i].name, name) == 0) {
            strncpy(g_vars[i].value, value, MAX_STR-1);
            g_vars[i].is_array = 0;
            cache_numeric(&g_vars[i], value);
            return &g_vars[i];
        }
    if (g_var_count >= MAX_VARS) {
        fprintf(stderr, "NAT: variable table overflow\n");
        return NULL;
    }
    Variable *v = &g_vars[g_var_count++];
    strncpy(v->name,  name,  63);
    strncpy(v->value, value, MAX_STR-1);
    v->is_array = 0;
    v->arr_len  = 0;
    cache_numeric(v, value);
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

static int is_number(const char *s) {
    if (!s || !*s) return 0;
    const char *p = s;
    if (*p == '-' || *p == '+') p++;
    if (!*p) return 0;
    int dots = 0, has_digits = 0;
    for (; *p; p++) {
        if (*p == '.') {
            if (++dots > 1) return 0;
        } else if (*p == 'e' || *p == 'E') {
            /* scientific notation: must have seen digits before e */
            if (!has_digits) return 0;
            p++;
            if (*p == '-' || *p == '+') p++;
            if (!*p) return 0;
            /* rest must be digits */
            for (; *p; p++)
                if (!isdigit((unsigned char)*p)) return 0;
            return 1;
        } else if (!isdigit((unsigned char)*p)) {
            return 0;
        } else {
            has_digits = 1;
        }
    }
    return has_digits;
}

static void fmt_num(char *out, int out_size, double v) {
    long long iv = (long long)v;
    if ((double)iv == v)
        snprintf(out, out_size, "%lld", iv);
    else {
        /* %.10g avoids premature scientific notation on numbers
           like 1000000.5 while still trimming trailing zeros */
        snprintf(out, out_size, "%.10g", v);
    }
}

/* truth value of a string result — also used in eval for AND/OR */
static int is_truthy(const char *s) {
    return (s && strlen(s) > 0 && strcmp(s, "0") != 0);
}

/* ─────────────────────────────────────────────────────────────────
   FUNCTION CALL HELPER
   ───────────────────────────────────────────────────────────────── */
static void call_function(const char *fname,
                          Node **call_args, int call_argc,
                          char *out, int out_size) {
    FuncDef *fn = NULL;
    for (int i = 0; i < g_func_count; i++)
        if (strcmp(g_funcs[i].name, fname) == 0) { fn = &g_funcs[i]; break; }

    if (!fn) {
        const char *_fcands[MAX_FUNCS+1];
        int _nf = 0;
        for (int _fi = 0; _fi < g_func_count; _fi++) _fcands[_nf++] = g_funcs[_fi].name;
        _fcands[_nf] = NULL;
        err_unknown_func(g_current_line, fname, _fcands);
        strncpy(out, "", out_size);
        return;
    }

    /* argument count check */
    if (call_argc != fn->param_count) {
        err_arg_count(g_current_line, fn->name, fn->param_count, call_argc);
        strncpy(out, "", out_size);
        return;
    }

    int  saved_vc  = g_var_count;
    int  saved_ret = g_has_return;
    char saved_rv[MAX_STR];
    strncpy(saved_rv, g_return_val, MAX_STR-1);

    for (int j = 0; j < fn->param_count; j++) {
        char val[MAX_STR] = {0};
        if (j < call_argc && call_args[j])
            eval(call_args[j], val, MAX_STR);
        if (g_var_count < MAX_VARS) {
            strncpy(g_vars[g_var_count].name,  fn->params[j], 63);
            strncpy(g_vars[g_var_count].value, val,           MAX_STR-1);
            g_vars[g_var_count].is_array = 0;
            cache_numeric(&g_vars[g_var_count], val);
            g_var_count++;
        }
    }

    g_has_return    = 0;
    g_return_val[0] = '\0';

    execute_func_body(fn);

    strncpy(out, g_return_val, out_size-1);

    g_var_count  = saved_vc;
    g_has_return = saved_ret;
    strncpy(g_return_val, saved_rv, MAX_STR-1);
}

/* ─────────────────────────────────────────────────────────────────
   eval_num  (v4.0 Phase 1b)
   Fast numeric evaluator — returns double directly, zero string
   conversion for the hot arithmetic path.
   Falls back to eval()+atof() for anything it can't handle directly.
   ───────────────────────────────────────────────────────────────── */
static double eval_num(Node *n) {
    if (!n) return 0.0;

    switch (n->kind) {

    /* literal number */
    case NODE_VAL:
        return atof(n->value);

    /* variable — use cached double if available */
    case NODE_VAR: {
        Constant *c = find_const(n->name);
        if (c) return atof(c->value);
        Variable *v = find_var(n->name);
        if (!v) return 0.0;
        if (v->is_num) return v->num_value;
        return atof(v->value);
    }

    /* arithmetic — fully recursive, pure double, zero string I/O */
    case NODE_ADD_EXPR: return eval_num(n->left) + eval_num(n->right);
    case NODE_SUB_EXPR: return eval_num(n->left) - eval_num(n->right);
    case NODE_MUL_EXPR: return eval_num(n->left) * eval_num(n->right);
    case NODE_POW_EXPR: return pow(eval_num(n->left), eval_num(n->right));
    case NODE_DIV_EXPR: {
        double b = eval_num(n->right);
        if (b == 0.0) { err_div_zero(g_current_line); return 0.0; }
        return eval_num(n->left) / b;
    }
    case NODE_MOD_EXPR: {
        long long b = (long long)eval_num(n->right);
        if (b == 0) { err_mod_zero(g_current_line); return 0.0; }
        return (double)((long long)eval_num(n->left) % b);
    }

    /* unary minus — check how it's stored */
    /* NAT folds unary minus at parse time into NODE_VAL("-5"),
       so no separate NODE_NEG exists — nothing to handle here */

    /* fallback — let the string eval handle it */
    default: {
        char buf[MAX_STR] = {0};
        eval(n, buf, MAX_STR);
        return atof(buf);
    }
    }
}

/* ─────────────────────────────────────────────────────────────────
   eval
   ───────────────────────────────────────────────────────────────── */
void eval(Node *n, char *out, int out_size) {
    if (!n || !out) return;
    out[0] = '\0';

    switch (n->kind) {

    case NODE_VAL:
        strncpy(out, n->value, out_size-1);
        return;

    case NODE_VAR: {
        Constant *c = find_const(n->name);
        if (c) { strncpy(out, c->value, out_size-1); return; }

        Variable *v = find_var(n->name);
        if (!v) {
            {
            const char *_cands[MAX_VARS+1];
            int _nc = 0;
            for (int _i = 0; _i < g_var_count; _i++) _cands[_nc++] = g_vars[_i].name;
            _cands[_nc] = NULL;
            err_unknown_var(g_current_line, n->name, _cands);
        }
            return;
        }
        if (v->is_array) {
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

    case NODE_VAR_IDX: {
        Variable *v = find_var(n->name);
        if (!v) {
            char _what[256];
            snprintf(_what, sizeof(_what),
                "'%s' is not an array — cannot use index [ ]", n->name);
            nat_error(g_current_line, _what,
                "declare it as an array:  let nums are 1 2 3.");
            return;
        }
        char idxstr[MAX_STR] = {0};
        eval(n->left, idxstr, MAX_STR);
        int idx = atoi(idxstr);

        if (v->is_array) {
            /* array index */
            if (idx < 0 || idx >= v->arr_len) {
                err_index_range(g_current_line, n->name, idx, v->arr_len);
                return;
            }
            strncpy(out, v->arr[idx], out_size-1);
        } else {
            /* string character index */
            int slen = (int)strlen(v->value);
            if (idx < 0 || idx >= slen) {
                err_index_range(g_current_line, n->name, idx, slen);
                return;
            }
            out[0] = v->value[idx];
            out[1] = '\0';
        }
        return;
    }

    /* ── string concat ── */
    case NODE_CONCAT: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);
        strncpy(out, l, out_size-1);
        strncat(out, r, out_size-1-strlen(out));
        return;
    }

    /* ── logical AND: both sides must be truthy ── */
    case NODE_AND: {
        char l[MAX_STR]={0};
        eval(n->left, l, MAX_STR);
        if (!is_truthy(l)) { strncpy(out, "0", out_size-1); return; }
        char r[MAX_STR]={0};
        eval(n->right, r, MAX_STR);
        strncpy(out, is_truthy(r) ? "1" : "0", out_size-1);
        return;
    }

    /* ── logical OR: at least one side truthy ── */
    case NODE_OR: {
        char l[MAX_STR]={0};
        eval(n->left, l, MAX_STR);
        if (is_truthy(l)) { strncpy(out, "1", out_size-1); return; }
        char r[MAX_STR]={0};
        eval(n->right, r, MAX_STR);
        strncpy(out, is_truthy(r) ? "1" : "0", out_size-1);
        return;
    }

    /* ── contains ── */
    case NODE_CONTAINS: {
        char haystack[MAX_STR]={0}, needle[MAX_STR]={0};
        eval(n->left,  haystack, MAX_STR);
        eval(n->right, needle,   MAX_STR);
        strncpy(out, strstr(haystack, needle) ? "1" : "0", out_size-1);
        return;
    }

    /* ── length of x ── */
    case NODE_LENGTH: {
        /* if left is a variable that is an array, return element count */
        if (n->left && n->left->kind == NODE_VAR) {
            Variable *v = find_var(n->left->name);
            if (v && v->is_array) {
                snprintf(out, out_size, "%d", v->arr_len);
                return;
            }
        }
        /* otherwise return string length */
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        snprintf(out, out_size, "%d", (int)strlen(val));
        return;
    }

    /* ── upper of x ── */
    case NODE_UPPER: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        for (int i = 0; val[i]; i++)
            val[i] = (char)toupper((unsigned char)val[i]);
        strncpy(out, val, out_size-1);
        return;
    }

    /* ── lower of x ── */
    case NODE_LOWER: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        for (int i = 0; val[i]; i++)
            val[i] = (char)tolower((unsigned char)val[i]);
        strncpy(out, val, out_size-1);
        return;
    }

    /* ── abs of x ── */
    case NODE_ABS: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        double v = atof(val);
        fmt_num(out, out_size, fabs(v));
        return;
    }

    /* ── larger(a,b,c,...) — returns the maximum value ── */
    case NODE_LARGER: {
        if (n->arg_count == 0) { strncpy(out, "0", out_size-1); return; }
        char v[MAX_STR] = {0};
        eval(n->args[0], v, MAX_STR);
        double best = atof(v);
        for (int i = 1; i < n->arg_count; i++) {
            char a[MAX_STR] = {0};
            eval(n->args[i], a, MAX_STR);
            double d = atof(a);
            if (d > best) best = d;
        }
        fmt_num(out, out_size, best);
        return;
    }

    /* ── smallest(a,b,c,...) — returns the minimum value ── */
    case NODE_SMALLEST: {
        if (n->arg_count == 0) { strncpy(out, "0", out_size-1); return; }
        char v[MAX_STR] = {0};
        eval(n->args[0], v, MAX_STR);
        double best = atof(v);
        for (int i = 1; i < n->arg_count; i++) {
            char a[MAX_STR] = {0};
            eval(n->args[i], a, MAX_STR);
            double d = atof(a);
            if (d < best) best = d;
        }
        fmt_num(out, out_size, best);
        return;
    }

    /* ── round of x ── */
    case NODE_ROUND: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        double v = atof(val);
        fmt_num(out, out_size, round(v));
        return;
    }

    /* ── text of x — number to string ── */
    case NODE_TEXT_OF: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        strncpy(out, val, out_size-1);
        return;
    }

    /* ── repeat "ha" N times ── */
    case NODE_STR_REPEAT: {
        char str[MAX_STR]={0}, cnt[MAX_STR]={0};
        eval(n->left,  str, MAX_STR);
        eval(n->right, cnt, MAX_STR);
        int times = atoi(cnt);
        if (times < 0) times = 0;
        int slen = (int)strlen(str);
        int pos  = 0;
        for (int i = 0; i < times && pos < out_size - 1; i++) {
            int space = out_size - 1 - pos;
            int copy  = slen < space ? slen : space;
            strncpy(out + pos, str, copy);
            pos += copy;
        }
        out[pos] = '\0';
        return;
    }

    /* ── reverse of x — string or array ── */
    case NODE_REVERSE: {
        /* if left is array var — return reversed as space-joined string */
        if (n->left && n->left->kind == NODE_VAR) {
            Variable *v = find_var(n->left->name);
            if (v && v->is_array) {
                int rem = out_size - 1;
                for (int i = v->arr_len - 1; i >= 0 && rem > 0; i--) {
                    strncat(out, v->arr[i], rem);
                    rem -= (int)strlen(v->arr[i]);
                    if (i > 0 && rem > 1) { strcat(out, " "); rem--; }
                }
                return;
            }
        }
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        int len = (int)strlen(val);
        for (int i = 0; i < len && i < out_size-1; i++)
            out[i] = val[len - 1 - i];
        out[len < out_size-1 ? len : out_size-1] = '\0';
        return;
    }

    /* ── first N of x ── */
    case NODE_FIRST_N: {
        char cnt[MAX_STR]={0}, src[MAX_STR]={0};
        eval(n->left,  cnt, MAX_STR);
        eval(n->right, src, MAX_STR);
        int N = atoi(cnt);
        if (N < 0) N = 0;
        if (N > (int)strlen(src)) N = (int)strlen(src);
        strncpy(out, src, N < out_size-1 ? N : out_size-1);
        out[N < out_size-1 ? N : out_size-1] = '\0';
        return;
    }

    /* ── last N of x ── */
    case NODE_LAST_N: {
        char cnt[MAX_STR]={0}, src[MAX_STR]={0};
        eval(n->left,  cnt, MAX_STR);
        eval(n->right, src, MAX_STR);
        int N   = atoi(cnt);
        int len = (int)strlen(src);
        if (N < 0) N = 0;
        if (N > len) N = len;
        strncpy(out, src + (len - N), out_size-1);
        return;
    }

    /* ── first of arr ── */
    case NODE_FIRST_ELEM: {
        if (n->left && n->left->kind == NODE_VAR) {
            Variable *v = find_var(n->left->name);
            if (v && v->is_array) {
                if (v->arr_len == 0) {
                    err_empty_array_op(g_current_line, "first", n->left->name);
                    return;
                }
                strncpy(out, v->arr[0], out_size-1);
                return;
            }
        }
        /* fallback: first char of string */
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        if (val[0]) { out[0] = val[0]; out[1] = '\0'; }
        return;
    }

    /* ── last of arr ── */
    case NODE_LAST_ELEM: {
        if (n->left && n->left->kind == NODE_VAR) {
            Variable *v = find_var(n->left->name);
            if (v && v->is_array) {
                if (v->arr_len == 0) {
                    err_empty_array_op(g_current_line, "last", n->left->name);
                    return;
                }
                strncpy(out, v->arr[v->arr_len-1], out_size-1);
                return;
            }
        }
        /* fallback: last char of string */
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        int len = (int)strlen(val);
        if (len > 0) { out[0] = val[len-1]; out[1] = '\0'; }
        return;
    }

    /* ── x is even ── */
    case NODE_IS_EVEN: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        long long iv = (long long)atof(val);
        strncpy(out, (iv % 2 == 0) ? "1" : "0", out_size-1);
        return;
    }

    /* ── x is odd ── */
    case NODE_IS_ODD: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        long long iv = (long long)atof(val);
        strncpy(out, (iv % 2 != 0) ? "1" : "0", out_size-1);
        return;
    }
    /* ── clamp x from A to B ── */
    case NODE_CLAMP: {
        char val[MAX_STR]={0}, lo[MAX_STR]={0}, hi[MAX_STR]={0};
        eval(n->left,    val, MAX_STR);
        eval(n->args[0], lo,  MAX_STR);
        eval(n->args[1], hi,  MAX_STR);
        double v = atof(val), a = atof(lo), b = atof(hi);
        if (v < a) v = a;
        if (v > b) v = b;
        fmt_num(out, out_size, v);
        return;
    }

    case NODE_MAX: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);
        double a = atof(l), b = atof(r);
        fmt_num(out, out_size, a > b ? a : b);
        return;
    }

    /* ── min of a and b ── */
    case NODE_MIN: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};
        eval(n->left,  l, MAX_STR);
        eval(n->right, r, MAX_STR);
        double a = atof(l), b = atof(r);
        fmt_num(out, out_size, a < b ? a : b);
        return;
    }

    /* ── floor of x ── */
    case NODE_FLOOR: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        fmt_num(out, out_size, floor(atof(val)));
        return;
    }

    /* ── ceil of x ── */
    case NODE_CEIL: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        fmt_num(out, out_size, ceil(atof(val)));
        return;
    }

    /* ── x is number ── */
    case NODE_IS_NUMBER: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        strncpy(out, is_number(val) ? "1" : "0", out_size-1);
        return;
    }

    /* ── x is text ── */
    case NODE_IS_TEXT: {
        char val[MAX_STR]={0};
        eval(n->left, val, MAX_STR);
        strncpy(out, is_number(val) ? "0" : "1", out_size-1);
        return;
    }
    case NODE_TRIM: {
        char val[MAX_STR] = {0};
        eval(n->left, val, MAX_STR);
        /* trim leading */
        char *start = val;
        while (*start == ' ' || *start == '\t') start++;
        /* trim trailing */
        int end = (int)strlen(start) - 1;
        while (end >= 0 && (start[end] == ' ' || start[end] == '\t')) end--;
        start[end + 1] = '\0';
        strncpy(out, start, out_size - 1);
        return;
    }

    /* ── replace "A" with "B" in x ── */
    case NODE_REPLACE: {
        char src[MAX_STR]  = {0};
        char find[MAX_STR] = {0};
        char rep[MAX_STR]  = {0};
        eval(n->args[0], find, MAX_STR);   /* search  */
        eval(n->args[1], rep,  MAX_STR);   /* replace */
        eval(n->args[2], src,  MAX_STR);   /* source  */
        int flen = (int)strlen(find);
        if (flen == 0) { strncpy(out, src, out_size - 1); return; }
        char result[MAX_STR] = {0};
        int  ri = 0;
        char *p = src;
        while (*p && ri < MAX_STR - 1) {
            if (strncmp(p, find, flen) == 0) {
                int rlen = (int)strlen(rep);
                strncpy(result + ri, rep, MAX_STR - ri - 1);
                ri += rlen;
                p  += flen;
            } else {
                result[ri++] = *p++;
            }
        }
        result[ri] = '\0';
        strncpy(out, result, out_size - 1);
        return;
    }

    /* ── split x by "sep" — stores as array, returns first element ── */
    case NODE_SPLIT: {
        char src[MAX_STR] = {0};
        char sep[MAX_STR] = {0};
        eval(n->left,  src, MAX_STR);
        eval(n->right, sep, MAX_STR);
        /* result stored in g_split_result — accessed via split_result var */
        /* We store split result into a special variable __split__ */
        if (g_var_count >= MAX_VARS) { err_var_overflow(); return; }
        Variable *v = NULL;
        /* find or create __split__ */
        for (int i = g_var_count - 1; i >= 0; i--)
            if (strcmp(g_vars[i].name, "__split__") == 0) { v = &g_vars[i]; break; }
        if (!v) {
            v = &g_vars[g_var_count++];
            strncpy(v->name, "__split__", 63);
        }
        memset(v->arr, 0, sizeof(v->arr));
        v->is_array = 1;
        v->arr_len  = 0;
        int slen = (int)strlen(sep);
        if (slen == 0) {
            strncpy(v->arr[v->arr_len++], src, MAX_STR - 1);
        } else {
            char tmp[MAX_STR]; strncpy(tmp, src, MAX_STR - 1);
            char *p = tmp;
            while (*p && v->arr_len < MAX_ARRAY_ELEM) {
                char *found = strstr(p, sep);
                if (found) {
                    int seglen = (int)(found - p);
                    char seg[MAX_STR] = {0};
                    strncpy(seg, p, seglen < MAX_STR - 1 ? seglen : MAX_STR - 1);
                    strncpy(v->arr[v->arr_len++], seg, MAX_STR - 1);
                    p = found + slen;
                } else {
                    strncpy(v->arr[v->arr_len++], p, MAX_STR - 1);
                    break;
                }
            }
        }
        /* return first element as value, array accessible via __split__ */
        if (v->arr_len > 0) strncpy(out, v->arr[0], out_size - 1);
        return;
    }
    case NODE_ADD_EXPR:
    case NODE_SUB_EXPR:
    case NODE_MUL_EXPR:
    case NODE_DIV_EXPR:
    case NODE_MOD_EXPR:
    case NODE_POW_EXPR: {
        char l[MAX_STR]={0}, r[MAX_STR]={0};

        /* check for array used in math — left side */
        if (n->left && n->left->kind == NODE_VAR) {
            Variable *vl = find_var(n->left->name);
            if (vl && vl->is_array) {
                err_array_in_math(g_current_line, n->left->name);
                return;
            }
        }
        /* check for array used in math — right side */
        if (n->right && n->right->kind == NODE_VAR) {
            Variable *vr = find_var(n->right->name);
            if (vr && vr->is_array) {
                err_array_in_math(g_current_line, n->right->name);
                return;
            }
        }

        /* v4.0 Phase 1b — eval both sides into string buffers first
           (same as before), then use eval_num() for the actual computation
           to avoid redundant atof() calls at every tree level.
           This is safe: eval() for probe, eval_num() for speed. */
        char lcheck[MAX_STR] = {0}, rcheck[MAX_STR] = {0};
        eval(n->left,  lcheck, MAX_STR);
        eval(n->right, rcheck, MAX_STR);

        if (is_number(lcheck) && is_number(rcheck)) {
            /* both sides numeric — eval_num handles deep subtrees in double */
            double a = eval_num(n->left);
            double b = eval_num(n->right);
            double res = 0;
            switch (n->kind) {
                case NODE_ADD_EXPR: res = a + b; break;
                case NODE_SUB_EXPR: res = a - b; break;
                case NODE_MUL_EXPR: res = a * b; break;
                case NODE_POW_EXPR: res = pow(a, b); break;
                case NODE_MOD_EXPR:
                    if ((long long)b == 0) {
                        err_mod_zero(g_current_line);
                        strncpy(out, "0", out_size-1); return;
                    }
                    res = (double)((long long)a % (long long)b);
                    break;
                default: /* DIV */
                    if (b == 0.0) {
                        err_div_zero(g_current_line);
                        strncpy(out, "0", out_size-1); return;
                    }
                    res = a / b;
                    break;
            }
            fmt_num(out, out_size, res);
        } else if (n->kind == NODE_ADD_EXPR) {
            /* string + string → concat */
            strncpy(out, lcheck, out_size-1);
            strncat(out, rcheck, out_size-1-strlen(out));
        } else {
            /* math op on text — fire error */
            if (!is_number(lcheck) && n->left && n->left->kind == NODE_VAR)
                err_math_on_text(g_current_line, n->left->name);
            else if (!is_number(rcheck) && n->right && n->right->kind == NODE_VAR)
                err_math_on_text(g_current_line, n->right->name);
            else
                nat_error(g_current_line,
                    "cannot do math on a text value",
                    "make sure both sides of the operator are numbers");
        }
        return;
    }

    /* ── comparisons ── */
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

    /* ── random from A to B (expression style) ── */
    case NODE_RANDOM: {
        char lo[MAX_STR]={0}, hi[MAX_STR]={0};
        eval(n->left,  lo, MAX_STR);
        eval(n->right, hi, MAX_STR);
        int a = atoi(lo), b = atoi(hi);
        if (a > b) { int tmp = a; a = b; b = tmp; }
        int r = a + rand() % (b - a + 1);
        fmt_num(out, out_size, r);
        return;
    }

    case NODE_CALL_EXPR:
        call_function(n->name, n->args, n->arg_count, out, out_size);
        return;

    /* ── read "file.txt" at line N — expression ── */
    case NODE_FILE_READ_LINE: {
        char lnum[64] = {0};
        if (n->right) eval(n->right, lnum, 64);
        int target = atoi(lnum);
        FILE *fp = fopen(n->name, "rb");
        if (!fp) { err_file_not_found(g_current_line, n->name); return; }
        char buf[MAX_STR]; int ln = 1;
        while (fgets(buf, MAX_STR, fp)) {
            if (ln == target) {
                int bl = strlen(buf);
                if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
                strncpy(out, buf, out_size-1);
                fclose(fp);
                return;
            }
            ln++;
        }
        fclose(fp);
        err_line_range(g_current_line, n->name, target);
        return;
    }

    /* ── file "x.txt" exists — boolean ── */
    case NODE_FILE_EXISTS: {
        FILE *fp = fopen(n->name, "rb");
        if (fp) { fclose(fp); strncpy(out, "1", out_size-1); }
        else    { strncpy(out, "0", out_size-1); }
        return;
    }

    case NODE_NOOP:
    default:
        return;
    }
}
