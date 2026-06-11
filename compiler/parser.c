/*
 * parser.c — NAT Language v3.2 Recursive-Descent Parser
 *
 * v3.2 additions:
 *   Logic   : and/or chaining in conditions
 *             English "greater than" / "less than" with and/or
 *             chained compare:  1 < x < 10
 *             "x in middle of A and B"
 *   Strings : length of x, upper of x, lower of x, contains
 *   Math    : 2^8 (POW), abs of x, round of x
 *   Vars    : let x, b, c.   (multi-declare, no value)
 *             x be 10.       (bare assignment)
 *
 * Operator precedence (low → high):
 *   logical or → logical and → concat → compare → add/sub
 *   → mul/div/mod → power → unary/builtin → factor
 */

#include "nat.h"

/* ── forward declarations ─────────────────────────────────────── */
static Node *parse_logical(void);
static Node *parse_logical_and(void);
static Node *parse_concat(void);
static Node *parse_compare(void);
static Node *parse_additive(void);
static Node *parse_multiplicative(void);
static Node *parse_power(void);
static Node *parse_unary(void);
static Node *parse_factor(void);

/* ─────────────────────────────────────────────────────────────────
   NODE ALLOCATOR
   ───────────────────────────────────────────────────────────────── */
Node *node_new(NodeType kind) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    if (!n) { fprintf(stderr, "NAT: out of memory\n"); exit(1); }
    n->kind       = kind;
    n->body_start = -1;
    n->body_end   = -1;
    return n;
}

void node_free(Node *n) {
    if (!n) return;
    node_free(n->left);
    node_free(n->right);
    node_free(n->else_branch);
    for (int i = 0; i < n->arg_count; i++) node_free(n->args[i]);
    free(n);
}

/* ─────────────────────────────────────────────────────────────────
   TOKEN HELPERS
   ───────────────────────────────────────────────────────────────── */
static int tok_is(const char *type) {
    return g_tok_pos < g_tok_count &&
           strcmp(g_tokens[g_tok_pos].type, type) == 0;
}
static int tok_peek(const char *type, int offset) {
    int idx = g_tok_pos + offset;
    return idx < g_tok_count &&
           strcmp(g_tokens[idx].type, type) == 0;
}
static Token *tok_cur(void) {
    return (g_tok_pos < g_tok_count) ? &g_tokens[g_tok_pos] : NULL;
}
static Token *tok_consume(void) {
    return (g_tok_pos < g_tok_count) ? &g_tokens[g_tok_pos++] : NULL;
}
static void tok_expect(const char *type) {
    if (!tok_is(type)) {
        fprintf(stderr,
            "NAT parse error: expected '%s', got '%s' ('%s')\n",
            type,
            g_tok_pos < g_tok_count ? g_tokens[g_tok_pos].type  : "EOF",
            g_tok_pos < g_tok_count ? g_tokens[g_tok_pos].value : "");
        return;
    }
    g_tok_pos++;
}

/* ─────────────────────────────────────────────────────────────────
   EXPRESSION PARSER
   ───────────────────────────────────────────────────────────────── */

Node *parse_expression(void) { return parse_logical(); }

/* ── logical or: left or right ── */
static Node *parse_logical(void) {
    Node *left = parse_logical_and();
    while (tok_is(T_OR)) {
        tok_consume();
        Node *n  = node_new(NODE_OR);
        n->left  = left;
        n->right = parse_logical_and();
        left     = n;
    }
    return left;
}

/* ── logical and: left and right  (when used in conditions, not concat) ──
   NOTE: T_AND is overloaded — it means "and" for both concat and logic.
   We distinguish at parse time: if the left side evaluates to a boolean
   (came from a comparison), this is logical AND; otherwise it's concat.
   We handle this by letting parse_compare handle the chaining directly,
   and parse_logical_and handles the condition-level "and". */
static Node *parse_logical_and(void) {
    Node *left = parse_concat();

    /* Only treat "and" as logical AND if left side looks like a condition.
       Heuristic: left is a compare node or a logical node. */
    while (tok_is(T_AND)) {
        /* peek: if right side starts with something that makes a condition,
           treat as logical AND; otherwise concat handles it downstream.
           Since we can't easily peek, we use the node kind of left. */
        NodeType k = left->kind;
        int is_cond = (k == NODE_CMP_EQ || k == NODE_CMP_NEQ ||
                       k == NODE_CMP_GT  || k == NODE_CMP_LT  ||
                       k == NODE_CMP_GTE || k == NODE_CMP_LTE ||
                       k == NODE_AND     || k == NODE_OR       ||
                       k == NODE_CONTAINS);
        if (!is_cond) break;   /* leave for concat level */
        tok_consume();
        Node *n  = node_new(NODE_AND);
        n->left  = left;
        n->right = parse_concat();
        left     = n;
    }
    return left;
}

