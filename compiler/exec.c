/*
 * exec.c — NAT Language v3.0 Executor
 *
 * Walks statement-level AST nodes and carries out their effects.
 *
 * v3.0 additions:
 *   NODE_ADD_ASSIGN  — add x with y to z.
 *   constants        — registered by main.c before execute_block
 *   FOR loop         — INCLUSIVE upper bound (i <= to_val)
 */

#include "nat.h"

void execute(Node *n);
void execute_block(int start, int end);

/* ─────────────────────────────────────────────────────────────────
   execute
   ───────────────────────────────────────────────────────────────── */
void execute(Node *n) {
    if (!n) return;

    switch (n->kind) {

    /* ── let name be value ── */
    case NODE_LET: {
        char val[MAX_STR] = {0};
        if (n->left) eval(n->left, val, MAX_STR);
        else         strncpy(val, n->value, MAX_STR-1);
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

    /* ── show(expr) ── */
    case NODE_SHOW: {
        char result[MAX_STR] = {0};
        eval(n->left, result, MAX_STR);
        printf("%s\n", result);
        return;
    }

    /* ── give expr ── */
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
            fprintf(stderr, "NAT: function table overflow\n");
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

    /* ── bare function call statement ── */
    case NODE_FUNC_CALL: {
        FuncDef *fn = NULL;
        for (int i = 0; i < g_func_count; i++)
            if (strcmp(g_funcs[i].name, n->name) == 0) { fn = &g_funcs[i]; break; }
        if (!fn) {
            fprintf(stderr, "NAT error: unknown function '%s'\n", n->name);
            return;
        }
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
        for (int i = 0; i < count; i++) {
            execute_block(n->body_start, n->body_end);
            if (g_has_return) return;
        }
        return;
    }

    /* ── repeat i from X to Y step Z  (INCLUSIVE: i <= to_val) ── */
    case NODE_FOR_LOOP: {
        char tmp[MAX_STR] = {0};
        int from_val = 0, to_val = 0, step_val = 1;

        if (n->left)        { eval(n->left,        tmp, MAX_STR); from_val = atoi(tmp); }
        if (n->right)       { eval(n->right,       tmp, MAX_STR); to_val   = atoi(tmp); }
        if (n->else_branch) { eval(n->else_branch, tmp, MAX_STR); step_val = atoi(tmp); }
        if (step_val == 0) step_val = 1;

        /* INCLUSIVE: loop while i <= to_val */
        for (int i = from_val; i <= to_val; i += step_val) {
            snprintf(tmp, sizeof(tmp), "%d", i);

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

    /* ── while COND: ... end ── */
    case NODE_WHILE: {
        int safety = 1000000;
        while (safety-- > 0) {
            char cond[MAX_STR] = {0};
            eval(n->left, cond, MAX_STR);
            int truth = (strlen(cond) > 0 && strcmp(cond, "0") != 0);
            if (!truth) break;
            execute_block(n->body_start, n->body_end);
            if (g_has_return) return;
        }
        if (safety <= 0)
            fprintf(stderr, "NAT warning: while loop exceeded 1000000 iterations\n");
        return;
    }

    /* ── if COND: ... [else: ...] end ── */
    case NODE_IF: {
        char cond[MAX_STR] = {0};
        eval(n->left, cond, MAX_STR);
        int truth = (strlen(cond) > 0 && strcmp(cond, "0") != 0);
        if (truth) {
            execute_block(n->body_start, n->body_end);
        } else if (n->else_branch) {
            execute_block(n->else_branch->body_start,
                          n->else_branch->body_end);
        }
        return;
    }

    /*
     * ── add x with y to z.
     *    Evaluates left and right, adds them, stores in n->name.
     *    Works for both numbers and strings (string: concatenates).
     */
    case NODE_ADD_ASSIGN: {
        char lv[MAX_STR]={0}, rv[MAX_STR]={0};
        eval(n->left,  lv, MAX_STR);
        eval(n->right, rv, MAX_STR);

        char result[MAX_STR] = {0};
        /* numeric add */
        if (strlen(lv) > 0 && strlen(rv) > 0) {
            /* check if both numeric */
            int lnum=1, rnum=1;
            for (int i=0; lv[i]; i++) if(!isdigit((unsigned char)lv[i]) && lv[i]!='.' && lv[i]!='-') { lnum=0; break; }
            for (int i=0; rv[i]; i++) if(!isdigit((unsigned char)rv[i]) && rv[i]!='.' && rv[i]!='-') { rnum=0; break; }

            if (lnum && rnum) {
                double a = atof(lv), b = atof(rv), res = a + b;
                long long iv = (long long)res;
                if ((double)iv == res) snprintf(result, MAX_STR, "%lld", iv);
                else                  snprintf(result, MAX_STR, "%g",   res);
            } else {
                /* string concatenation */
                strncpy(result, lv, MAX_STR - 1);
                result[MAX_STR - 1] = '\0';
                strncat(result, rv, MAX_STR - 1 - strlen(result));
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

    case NODE_NOOP:
    default:
        return;
    }
}

/* ─────────────────────────────────────────────────────────────────
   execute_block — run lines [start..end] inclusive
   ───────────────────────────────────────────────────────────────── */
void execute_block(int start, int end) {
    for (int i = start; i <= end && i < g_line_count; i++) {

        if (g_has_return) return;

        tokenize(g_lines[i]);
        if (g_tok_count == 0) continue;

        g_tok_pos = 0;
        const char *ty = g_tokens[0].type;

        /* structural markers — skip */
        if (strcmp(ty, T_END)  == 0) continue;
        if (strcmp(ty, T_ELSE) == 0) continue;

        /* NOTE: 'fix' lines are handled in main.c pre-pass, skip here */
        if (strcmp(ty, T_FIX) == 0) continue;

        Node *n = parse_statement(i);
        if (!n) continue;

        /*
         * Block statements: execute the node, then advance i to the
         * end. line so the for-loop's i++ lands on end_line+1.
         */
        if (n->kind == NODE_FUNC_DEF) {
            execute(n);
            i = n->body_end;
            node_free(n);
            continue;
        }
        if (n->kind == NODE_FOR_LOOP ||
            n->kind == NODE_LOOP     ||
            n->kind == NODE_WHILE) {
            execute(n);
            i = n->body_end;
            node_free(n);
            continue;
        }
        if (n->kind == NODE_IF) {
            execute(n);
            i = n->loop_to;   /* loop_to = index of end. line */
            node_free(n);
            continue;
        }

        execute(n);
        node_free(n);
    }
}
