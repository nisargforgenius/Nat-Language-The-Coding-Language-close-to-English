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
static Node *parse_read_stmt(void);   /* v3.6 forward decl */

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
        /* Don't steal AND if left is a compare/logical node — 
           that belongs to parse_logical_and as logical AND */
        NodeType k = left->kind;
        int is_cond = (k == NODE_CMP_EQ || k == NODE_CMP_NEQ ||
                       k == NODE_CMP_GT  || k == NODE_CMP_LT  ||
                       k == NODE_CMP_GTE || k == NODE_CMP_LTE ||
                       k == NODE_AND     || k == NODE_OR       ||
                       k == NODE_CONTAINS|| k == NODE_IS_EVEN  ||
                       k == NODE_IS_ODD  || k == NODE_IS_NUMBER||
                       k == NODE_IS_TEXT);
        if (is_cond) break;  /* let parse_logical_and handle it */
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

    /* "needle not in haystack" — inverted contains
       e.g. "Nis" not in name  → left=needle, right=haystack
       eval expects left=haystack, right=needle so swap */
    if (tok_is(T_NOT) && tok_peek(T_IN, 1)) {
        tok_consume(); /* not */
        tok_consume(); /* in  */
        Node *cont  = node_new(NODE_CONTAINS);
        cont->right = left;             /* needle — was parsed as left */
        cont->left  = parse_additive(); /* haystack */
        /* wrap in NOT: (contains == 0) */
        Node *n  = node_new(NODE_CMP_EQ);
        n->left  = cont;
        n->right = node_new(NODE_VAL);
        strcpy(n->right->value, "0");
        return n;
    }

    /* "needle in haystack" — contains, order flipped
       e.g. "Nis" in name  → left=needle, right=haystack */
    if (tok_is(T_IN) && !tok_peek(T_MIDDLE, 1)) {
        tok_consume(); /* in */
        Node *n  = node_new(NODE_CONTAINS);
        n->right = left;             /* needle */
        n->left  = parse_additive(); /* haystack */
        return n;
    }

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

    /* English-style: "is greater than", "is less than", "is not", "is number", "is text", "is" */
    if (tok_is(T_IS)) {
        tok_consume();
        /* "is between A and B" */
        if (tok_is(T_BETWEEN)) {
            tok_consume(); /* between */
            Node *lo = parse_additive();
            if (tok_is(T_AND)) tok_consume();
            Node *hi = parse_additive();
            Node *left2 = node_new(left->kind);
            strncpy(left2->name,  left->name,  63);
            strncpy(left2->value, left->value, MAX_STR-1);
            Node *clo = node_new(NODE_CMP_GTE); clo->left = left;  clo->right = lo;
            Node *chi = node_new(NODE_CMP_LTE); chi->left = left2; chi->right = hi;
            Node *n   = node_new(NODE_AND);     n->left   = clo;   n->right   = chi;
            return n;
        }
        /* type checks */
        if (tok_is("NUMBER_T")) {
            tok_consume();
            Node *n = node_new(NODE_IS_NUMBER);
            n->left = left;
            return n;
        }
        if (tok_is("TEXT_T")) {
            tok_consume();
            Node *n = node_new(NODE_IS_TEXT);
            n->left = left;
            return n;
        }
        if (tok_is("EVEN")) {
            tok_consume();
            Node *n = node_new(NODE_IS_EVEN);
            n->left = left;
            return n;
        }
        if (tok_is("ODD")) {
            tok_consume();
            Node *n = node_new(NODE_IS_ODD);
            n->left = left;
            return n;
        }
        if (tok_is(T_NOT)) {
            tok_consume();
            /* is not equal to */
            if (tok_is("EQUAL")) {
                tok_consume();
                if (tok_is(T_TO)) tok_consume();
            }
            Node *n  = node_new(NODE_CMP_NEQ);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        if (tok_is(T_GREATER)) {
            tok_consume();
            if (tok_is(T_THAN)) tok_consume();
            if (tok_is(T_OR))   { tok_consume();
                if (tok_is("EQUAL")) tok_consume();
                if (tok_is(T_TO))   tok_consume();
                Node *n  = node_new(NODE_CMP_GTE);
                n->left  = left; n->right = parse_additive(); return n;
            }
            Node *n  = node_new(NODE_CMP_GT);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        if (tok_is(T_LESS)) {
            tok_consume();
            if (tok_is(T_THAN)) tok_consume();
            if (tok_is(T_OR))   { tok_consume();
                if (tok_is("EQUAL")) tok_consume();
                if (tok_is(T_TO))   tok_consume();
                Node *n  = node_new(NODE_CMP_LTE);
                n->left  = left; n->right = parse_additive(); return n;
            }
            Node *n  = node_new(NODE_CMP_LT);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        /* is equal to */
        if (tok_is("EQUAL")) {
            tok_consume();
            if (tok_is(T_TO)) tok_consume();
            Node *n  = node_new(NODE_CMP_EQ);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        Node *n  = node_new(NODE_CMP_EQ);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }

    /* English-style without "is": "greater than X", "less than X",
       "greater than or equal to X", "less than or equal to X" */
    if (tok_is(T_GREATER)) {
        tok_consume();
        if (tok_is(T_THAN)) tok_consume();
        /* greater than or equal to */
        if (tok_is(T_OR)) {
            tok_consume();
            if (tok_is("EQUAL"))   tok_consume();
            if (tok_is(T_TO))     tok_consume();
            Node *n  = node_new(NODE_CMP_GTE);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
        Node *n  = node_new(NODE_CMP_GT);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }
    if (tok_is(T_LESS)) {
        tok_consume();
        if (tok_is(T_THAN)) tok_consume();
        /* less than or equal to */
        if (tok_is(T_OR)) {
            tok_consume();
            if (tok_is("EQUAL"))   tok_consume();
            if (tok_is(T_TO))     tok_consume();
            Node *n  = node_new(NODE_CMP_LTE);
            n->left  = left;
            n->right = parse_additive();
            return n;
        }
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
    /* max of a and b */
    if (tok_is("MAX") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n  = node_new(NODE_MAX);
        n->left  = parse_multiplicative();
        if (tok_is(T_AND)) tok_consume();
        n->right = parse_multiplicative();
        return n;
    }
    if (tok_is("MAX")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }

    /* min of a and b */
    if (tok_is("MIN") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n  = node_new(NODE_MIN);
        n->left  = parse_multiplicative();
        if (tok_is(T_AND)) tok_consume();
        n->right = parse_multiplicative();
        return n;
    }
    if (tok_is("MIN")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }

    /* floor of x */
    if (tok_is("FLOOR") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_FLOOR);
        n->left = parse_factor();
        return n;
    }
    if (tok_is("FLOOR")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }

    /* ceil of x */
    if (tok_is("CEIL") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_CEIL);
        n->left = parse_factor();
        return n;
    }
    if (tok_is("CEIL")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }
    if (tok_is("TRIM") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_TRIM);
        n->left = parse_factor();
        return n;
    }
    /* trim used as variable name */
    if (tok_is("TRIM")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        return n;
    }

    /* replace "A" with "B" in x */
    if (tok_is("REPLACE")) {
        tok_consume();
        Node *n  = node_new(NODE_REPLACE);
        n->args[0] = parse_factor();          /* search  */
        if (tok_is(T_WITH)) tok_consume();
        n->args[1] = parse_factor();          /* replace */
        if (tok_is(T_IN)) tok_consume();
        n->args[2] = parse_factor();          /* source  */
        n->arg_count = 3;
        return n;
    }

    /* split x by "sep" */
    if (tok_is("SPLIT")) {
        tok_consume();
        Node *n  = node_new(NODE_SPLIT);
        n->left  = parse_factor();            /* source  */
        if (tok_is("BY")) tok_consume();
        n->right = parse_factor();            /* separator */
        return n;
    }
    /* clamp x from A to B */
    if (tok_is("CLAMP")) {
        tok_consume();
        Node *n  = node_new(NODE_CLAMP);
        n->left  = parse_factor();        /* value */
        if (tok_is(T_FROM)) tok_consume();
        n->args[0] = parse_factor();      /* lo */
        if (tok_is(T_TO)) tok_consume();
        n->args[1] = parse_factor();      /* hi */
        n->arg_count = 2;
        return n;
    }

    /* keyword used as variable name fallback */
    if (tok_is(T_LENGTH) || tok_is(T_UPPER) || tok_is(T_LOWER) ||
        tok_is(T_ABS)    || tok_is(T_ROUND)) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        return n;
    }

    /* ── v3.4 p5 new builtins ── */

    /* text of x — number to string */
    if (tok_is("TEXT_T") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_TEXT_OF);
        n->left = parse_factor();
        return n;
    }
    if (tok_is("TEXT_T")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }

    /* repeat "ha" N times — expression style */
    if (tok_is(T_REPEAT) && (tok_peek(T_STRING, 1) ||
        (g_tok_pos + 1 < g_tok_count &&
         strcmp(g_tokens[g_tok_pos+1].type, T_IDENT) == 0))) {
        tok_consume();
        Node *n  = node_new(NODE_STR_REPEAT);
        n->left  = parse_factor();   /* string */
        n->right = parse_factor();   /* count  */
        if (tok_is(T_TIMES)) tok_consume();
        return n;
    }

    /* reverse of x */
    if (tok_is("REVERSE") && tok_peek(T_OF, 1)) {
        tok_consume();
        if (tok_is(T_OF)) tok_consume();
        Node *n = node_new(NODE_REVERSE);
        n->left = parse_factor();
        return n;
    }
    if (tok_is("REVERSE")) {
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); return n;
    }

    /* first of arr  OR  first N of x */
    /* first of arr  OR  first N of x  OR  variable named 'first' */
    if (tok_is("FIRST")) {
        /* if followed by DOT, COMMA, operator, or end — it's a variable */
        int next = g_tok_pos + 1;
        int next_is_op = (next >= g_tok_count) ||
            tok_peek(T_DOT,   1) || tok_peek(T_COMMA, 1) ||
            tok_peek(T_PLUS,  1) || tok_peek(T_MINUS, 1) ||
            tok_peek(T_STAR,  1) || tok_peek(T_SLASH, 1) ||
            tok_peek(T_EQ,    1) || tok_peek(T_NEQ,   1) ||
            tok_peek(T_GT,    1) || tok_peek(T_LT,    1) ||
            tok_peek(T_GTE,   1) || tok_peek(T_LTE,   1) ||
            tok_peek(T_AND,   1) || tok_peek(T_OR,    1) ||
            tok_peek(T_RPAREN,1) || tok_peek(T_RBRACK,1);
        if (next_is_op) {
            Node *n = node_new(NODE_VAR);
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); return n;
        }
        tok_consume();
        if (tok_is(T_OF)) {
            tok_consume();
            Node *n = node_new(NODE_FIRST_ELEM);
            n->left = parse_factor();
            return n;
        }
        Node *n  = node_new(NODE_FIRST_N);
        n->left  = parse_factor();
        if (tok_is(T_OF)) tok_consume();
        n->right = parse_factor();
        return n;
    }

    /* last of arr  OR  last N of x  OR  variable named 'last' */
    if (tok_is("LAST")) {
        int next_is_op = (g_tok_pos + 1 >= g_tok_count) ||
            tok_peek(T_DOT,   1) || tok_peek(T_COMMA, 1) ||
            tok_peek(T_PLUS,  1) || tok_peek(T_MINUS, 1) ||
            tok_peek(T_STAR,  1) || tok_peek(T_SLASH, 1) ||
            tok_peek(T_EQ,    1) || tok_peek(T_NEQ,   1) ||
            tok_peek(T_GT,    1) || tok_peek(T_LT,    1) ||
            tok_peek(T_GTE,   1) || tok_peek(T_LTE,   1) ||
            tok_peek(T_AND,   1) || tok_peek(T_OR,    1) ||
            tok_peek(T_RPAREN,1) || tok_peek(T_RBRACK,1);
        if (next_is_op) {
            Node *n = node_new(NODE_VAR);
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); return n;
        }
        tok_consume();
        if (tok_is(T_OF)) {
            tok_consume();
            Node *n = node_new(NODE_LAST_ELEM);
            n->left = parse_factor();
            return n;
        }
        Node *n  = node_new(NODE_LAST_N);
        n->left  = parse_factor();
        if (tok_is(T_OF)) tok_consume();
        n->right = parse_factor();
        return n;
    }

    /* is even x / is odd x — prefix style */
    if (tok_is(T_IS) && tok_peek("EVEN", 1)) {
        tok_consume(); tok_consume();
        Node *n = node_new(NODE_IS_EVEN);
        n->left = parse_factor();
        return n;
    }
    if (tok_is(T_IS) && tok_peek("ODD", 1)) {
        tok_consume(); tok_consume();
        Node *n = node_new(NODE_IS_ODD);
        n->left = parse_factor();
        return n;
    }

    /* read "file.txt" — as expression for let x be read "file.txt". */
    if (tok_is("FREAD")) {
        return parse_read_stmt();
    }

    return parse_factor();
}