/* concat: left (and right)*  — string concat */
static Node *parse_concat(void) {
    Node *left = parse_compare();
    while (tok_is(T_AND)) {
        tok_consume();
        Node *n  = node_new(NODE_CONCAT);
        n->left  = left;
        n->right = parse_compare();
        left     = n;
    }
    return left;
}

/* ── comparison — handles chained, English style, contains, in middle of ── */
static Node *parse_compare(void) {

    /* prefix "not expr" */
    if (tok_is(T_NOT)) {
        tok_consume();
        Node *inner = parse_compare();
        Node *n  = node_new(NODE_CMP_EQ);
        n->left  = inner;
        n->right = node_new(NODE_VAL);
        strcpy(n->right->value, "0");
        return n;
    }

    Node *left = parse_additive();

    /* "x in middle of A and B" — between check */
    if (tok_is(T_IN) && tok_peek(T_MIDDLE, 1)) {
        tok_consume(); /* in */
        tok_consume(); /* middle */
        if (tok_is(T_OF)) tok_consume();
        Node *lo = parse_additive();
        if (tok_is(T_AND)) tok_consume();
        Node *hi = parse_additive();
        /* duplicate left so clo and chi don't share the same pointer */
        Node *left2 = node_new(left->kind);
        strncpy(left2->name,  left->name,  63);
        strncpy(left2->value, left->value, MAX_STR-1);
        Node *clo = node_new(NODE_CMP_GT); clo->left = left;  clo->right = lo;
        Node *chi = node_new(NODE_CMP_LT); chi->left = left2; chi->right = hi;
        Node *n   = node_new(NODE_AND);    n->left   = clo;   n->right   = chi;
        return n;
    }

    /* "name contains expr" */
    if (tok_is(T_CONTAINS)) {
        tok_consume();
        Node *n  = node_new(NODE_CONTAINS);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }

    /* English-style: "is greater than", "is less than", "is not", "is" */
    if (tok_is(T_IS)) {
        tok_consume();
        if (tok_is(T_NOT)) {
            tok_consume();
            Node *n  = node_new(NODE_CMP_NEQ);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        if (tok_is(T_GREATER)) {
            tok_consume();
            if (tok_is(T_THAN)) tok_consume();
            Node *n  = node_new(NODE_CMP_GT);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        if (tok_is(T_LESS)) {
            tok_consume();
            if (tok_is(T_THAN)) tok_consume();
            Node *n  = node_new(NODE_CMP_LT);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        Node *n  = node_new(NODE_CMP_EQ);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }

    /* English-style without "is": "greater than X", "less than X" */
    if (tok_is(T_GREATER)) {
        tok_consume();
        if (tok_is(T_THAN)) tok_consume();
        Node *n  = node_new(NODE_CMP_GT);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }
    if (tok_is(T_LESS)) {
        tok_consume();
        if (tok_is(T_THAN)) tok_consume();
        Node *n  = node_new(NODE_CMP_LT);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }

    /* symbolic operators */
    NodeType kind = NODE_NOOP;
    if      (tok_is(T_EQ))  kind = NODE_CMP_EQ;
    else if (tok_is(T_NEQ)) kind = NODE_CMP_NEQ;
    else if (tok_is(T_GT))  kind = NODE_CMP_GT;
    else if (tok_is(T_LT))  kind = NODE_CMP_LT;
    else if (tok_is(T_GTE)) kind = NODE_CMP_GTE;
    else if (tok_is(T_LTE)) kind = NODE_CMP_LTE;

    if (kind != NODE_NOOP) {
        tok_consume();
        Node *mid = parse_additive();

        /* chained compare: 1 < x < 10
           After parsing left OP mid, check if another compare op follows.
           If so, build: (left OP mid) AND (mid OP2 right)            */
        NodeType kind2 = NODE_NOOP;
        if      (tok_is(T_EQ))  kind2 = NODE_CMP_EQ;
        else if (tok_is(T_NEQ)) kind2 = NODE_CMP_NEQ;
        else if (tok_is(T_GT))  kind2 = NODE_CMP_GT;
        else if (tok_is(T_LT))  kind2 = NODE_CMP_LT;
        else if (tok_is(T_GTE)) kind2 = NODE_CMP_GTE;
        else if (tok_is(T_LTE)) kind2 = NODE_CMP_LTE;

        if (kind2 != NODE_NOOP) {
            tok_consume();
            Node *right = parse_additive();
            /* mid is shared — duplicate it as a VAL to avoid double-free */
            char mid_val[MAX_STR] = {0};
            /* We can only safely duplicate VAL and VAR nodes here.
               For VAR nodes store the name; for VAL store value. */
            Node *mid2 = node_new(mid->kind);
            strncpy(mid2->name,  mid->name,  63);
            strncpy(mid2->value, mid->value, MAX_STR-1);
            /* shallow copy is safe — mid is a simple factor (no children) */
            Node *cmp1  = node_new(kind);  cmp1->left = left; cmp1->right = mid;
            Node *cmp2  = node_new(kind2); cmp2->left = mid2; cmp2->right = right;
            Node *n     = node_new(NODE_AND);
            n->left  = cmp1;
            n->right = cmp2;
            (void)mid_val;
            return n;
        }

        Node *n  = node_new(kind);
        n->left  = left;
        n->right = mid;
        return n;
    }

    return left;
}

/* additive */
static Node *parse_additive(void) {
    Node *left = parse_multiplicative();
    while (tok_is(T_PLUS) || tok_is(T_MINUS)) {
        NodeType kind = tok_is(T_PLUS) ? NODE_ADD_EXPR : NODE_SUB_EXPR;
        tok_consume();
        Node *n  = node_new(kind);
        n->left  = left;
        n->right = parse_multiplicative();
        left     = n;
    }
    return left;
}

/* multiplicative */
static Node *parse_multiplicative(void) {
    Node *left = parse_power();
    while (tok_is(T_STAR) || tok_is(T_SLASH) || tok_is(T_MOD)) {
        NodeType kind;
        if      (tok_is(T_STAR))  kind = NODE_MUL_EXPR;
        else if (tok_is(T_SLASH)) kind = NODE_DIV_EXPR;
        else                      kind = NODE_MOD_EXPR;
        tok_consume();
        Node *n  = node_new(kind);
        n->left  = left;
        n->right = parse_power();
        left     = n;
    }
    return left;
}

/* power: right-associative  2^8^2 = 2^(8^2) */
static Node *parse_power(void) {
    Node *base = parse_unary();
    if (tok_is(T_POW)) {
        tok_consume();
        Node *n  = node_new(NODE_POW_EXPR);
        n->left  = base;
        n->right = parse_power();   /* right-associative */
        return n;
    }
    return base;
}

/* unary / builtins: length of, upper of, lower of, abs of, round of */
static Node *parse_unary(void) {

    /* length of x — only a builtin when followed by 'of', otherwise it's a var name */
    if (tok_is(T_LENGTH) && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_LENGTH);
        n->left = parse_factor();
        return n;
    }
    /* upper of x */
    if (tok_is(T_UPPER) && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_UPPER);
        n->left = parse_factor();
        return n;
    }
    /* lower of x */
    if (tok_is(T_LOWER) && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_LOWER);
        n->left = parse_factor();
        return n;
    }
    /* abs of x */
    if (tok_is(T_ABS) && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_ABS);
        n->left = parse_factor();
        return n;
    }
    /* round of x */
    if (tok_is(T_ROUND) && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_ROUND);
        n->left = parse_factor();
        return n;
    }
    /* keyword used as variable name (e.g. 'length', 'upper' as param) */
    if (tok_is(T_LENGTH) || tok_is(T_UPPER) || tok_is(T_LOWER) ||
        tok_is(T_ABS)    || tok_is(T_ROUND)) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        return n;
    }

    return parse_factor();
}

