/*
 * tokenizer.c — NAT Language v3.0 Tokenizer
 *
 * Converts one source line into a flat token array.
 * Fully whitespace-tolerant: any amount of spaces/tabs is fine.
 *
 * v3.0 keyword changes:
 *   num     — universal number wrapper (replaces int/float/double)
 *   be      — assignment keyword (replaces =)
 *   fix     — constant declaration
 *   add     — English arithmetic assignment
 */

#include "nat.h"

/* ── add one token ─────────────────────────────────────────────── */
static void tok_add(const char *type, const char *value) {
    if (g_tok_count >= MAX_TOKENS) return;
    strncpy(g_tokens[g_tok_count].type,  type,  sizeof(g_tokens[0].type)  - 1);
    strncpy(g_tokens[g_tok_count].value, value, sizeof(g_tokens[0].value) - 1);
    g_tokens[g_tok_count].type[sizeof(g_tokens[0].type)-1]   = '\0';
    g_tokens[g_tok_count].value[sizeof(g_tokens[0].value)-1] = '\0';
    g_tok_count++;
}

/* ── keyword table ─────────────────────────────────────────────── */
typedef struct { const char *word; const char *tok; } KW;

static const KW keywords[] = {
    /* core language */
    {"let",     T_LET    },
    {"be",      T_BE     },   /* assignment: let x be 5. */
    {"fix",     T_FIX    },   /* constant:   fix pi 3.14 */
    {"add",     T_ADD    },   /* arithmetic: add x with y to z. */
    {"show",    T_SHOW   },
    {"make",    T_MAKE   },
    {"with",    T_WITH   },
    {"inside",  T_INSIDE },
    {"end",     T_END    },
    {"give",    T_GIVE   },
    /* loops */
    {"repeat",  T_REPEAT },
    {"times",   T_TIMES  },
    {"from",    T_FROM   },
    {"to",      T_TO     },
    {"step",    T_STEP   },
    {"while",   T_WHILE  },
    /* input */
    {"ask",     T_ASK    },
    {"for",     T_FOR    },
    /* expressions */
    {"and",     T_AND    },
    {"are",     T_ARE    },
    /* conditionals */
    {"if",      T_IF     },
    {"else",    T_ELSE   },
    {"is",      T_IS     },
    {"not",     T_NOT    },
    {"greater", T_GREATER},
    {"less",    T_LESS   },
    {"than",    T_THAN   },
    /* number wrapper — universal type */
    {"num",     T_NUM    },
    /* legacy: int() still works as an alias for num() */
    {"int",     T_NUM    },
    /* NOTE: float/double are NOT keywords — they are valid identifiers
       that users can freely use as variable or function names           */
    {"string",  "STR_TYPE"},
    {NULL, NULL}
};

static const char *keyword_lookup(const char *w) {
    for (int i = 0; keywords[i].word; i++)
        if (strcmp(keywords[i].word, w) == 0)
            return keywords[i].tok;
    return NULL;
}