/* factor */
static Node *parse_factor(void) {

    /* file "x.txt" exists — boolean expression */
    if (tok_is("FILE_KW")) {
        tok_consume(); /* file */
        Node *n = node_new(NODE_FILE_EXISTS);
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
        if (tok_cur() && strcmp(tok_cur()->type, "EXISTS") == 0)
            tok_consume(); /* exists */
        return n;
    }

    /* random from A to B  /  random between A and B — expression style */
    if (tok_is("RANDOM")) {
        tok_consume();
        Node *n = node_new(NODE_RANDOM);
        if (tok_is(T_FROM) || tok_is(T_BETWEEN)) tok_consume();
        n->left  = parse_factor();
        if (tok_is(T_TO) || tok_is(T_AND)) tok_consume();
        n->right = parse_factor();
        n->param_count = 0;
        return n;
    }

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
    err_unclosed_block(g_current_line, keyword);
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

    /* Accept IDENT or any keyword used as a variable name (e.g. 'upper', 'lower', 'round') */
    Token *vt = tok_cur();
    if (!vt || tok_is(T_DOT) || tok_is(T_COLON) ||
        tok_is(T_LPAREN) || tok_is(T_RPAREN)) {
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

    /* array: let nums are 1 2 3 4.  OR  let nums are 10. (empty array of size N) */
    if (tok_is(T_ARE)) {
        tok_consume();
        /* detect: single NUMBER followed by DOT = sized empty array */
        if (tok_is(T_NUMBER) && tok_peek(T_DOT, 1)) {
            Node *n = node_new(NODE_ARR_DECLARE_N);
            strncpy(n->name, vname, 63);
            n->left = parse_factor(); /* size */
            return n;
        }
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

    /* v3.5 rule: bare IDENT after 'be' is ALWAYS a variable lookup.
       String literals MUST use quotes: let name be "Nisarg".
       This prevents silent bugs like: let x be y. where y is undeclared.
       Exception: single bare word that is clearly a name (no operators)
       is kept for backward compat BUT emits a warning if not declared. */
    n->left = parse_expression();
    return n;
}

/* show */
static Node *parse_show(void) {
    tok_expect(T_SHOW);
    Node *n = node_new(NODE_SHOW);

    /* show each item in arr. / online / with "sep" */
    if (tok_is("EACH")) {
        tok_consume(); /* each */
        Node *se = node_new(NODE_SHOW_EACH);
        /* loop variable name */
        strncpy(se->params[0], tok_cur()->value, 63);
        tok_consume();
        if (tok_is(T_IN)) tok_consume();
        /* source — array or string variable */
        se->left = parse_factor();
        /* online modifier */
        if (tok_is("ONLINE")) {
            tok_consume();
            se->value[0] = 'O'; /* flag: online */
            /* with "sep" */
            if (tok_is(T_WITH)) {
                tok_consume();
                strncpy(se->params[1], tok_cur()->value, 63);
                tok_consume();
                se->value[1] = 'S'; /* flag: custom separator */
            } else {
                /* default separator: space */
                strncpy(se->params[1], " ", 63);
            }
        }
        n->left = se;
        return n;
    }

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
        err_make_no_inside(g_current_line, n->name);
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
        err_missing_colon(g_current_line, "repeat");
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
        err_missing_colon(g_current_line, "while");
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
        err_missing_colon(g_current_line, "if");
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
        n->body_start  = line_index + 1;
        n->body_end    = else_line - 1;

        /* check if it's "else if" or plain "else" */
        char *ep = g_lines[else_line];
        while (*ep == ' ' || *ep == '\t') ep++;

        if (strncmp(ep, "else if ", 8) == 0 ||
            strncmp(ep, "else if:", 8) == 0) {
            /* else if — parse the else line as a new if node */
            tokenize(g_lines[else_line]);
            g_tok_pos = 0;
            tok_expect(T_ELSE);   /* consume 'else' */
            /* now parse the 'if ...:' as a nested if */
            Node *elif_node = parse_if(else_line);
            n->else_branch  = elif_node;
        } else {
            /* plain else */
            Node *eb       = node_new(NODE_NOOP);
            eb->body_start = else_line + 1;
            eb->body_end   = end_line - 1;
            n->else_branch = eb;
        }
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
    /* check for "add X to arr" vs "add X with Y to Z" */
    if (!tok_is(T_TO)) n->right = parse_expression();
    if (tok_is(T_TO)) tok_consume();
    if (tok_is(T_IDENT)) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
    }
    /* "add X to arr at N" — insert at index */
    if (tok_is("AT")) {
        tok_consume();
        n->kind  = NODE_ARR_INSERT;
        n->right = parse_factor(); /* index */
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

/*
 * use math.tree.
 * use mylib.tree.
 * Captures the full filename including extension.
 */

/* ═══════════════════════════════════════════════════════════
 *  v3.6 — File I/O parse helpers
 * ═══════════════════════════════════════════════════════════ */

/* write "text" to "file.txt".
   write "text" to "file.txt" at line N.
   write varname inside "file.nat".          ← runtime injection */
static Node *parse_write_stmt(void) {
    tok_consume(); /* write */
    Node *n = node_new(NODE_FILE_WRITE);
    n->left = parse_expression();   /* value to write */

    Token *nxt = tok_cur();
    if (!nxt) return n;

    if (strcmp(nxt->type, T_INSIDE) == 0) {
        /* write x inside "file.nat" */
        tok_consume(); /* inside */
        n->kind = NODE_WRITE_INSIDE;
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
        return n;
    }

    /* write "text" to "file.txt" */
    if (strcmp(nxt->type, T_TO) == 0) {
        tok_consume(); /* to */
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
        /* optional: at line N */
        if (tok_cur() && strcmp(tok_cur()->type, "AT") == 0) {
            tok_consume(); /* at */
            if (tok_cur() && strcmp(tok_cur()->type, "LINE") == 0) {
                tok_consume(); /* line */
                n->right = parse_expression(); /* line number */
                n->kind  = NODE_FILE_WRITE_LINE;
            }
        }
    }
    return n;
}

/* append "text" to "file.txt". */
static Node *parse_append_stmt(void) {
    tok_consume(); /* append */
    Node *n  = node_new(NODE_FILE_APPEND);
    n->left  = parse_expression(); /* value */
    if (tok_cur() && strcmp(tok_cur()->type, T_TO) == 0) {
        tok_consume(); /* to */
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
    }
    return n;
}

/* insert "text" to "file.txt" at line N. */
static Node *parse_insert_stmt(void) {
    tok_consume(); /* insert */
    Node *n  = node_new(NODE_FILE_INSERT);
    n->left  = parse_expression(); /* value */
    if (tok_cur() && strcmp(tok_cur()->type, T_TO) == 0) {
        tok_consume(); /* to */
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
    }
    if (tok_cur() && strcmp(tok_cur()->type, "AT") == 0) {
        tok_consume(); /* at */
        if (tok_cur() && strcmp(tok_cur()->type, "LINE") == 0) {
            tok_consume(); /* line */
            n->right = parse_expression(); /* line number */
        }
    }
    return n;
}

/* remove line N from "file.txt". */
static Node *parse_remove_line_stmt(void) {
    /* called when REMOVE is followed by LINE token */
    tok_consume(); /* remove */
    tok_consume(); /* line   */
    Node *n  = node_new(NODE_FILE_REMOVE_LINE);
    n->right = parse_expression(); /* line number */
    if (tok_cur() && strcmp(tok_cur()->type, T_FROM) == 0) {
        tok_consume(); /* from */
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
    }
    return n;
}

/* read "file.txt".
   used in expression too: read "file.txt" at line N / as array */
static Node *parse_read_stmt(void) {
    tok_consume(); /* read */
    Node *n = node_new(NODE_FILE_READ);
    if (tok_cur()) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); /* filename */
    }
    /* at line N */
    if (tok_cur() && strcmp(tok_cur()->type, "AT") == 0) {
        tok_consume(); /* at */
        if (tok_cur() && strcmp(tok_cur()->type, "LINE") == 0) {
            tok_consume(); /* line */
            n->right = parse_expression();
            n->kind  = NODE_FILE_READ_LINE;
        }
    }
    return n;
}

/* delete file "file.txt". */
static Node *parse_delete_file_stmt(void) {
    tok_consume(); /* delete */
    if (tok_cur() && strcmp(tok_cur()->type, "FILE_KW") == 0)
        tok_consume(); /* file */
    Node *n = node_new(NODE_FILE_DELETE);
    if (tok_cur()) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume(); /* filename */
    }
    return n;
}