/* factor */
static Node *parse_factor(void) {

    if (tok_is(T_LPAREN)) {
        tok_consume();
        Node *n = parse_expression();
        if (tok_is(T_RPAREN)) tok_consume();
        return n;
    }

    if (tok_is(T_NUMBER)) {
        Node *n = node_new(NODE_VAL);
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
        return n;
    }

    if (tok_is(T_STRING)) {
        Node *n = node_new(NODE_VAL);
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
        return n;
    }

    /* num(expr) */
    if (tok_is(T_NUM)) {
        tok_consume();
        if (tok_is(T_LPAREN)) {
            tok_consume();
            Node *n = parse_expression();
            if (tok_is(T_RPAREN)) tok_consume();
            return n;
        }
        return parse_factor();
    }

    /* string() legacy */
    if (tok_is("STR_TYPE")) {
        tok_consume();
        if (tok_is(T_LPAREN)) {
            tok_consume();
            Node *n;
            if (tok_is(T_IDENT)) {
                n = node_new(NODE_VAL);
                strncpy(n->value, tok_cur()->value, MAX_STR-1);
                tok_consume();
            } else {
                n = parse_expression();
            }
            if (tok_is(T_RPAREN)) tok_consume();
            return n;
        }
    }

    /* keyword used as function name */
    if (!tok_is(T_IDENT) && tok_peek(T_LPAREN, 1) &&
        !tok_is(T_LPAREN) && !tok_is(T_RPAREN) &&
        !tok_is(T_LBRACK) && !tok_is(T_DOT)   &&
        !tok_is(T_COMMA)  && !tok_is(T_COLON)) {
        strncpy(g_tokens[g_tok_pos].type, T_IDENT,
                sizeof(g_tokens[0].type)-1);
    }

    /* identifier: variable, array index, or function call */
    if (tok_is(T_IDENT)) {
        char name[64];
        strncpy(name, tok_cur()->value, 63);
        tok_consume();

        if (tok_is(T_LPAREN)) {
            tok_consume();
            Node *n = node_new(NODE_CALL_EXPR);
            strncpy(n->name, name, 63);
            n->arg_count = 0;
            while (!tok_is(T_RPAREN) && g_tok_pos < g_tok_count) {
                if (tok_is(T_COMMA)) { tok_consume(); continue; }
                int prev = g_tok_pos;
                n->args[n->arg_count++] = parse_expression();
                if (g_tok_pos == prev) { tok_consume(); break; }
                if (n->arg_count >= MAX_ARGS) break;
            }
            if (tok_is(T_RPAREN)) tok_consume();
            return n;
        }

        if (tok_is(T_LBRACK)) {
            tok_consume();
            Node *idx = parse_expression();
            if (tok_is(T_RBRACK)) tok_consume();
            Node *n  = node_new(NODE_VAR_IDX);
            strncpy(n->name, name, 63);
            n->left  = idx;
            return n;
        }

        Node *n = node_new(NODE_VAR);
        strncpy(n->name, name, 63);
        return n;
    }

    return node_new(NODE_NOOP);
}

