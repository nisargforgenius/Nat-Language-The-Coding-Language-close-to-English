/*
 * exec.c — NAT Language v3.5
 *
 * v3.5 additions:
 *   NODE_USE — use math.tree. — module import system
 */

#include "nat.h"
#include <stdlib.h>

void execute(Node *n);
void execute_block(int start, int end);

static int is_truthy(const char *s) {
    return (s && strlen(s) > 0 && strcmp(s, "0") != 0);
}

/* ─────────────────────────────────────────────────────────────────
   load_tree — find, load and execute a .tree file
   Search order:
     1. same directory as the .nat file being run (g_nat_exe_dir)
     2. lib/ subdirectory next to nat executable
   ───────────────────────────────────────────────────────────────── */
static void load_tree(const char *filename, int call_line) {
    /* check circular import */
    for (int i = 0; i < g_import_count; i++) {
        if (strcmp(g_imported[i], filename) == 0)
            return; /* already loaded — skip silently */
    }

    /* build candidate paths */
    char path1[MAX_PATH_LEN * 2] = {0}; /* local: same dir as .nat */
    char path2[MAX_PATH_LEN * 2] = {0}; /* system: lib/ next to nat */

    snprintf(path1, sizeof(path1), "%s%s", g_nat_exe_dir, filename);
    snprintf(path2, sizeof(path2), "%slib/%s", g_nat_exe_dir, filename);

    /* try local first, then lib/ */
    const char *found_path = NULL;
    FILE *fp = fopen(path1, "r");
    if (fp) { found_path = path1; }
    else {
        fp = fopen(path2, "r");
        if (fp) found_path = path2;
    }

    if (!fp) {
        char what[256], hint[256];
        snprintf(what, sizeof(what),
            "cannot find tree file '%s'", filename);
        snprintf(hint, sizeof(hint),
            "place '%s' in the same folder as your .nat file, or in the lib/ folder",
            filename);
        nat_error(call_line, what, hint);
        return;
    }

    /* register as imported BEFORE executing (prevents circular) */
    if (g_import_count < MAX_IMPORTS)
        strncpy(g_imported[g_import_count++], filename, MAX_PATH_LEN-1);

    /* save/restore g_lines — heap alloc to avoid stack overflow and support nesting */
    char (*saved_lines)[MAX_LINE_LEN] = malloc(MAX_LINES * MAX_LINE_LEN);
    if (!saved_lines) {
        nat_error(call_line, "out of memory loading tree file", "");
        fclose(fp); return;
    }
    int   saved_count = g_line_count;
    memcpy(saved_lines, g_lines, sizeof(g_lines));

    g_line_count = 0;
    char buf[MAX_LINE_LEN];
    while (fgets(buf, sizeof(buf), fp) && g_line_count < MAX_LINES) {
        /* strip newline */
        int len = (int)strlen(buf);
        if (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
            buf[--len] = '\0';
        if (len > 0 && buf[len-1] == '\r')
            buf[--len] = '\0';
        strncpy(g_lines[g_line_count++], buf, MAX_LINE_LEN-1);
    }
    fclose(fp);

    /* execute the tree file — registers all its functions */
    int saved_has_return = g_has_return;
    char saved_return[MAX_STR];
    strncpy(saved_return, g_return_val, MAX_STR-1);

    g_has_return = 0;
    g_return_val[0] = '\0';

    /* register fix constants from tree file before executing */
    extern void pre_pass_fix(void);
    pre_pass_fix();

    /* remember how many funcs existed before loading */
    int funcs_before = g_func_count;

    execute_block(0, g_line_count - 1);

    /* snapshot body lines for every newly registered function */
    for (int fi = funcs_before; fi < g_func_count; fi++) {
        FuncDef *fn = &g_funcs[fi];
        /* skip functions already snapshotted by a nested tree load */
        if (fn->is_tree_func && fn->body_lines) continue;
        int bstart = fn->body_start;
        int bend   = fn->body_end;
        int bcount = (bend >= bstart && bstart >= 0 && bend < g_line_count)
                     ? (bend - bstart + 1) : 0;
        fn->is_tree_func    = 1;
        fn->body_line_count = bcount;
        fn->body_lines      = NULL;
        if (bcount > 0) {
            fn->body_lines = (char **)malloc(bcount * sizeof(char *));
            if (fn->body_lines) {
                for (int li = 0; li < bcount; li++) {
                    fn->body_lines[li] = (char *)malloc(MAX_LINE_LEN);
                    if (fn->body_lines[li])
                        strncpy(fn->body_lines[li],
                                g_lines[bstart + li], MAX_LINE_LEN-1);
                }
            }
        }
    }

    /* restore main program state */
    g_has_return = saved_has_return;
    strncpy(g_return_val, saved_return, MAX_STR-1);
    g_line_count = saved_count;
    memcpy(g_lines, saved_lines, sizeof(g_lines));
    free(saved_lines);

    (void)found_path;
}

/* ─────────────────────────────────────────────────────────────────
   execute_func_body — run a function's body, handling tree funcs
   ───────────────────────────────────────────────────────────────── */
void execute_func_body(FuncDef *fn) {
    if (fn->is_tree_func && fn->body_lines && fn->body_line_count > 0) {
        /* tree function: temporarily swap in its body lines */
        char (*saved)[MAX_LINE_LEN] = malloc(MAX_LINES * MAX_LINE_LEN);
        if (!saved) {
            nat_error(g_current_line, "out of memory executing tree function", "");
            return;
        }
        int saved_count = g_line_count;
        memcpy(saved, g_lines, sizeof(g_lines));

        g_line_count = fn->body_line_count;
        for (int i = 0; i < fn->body_line_count; i++)
            strncpy(g_lines[i], fn->body_lines[i], MAX_LINE_LEN-1);

        execute_block(0, g_line_count - 1);

        g_line_count = saved_count;
        memcpy(g_lines, saved, sizeof(g_lines));
        free(saved);
    } else {
        /* normal function — body lives in main g_lines */
        execute_block(fn->body_start, fn->body_end);
    }
}

void execute(Node *n) {
    if (!n) return;

    switch (n->kind) {

    /* ── use math.tree. — load a tree file ── */
    case NODE_USE: {
        load_tree(n->value, g_current_line);
        return;
    }

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
        execute_func_body(fn);
        /* auto-print return value when called as a bare statement */
        char stmt_rv[MAX_STR];
        strncpy(stmt_rv, g_return_val, MAX_STR-1);
        int  stmt_had_return = g_has_return;
        g_var_count  = saved_vc;
        g_has_return = saved_ret;
        strncpy(g_return_val, saved_rv, MAX_STR-1);
        if (stmt_had_return && stmt_rv[0] != '\0')
            printf("%s\n", stmt_rv);
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

    /* ── create graph "title": ... end. ── */
    case NODE_GRAPH: {
        GraphDef g;
        memset(&g, 0, sizeof(GraphDef));
        strncpy(g.title,       n->value,  MAX_STR-1);
        strncpy(g.x_label,     "X",       63);
        strncpy(g.y_label,     "Y",       63);
        strncpy(g.graph_type,  "linear",  31);
        strncpy(g.color,       "#3b82f6", 31);
        g.x_min = 0; g.x_max = 10; g.x_step = 0;
        g.y_min = 0; g.y_max = 10; g.y_step = 0;

        /* parse config lines */
        for (int i = n->body_start; i <= n->body_end && i < g_line_count; i++) {
            tokenize(g_lines[i]);
            if (g_tok_count == 0) continue;
            char *ty0 = g_tokens[0].type;
            char *ty1 = (g_tok_count > 1) ? g_tokens[1].type  : "";
            char *va1 = (g_tok_count > 1) ? g_tokens[1].value : "";

            /* x axis be "label" */
            if (strcmp(ty0, "AXIS") == 0 || strcmp(va1, "axis") == 0) {
                /* handled below */
            }
            /* x axis be "label" — tokens: IDENT(x) AXIS BE STRING */
            if (g_tok_count >= 4 && strcmp(ty1, "AXIS") == 0) {
                char axis = g_tokens[0].value[0];
                char *label = g_tokens[3].value;
                if (axis == 'x' || axis == 'X')
                    strncpy(g.x_label, label, 63);
                else
                    strncpy(g.y_label, label, 63);
            }
            /* range x is 0 to 10 — tokens: RANGE IDENT(x) IS NUM TO NUM [STEP/GAP NUM] */
            if (strcmp(ty0, "RANGE") == 0 && g_tok_count >= 5) {
                char axis = g_tokens[1].value[0];
                double lo   = atof(g_tokens[3].value);
                double hi   = atof(g_tokens[5].value);
                double step = 0; /* 0 = auto */
                /* check for step or gap keyword */
                if (g_tok_count >= 7) {
                    char *sk = g_tokens[6].type;
                    if (strcmp(sk, T_STEP)==0 || strcmp(sk,"GAP")==0)
                        step = atof(g_tokens[7].value);
                }
                if (axis == 'x' || axis == 'X') {
                    g.x_min = lo; g.x_max = hi; g.x_step = step;
                } else {
                    g.y_min = lo; g.y_max = hi; g.y_step = step;
                }
            }
            /* type be linear/bar/etc */
            if (strcmp(ty0, "TYPE") == 0 && g_tok_count >= 3) {
                strncpy(g.graph_type, g_tokens[2].value, 31);
            }
            /* color be "blue" */
            if (strcmp(ty0, "COLOR") == 0 && g_tok_count >= 3) {
                char *cv = g_tokens[2].value;
                /* map common names to hex */
                if      (strcmp(cv,"blue")   == 0) strncpy(g.color, "#3b82f6", 31);
                else if (strcmp(cv,"red")    == 0) strncpy(g.color, "#ef4444", 31);
                else if (strcmp(cv,"green")  == 0) strncpy(g.color, "#22c55e", 31);
                else if (strcmp(cv,"purple") == 0) strncpy(g.color, "#a855f7", 31);
                else if (strcmp(cv,"orange") == 0) strncpy(g.color, "#f97316", 31);
                else if (strcmp(cv,"pink")   == 0) strncpy(g.color, "#ec4899", 31);
                else if (strcmp(cv,"yellow") == 0) strncpy(g.color, "#eab308", 31);
                else if (strcmp(cv,"cyan")   == 0) strncpy(g.color, "#06b6d4", 31);
                else strncpy(g.color, cv, 31); /* use as-is (hex) */
            }
            /* points are (x,y) (x,y) ... */
            if (strcmp(ty0, "POINTS") == 0) {
                /* scan raw line for (x,y) pairs */
                char *raw = g_lines[i];
                char *p   = raw;
                while (*p && g.point_count < MAX_POINTS) {
                    while (*p && *p != '(') p++;
                    if (!*p) break;
                    p++; /* skip ( */
                    char xbuf[32]={0}, ybuf[32]={0};
                    int j = 0;
                    while (*p && *p != ',' && *p != ')' && j < 31)
                        xbuf[j++] = *p++;
                    if (*p == ',') p++;
                    j = 0;
                    while (*p && *p != ')' && j < 31)
                        ybuf[j++] = *p++;
                    if (*p == ')') p++;
                    g.px[g.point_count] = atof(xbuf);
                    g.py[g.point_count] = atof(ybuf);
                    g.point_count++;
                }
            }
        }

        /* generate filename from title */
        char fname[256] = {0};
        int fi = 0;
        for (int ci = 0; g.title[ci] && fi < 240; ci++) {
            char c = g.title[ci];
            if (c == ' ') fname[fi++] = '_';
            else if ((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9'))
                fname[fi++] = c;
        }
        if (fi == 0) { strcpy(fname, "graph"); fi = 5; }
        strcpy(fname + fi, ".html");

        /* build points JS array string */
        char ptsbuf[4096] = {0};
        {
            int pos = 0;
            pos += snprintf(ptsbuf+pos, sizeof(ptsbuf)-pos, "[");
            for (int pi = 0; pi < g.point_count; pi++) {
                pos += snprintf(ptsbuf+pos, sizeof(ptsbuf)-pos,
                    "[%g,%g]%s", g.px[pi], g.py[pi],
                    pi < g.point_count-1 ? "," : "");
            }
            snprintf(ptsbuf+pos, sizeof(ptsbuf)-pos, "]");
        }

        /* generate HTML */
        FILE *hf = fopen(fname, "w");
        if (!hf) {
            nat_error(g_current_line,
                "cannot create graph file — check write permissions",
                "make sure NAT has permission to write in this folder");
            return;
        }

        /* determine if smooth curve or straight lines */
        int smooth = (strcmp(g.graph_type,"nonlinear")==0 ||
                      strcmp(g.graph_type,"exponential")==0);
        int is_bar = (strcmp(g.graph_type,"bar")==0);
        int is_scatter = (strcmp(g.graph_type,"scatter")==0);

        fprintf(hf,
"<!DOCTYPE html>\n"
"<html lang=\"en\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>%s</title>\n"
"<style>\n"
"* { margin:0; padding:0; box-sizing:border-box; }\n"
"body { background:#0f172a; display:flex; flex-direction:column;\n"
"       align-items:center; justify-content:center;\n"
"       min-height:100vh; font-family:'Segoe UI',sans-serif; color:#e2e8f0; }\n"
"h1 { font-size:1.6rem; margin-bottom:1rem; color:#f8fafc;\n"
"     letter-spacing:0.05em; text-transform:uppercase; }\n"
".badge { background:#1e293b; border:1px solid #334155;\n"
"         padding:4px 12px; border-radius:999px; font-size:0.75rem;\n"
"         color:#94a3b8; margin-bottom:1.5rem; }\n"
"canvas { background:#1e293b; border-radius:12px;\n"
"         border:1px solid #334155; display:block; }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<h1>%s</h1>\n"
"<div class=\"badge\">%s graph &nbsp;|&nbsp; NAT v3.5</div>\n"
"<canvas id=\"c\"></canvas>\n"
"<script>\n"
"const canvas = document.getElementById('c');\n"
"const ctx    = canvas.getContext('2d');\n"
"const PAD = 70;\n"
"function resize() {\n"
"  canvas.width  = Math.min(window.innerWidth  - 40, 900);\n"
"  canvas.height = Math.min(window.innerHeight - 180, 600);\n"
"  draw();\n"
"}\n"
"const pts    = %s;\n"
"const xMin   = %g, xMax = %g, xStepUser = %g;\n"
"const yMin   = %g, yMax = %g, yStepUser = %g;\n"
"const color  = '%s';\n"
"const xLabel = '%s';\n"
"const yLabel = '%s';\n"
"const isBar  = %s;\n"
"const isScatter = %s;\n"
"const smooth = %s;\n"
"\n"
"function toCanvasX(x) {\n"
"  return PAD + (x - xMin) / (xMax - xMin) * (canvas.width  - PAD*2);\n"
"}\n"
"function toCanvasY(y) {\n"
"  return canvas.height - PAD - (y - yMin) / (yMax - yMin) * (canvas.height - PAD*2);\n"
"}\n"
"\n"
"function draw() {\n"
"  const W = canvas.width, H = canvas.height;\n"
"  ctx.clearRect(0,0,W,H);\n"
"\n"
"  /* step calculation — default 1 if not specified */\n"
"  const xStep = xStepUser > 0 ? xStepUser : 1;\n"
"  const yStep = yStepUser > 0 ? yStepUser : 1;\n"
"\n"
"  /* grid */\n"
"  ctx.strokeStyle = '#334155'; ctx.lineWidth = 1;\n"
"  for (let v = xMin; v <= xMax + xStep*0.01; v += xStep) {\n"
"    const x = toCanvasX(v);\n"
"    ctx.beginPath(); ctx.moveTo(x,PAD); ctx.lineTo(x,H-PAD); ctx.stroke();\n"
"  }\n"
"  for (let v = yMin; v <= yMax + yStep*0.01; v += yStep) {\n"
"    const y = toCanvasY(v);\n"
"    ctx.beginPath(); ctx.moveTo(PAD,y); ctx.lineTo(W-PAD,y); ctx.stroke();\n"
"  }\n"
"\n"
"  /* axes */\n"
"  ctx.strokeStyle = '#64748b'; ctx.lineWidth = 2;\n"
"  ctx.beginPath(); ctx.moveTo(PAD,PAD); ctx.lineTo(PAD,H-PAD); ctx.stroke();\n"
"  ctx.beginPath(); ctx.moveTo(PAD,H-PAD); ctx.lineTo(W-PAD,H-PAD); ctx.stroke();\n"
"\n"
"  /* axis labels */\n"
"  ctx.fillStyle = '#94a3b8'; ctx.font = '13px Segoe UI';\n"
"  ctx.textAlign = 'center';\n"
"  for (let v = xMin; v <= xMax + xStep*0.01; v += xStep) {\n"
"    const x = toCanvasX(v);\n"
"    const disp = Number.isInteger(v) ? v : +v.toFixed(2);\n"
"    ctx.fillText(disp, x, H-PAD+20);\n"
"  }\n"
"  ctx.textAlign = 'right';\n"
"  for (let v = yMin; v <= yMax + yStep*0.01; v += yStep) {\n"
"    const y = toCanvasY(v);\n"
"    const disp = Number.isInteger(v) ? v : +v.toFixed(2);\n"
"    ctx.fillText(disp, PAD-8, y+4);\n"
"  }\n"
"\n"
"  /* axis names */\n"
"  ctx.textAlign = 'center'; ctx.font = 'bold 14px Segoe UI';\n"
"  ctx.fillStyle = '#cbd5e1';\n"
"  ctx.fillText(xLabel, W/2, H-8);\n"
"  ctx.save(); ctx.translate(14, H/2); ctx.rotate(-Math.PI/2);\n"
"  ctx.fillText(yLabel, 0, 0); ctx.restore();\n"
"\n"
"  if (pts.length === 0) return;\n"
"\n"
"  if (isBar) {\n"
"    const bw = (W-PAD*2) / (pts.length * 1.5);\n"
"    pts.forEach(([x,y], i) => {\n"
"      const cx = toCanvasX(x) - bw/2;\n"
"      const cy = toCanvasY(y);\n"
"      const bh = H - PAD - cy;\n"
"      const grad = ctx.createLinearGradient(0,cy,0,H-PAD);\n"
"      grad.addColorStop(0, color);\n"
"      grad.addColorStop(1, color + '44');\n"
"      ctx.fillStyle = grad;\n"
"      ctx.beginPath();\n"
"      ctx.roundRect(cx, cy, bw, bh, [4,4,0,0]);\n"
"      ctx.fill();\n"
"    });\n"
"  } else {\n"
"    /* fill under line */\n"
"    ctx.beginPath();\n"
"    ctx.moveTo(toCanvasX(pts[0][0]), H-PAD);\n"
"    ctx.lineTo(toCanvasX(pts[0][0]), toCanvasY(pts[0][1]));\n"
"    if (smooth && pts.length > 2) {\n"
"      for (let i=1; i<pts.length; i++) {\n"
"        const xc = (toCanvasX(pts[i-1][0]) + toCanvasX(pts[i][0])) / 2;\n"
"        const yc = (toCanvasY(pts[i-1][1]) + toCanvasY(pts[i][1])) / 2;\n"
"        ctx.quadraticCurveTo(toCanvasX(pts[i-1][0]), toCanvasY(pts[i-1][1]), xc, yc);\n"
"      }\n"
"      ctx.lineTo(toCanvasX(pts[pts.length-1][0]), toCanvasY(pts[pts.length-1][1]));\n"
"    } else {\n"
"      pts.slice(1).forEach(([x,y]) => ctx.lineTo(toCanvasX(x), toCanvasY(y)));\n"
"    }\n"
"    ctx.lineTo(toCanvasX(pts[pts.length-1][0]), H-PAD);\n"
"    ctx.closePath();\n"
"    const grad = ctx.createLinearGradient(0, PAD, 0, H-PAD);\n"
"    grad.addColorStop(0, color + '44');\n"
"    grad.addColorStop(1, color + '08');\n"
"    if (!isScatter) { ctx.fillStyle = grad; ctx.fill(); }\n"
"\n"
"    /* line */\n"
"    if (!isScatter) {\n"
"      ctx.beginPath();\n"
"      ctx.moveTo(toCanvasX(pts[0][0]), toCanvasY(pts[0][1]));\n"
"      if (smooth && pts.length > 2) {\n"
"        for (let i=1; i<pts.length; i++) {\n"
"          const xc = (toCanvasX(pts[i-1][0]) + toCanvasX(pts[i][0])) / 2;\n"
"          const yc = (toCanvasY(pts[i-1][1]) + toCanvasY(pts[i][1])) / 2;\n"
"          ctx.quadraticCurveTo(toCanvasX(pts[i-1][0]), toCanvasY(pts[i-1][1]), xc, yc);\n"
"        }\n"
"        ctx.lineTo(toCanvasX(pts[pts.length-1][0]), toCanvasY(pts[pts.length-1][1]));\n"
"      } else {\n"
"        pts.slice(1).forEach(([x,y]) => ctx.lineTo(toCanvasX(x), toCanvasY(y)));\n"
"      }\n"
"      ctx.strokeStyle = color; ctx.lineWidth = 3;\n"
"      ctx.lineJoin = 'round'; ctx.stroke();\n"
"    }\n"
"  }\n"
"\n"
"  /* dots */\n"
"  pts.forEach(([x,y]) => {\n"
"    ctx.beginPath();\n"
"    ctx.arc(toCanvasX(x), toCanvasY(y), isScatter ? 6 : 5, 0, Math.PI*2);\n"
"    ctx.fillStyle = color; ctx.fill();\n"
"    ctx.strokeStyle = '#0f172a'; ctx.lineWidth = 2; ctx.stroke();\n"
"  });\n"
"\n"
"  /* value labels on dots */\n"
"  ctx.fillStyle = '#f8fafc'; ctx.font = '11px Segoe UI';\n"
"  ctx.textAlign = 'center';\n"
"  pts.forEach(([x,y]) => {\n"
"    ctx.fillText('('+x+','+y+')', toCanvasX(x), toCanvasY(y)-12);\n"
"  });\n"
"}\n"
"\n"
"window.addEventListener('resize', resize);\n"
"resize();\n"
"</script>\n"
"</body></html>\n",
            /* substitutions */
            g.title, g.title, g.graph_type,
            ptsbuf,
            g.x_min, g.x_max, g.x_step,
            g.y_min, g.y_max, g.y_step,
            g.color, g.x_label, g.y_label,
            is_bar     ? "true" : "false",
            is_scatter ? "true" : "false",
            smooth     ? "true" : "false"
        );

        fclose(hf);
        printf("Graph created: %s\n", fname);

        /* auto-open in browser */
#ifdef _WIN32
        { char cmd[300]; snprintf(cmd,sizeof(cmd),"start %s",fname); system(cmd); }
#elif __APPLE__
        { char cmd[300]; snprintf(cmd,sizeof(cmd),"open %s",fname); system(cmd); }
#else
        { char cmd[300]; snprintf(cmd,sizeof(cmd),"xdg-open %s 2>/dev/null &",fname); system(cmd); }
#endif
        return;
    }

    case NODE_NEWLINE:
        printf("\n");
        return;

    /* ═══════════════════════════════════════════════════════════
     *  v3.6 — File I/O execution
     * ═══════════════════════════════════════════════════════════ */

    /* ── write "text" to "file.txt". ── */
    case NODE_FILE_WRITE: {
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        FILE *fp = fopen(n->name, "wb");
        if (!fp) {
            err_file_open(g_current_line, n->name);
            return;
        }
        fprintf(fp, "%s\n", val);
        fclose(fp);
        return;
    }

    /* ── write "text" to "file.txt" at line N. ── */
    case NODE_FILE_WRITE_LINE: {
        char val[MAX_STR] = {0};
        char lnum[64]     = {0};
        if (n->left)  eval(n->left,  val,  MAX_STR);
        if (n->right) eval(n->right, lnum, 64);
        int target = atoi(lnum);
        if (target < 1) { err_line_range(g_current_line, n->name, target); return; }

        /* read all lines */
        FILE *fp = fopen(n->name, "rb");
        char *lines[4096]; int lc = 0;
        char buf[MAX_STR];
        if (fp) {
            while (fgets(buf, MAX_STR, fp) && lc < 4096) {
                int bl = strlen(buf);
                if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
                lines[lc++] = strdup(buf);
            }
            fclose(fp);
        }
        /* expand if needed */
        while (lc < target) lines[lc++] = strdup("");
        free(lines[target-1]);
        lines[target-1] = strdup(val);
        /* write back */
        fp = fopen(n->name, "wb");
        if (!fp) { err_file_open(g_current_line, n->name); goto file_write_line_cleanup; }
        for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
        fclose(fp);
        file_write_line_cleanup:
        for (int i = 0; i < lc; i++) free(lines[i]);
        return;
    }

    /* ── append "text" to "file.txt". ── */
    case NODE_FILE_APPEND: {
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        FILE *fp = fopen(n->name, "ab");
        if (!fp) { err_file_open(g_current_line, n->name); return; }
        fprintf(fp, "%s\n", val);
        fclose(fp);
        return;
    }

    /* ── insert "text" to "file.txt" at line N. ── */
    case NODE_FILE_INSERT: {
        char val[MAX_STR] = {0};
        char lnum[64]     = {0};
        if (n->left)  eval(n->left,  val,  MAX_STR);
        if (n->right) eval(n->right, lnum, 64);
        int target = atoi(lnum);
        if (target < 1) { err_line_range(g_current_line, n->name, target); return; }

        FILE *fp = fopen(n->name, "rb");
        char *lines[4096]; int lc = 0;
        char buf[MAX_STR];
        if (fp) {
            while (fgets(buf, MAX_STR, fp) && lc < 4095) {
                int bl = strlen(buf);
                if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
                lines[lc++] = strdup(buf);
            }
            fclose(fp);
        }
        /* shift lines from target down */
        if (lc < 4095) {
            for (int i = lc; i >= target; i--) lines[i] = lines[i-1];
            lines[target-1] = strdup(val);
            lc++;
        }
        fp = fopen(n->name, "wb");
        if (!fp) { err_file_open(g_current_line, n->name); goto file_insert_cleanup; }
        for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
        fclose(fp);
        file_insert_cleanup:
        for (int i = 0; i < lc; i++) free(lines[i]);
        return;
    }

    /* ── remove line N from "file.txt". ── */
    case NODE_FILE_REMOVE_LINE: {
        char lnum[64] = {0};
        if (n->right) eval(n->right, lnum, 64);
        int target = atoi(lnum);
        if (target < 1) { err_line_range(g_current_line, n->name, target); return; }

        FILE *fp = fopen(n->name, "rb");
        if (!fp) { err_file_not_found(g_current_line, n->name); return; }
        char *lines[4096]; int lc = 0;
        char buf[MAX_STR];
        while (fgets(buf, MAX_STR, fp) && lc < 4096) {
            int bl = strlen(buf);
            if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
            lines[lc++] = strdup(buf);
        }
        fclose(fp);
        if (target > lc) { err_line_range(g_current_line, n->name, target); goto file_remove_cleanup; }
        free(lines[target-1]);
        for (int i = target-1; i < lc-1; i++) lines[i] = lines[i+1];
        lc--;
        fp = fopen(n->name, "wb");
        if (!fp) { err_file_open(g_current_line, n->name); goto file_remove_cleanup; }
        for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
        fclose(fp);
        file_remove_cleanup:
        for (int i = 0; i < lc; i++) free(lines[i]);
        return;
    }

    /* ── read "file.txt". — prints with line numbers ── */
    case NODE_FILE_READ: {
        FILE *fp = fopen(n->name, "rb");
        if (!fp) { err_file_not_found(g_current_line, n->name); return; }
        char buf[MAX_STR]; int ln = 1;
        while (fgets(buf, MAX_STR, fp)) {
            int bl = strlen(buf);
            if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
            printf("line %d | %s\n", ln++, buf);
        }
        fclose(fp);
        return;
    }

    /* ── delete file "file.txt". ── */
    case NODE_FILE_DELETE: {
        if (remove(n->name) != 0)
            err_file_not_found(g_current_line, n->name);
        return;
    }

    /* ── write x inside "one.nat". — runtime injection ── */
    case NODE_WRITE_INSIDE: {
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        /* store in injection table: varname -> value */
        /* n->params[0] holds the var name (set during parse via n->left->name) */
        char varname[64] = {0};
        if (n->left && n->left->kind == NODE_VAR)
            strncpy(varname, n->left->name, 63);
        else
            strncpy(varname, "__injected__", 63);
        /* find or create injection slot */
        for (int i = 0; i < g_inject_count; i++) {
            if (strcmp(g_injects[i].name, varname) == 0) {
                strncpy(g_injects[i].value, val, MAX_STR-1);
                return;
            }
        }
        if (g_inject_count < MAX_INJECTS) {
            strncpy(g_injects[g_inject_count].name,  varname, 63);
            strncpy(g_injects[g_inject_count].value, val,     MAX_STR-1);
            g_inject_count++;
        }
        return;
    }

    /* ── each line in file "x.txt": ── */
    case NODE_FILE_EACH: {
        FILE *fp = fopen(n->name, "rb");
        if (!fp) { err_file_not_found(g_current_line, n->name); return; }
        char buf[MAX_STR];
        while (fgets(buf, MAX_STR, fp)) {
            int bl = strlen(buf);
            if (bl > 0 && buf[bl-1] == '\n') buf[bl-1] = '\0';
            /* skip empty trailing line from final newline in file */
            if (buf[0] == '\0' && feof(fp)) break;
            /* set loop variable */
            set_var(n->params[0], buf);
            execute_block(n->body_start, n->body_end);
            if (g_has_return) break;
        }
        fclose(fp);
        return;
    }
    } /* end switch */
}  /* end execute() */

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
            n->kind == NODE_EACH     ||
            n->kind == NODE_FILE_EACH) {
            execute(n); i = n->body_end; node_free(n); continue;
        }
        if (n->kind == NODE_IF) {
            execute(n); i = n->loop_to; node_free(n); continue;
        }
        if (n->kind == NODE_GRAPH) {
            execute(n); i = n->loop_to; node_free(n); continue;
        }

        execute(n);
        node_free(n);
    }
}