static Node *parse_use_stmt(void) {
    tok_consume(); /* use */
    Node *n = node_new(NODE_USE);

    /* collect filename: IDENT DOT IDENT DOT(end)
       e.g. tokens: IDENT("testlib") DOT IDENT("tree") DOT
       result: "testlib.tree"                                  */
    char filename[MAX_PATH_LEN] = {0};

    while (g_tok_pos < g_tok_count) {
        /* collect word token */
        if (!tok_is(T_DOT)) {
            strncat(filename, tok_cur()->value,
                    MAX_PATH_LEN-1-strlen(filename));
            tok_consume();
        }
        /* now expect a dot */
        if (!tok_is(T_DOT)) break;
        tok_consume(); /* consume dot */

        /* if next token is also a dot or end-of-line, this was the terminator */
        if (g_tok_pos >= g_tok_count || tok_is(T_DOT)) break;

        /* otherwise it's a separator — add dot and continue */
        strncat(filename, ".", MAX_PATH_LEN-1-strlen(filename));
    }

    strncpy(n->value, filename, MAX_STR-1);
    return n;
}

/*
 * random from A to B for x y z.   — statement style, multi-var
 * let n be random from A to B.    — expression style, single var (handled in parse_factor)
 */
static Node *parse_random_stmt(void) {
    tok_consume(); /* random */
    Node *n = node_new(NODE_RANDOM);
    if (tok_is(T_FROM) || tok_is(T_BETWEEN)) tok_consume();
    n->left  = parse_factor();   /* lo */
    if (tok_is(T_TO) || tok_is(T_AND)) tok_consume();
    n->right = parse_factor();   /* hi */
    /* for x y z */
    if (tok_is(T_FOR)) tok_consume();
    int idx = 0;
    while (g_tok_pos < g_tok_count && !tok_is(T_DOT) && idx < MAX_PARAMS) {
        if (tok_is(T_COMMA)) { tok_consume(); continue; }
        Token *pt = tok_cur();
        if (!pt || tok_is(T_DOT)) break;
        strncpy(n->params[idx++], pt->value, 63);
        tok_consume();
    }
    n->param_count = idx;
    return n;
}