/* ─────────────────────────────────────────────────────────────────
   find matching 'end.' for a block
   ───────────────────────────────────────────────────────────────── */
static int find_end(int line_index) {
    int depth = 1;
    for (int j = line_index + 1; j < g_line_count; j++) {
        char *p = g_lines[j];
        while (*p == ' ' || *p == '\t') p++;

        if (strncmp(p, "make ",   5) == 0 ||
            strncmp(p, "repeat ", 7) == 0 ||
            strncmp(p, "while ",  6) == 0 ||
            strncmp(p, "if ",     3) == 0)
            depth++;

        if (strncmp(p, "end.",    4) == 0 ||
            strncmp(p, "end ",    4) == 0 ||
            strcmp (p, "end")        == 0)
            depth--;

        if (depth == 0) return j;
    }
    /* never found end — report unclosed block */
    char *kw = g_lines[line_index];
    while (*kw == ' ' || *kw == '\t') kw++;
    char keyword[16] = {0};
    int ki = 0;
    while (kw[ki] && kw[ki] != ' ' && kw[ki] != ':' && ki < 15) {
        keyword[ki] = kw[ki];
        ki++;
    }
    keyword[ki] = '\0';
    err_unclosed_block(line_index + 1, keyword);
    return g_line_count - 1;
}

/* ─────────────────────────────────────────────────────────────────
   STATEMENT PARSERS
   ───────────────────────────────────────────────────────────────── */

/*
 * let x be 5.
 * let x be num(5).
 * let name be hello.
 * let nums are 1 2 3.
 * let x, b, c.          ← multi-declare, no value
 */
