/*
 * tokenizer.c — NAT Language v3.2 Tokenizer
 *
 * v3.2 new keywords:
 *   or, in, middle, of, length, upper, lower, contains, abs, round
 * v3.2 new operator:
 *   ^ tokenised as T_POW (only when attached: 2^8, no spaces)
 */

#include "nat.h"

static void tok_add(const char *type, const char *value) {
    if (g_tok_count >= MAX_TOKENS) return;
    strncpy(g_tokens[g_tok_count].type,  type,  sizeof(g_tokens[0].type)  - 1);
    strncpy(g_tokens[g_tok_count].value, value, sizeof(g_tokens[0].value) - 1);
    g_tokens[g_tok_count].type[sizeof(g_tokens[0].type)-1]   = '\0';
    g_tokens[g_tok_count].value[sizeof(g_tokens[0].value)-1] = '\0';
    g_tok_count++;
}

typedef struct { const char *word; const char *tok; } KW;

static const KW keywords[] = {
    /* core */
    {"let",      T_LET     },
    {"be",       T_BE      },
    {"fix",      T_FIX     },
    {"add",      T_ADD     },
    {"show",     T_SHOW    },
    {"make",     T_MAKE    },
    {"with",     T_WITH    },
    {"inside",   T_INSIDE  },
    {"end",      T_END     },
    {"give",     T_GIVE    },
    /* loops */
    {"repeat",   T_REPEAT  },
    {"times",    T_TIMES   },
    {"from",     T_FROM    },
    {"to",       T_TO      },
    {"step",     T_STEP    },
    {"while",    T_WHILE   },
    /* input */
    {"ask",      T_ASK     },
    {"for",      T_FOR     },
    /* logic */
    {"and",      T_AND     },
    {"or",       T_OR      },
    {"are",      T_ARE     },
    /* conditionals */
    {"if",       T_IF      },
    {"else",     T_ELSE    },
    {"is",       T_IS      },
    {"not",      T_NOT     },
    {"greater",  T_GREATER },
    {"less",     T_LESS    },
    {"than",     T_THAN    },
    /* string/math builtins */
    {"in",       T_IN      },
    {"middle",   T_MIDDLE  },
    {"of",       T_OF      },
    {"length",   T_LENGTH  },
    {"upper",    T_UPPER   },
    {"lower",    T_LOWER   },
    {"contains", T_CONTAINS},
    {"abs",      T_ABS     },
    {"round",    T_ROUND   },
    /* v3.4 string stdlib */
    {"trim",     "TRIM"    },
    {"replace",  "REPLACE" },
    {"split",    "SPLIT"   },
    {"by",       "BY"      },
    /* v3.4 math stdlib */
    {"max",      "MAX"     },
    {"min",      "MIN"     },
    {"floor",    "FLOOR"   },
    {"ceil",     "CEIL"    },
    /* v3.4 type checking */
    {"number",   "NUMBER_T"},
    {"text",     "TEXT_T"  },
    /* v3.4 p5 — array/string ops */
    {"reverse",  "REVERSE" },
    {"first",    "FIRST"   },
    {"last",     "LAST"    },
    {"remove",   "REMOVE"  },
    {"swap",     "SWAP"    },
    {"even",     "EVEN"    },
    {"odd",      "ODD"     },
    {"at",       "AT"      },
    /* v3.5 — module system */
    {"use",      "USE"     },
    {"each",     "EACH"    },
    {"clamp",    "CLAMP"   },
    {"online",   "ONLINE"  },
    {"inline",   "ONLINE"  },   /* alias */
    /* v3.5 p3 — create graph */
    {"create",   "CREATE"  },
    {"graph",    "GRAPH"   },
    {"gap",      "GAP"     },
    {"axis",     "AXIS"    },
    {"range",    "RANGE"   },
    {"points",   "POINTS"  },
    {"type",     "TYPE"    },
    {"color",    "COLOR"   },
    {"linear",   "LINEAR"  },
    {"nonlinear","NONLINEAR"},
    {"exponential","EXPONENTIAL"},
    {"uniform",  "UNIFORM" },
    {"bar",      "BAR"     },
    {"scatter",  "SCATTER" },
    {"title",    "TITLE"   },
    /* v3.5 p3 — newline/skip */
    {"newline",  "NEWLINE" },
    {"skip",     "NEWLINE" },   /* alias */
    {"random",   "RANDOM"  },
    {"equal",    "EQUAL"   },
    /* number wrapper */
    {"num",      T_NUM     },
    {"int",      T_NUM     },   /* legacy alias */
    {"string",   "STR_TYPE"},
    {NULL, NULL}
};