/*
 * remove arr at N.
 * remove last from arr.
 * remove first from arr.
 */
static Node *parse_remove_stmt(void) {
    tok_consume(); /* remove */
    Node *n = node_new(NODE_ARR_REMOVE);

    /* remove last from arr */
    if (tok_is("LAST")) {
        tok_consume();
        if (tok_is(T_FROM)) tok_consume();
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        n->value[0] = 'L'; /* flag: last */
        return n;
    }
    /* remove first from arr */
    if (tok_is("FIRST")) {
        tok_consume();
        if (tok_is(T_FROM)) tok_consume();
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
        n->value[0] = 'F'; /* flag: first */
        return n;
    }
    /* remove arr at N */
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();
    if (tok_is("AT")) tok_consume();
    n->left = parse_factor(); /* index */
    n->value[0] = 'I';        /* flag: index */
    return n;
}

/*
 * swap arr at 1 and 3.
 */
static Node *parse_swap_stmt(void) {
    tok_consume(); /* swap */
    Node *n = node_new(NODE_ARR_SWAP);
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();
    if (tok_is("AT")) tok_consume();
    n->left  = parse_factor();   /* index 1 */
    if (tok_is(T_AND)) tok_consume();
    n->right = parse_factor();   /* index 2 */
    return n;
}