static Node *parse_let(int line_index) {
    (void)line_index;
    tok_expect(T_LET);

    if (!tok_is(T_IDENT)) {
        fprintf(stderr, "NAT: expected variable name after 'let'\n");
        return node_new(NODE_NOOP);
    }

    /* multi-declare: let x, b, c. — next meaningful token after name is COMMA or DOT */
    /* check: name followed by COMMA → multi-declare */
    if (tok_peek(T_COMMA, 1)) {
        Node *n = node_new(NODE_LET_DECLARE);
        int idx = 0;
        while (tok_is(T_IDENT) && idx < MAX_ARGS) {
            strncpy(n->params[idx++], tok_cur()->value, 63);
            tok_consume();
            if (tok_is(T_COMMA)) tok_consume();
        }
        n->param_count = idx;
        return n;
    }

    /* single declare with no value: let x. */
    if (tok_peek(T_DOT, 1)) {
        char vname[64];
        strncpy(vname, tok_cur()->value, 63);
        tok_consume();
        Node *n = node_new(NODE_LET_DECLARE);
        strncpy(n->params[0], vname, 63);
        n->param_count = 1;
        return n;
    }

    char vname[64];
    strncpy(vname, tok_cur()->value, 63);
    tok_consume();

    /* array: let nums are 1 2 3 4. */
    if (tok_is(T_ARE)) {
        tok_consume();
        Node *n = node_new(NODE_ARRAY);
        strncpy(n->name, vname, 63);
        int idx = 0;
        while (g_tok_pos < g_tok_count && !tok_is(T_DOT)) {
            if (tok_is(T_COMMA)) { tok_consume(); continue; }
            Node *elem;
            if (tok_is(T_IDENT) && !tok_peek(T_LPAREN, 1)) {
                elem = node_new(NODE_VAL);
                strncpy(elem->value, tok_cur()->value, MAX_STR-1);
                tok_consume();
            } else {
                elem = parse_factor();
            }
            if (elem && elem->kind != NODE_NOOP && idx < MAX_ARGS)
                n->args[idx++] = elem;
        }
        n->arg_count = idx;
        return n;
    }

    /* skip 'be' or '=' */
    if (tok_is(T_BE) || tok_is("ASSIGN")) tok_consume();

    Node *n = node_new(NODE_LET);
    strncpy(n->name, vname, 63);

    /* bare-word string: let name be Nisarg. */
    if (tok_is(T_IDENT) && !tok_peek(T_LPAREN, 1)) {
        int next_is_end = (g_tok_pos + 1 >= g_tok_count) ||
                          tok_peek(T_DOT, 1);
        if (next_is_end) {
            Node *sv = node_new(NODE_VAL);
            strncpy(sv->value, tok_cur()->value, MAX_STR-1);
            tok_consume();
            n->left = sv;
            return n;
        }
    }

    n->left = parse_expression();
    return n;
}

/* show */
static Node *parse_show(void) {
    tok_expect(T_SHOW);
    Node *n = node_new(NODE_SHOW);

    if (tok_is(T_LPAREN)) {
        tok_consume();
        n->left = parse_expression();
        if (tok_is(T_RPAREN)) tok_consume();
        return n;
    }

    /* check if current token is a known function name for English-style call */
    if (g_tok_pos < g_tok_count) {
        int is_func = 0;
        for (int _fi = 0; _fi < g_func_count; _fi++)
            if (strcmp(g_funcs[_fi].name, g_tokens[g_tok_pos].value) == 0)
                { is_func = 1; break; }

        if (is_func && (g_tok_pos + 1 < g_tok_count) &&
            !tok_peek(T_DOT, 1) && !tok_peek(T_COLON, 1) &&
            !tok_peek(T_LPAREN, 1)) {
            Node *call = node_new(NODE_CALL_EXPR);
            strncpy(call->name, tok_cur()->value, 63);
            tok_consume();
            call->arg_count = 0;
            while (g_tok_pos < g_tok_count &&
                   !tok_is(T_DOT) && !tok_is(T_AND)) {
                if (tok_is(T_COMMA)) { tok_consume(); continue; }
                int prev = g_tok_pos;
                Node *arg = parse_factor();
                if (!arg || arg->kind == NODE_NOOP || g_tok_pos == prev) {
                    tok_consume(); break;
                }
                call->args[call->arg_count++] = arg;
                if (call->arg_count >= MAX_ARGS) break;
            }
            if (tok_is(T_AND)) {
                tok_consume();
                Node *concat = node_new(NODE_CONCAT);
                concat->left  = call;
                concat->right = parse_expression();
                n->left = concat;
            } else {
                n->left = call;
            }
            return n;
        }
    }

    n->left = parse_expression();
    return n;
}

