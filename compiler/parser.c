/*
 * parser.c — NAT Language v3.0 Recursive-Descent Parser
 *
 * Converts the flat token array into AST nodes.
 *
 * v3.0 syntax changes:
 *   let x be 5.             (be replaces =)
 *   let x be num(5).        (num() is universal number type)
 *   let name be hello.      (bare word after 'be' = string literal)
 *   fix pi 3.14             (one-line constant, no dot needed)
 *   add x with 5 to x.      (English arithmetic assignment)
 *   repeat i from 1 to 6   → INCLUSIVE (i goes 1..6)
 *
 * Operator precedence (low → high):
 *   concat (and) → compare → add/sub → mul/div/mod → unary → factor
 */

#include "nat.h"

/* ── forward declarations ─────────────────────────────────────── */
static Node *parse_concat(void);
static Node *parse_compare(void);
static Node *parse_additive(void);
static Node *parse_multiplicative(void);
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

Node *parse_expression(void) { return parse_concat(); }

/* concat: left (and right)* */
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

/* comparison */
static Node *parse_compare(void) {

    /* prefix "not expr" — wraps as (expr == 0) */
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
        /* plain "is" = equals */
        Node *n  = node_new(NODE_CMP_EQ);
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
        Node *n  = node_new(kind);
        n->left  = left;
        n->right = parse_additive();
        return n;
    }
    return left;
}

/* additive: left (+|-) right */
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

/* multiplicative: left (*|/|%) right */
static Node *parse_multiplicative(void) {
    Node *left = parse_factor();
    while (tok_is(T_STAR) || tok_is(T_SLASH) || tok_is(T_MOD)) {
        NodeType kind;
        if      (tok_is(T_STAR))  kind = NODE_MUL_EXPR;
        else if (tok_is(T_SLASH)) kind = NODE_DIV_EXPR;
        else                      kind = NODE_MOD_EXPR;
        tok_consume();
        Node *n  = node_new(kind);
        n->left  = left;
        n->right = parse_factor();
        left     = n;
    }
    return left;
}

/* factor: literal | identifier | call | array-index | (expr) */
static Node *parse_factor(void) {

    /* parenthesised sub-expression */
    if (tok_is(T_LPAREN)) {
        tok_consume();
        Node *n = parse_expression();
        if (tok_is(T_RPAREN)) tok_consume();
        return n;
    }

    /* numeric literal */
    if (tok_is(T_NUMBER)) {
        Node *n = node_new(NODE_VAL);
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
        return n;
    }

    /* string literal */
    if (tok_is(T_STRING)) {
        Node *n = node_new(NODE_VAL);
        strncpy(n->value, tok_cur()->value, MAX_STR-1);
        tok_consume();
        return n;
    }

    /* num(expr)  — universal number wrapper, just unwrap */
    if (tok_is(T_NUM)) {
        tok_consume();
        if (tok_is(T_LPAREN)) {
            tok_consume();
            Node *n = parse_expression();
            if (tok_is(T_RPAREN)) tok_consume();
            return n;
        }
        /* num without parens: treat next token as value */
        return parse_factor();
    }

    /* string() legacy wrapper — treat bare word inside as string literal */
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

    /* keyword used as function name — e.g. add(5,10), double(x)
       If ANY non-structural token is followed by LPAREN, promote it
       to T_IDENT so the identifier branch below handles it as a call. */
    if (!tok_is(T_IDENT) && tok_peek(T_LPAREN, 1) &&
        !tok_is(T_LPAREN) && !tok_is(T_RPAREN) &&
        !tok_is(T_LBRACK) && !tok_is(T_DOT)   &&
        !tok_is(T_COMMA)  && !tok_is(T_COLON)) {
        strncpy(g_tokens[g_tok_pos].type, T_IDENT,
                sizeof(g_tokens[0].type)-1);
    }

    /* identifier: variable, array-index, or function call */
    if (tok_is(T_IDENT)) {
        char name[64];
        strncpy(name, tok_cur()->value, 63);
        tok_consume();

        /* function call: name(args) */
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

        /* array index: name[expr] */
        if (tok_is(T_LBRACK)) {
            tok_consume();
            Node *idx = parse_expression();
            if (tok_is(T_RBRACK)) tok_consume();
            Node *n  = node_new(NODE_VAR_IDX);
            strncpy(n->name, name, 63);
            n->left  = idx;
            return n;
        }

        /* plain variable */
        Node *n = node_new(NODE_VAR);
        strncpy(n->name, name, 63);
        return n;
    }

    /* fallback: no-op so we never crash */
    return node_new(NODE_NOOP);
}

/* ─────────────────────────────────────────────────────────────────
   STATEMENT HELPER: find matching 'end.' for a block
   ───────────────────────────────────────────────────────────────── */