/*
 * reverse arr.   — reverse array in place
 */
static Node *parse_reverse_stmt(void) {
    tok_consume(); /* reverse */
    Node *n = node_new(NODE_REVERSE);
    /* if followed by IDENT (no 'of') it's a statement — arr name */
    if (tok_is(T_IDENT)) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
    } else {
        n->left = parse_factor();
    }
    return n;
}

/*
 * repeat "ha" 5 times.  — statement style, just prints
 */
static Node *parse_str_repeat_stmt(void) {
    tok_consume(); /* repeat */
    Node *n  = node_new(NODE_STR_REPEAT);
    n->left  = parse_factor();   /* string */
    n->right = parse_factor();   /* count  */
    if (tok_is(T_TIMES)) tok_consume();
    n->name[0] = 'P'; /* flag: print directly */
    return n;
}

/*
 * each item in fruits:
 *     show item.
 * end.
 */
static Node *parse_each_stmt(int line_index) {
    tok_consume(); /* each */
    Node *n = node_new(NODE_EACH);
    /* loop variable name */
    strncpy(n->params[0], tok_cur()->value, 63);
    n->param_count = 1;
    tok_consume();
    if (tok_is(T_IN)) tok_consume();

    /* each line in file "x.txt": — FILE_EACH variant */
    if (tok_cur() && strcmp(tok_cur()->type, "FILE_KW") == 0) {
        tok_consume(); /* file */
        n->kind = NODE_FILE_EACH;
        if (tok_cur()) {
            strncpy(n->name, tok_cur()->value, 63);
            tok_consume(); /* filename */
        }
    } else {
        /* source — normal array/string */
        n->left = parse_factor();
    }

    if (tok_is(T_COLON)) tok_consume();
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    n->loop_to    = n->body_end;
    return n;
}