/* give */
static Node *parse_give(void) {
    tok_expect(T_GIVE);
    Node *n = node_new(NODE_GIVE);
    n->left = parse_expression();
    return n;
}

/* make */
static Node *parse_make(int line_index) {
    tok_expect(T_MAKE);
    Node *n = node_new(NODE_FUNC_DEF);

    if (g_tok_pos >= g_tok_count) {
        fprintf(stderr, "NAT: expected function name after 'make'\n");
        return node_new(NODE_NOOP);
    }
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();

    if (tok_is(T_WITH)) {
        tok_consume();
        while (!tok_is(T_INSIDE) && g_tok_pos < g_tok_count) {
            /* accept IDENT or any keyword used as a param name (e.g. 'length', 'width') */
            Token *pt = tok_cur();
            if (pt && pt->value[0] && !tok_is(T_COLON) && n->param_count < MAX_PARAMS) {
                strncpy(n->params[n->param_count++], pt->value, 63);
            }
            tok_consume();
        }
    }
    if (tok_is(T_INSIDE)) {
        tok_consume();
    } else {
        err_make_no_inside(line_index + 1, n->name);
    }

    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/* repeat */
static Node *parse_repeat(int line_index) {
    tok_expect(T_REPEAT);

    if (tok_is(T_IDENT) && tok_peek(T_FROM, 1)) {
        Node *n = node_new(NODE_FOR_LOOP);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        tok_consume(); /* from */
        n->left  = parse_factor();
        if (tok_is(T_TO)) tok_consume();
        n->right = parse_factor();
        if (tok_is(T_STEP)) {
            tok_consume();
            n->else_branch = parse_factor();
        }
        n->body_start = line_index + 1;
        n->body_end   = find_end(line_index);
        return n;
    }

    Node *n = node_new(NODE_LOOP);
    if (!tok_is(T_TIMES) && !tok_is(T_COLON) && g_tok_pos < g_tok_count)
        n->left = parse_factor();
    if (tok_is(T_TIMES)) tok_consume();
    if (tok_is(T_COLON)) {
        tok_consume();
    } else {
        err_missing_colon(line_index + 1, "repeat");
    }
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/* while */
static Node *parse_while(int line_index) {
    tok_consume();
    Node *n   = node_new(NODE_WHILE);
    n->left   = parse_expression();
    if (tok_is(T_COLON)) {
        tok_consume();
    } else {
        err_missing_colon(line_index + 1, "while");
    }
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/* if */
static Node *parse_if(int line_index) {
    tok_expect(T_IF);
    Node *n = node_new(NODE_IF);
    n->left = parse_expression();
    if (tok_is(T_COLON)) {
        tok_consume();
    } else {
        err_missing_colon(line_index + 1, "if");
    }

    int end_line  = find_end(line_index);
    int else_line = -1;
    int depth     = 1;

    for (int j = line_index + 1; j < end_line; j++) {
        char *p = g_lines[j];
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "make ",   5) == 0 ||
            strncmp(p, "repeat ", 7) == 0 ||
            strncmp(p, "while ",  6) == 0 ||
            strncmp(p, "if ",     3) == 0)
            depth++;
        if ((strncmp(p, "end.", 4) == 0 ||
             strncmp(p, "end ", 4) == 0 ||
             strcmp (p, "end")     == 0) && depth > 1)
            depth--;
        if (depth == 1 && strncmp(p, "else", 4) == 0) {
            else_line = j;
            break;
        }
    }

    if (else_line != -1) {
        n->body_start      = line_index + 1;
        n->body_end        = else_line - 1;
        Node *eb           = node_new(NODE_NOOP);
        eb->body_start     = else_line + 1;
        eb->body_end       = end_line - 1;
        n->else_branch     = eb;
    } else {
        n->body_start = line_index + 1;
        n->body_end   = end_line - 1;
    }
    n->loop_to = end_line;
    return n;
}

/* ask */
static Node *parse_ask(void) {
    tok_expect(T_ASK);
    if (tok_is(T_FOR)) tok_consume();
    Node *n = node_new(NODE_ASK);
    while (g_tok_pos < g_tok_count && !tok_is(T_DOT)) {
        if (tok_is(T_IDENT) && n->ask_count < MAX_ASK_VARS)
            strncpy(n->ask_vars[n->ask_count++], tok_cur()->value, 63);
        tok_consume();
    }
    return n;
}

/* add x with y to z */
static Node *parse_add_stmt(void) {
    tok_expect(T_ADD);
    Node *n  = node_new(NODE_ADD_ASSIGN);
    n->left  = parse_factor();
    if (tok_is(T_WITH)) tok_consume();
    n->right = parse_expression();
    if (tok_is(T_TO)) tok_consume();
    if (tok_is(T_IDENT)) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
    }
    return n;
}