static int find_end(int line_index) {
    int depth = 1;
    for (int j = line_index + 1; j < g_line_count; j++) {
        /* check stripped line for block openers / closers */
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
    return g_line_count - 1;
}

/* ─────────────────────────────────────────────────────────────────
   STATEMENT PARSERS
   ───────────────────────────────────────────────────────────────── */

/*
 * let x be 5.
 * let x be num(3.14).
 * let name be hello.       ← bare word becomes string literal
 * let name be "hello world".
 * let nums are 1 2 3.
 */
static Node *parse_let(int line_index) {
    (void)line_index;
    tok_expect(T_LET);

    if (!tok_is(T_IDENT)) {
        fprintf(stderr, "NAT: expected variable name after 'let'\n");
        return node_new(NODE_NOOP);
    }
    char vname[64];
    strncpy(vname, tok_cur()->value, 63);
    tok_consume();

    /* array:  let nums are 1 2 3 4. */
    if (tok_is(T_ARE)) {
        tok_consume();
        Node *n = node_new(NODE_ARRAY);
        strncpy(n->name, vname, 63);
        int idx = 0;
        while (g_tok_pos < g_tok_count && !tok_is(T_DOT)) {
            if (tok_is(T_COMMA)) { tok_consume(); continue; }
            Node *elem;
            /* bare IDENT (no paren after) → treat as string literal */
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

    /* skip 'be' or legacy '=' */
    if (tok_is(T_BE) || tok_is("ASSIGN")) tok_consume();

    Node *n = node_new(NODE_LET);
    strncpy(n->name, vname, 63);

    /*
     * Bare-word string shortcut:
     *   let name be Nisarg.
     * Only applies when the IDENT is the sole token before the DOT.
     * i.e. token[pos] = IDENT AND token[pos+1] = DOT (or end of tokens).
     * This prevents "let i be i + 1" from treating 'i' as a bare string.
     */
    if (tok_is(T_IDENT) && !tok_peek(T_LPAREN, 1)) {
        /* peek two ahead: if next is DOT or we are at end, it is bare-word */
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

/*
 * show — two styles:
 *   show(expr).          classic parens style
 *   show "Hello".        English style — no parens needed
 *   show x.
 *   show x + y.
 *   show "Hi " and name.
 *
 * When there are NO parens, we also support a chained function call:
 *   show add 5 10.       → evaluates add(5,10) then shows result
 * This works because parse_expression() will see the IDENT 'add'
 * and if it matches a known function name followed by args, the
 * factor-level keyword-promote logic handles it.
 */
static Node *parse_show(void) {
    tok_expect(T_SHOW);
    Node *n = node_new(NODE_SHOW);

    /* Classic: show(expr) */
    if (tok_is(T_LPAREN)) {
        tok_consume();
        n->left = parse_expression();
        if (tok_is(T_RPAREN)) tok_consume();
        return n;
    }

    /* English: show expr   — if next token's VALUE matches a known
       function name (regardless of token type — 'add' is T_ADD etc.),
       and there are args before the dot, build a CALL_EXPR inline.     */
    if (g_tok_pos < g_tok_count) {
        /* check if current token value is a known function */
        int is_func = 0;
        for (int _fi = 0; _fi < g_func_count; _fi++)
            if (strcmp(g_funcs[_fi].name, g_tokens[g_tok_pos].value) == 0)
                { is_func = 1; break; }

        if (is_func && (g_tok_pos + 1 < g_tok_count) &&
            !tok_peek(T_DOT, 1) && !tok_peek(T_COLON, 1) &&
            !tok_peek(T_LPAREN, 1)) {
            /* English function call inside show: show funcname arg1 arg2 */
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
            /* allow "show add 5 10 and name" to concat result */
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

    /* Plain expression: show x. / show "hi". / show x + y. */
    n->left = parse_expression();
    return n;
}

/* give expr. */
static Node *parse_give(void) {
    tok_expect(T_GIVE);
    Node *n = node_new(NODE_GIVE);
    n->left = parse_expression();
    return n;
}

/* make funcname with p1 p2 inside: ... end. */
static Node *parse_make(int line_index) {
    tok_expect(T_MAKE);
    Node *n = node_new(NODE_FUNC_DEF);

    /* Accept ANY token as function name — users can name functions
       anything including words that are also keywords (e.g. add, fix).
       We just grab whatever token comes after 'make'. */
    if (g_tok_pos >= g_tok_count) {
        fprintf(stderr, "NAT: expected function name after 'make'\n");
        return node_new(NODE_NOOP);
    }
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();

    if (tok_is(T_WITH)) {
        tok_consume();
        while (!tok_is(T_INSIDE) && g_tok_pos < g_tok_count) {
            if (tok_is(T_IDENT) && n->param_count < MAX_PARAMS)
                strncpy(n->params[n->param_count++], tok_cur()->value, 63);
            tok_consume();
        }
    }
    if (tok_is(T_INSIDE)) tok_consume();

    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/*
 * repeat i from 1 to 6 step 1:   — INCLUSIVE: i goes 1, 2, 3, 4, 5, 6
 * repeat 5 times:
 * repeat n times:                 — n can be a variable
 */
static Node *parse_repeat(int line_index) {
    tok_expect(T_REPEAT);

    /* repeat i from X to Y step Z */
    if (tok_is(T_IDENT) && tok_peek(T_FROM, 1)) {
        Node *n = node_new(NODE_FOR_LOOP);
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();         /* i    */
        tok_consume();         /* from */

        n->left  = parse_factor(); /* from-value (expression node) */
        if (tok_is(T_TO)) tok_consume();
        n->right = parse_factor(); /* to-value   (expression node, INCLUSIVE) */

        if (tok_is(T_STEP)) {
            tok_consume();
            n->else_branch = parse_factor(); /* step-value */
        }
        n->body_start = line_index + 1;
        n->body_end   = find_end(line_index);
        return n;
    }

    /* repeat N times  (N = literal or variable) */
    Node *n = node_new(NODE_LOOP);
    if (!tok_is(T_TIMES) && !tok_is(T_COLON) && g_tok_pos < g_tok_count)
        n->left = parse_factor(); /* count expression */
    if (tok_is(T_TIMES)) tok_consume();
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/* while COND: ... end. */
static Node *parse_while(int line_index) {
    tok_consume(); /* eat 'while' */
    Node *n   = node_new(NODE_WHILE);
    n->left   = parse_compare();
    if (tok_is(T_COLON)) tok_consume();
    n->body_start = line_index + 1;
    n->body_end   = find_end(line_index);
    return n;
}

/*
 * if COND: ... [else: ...] end.
 * Stores end-line index in n->loop_to so executor can skip past.
 */
static Node *parse_if(int line_index) {
    tok_expect(T_IF);
    Node *n = node_new(NODE_IF);
    n->left = parse_compare();
    if (tok_is(T_COLON)) tok_consume();

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

/* ask for x y. */
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

/*
 * add x with y to z.
 *   → z = x + y
 *
 * add VALUE with VALUE to DEST.
 *   n->left  = expression for VALUE (first operand)
 *   n->right = expression for VALUE (second operand)  
 *   n->name  = destination variable name
 */
static Node *parse_add_stmt(void) {
    tok_expect(T_ADD);
    Node *n  = node_new(NODE_ADD_ASSIGN);
    n->left  = parse_factor();               /* first value  */
    if (tok_is(T_WITH)) tok_consume();
    n->right = parse_expression();           /* second value (full expr) */
    if (tok_is(T_TO)) tok_consume();
    /* destination variable */
    if (tok_is(T_IDENT)) {
        strncpy(n->name, tok_cur()->value, 63);
        tok_consume();
    }
    return n;
}

/*
 * parse_call_stmt — handles both call styles:
 *
 *   greet("World").        — classic parens style
 *   greet "World".         — English style, string arg
 *   greet name.            — English style, variable arg
 *   add 5 10.              — English style, multiple space-separated args
 *   show("hi").            — parens still work everywhere
 */
static Node *parse_call_stmt(void) {
    Node *n = node_new(NODE_FUNC_CALL);
    strncpy(n->name, tok_cur()->value, 63);
    tok_consume();   /* consume function name */
    n->arg_count = 0;

    /* ── Classic style: funcname(arg1, arg2, ...) ── */
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

    /* ── English style: funcname arg1 arg2 ...  (no parens) ──
       Collect all tokens up to the terminating DOT as arguments.
       Each argument is parsed as a factor (literal, variable, or
       parenthesised expression) so spaces separate arguments naturally.
       Examples:
           greet "World".         → one STRING arg
           greet name.            → one IDENT (variable) arg
           add 5 10.              → two NUMBER args
           compute x y z.         → three variable args               */
    while (g_tok_pos < g_tok_count &&
           !tok_is(T_DOT) && !tok_is(T_COLON)) {
        if (tok_is(T_COMMA)) { tok_consume(); continue; }
        int prev = g_tok_pos;
        Node *arg = parse_factor();   /* parse_factor handles IDENT/NUM/STR/(expr) */
        if (!arg || arg->kind == NODE_NOOP || g_tok_pos == prev) {
            /* safety: avoid infinite loop if factor consumed nothing */
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

    /* function call — works for both T_IDENT and keyword-named functions
       (e.g. 'add' tokenises as T_ADD but the user defined 'make add').
       Check if the current token's VALUE matches any known function. */
    {
        int is_known_func = 0;
        for (int _fi = 0; _fi < g_func_count; _fi++)
            if (strcmp(g_funcs[_fi].name, t->value) == 0)
                { is_known_func = 1; break; }

        if (is_known_func) {
            /* parens style: add(5,10). or sayhi(). */
            if (tok_peek(T_LPAREN, 1)) return parse_call_stmt();
            /* English style: add 5 10. — or no-arg call: sayhi. */
            return parse_call_stmt();
        } else if (strcmp(ty, T_IDENT) == 0 && tok_peek(T_LPAREN, 1)) {
            /* Unknown function called with parens — still try (will error at runtime) */
            return parse_call_stmt();
        }
    }

    return node_new(NODE_NOOP);
}