/*
 * create graph "Title":
 *     x axis be "Time".
 *     y axis be "Distance".
 *     range x is 0 to 10.
 *     range y is 0 to 50.
 *     type be linear.
 *     color be "blue".
 *     points are (0,0) (2,10) (4,20).
 * end.
 *
 * Stores all graph config into node->value (JSON-like string)
 * and body_start/body_end for the config lines.
 */
static Node *parse_create_graph(int line_index) {
    tok_consume(); /* create */
    if (tok_is("GRAPH")) tok_consume();
    Node *n = node_new(NODE_GRAPH);
    /* title — quoted string */
    if (tok_is(T_STRING)) {
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
    } else if (tok_is(T_IDENT)) {
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
    }
    if (tok_is(T_COLON)) tok_consume();
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    n->loop_to    = n->body_end;
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

    if (strcmp(ty, "USE")    == 0) return parse_use_stmt();
    if (strcmp(ty, "WRITE")  == 0) return parse_write_stmt();
    if (strcmp(ty, "FAPPEND")== 0) return parse_append_stmt();
    if (strcmp(ty, "FINSERT")== 0) return parse_insert_stmt();
    if (strcmp(ty, "FREAD")  == 0) return parse_read_stmt();
    if (strcmp(ty, "FDELETE")== 0) return parse_delete_file_stmt();
    /* remove line N from "x" vs remove item from array — check next token */
    if (strcmp(ty, "REMOVE") == 0 &&
        g_tok_pos + 1 < g_tok_count &&
        strcmp(g_tokens[g_tok_pos+1].type, "LINE") == 0)
        return parse_remove_line_stmt();
    if (strcmp(ty, T_LET)    == 0) return parse_let(line_index);
    if (strcmp(ty, T_SHOW)   == 0) return parse_show();
    if (strcmp(ty, T_GIVE)   == 0) return parse_give();
    if (strcmp(ty, T_MAKE)   == 0) return parse_make(line_index);
    /* repeat "str" N times. — statement style MUST come before loop repeat */
    if (strcmp(ty, T_REPEAT) == 0 &&
        g_tok_pos + 1 < g_tok_count &&
        strcmp(g_tokens[g_tok_pos+1].type, T_STRING) == 0)
        return parse_str_repeat_stmt();
    if (strcmp(ty, T_REPEAT) == 0) return parse_repeat(line_index);
    if (strcmp(ty, T_WHILE)  == 0) return parse_while(line_index);
    if (strcmp(ty, T_IF)     == 0) return parse_if(line_index);
    if (strcmp(ty, T_ASK)    == 0) return parse_ask();
    if (strcmp(ty, T_ADD)    == 0) return parse_add_stmt();
    if (strcmp(ty, "RANDOM") == 0) return parse_random_stmt();
    if (strcmp(ty, "REMOVE") == 0) return parse_remove_stmt();
    if (strcmp(ty, "SWAP")   == 0) return parse_swap_stmt();
    if (strcmp(ty, "REVERSE")== 0) return parse_reverse_stmt();
    if (strcmp(ty, "EACH")   == 0) return parse_each_stmt(line_index);
    if (strcmp(ty, "CREATE") == 0) return parse_create_graph(line_index);
    if (strcmp(ty, "NEWLINE")== 0) {
        tok_consume();
        return node_new(NODE_NEWLINE);
    }

    /* array index assignment: fruits[1] be "mango". */
    if (strcmp(ty, T_IDENT) == 0 && tok_peek(T_LBRACK, 1)) {
        char aname[64];
        strncpy(aname, t->value, 63);
        tok_consume(); /* name  */
        tok_consume(); /* [     */
        Node *idx = parse_expression();
        tok_consume(); /* ]     */
        if (tok_is(T_BE) || tok_is("ASSIGN")) tok_consume();
        Node *n  = node_new(NODE_ARR_INSERT); /* reuse with flag 'S' = set */
        strncpy(n->name, aname, 63);
        n->left  = parse_expression(); /* value */
        n->right = idx;                /* index */
        n->value[0] = 'S';             /* flag: set/replace */
        return n;
    }

    /* bare assignment: x be 10.  OR  x = 10. */
    if (strcmp(ty, T_IDENT) == 0 &&
        (tok_peek(T_BE, 1) || tok_peek("ASSIGN", 1))) {
        Node *n = node_new(NODE_ASSIGN);
        strncpy(n->name, t->value, 63);
        tok_consume(); /* name     */
        tok_consume(); /* be or =  */
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
                 !tok_peek(T_DOT,    1) &&
                 !tok_peek(T_COLON,  1) &&
                 !tok_peek(T_BE,     1) &&
                 !tok_peek("ASSIGN", 1)))
                return parse_call_stmt();
        }
    }

    return node_new(NODE_NOOP);
}