/* function call statement */
static Node *parse_call_stmt(void) {
    Node *n = node_new(NODE_FUNC_CALL);
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();
    n->arg_count = 0;

    if (tok_is(T_LPAREN)) {
        tok_consume();
        while (!tok_is(T_RPAREN) && g_tok_pos < g_tok_count) {
            if (tok_is(T_COMMA)) { tok_consume(); continue; }
            int prev = g_tok_pos;
            n->args[n->arg_count++] = parse_expression();
            if (g_tok_pos == prev) { tok_consume(); break; }
            if (n->arg_count >= MAX_ARGS) break;
        }
        if (tok_is(T_RPAREN)) tok_consume();
        return n;
    }

    while (g_tok_pos < g_tok_count &&
           !tok_is(T_DOT) && !tok_is(T_COLON)) {
        if (tok_is(T_COMMA)) { tok_consume(); continue; }
        int prev = g_tok_pos;
        Node *arg = parse_factor();
        if (!arg || arg->kind == NODE_NOOP || g_tok_pos == prev) {
            tok_consume();
            break;
        }
        n->args[n->arg_count++] = arg;
        if (n->arg_count >= MAX_ARGS) break;
    }
    return n;
}

/* ─────────────────────────────────────────────────────────────────
   parse_statement — main dispatch
   ───────────────────────────────────────────────────────────────── */
Node *parse_statement(int line_index) {

    if (g_tok_count == 0) return node_new(NODE_NOOP);

    Token *t = tok_cur();
    if (!t) return node_new(NODE_NOOP);

    const char *ty = t->type;

    if (strcmp(ty, T_LET)    == 0) return parse_let(line_index);
    if (strcmp(ty, T_SHOW)   == 0) return parse_show();
    if (strcmp(ty, T_GIVE)   == 0) return parse_give();
    if (strcmp(ty, T_MAKE)   == 0) return parse_make(line_index);
    if (strcmp(ty, T_REPEAT) == 0) return parse_repeat(line_index);
    if (strcmp(ty, T_WHILE)  == 0) return parse_while(line_index);
    if (strcmp(ty, T_IF)     == 0) return parse_if(line_index);
    if (strcmp(ty, T_ASK)    == 0) return parse_ask();
    if (strcmp(ty, T_ADD)    == 0) return parse_add_stmt();

    /* bare assignment: x be 10. */
    if (strcmp(ty, T_IDENT) == 0 && tok_peek(T_BE, 1)) {
        Node *n = node_new(NODE_ASSIGN);
        strncpy(n->name, t->value, 63);
        tok_consume(); /* name */
        tok_consume(); /* be   */
        n->left = parse_expression();
        return n;
    }

    /* function call — known or unknown (unknown will error at runtime) */
    {
        int is_known_func = 0;
        for (int _fi = 0; _fi < g_func_count; _fi++)
            if (strcmp(g_funcs[_fi].name, t->value) == 0)
                { is_known_func = 1; break; }

        if (strcmp(ty, T_IDENT) == 0) {
            /* known func: English or paren style
               unknown func with parens: still parse, runtime will error
               unknown bare word with args: also parse for runtime error */
            if (is_known_func || tok_peek(T_LPAREN, 1) ||
                (g_tok_pos + 1 < g_tok_count &&
                 !tok_peek(T_DOT,   1) &&
                 !tok_peek(T_COLON, 1) &&
                 !tok_peek(T_BE,    1)))
                return parse_call_stmt();
        }
    }

    return node_new(NODE_NOOP);
}