/* ── main tokenize ─────────────────────────────────────────────── */
void tokenize(const char *src) {
    g_tok_count = 0;
    g_tok_pos   = 0;

    int i   = 0;
    int len = (int)strlen(src);

    while (i < len) {

        /* skip whitespace */
        if (isspace((unsigned char)src[i])) { i++; continue; }

        /* comment: # to end of line */
        if (src[i] == '#') break;

        /* ── word (keyword or identifier) ── */
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            char word[128]; int j = 0;
            while (i < len && (isalnum((unsigned char)src[i]) || src[i] == '_'))
                word[j++] = src[i++];
            word[j] = '\0';

            const char *kw = keyword_lookup(word);
            tok_add(kw ? kw : T_IDENT, word);
            continue;
        }

        /* ── number (integer or decimal) ──
           Only consume '.' as decimal if next char is a digit.
           Trailing "300." — the dot is a statement terminator, not decimal. */
        if (isdigit((unsigned char)src[i])) {
            char num[64]; int j = 0;
            while (i < len) {
                if (isdigit((unsigned char)src[i])) {
                    num[j++] = src[i++];
                } else if (src[i] == '.' && (i+1) < len
                           && isdigit((unsigned char)src[i+1])) {
                    /* true decimal point followed by digit */
                    num[j++] = src[i++];
                } else {
                    break;
                }
                if (j >= 63) break;
            }
            num[j] = '\0';
            tok_add(T_NUMBER, num);
            continue;
        }

        /* ── negative number literal: -5, -3.14 ── */
        if (src[i] == '-' && (i+1) < len && isdigit((unsigned char)src[i+1])) {
            /* only treat as negative literal if preceded by an operator/open
               context — simple heuristic: if last token is not NUMBER/IDENT/RPAREN */
            int is_neg = 1;
            if (g_tok_count > 0) {
                const char *lt = g_tokens[g_tok_count-1].type;
                if (strcmp(lt, T_NUMBER) == 0 || strcmp(lt, T_IDENT)  == 0 ||
                    strcmp(lt, T_RPAREN) == 0 || strcmp(lt, T_RBRACK) == 0)
                    is_neg = 0; /* it's a minus operator */
            }
            if (is_neg) {
                char num[64]; int j = 0;
                num[j++] = src[i++]; /* '-' */
                while (i < len) {
                    if (isdigit((unsigned char)src[i])) {
                        num[j++] = src[i++];
                    } else if (src[i] == '.' && (i+1) < len
                               && isdigit((unsigned char)src[i+1])) {
                        num[j++] = src[i++];
                    } else break;
                    if (j >= 63) break;
                }
                num[j] = '\0';
                tok_add(T_NUMBER, num);
                continue;
            }
        }

        /* ── quoted string ── */
        if (src[i] == '"') {
            char str[MAX_STR]; int j = 0;
            i++; /* skip opening quote */
            while (i < len && src[i] != '"') {
                if (src[i] == '\\' && i+1 < len) {
                    i++;
                    switch (src[i]) {
                        case 'n':  str[j++] = '\n'; break;
                        case 't':  str[j++] = '\t'; break;
                        case '"':  str[j++] = '"';  break;
                        case '\\': str[j++] = '\\'; break;
                        default:   str[j++] = src[i]; break;
                    }
                } else {
                    str[j++] = src[i];
                }
                i++;
                if (j >= MAX_STR - 1) break;
            }
            str[j] = '\0';
            if (i < len) i++; /* skip closing quote */
            tok_add(T_STRING, str);
            continue;
        }

        /* ── two-character operators ── */
        if (i+1 < len) {
            if (src[i]=='=' && src[i+1]=='=') { tok_add(T_EQ,  "=="); i+=2; continue; }
            if (src[i]=='!' && src[i+1]=='=') { tok_add(T_NEQ, "!="); i+=2; continue; }
            if (src[i]=='>' && src[i+1]=='=') { tok_add(T_GTE, ">="); i+=2; continue; }
            if (src[i]=='<' && src[i+1]=='=') { tok_add(T_LTE, "<="); i+=2; continue; }
        }

        /* ── single-character tokens ── */
        switch (src[i]) {
            case '+': tok_add(T_PLUS,  "+"); break;
            case '-': tok_add(T_MINUS, "-"); break;
            case '*': tok_add(T_STAR,  "*"); break;
            case '/': tok_add(T_SLASH, "/"); break;
            case '%': tok_add(T_MOD,   "%"); break;
            case '>': tok_add(T_GT,    ">"); break;
            case '<': tok_add(T_LT,    "<"); break;
            case '=': tok_add("ASSIGN", "="); break;
            case '(': tok_add(T_LPAREN,"("); break;
            case ')': tok_add(T_RPAREN,")"); break;
            case '[': tok_add(T_LBRACK,"["); break;
            case ']': tok_add(T_RBRACK,"]"); break;
            case ',': tok_add(T_COMMA, ","); break;
            case '.': tok_add(T_DOT,   "."); break;
            case ':': tok_add(T_COLON, ":"); break;
            /* silently skip unknown characters */
        }
        i++;
    }
}
