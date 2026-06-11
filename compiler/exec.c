/*
 * exec.c — NAT Language v3.2 Executor
 *
 * v3.2 additions:
 *   NODE_LET_DECLARE  — let x, b, c.   (declare without value)
 *   NODE_ASSIGN       — x be 10.       (assign after declaration)
 */

#include "nat.h"

void execute(Node *n);
void execute_block(int start, int end);

static int is_truthy(const char *s) {
    return (s && strlen(s) > 0 && strcmp(s, "0") != 0);
}

void execute(Node *n) {
    if (!n) return;

    switch (n->kind) {

    /* ── let name be value ── */
    case NODE_LET: {
        /* check if trying to reassign a constant */
        if (find_const(n->name)) {
            err_const_reassign(g_current_line, n->name);
            return;
        }
        /* special case: right side is a split — store array under this var name */
        if (n->left && n->left->kind == NODE_SPLIT) {
            char dummy[MAX_STR] = {0};
            eval(n->left, dummy, MAX_STR);
            /* rename __split__ to n->name */
            Variable *sp = find_var("__split__");
            if (sp) strncpy(sp->name, n->name, 63);
            return;
        }
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        else         strncpy(val, n->value, MAX_STR-1);
        set_var(n->name, val);
        return;
    }

    /* ── let x, b, c.  — declare with empty value ── */
    case NODE_LET_DECLARE: {
        for (int i = 0; i < n->param_count; i++)
            set_var(n->params[i], "0");
        return;
    }

    /* ── x be 10.  — bare assignment ── */
    case NODE_ASSIGN: {
        if (find_const(n->name)) {
            err_const_reassign(g_current_line, n->name);
            return;
        }
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        /* update if exists, otherwise create */
        set_var(n->name, val);
        return;
    }

    /* ── let nums are 1 2 3 ── */
    case NODE_ARRAY: {
        if (g_var_count >= MAX_VARS) return;
        Variable *v = &g_vars[g_var_count++];
        memset(v, 0, sizeof(Variable));
        strncpy(v->name, n->name, 63);
        v->is_array = 1;
        v->arr_len  = 0;
        for (int i = 0; i < n->arg_count && i < MAX_ARRAY_ELEM; i++) {
            char elem[MAX_STR] = {0};
            eval(n->args[i], elem, MAX_STR);
            strncpy(v->arr[v->arr_len++], elem, MAX_STR-1);
        }
        return;
    }

    /* ── show ── */
    case NODE_SHOW: {
        /* show each item in arr. / online / with "sep" */
        if (n->left && n->left->kind == NODE_SHOW_EACH) {
            Node *se = n->left;
            int online = (se->value[0] == 'O');
            const char *sep = online ? se->params[1] : "\n";

            Variable *src_arr = NULL;
            if (se->left && se->left->kind == NODE_VAR)
                src_arr = find_var(se->left->name);

            if (src_arr && src_arr->is_array) {
                for (int i = 0; i < src_arr->arr_len; i++) {
                    if (online) {
                        if (i > 0) printf("%s", sep);
                        printf("%s", src_arr->arr[i]);
                    } else {
                        printf("%s\n", src_arr->arr[i]);
                    }
                }
            } else {
                /* string characters */
                char src_val[MAX_STR] = {0};
                eval(se->left, src_val, MAX_STR);
                int len = (int)strlen(src_val);
                for (int i = 0; i < len; i++) {
                    if (online) {
                        if (i > 0) printf("%s", sep);
                        printf("%c", src_val[i]);
                    } else {
                        printf("%c\n", src_val[i]);
                    }
                }
            }
            if (online) printf("\n");
            return;
        }
        char result[MAX_STR] = {0};
        eval(n->left, result, MAX_STR);
        printf("%s\n", result);
        return;
    }

    /* ── give ── */
    case NODE_GIVE: {
        char val[MAX_STR] = {0};
        eval(n->left, val, MAX_STR);
        strncpy(g_return_val, val, MAX_STR-1);
        g_has_return = 1;
        return;
    }

    /* ── make funcname with p1 p2 inside: ... end ── */
    case NODE_FUNC_DEF: {
        if (g_func_count >= MAX_FUNCS) {
            err_func_overflow();
            return;
        }
        FuncDef *fn = &g_funcs[g_func_count++];
        memset(fn, 0, sizeof(FuncDef));
        strncpy(fn->name, n->name, 63);
        fn->param_count = n->param_count;
        for (int i = 0; i < n->param_count; i++)
            strncpy(fn->params[i], n->params[i], 63);
        fn->body_start = n->body_start;
        fn->body_end   = n->body_end;
        return;
    }

    /* ── bare function call ── */
    case NODE_FUNC_CALL: {
        FuncDef *fn = NULL;
        for (int i = 0; i < g_func_count; i++)
            if (strcmp(g_funcs[i].name, n->name) == 0) { fn = &g_funcs[i]; break; }
        if (!fn) {
            const char *_fcands[MAX_FUNCS+1];
            int _nf = 0;
            for (int _fi = 0; _fi < g_func_count; _fi++) _fcands[_nf++] = g_funcs[_fi].name;
            _fcands[_nf] = NULL;
            err_unknown_func(g_current_line, n->name, _fcands);
            return;
        }
        /* argument count check */
        if (n->arg_count != fn->param_count) {
            err_arg_count(g_current_line, fn->name, fn->param_count, n->arg_count);
            return;
        }
        /* empty body warning */
        if (fn->body_start > fn->body_end)
            warn_empty_func(g_current_line, fn->name);
        int  saved_vc  = g_var_count;
        int  saved_ret = g_has_return;
        char saved_rv[MAX_STR];
        strncpy(saved_rv, g_return_val, MAX_STR-1);

        for (int j = 0; j < fn->param_count; j++) {
            char val[MAX_STR] = {0};
            if (j < n->arg_count && n->args[j])
                eval(n->args[j], val, MAX_STR);
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
        g_var_count  = saved_vc;
        g_has_return = saved_ret;
        strncpy(g_return_val, saved_rv, MAX_STR-1);
        return;
    }

    /* ── repeat N times ── */
    case NODE_LOOP: {
        int count = 0;
        if (n->left) {
            char tmp[MAX_STR] = {0};
            eval(n->left, tmp, MAX_STR);
            count = atoi(tmp);
        }
        if (count <= 0) {
            err_repeat_count(g_current_line);
            return;
        }
        for (int i = 0; i < count; i++) {
            execute_block(n->body_start, n->body_end);
            if (g_has_return) return;
        }
        return;
    }

    /* ── repeat i from X to Y step Z ── */
    case NODE_FOR_LOOP: {
        char tmp[MAX_STR] = {0};
        double from_val = 0, to_val = 0, step_val = 1;

        if (n->left)        { eval(n->left,        tmp, MAX_STR); from_val = atof(tmp); }
        if (n->right)       { eval(n->right,       tmp, MAX_STR); to_val   = atof(tmp); }
        if (n->else_branch) { eval(n->else_branch, tmp, MAX_STR); step_val = atof(tmp); }
        if (step_val == 0) {
            err_step_zero(g_current_line);
            return;
        }

        for (double i = from_val; i <= to_val; i += step_val) {

            long long iv = (long long)i;
            if ((double)iv == i)
                snprintf(tmp, sizeof(tmp), "%lld", iv);
            else
                snprintf(tmp, sizeof(tmp), "%g", i);

            Variable *lv = find_var(n->name);
            if (lv) {
                strncpy(lv->value, tmp, MAX_STR-1);
            } else if (g_var_count < MAX_VARS) {
                strncpy(g_vars[g_var_count].name,  n->name, 63);
                strncpy(g_vars[g_var_count].value, tmp,     MAX_STR-1);
                g_vars[g_var_count].is_array = 0;
                g_var_count++;
            }
            execute_block(n->body_start, n->body_end);
            if (g_has_return) return;

        }
        return;
    }

    /* ── while ── */
    case NODE_WHILE: {
        int safety = 1000000;
        while (safety-- > 0) {
            char cond[MAX_STR] = {0};
            eval(n->left, cond, MAX_STR);
            if (!is_truthy(cond)) break;
            execute_block(n->body_start, n->body_end);
            if (g_has_return) return;
        }
        if (safety <= 0)
            warn_infinite_loop(g_current_line);
        return;
    }

    /* ── if / else if / else ── */
    case NODE_IF: {
        char cond[MAX_STR] = {0};
        eval(n->left, cond, MAX_STR);
        if (is_truthy(cond)) {
            execute_block(n->body_start, n->body_end);
        } else if (n->else_branch) {
            /* else if — else_branch is itself a NODE_IF */
            if (n->else_branch->kind == NODE_IF) {
                execute(n->else_branch);
            } else {
                /* plain else — else_branch is NODE_NOOP with body range */
                execute_block(n->else_branch->body_start,
                              n->else_branch->body_end);
            }
        }
        return;
    }

    /* ── add x with y to z ── */
    case NODE_ADD_ASSIGN: {
        /* check if target is an array — if so, append */
        if (n->name[0]) {
            Variable *tv = find_var(n->name);
            if (tv && tv->is_array) {
                if (tv->arr_len >= MAX_ARRAY_ELEM) {
                    nat_error(g_current_line,
                        "array is full — cannot append more elements",
                        "increase MAX_ARRAY_ELEM or use fewer elements");
                    return;
                }
                char val[MAX_STR] = {0};
                eval(n->left, val, MAX_STR);
                strncpy(tv->arr[tv->arr_len++], val, MAX_STR-1);
                return;
            }
        }
        /* normal math add */
        char lv[MAX_STR]={0}, rv[MAX_STR]={0};
        eval(n->left,  lv, MAX_STR);
        eval(n->right, rv, MAX_STR);
        char result[MAX_STR] = {0};
        if (strlen(lv) > 0 && strlen(rv) > 0) {
            int lnum=1, rnum=1;
            for (int i=0; lv[i]; i++) if (!isdigit((unsigned char)lv[i]) && lv[i]!='.' && lv[i]!='-') { lnum=0; break; }
            for (int i=0; rv[i]; i++) if (!isdigit((unsigned char)rv[i]) && rv[i]!='.' && rv[i]!='-') { rnum=0; break; }
            if (lnum && rnum) {
                double a = atof(lv), b = atof(rv), res = a + b;
                long long iv = (long long)res;
                if ((double)iv == res) snprintf(result, MAX_STR, "%lld", iv);
                else                  snprintf(result, MAX_STR, "%g",   res);
            } else {
                strncpy(result, lv, MAX_STR-1);
                strncat(result, rv, MAX_STR-1-strlen(result));
            }
        }
        if (n->name[0]) set_var(n->name, result);
        return;
    }

    /* ── ask for x y ── */
    case NODE_ASK: {
        if (n->ask_count == 0) return;
        char input[MAX_STR * 2];
        if (!fgets(input, sizeof(input), stdin)) return;
        input[strcspn(input, "\n")] = '\0';
        char *tok = strtok(input, " \t");
        for (int i = 0; i < n->ask_count; i++) {
            set_var(n->ask_vars[i], tok ? tok : "");
            tok = tok ? strtok(NULL, " \t") : NULL;
        }
        return;
    }

    /* ── each item in arr/str: ... end. ── */
    case NODE_EACH: {
        char src_val[MAX_STR] = {0};
        /* evaluate source — could be array var or string */
        Variable *src_arr = NULL;
        if (n->left && n->left->kind == NODE_VAR)
            src_arr = find_var(n->left->name);

        if (src_arr && src_arr->is_array) {
            /* loop over array elements */
            for (int i = 0; i < src_arr->arr_len; i++) {
                set_var(n->params[0], src_arr->arr[i]);
                execute_block(n->body_start, n->body_end);
                if (g_has_return) return;
            }
        } else {
            /* loop over string characters */
            eval(n->left, src_val, MAX_STR);
            int len = (int)strlen(src_val);
            for (int i = 0; i < len; i++) {
                char ch[2] = {src_val[i], '\0'};
                set_var(n->params[0], ch);
                execute_block(n->body_start, n->body_end);
                if (g_has_return) return;
            }
        }
        return;
    }

    /* ── show each item in arr. / online / with "sep" ── */
    case NODE_SHOW_EACH: {
        /* n is NODE_SHOW, n->left is NODE_SHOW_EACH */
        /* this case won't be hit directly — handled in NODE_SHOW */
        return;
    }

    /* ── let arr are N — declare empty array of size N ── */
    case NODE_ARR_DECLARE_N: {
        char sz[MAX_STR]={0};
        eval(n->left, sz, MAX_STR);
        int size = atoi(sz);
        if (size <= 0 || size > MAX_ARRAY_ELEM) {
            char what[256];
            if (size == 0)
                snprintf(what, sizeof(what),
                    "array size cannot be 0 — '%s' needs at least 1 slot", n->name);
            else if (size < 0)
                snprintf(what, sizeof(what),
                    "array size cannot be negative (%d)", size);
            else
                snprintf(what, sizeof(what),
                    "array size %d is too large (maximum is %d)", size, MAX_ARRAY_ELEM);
            nat_error(g_current_line, what,
                "use a positive number between 1 and 256");
            return;
        }
        Variable *v = NULL;
        for (int i = g_var_count-1; i >= 0; i--)
            if (strcmp(g_vars[i].name, n->name) == 0) { v = &g_vars[i]; break; }
        if (!v) {
            if (g_var_count >= MAX_VARS) { err_var_overflow(); return; }
            v = &g_vars[g_var_count++];
        }
        memset(v, 0, sizeof(Variable));
        strncpy(v->name, n->name, 63);
        v->is_array = 1;
        v->arr_len  = size;
        for (int i = 0; i < size; i++)
            strncpy(v->arr[i], "0", MAX_STR-1); /* default 0 */
        return;
    }

    /* ── add x to arr at N — insert at index ── */
    case NODE_ARR_INSERT: {
        Variable *v = find_var(n->name);
        if (!v || !v->is_array) {
            char _what[256];
            snprintf(_what, sizeof(_what), "'%s' is not an array", n->name);
            nat_error(g_current_line, _what, "declare it first as an array");
            return;
        }
        char val[MAX_STR]={0}, idxstr[MAX_STR]={0};
        eval(n->left,  val,    MAX_STR);
        eval(n->right, idxstr, MAX_STR);
        int idx = atoi(idxstr);

        /* 'S' flag = set/replace at index (fruits[1] be "mango") */
        if (n->value[0] == 'S') {
            if (idx < 0 || idx >= v->arr_len) {
                err_index_range(g_current_line, n->name, idx, v->arr_len);
                return;
            }
            strncpy(v->arr[idx], val, MAX_STR-1);
            return;
        }

        /* normal insert */
        if (v->arr_len >= MAX_ARRAY_ELEM) {
            nat_error(g_current_line, "array is full — cannot insert",
                "remove elements first or increase array size");
            return;
        }
        if (idx < 0 || idx > v->arr_len) {
            err_insert_range(g_current_line, n->name, idx, v->arr_len);
            return;
        }
        /* shift elements right from idx onwards */
        for (int i = v->arr_len; i > idx; i--)
            strncpy(v->arr[i], v->arr[i-1], MAX_STR-1);
        strncpy(v->arr[idx], val, MAX_STR-1);
        v->arr_len++;
        return;
    }

    /* ── remove arr at N / remove last / remove first ── */
    case NODE_ARR_REMOVE: {
        Variable *v = find_var(n->name);
        if (!v || !v->is_array) {
            char _what[256];
            snprintf(_what, sizeof(_what), "'%s' is not an array", n->name);
            nat_error(g_current_line, _what, "declare it first as an array");
            return;
        }
        if (v->arr_len == 0) {
            err_empty_array_op(g_current_line, "remove", n->name);
            return;
        }
        if (n->value[0] == 'L') {
            /* remove last */
            v->arr_len--;
            memset(v->arr[v->arr_len], 0, MAX_STR);
        } else if (n->value[0] == 'F') {
            /* remove first — shift left */
            for (int i = 0; i < v->arr_len - 1; i++)
                strncpy(v->arr[i], v->arr[i+1], MAX_STR-1);
            v->arr_len--;
            memset(v->arr[v->arr_len], 0, MAX_STR);
        } else {
            /* remove at index — shift left */
            char idxstr[MAX_STR]={0};
            eval(n->left, idxstr, MAX_STR);
            int idx = atoi(idxstr);
            if (idx < 0 || idx >= v->arr_len) {
                err_remove_range(g_current_line, n->name, idx, v->arr_len);
                return;
            }
            for (int i = idx; i < v->arr_len - 1; i++)
                strncpy(v->arr[i], v->arr[i+1], MAX_STR-1);
            v->arr_len--;
            memset(v->arr[v->arr_len], 0, MAX_STR);
        }
        return;
    }

    /* ── swap arr at 1 and 3 ── */
    case NODE_ARR_SWAP: {
        Variable *v = find_var(n->name);
        if (!v || !v->is_array) {
            char _what[256];
            snprintf(_what, sizeof(_what), "'%s' is not an array", n->name);
            nat_error(g_current_line, _what, "declare it first as an array");
            return;
        }
        char i1s[MAX_STR]={0}, i2s[MAX_STR]={0};
        eval(n->left,  i1s, MAX_STR);
        eval(n->right, i2s, MAX_STR);
        int i1 = atoi(i1s), i2 = atoi(i2s);
        if (i1 < 0 || i1 >= v->arr_len) {
            err_swap_range(g_current_line, n->name, i1, v->arr_len); return;
        }
        if (i2 < 0 || i2 >= v->arr_len) {
            err_swap_range(g_current_line, n->name, i2, v->arr_len); return;
        }
        char tmp[MAX_STR]={0};
        strncpy(tmp,        v->arr[i1], MAX_STR-1);
        strncpy(v->arr[i1], v->arr[i2], MAX_STR-1);
        strncpy(v->arr[i2], tmp,         MAX_STR-1);
        return;
    }

    /* ── reverse arr — in place ── */
    case NODE_REVERSE: {
        /* statement style: n->name holds arr name */
        if (n->name[0] && !n->left) {
            Variable *v = find_var(n->name);
            if (!v) {
                char _what[256];
                snprintf(_what, sizeof(_what),
                    "unknown variable '%s' — cannot reverse", n->name);
                const char *_cands[MAX_VARS+1];
                int _nc = 0;
                for (int _i = 0; _i < g_var_count; _i++) _cands[_nc++] = g_vars[_i].name;
                _cands[_nc] = NULL;
                const char *sugg = nat_suggest(n->name, _cands);
                char _hint[256];
                if (sugg) snprintf(_hint, sizeof(_hint), "Did you mean: '%s' ?", sugg);
                else      snprintf(_hint, sizeof(_hint), "declare it first with:  let %s are <values>.", n->name);
                nat_error(g_current_line, _what, _hint);
                return;
            }
            if (!v->is_array) {
                /* reverse a string variable in place */
                int len = (int)strlen(v->value);
                for (int lo = 0, hi = len-1; lo < hi; lo++, hi--) {
                    char tmp = v->value[lo];
                    v->value[lo] = v->value[hi];
                    v->value[hi] = tmp;
                }
                return;
            }
            int lo = 0, hi = v->arr_len - 1;
            while (lo < hi) {
                char tmp[MAX_STR]={0};
                strncpy(tmp,        v->arr[lo], MAX_STR-1);
                strncpy(v->arr[lo], v->arr[hi], MAX_STR-1);
                strncpy(v->arr[hi], tmp,         MAX_STR-1);
                lo++; hi--;
            }
            return;
        }
        /* expression style — handled by eval */
        return;
    }

    /* ── repeat "ha" N times — statement print ── */
    case NODE_STR_REPEAT: {
        if (n->name[0] == 'P') {
            char result[MAX_STR]={0};
            eval(n, result, MAX_STR);
            printf("%s\n", result);
        }
        return;
    }

    /* ── random from A to B for x y z ── */
    case NODE_RANDOM: {
        char lo[MAX_STR]={0}, hi[MAX_STR]={0};
        if (n->left)  eval(n->left,  lo, MAX_STR);
        if (n->right) eval(n->right, hi, MAX_STR);
        int a = atoi(lo), b = atoi(hi);
        if (a > b) { int tmp = a; a = b; b = tmp; }
        if (n->param_count == 0) return; /* expression style — eval.c handles it */
        for (int i = 0; i < n->param_count; i++) {
            int r = a + rand() % (b - a + 1);
            char val[MAX_STR] = {0};
            snprintf(val, sizeof(val), "%d", r);
            set_var(n->params[i], val);
        }
        return;
    }

    case NODE_NOOP:
    default:
        return;
    }
}
/* ─────────────────────────────────────────────────────────────────
   execute_block
   ───────────────────────────────────────────────────────────────── */
void execute_block(int start, int end) {

    /* ── Pre-scan pass: register all function definitions first ──
       This allows functions to be called before their definition.
       Errors in make syntax are suppressed here — main pass handles them. */
    for (int i = start; i <= end && i < g_line_count; i++) {
        char *p = g_lines[i];
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "make ", 5) != 0) continue;
        tokenize(g_lines[i]);
        g_tok_pos = 0;
        if (g_tok_count == 0) continue;
        /* temporarily suppress errors during pre-scan */
        int saved_line = g_current_line;
        g_current_line = -1;   /* -1 = suppress error output */
        Node *n = parse_statement(i);
        g_current_line = saved_line;
        if (!n) continue;
        if (n->kind == NODE_FUNC_DEF) {
            execute(n);
            node_free(n);
        } else {
            node_free(n);
        }
    }

    /* ── Main execution pass ── */
    for (int i = start; i <= end && i < g_line_count; i++) {

        g_current_line = i + 1;

        if (g_has_return) return;

        tokenize(g_lines[i]);
        if (g_tok_count == 0) continue;

        g_tok_pos = 0;
        const char *ty = g_tokens[0].type;

        if (strcmp(ty, T_END)  == 0) continue;
        if (strcmp(ty, T_ELSE) == 0) continue;
        if (strcmp(ty, T_FIX)  == 0) continue;
        /* make blocks — already registered in pre-scan.
           Parse again here ONLY to surface syntax errors (missing inside etc)
           then skip execution since it's already registered. */
        if (strcmp(ty, T_MAKE) == 0) {
            Node *mn = parse_statement(i);
            if (mn) { i = mn->body_end > i ? mn->body_end : i; node_free(mn); }
            continue;
        }

        Node *n = parse_statement(i);
        if (!n) continue;

        if (n->kind == NODE_FUNC_DEF) {
            /* already registered — skip */
            i = n->body_end; node_free(n); continue;
        }
        if (n->kind == NODE_FOR_LOOP ||
            n->kind == NODE_LOOP     ||
            n->kind == NODE_WHILE    ||
            n->kind == NODE_EACH) {
            execute(n); i = n->body_end; node_free(n); continue;
        }
        if (n->kind == NODE_IF) {
            execute(n); i = n->loop_to; node_free(n); continue;
        }

        execute(n);
        node_free(n);
    }
}