static const char *keyword_lookup(const char *w) {
    for (int i = 0; keywords[i].word; i++)
        if (strcmp(keywords[i].word, w) == 0)
            return keywords[i].tok;
    return NULL;
}

void tokenize(const char *src) {
    g_tok_count = 0;
    g_tok_pos   = 0;

    int i   = 0;
    int len = (int)strlen(src);

    while (i < len) {

        if (isspace((unsigned char)src[i])) { i++; continue; }

        /* comment */
        if (src[i] == '#') break;

        /* word */
        if (isalpha((unsigned char)src[i]) || src[i] == '_') {
            char word[128]; int j = 0;
            while (i < len && (isalnum((unsigned char)src[i]) || src[i] == '_'))
                word[j++] = src[i++];
            word[j] = '\0';
            const char *kw = keyword_lookup(word);
            tok_add(kw ? kw : T_IDENT, word);
            continue;
        }

        /* number — may be followed immediately by ^ for power: 2^8 */
        if (isdigit((unsigned char)src[i])) {
            char num[64]; int j = 0;
            while (i < len) {
                if (isdigit((unsigned char)src[i])) {
                    num[j++] = src[i++];
                } else if (src[i] == '.' && (i+1) < len
                           && isdigit((unsigned char)src[i+1])) {
                    num[j++] = src[i++];
                } else {
                    break;
                }
                if (j >= 63) break;
            }
            num[j] = '\0';
            tok_add(T_NUMBER, num);
            /* immediately check for ^ (power, no spaces allowed) */
            if (i < len && src[i] == '^') {
                tok_add(T_POW, "^");
                i++;
            }
            continue;
        }

        /* negative number literal */
        if (src[i] == '-' && (i+1) < len && isdigit((unsigned char)src[i+1])) {
            int is_neg = 1;
            if (g_tok_count > 0) {
                const char *lt = g_tokens[g_tok_count-1].type;
                if (strcmp(lt, T_NUMBER) == 0 || strcmp(lt, T_IDENT)  == 0 ||
                    strcmp(lt, T_RPAREN) == 0 || strcmp(lt, T_RBRACK) == 0)
                    is_neg = 0;
            }
            if (is_neg) {
                char num[64]; int j = 0;
                num[j++] = src[i++];
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

        /* quoted string */
        if (src[i] == '"') {
            char str[MAX_STR]; int j = 0;
            i++;
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
            if (i < len) i++;
            tok_add(T_STRING, str);
            continue;
        }

        /* two-character operators */
        if (i+1 < len) {
            if (src[i]=='=' && src[i+1]=='=') { tok_add(T_EQ,  "=="); i+=2; continue; }
            if (src[i]=='!' && src[i+1]=='=') { tok_add(T_NEQ, "!="); i+=2; continue; }
            if (src[i]=='>' && src[i+1]=='=') { tok_add(T_GTE, ">="); i+=2; continue; }
            if (src[i]=='<' && src[i+1]=='=') { tok_add(T_LTE, "<="); i+=2; continue; }
        }

        /* single-character tokens */
        switch (src[i]) {
            case '+': tok_add(T_PLUS,  "+"); break;
            case '-': tok_add(T_MINUS, "-"); break;
            case '*': tok_add(T_STAR,  "*"); break;
            case '/': tok_add(T_SLASH, "/"); break;
            case '%': tok_add(T_MOD,   "%"); break;
            case '>': tok_add(T_GT,    ">"); break;
            case '<': tok_add(T_LT,    "<"); break;
            case '^': tok_add(T_POW,   "^"); break;
            case '=': tok_add("ASSIGN","="); break;
            case '(': tok_add(T_LPAREN,"("); break;
            case ')': tok_add(T_RPAREN,")"); break;
            case '[': tok_add(T_LBRACK,"["); break;
            case ']': tok_add(T_RBRACK,"]"); break;
            case ',': tok_add(T_COMMA, ","); break;
            case '.': tok_add(T_DOT,   "."); break;
            case ':': tok_add(T_COLON, ":"); break;
        }
        i++;
    }
}
