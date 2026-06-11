/*
 * errors.h — NAT Language v3.3 Error & Warning System
 *
 * Provides:
 *   nat_error()    — fatal error with line number + hint
 *   nat_warning()  — non-fatal warning with line number + hint
 *   nat_suggest()  — "did you mean X?" fuzzy match helper
 *
 * Every error format:
 *   NAT error on line N: <what went wrong>
 *     Hint: <how to fix it>
 *
 * Every warning format:
 *   NAT warning on line N: <what happened>
 *     Hint: <how to avoid it>
 */

#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* current executing line — set by execute_block each iteration */
extern int g_current_line;

/* ─────────────────────────────────────────────────────────────────
   FUZZY MATCH — Levenshtein distance (for "did you mean")
   ───────────────────────────────────────────────────────────────── */
static inline int nat_levenshtein(const char *a, const char *b) {
    int la = (int)strlen(a), lb = (int)strlen(b);
    if (la == 0) return lb;
    if (lb == 0) return la;

    /* cap to avoid stack overflow on wild input */
    if (la > 64) la = 64;
    if (lb > 64) lb = 64;

    int dp[65][65];
    for (int i = 0; i <= la; i++) dp[i][0] = i;
    for (int j = 0; j <= lb; j++) dp[0][j] = j;
    for (int i = 1; i <= la; i++)
        for (int j = 1; j <= lb; j++) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            int del  = dp[i-1][j] + 1;
            int ins  = dp[i][j-1] + 1;
            int sub  = dp[i-1][j-1] + cost;
            dp[i][j] = del < ins ? (del < sub ? del : sub)
                                 : (ins < sub ? ins : sub);
        }
    return dp[la][lb];
}

/*
 * nat_suggest — search a list of candidate strings and return the
 * closest one if within edit distance 2, else NULL.
 *
 * Usage:
 *   const char *candidates[] = {"score","name","age", NULL};
 *   const char *s = nat_suggest("scroe", candidates);
 *   if (s) printf("  Did you mean: '%s' ?\n", s);
 */
static inline const char *nat_suggest(const char *word,
                                       const char **candidates) {
    if (!word || !candidates) return NULL;
    const char *best     = NULL;
    int         best_d   = 3;   /* only suggest if distance <= 2 */
    for (int i = 0; candidates[i]; i++) {
        int d = nat_levenshtein(word, candidates[i]);
        if (d < best_d) { best_d = d; best = candidates[i]; }
    }
    return best;
}

/* ─────────────────────────────────────────────────────────────────
   PRINT HELPERS
   ───────────────────────────────────────────────────────────────── */
static inline void nat_error(int line, const char *what, const char *hint) {
    if (line == -1) return;   /* pre-scan suppression */
    if (line > 0)
        fprintf(stderr, "\nNAT error on line %d: %s\n", line, what);
    else
        fprintf(stderr, "\nNAT error: %s\n", what);
    if (hint && hint[0])
        fprintf(stderr, "  Hint: %s\n\n", hint);
    else
        fprintf(stderr, "\n");
}

static inline void nat_warning(int line, const char *what, const char *hint) {
    if (line == -1) return;   /* pre-scan suppression */
    if (line > 0)
        fprintf(stderr, "\nNAT warning on line %d: %s\n", line, what);
    else
        fprintf(stderr, "\nNAT warning: %s\n", what);
    if (hint && hint[0])
        fprintf(stderr, "  Hint: %s\n\n", hint);
    else
        fprintf(stderr, "\n");
}

/* ─────────────────────────────────────────────────────────────────
   SPECIFIC ERROR CONSTRUCTORS
   (called from eval.c / exec.c / parser.c)
   ───────────────────────────────────────────────────────────────── */

/* unknown variable — with did-you-mean from live var table */
static inline void err_unknown_var(int line, const char *name,
                                   const char **known_vars) {
    char what[256], hint[256];
    snprintf(what, sizeof(what), "unknown variable '%s'", name);
    const char *sugg = nat_suggest(name, known_vars);
    if (sugg) snprintf(hint, sizeof(hint), "Did you mean: '%s' ?", sugg);
    else       snprintf(hint, sizeof(hint),
                   "declare it first with:  let %s be <value>.", name);
    nat_error(line, what, hint);
}

/* unknown function — with did-you-mean from live func table */
static inline void err_unknown_func(int line, const char *name,
                                    const char **known_funcs) {
    char what[256], hint[256];
    snprintf(what, sizeof(what), "unknown function '%s'", name);
    const char *sugg = nat_suggest(name, known_funcs);
    if (sugg) snprintf(hint, sizeof(hint), "Did you mean: '%s' ?", sugg);
    else       snprintf(hint, sizeof(hint),
                   "define it first with:  make %s with <params> inside:", name);
    nat_error(line, what, hint);
}

/* wrong argument count */
static inline void err_arg_count(int line, const char *fname,
                                  int expected, int got) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "'%s' expects %d argument%s but got %d",
        fname, expected, expected == 1 ? "" : "s", got);
    snprintf(hint, sizeof(hint),
        "check your call to '%s' and make sure you pass exactly %d value%s",
        fname, expected, expected == 1 ? "" : "s");
    nat_error(line, what, hint);
}

/* division by zero */
static inline void err_div_zero(int line) {
    nat_error(line,
        "division by zero",
        "check that your divisor is not 0 before dividing");
}

/* modulo by zero */
static inline void err_mod_zero(int line) {
    nat_error(line,
        "modulo by zero",
        "check that the right side of '%' is not 0");
}

/* math on a non-number string */
static inline void err_not_a_number(int line, const char *varname) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "cannot do math on '%s' — it is a word not a number", varname);
    snprintf(hint, sizeof(hint),
        "use num(%s) to convert it first, or make sure it holds a number",
        varname);
    nat_error(line, what, hint);
}

/* array index out of range */
static inline void err_index_range(int line, const char *arrname,
                                    int idx, int size) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "index %d is out of range for '%s' (size is %d)",
        idx, arrname, size);
    snprintf(hint, sizeof(hint),
        "valid indexes are 0 to %d", size - 1);
    nat_error(line, what, hint);
}

/* missing colon after if/while/repeat */
static inline void err_missing_colon(int line, const char *keyword) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "expected ':' after '%s' condition", keyword);
    snprintf(hint, sizeof(hint),
        "%s <condition>:\n              %s",
        keyword, "              ^  colon goes here");
    nat_error(line, what, hint);
}

/* unclosed block */
static inline void err_unclosed_block(int line, const char *keyword) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "'%s' block on line %d was never closed with 'end.'", keyword, line);
    snprintf(hint, sizeof(hint),
        "add 'end.' after the last line of your %s block", keyword);
    nat_error(line, what, hint);
}

/* missing 'inside' in make */
static inline void err_make_no_inside(int line, const char *fname) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "function '%s' is missing the 'inside' keyword", fname);
    snprintf(hint, sizeof(hint),
        "make %s with <params> inside:\n    ...\nend.", fname);
    nat_error(line, what, hint);
}

/* bad repeat syntax */
static inline void err_bad_repeat(int line) {
    nat_error(line,
        "'repeat' has invalid syntax",
        "use:  repeat 5 times:\n"
        "  or:  repeat i from 1 to 10 step 1:");
}

/* empty function body warning */
static inline void warn_empty_func(int line, const char *fname) {
    char what[256];
    snprintf(what, sizeof(what), "function '%s' has an empty body", fname);
    nat_warning(line, what,
        "add at least one statement before 'end.'");
}

/* infinite loop warning */
static inline void warn_infinite_loop(int line) {
    nat_warning(line,
        "while loop ran over 1,000,000 iterations and was stopped",
        "make sure your loop condition eventually becomes false");
}

/* variable table overflow */
static inline void err_var_overflow(void) {
    nat_error(0,
        "too many variables — variable table is full",
        "try to reuse variables or break your program into functions");
}

/* function table overflow */
static inline void err_func_overflow(void) {
    nat_error(0,
        "too many functions — function table is full",
        "combine related functions or remove unused ones");
}

#endif /* ERRORS_H */

/* ── v3.4 error stretch ─────────────────────────────────────────── */

/* call before define */
static inline void err_call_before_define(int line, const char *fname) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "function '%s' is called before it is defined", fname);
    snprintf(hint, sizeof(hint),
        "move the definition of '%s' above the line where you call it", fname);
    nat_error(line, what, hint);
}

/* step zero */
static inline void err_step_zero(int line) {
    nat_error(line,
        "step value cannot be 0 in repeat loop",
        "use a non-zero step value like step 1 or step 2");
}

/* repeat negative/zero count */
static inline void err_repeat_count(int line) {
    nat_error(line,
        "repeat count cannot be negative or zero",
        "use a positive number like:  repeat 5 times:");
}

/* constant reassignment */
static inline void err_const_reassign(int line, const char *name) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "'%s' is a constant and cannot be changed", name);
    snprintf(hint, sizeof(hint),
        "use 'let' with a different name, or remove the 'fix' declaration");
    nat_error(line, what, hint);
}

/* bad type in math */
static inline void err_math_on_text(int line, const char *varname) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "cannot do math on '%s' — it is text not a number", varname);
    snprintf(hint, sizeof(hint),
        "use num(%s) to convert it first, or make sure it holds a number",
        varname);
    nat_error(line, what, hint);
}

/* array used in math */
static inline void err_array_in_math(int line, const char *name) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "'%s' is an array and cannot be used in math directly", name);
    snprintf(hint, sizeof(hint),
        "access a specific element like %s[0] instead", name);
    nat_error(line, what, hint);
}

/* ── v3.4 p5 array/string errors ───────────────────────────────── */

static inline void err_insert_range(int line, const char *name, int idx, int size) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "insert index %d is out of range for '%s' (size is %d)", idx, name, size);
    snprintf(hint, sizeof(hint),
        "valid insert positions are 0 to %d", size);
    nat_error(line, what, hint);
}

static inline void err_remove_range(int line, const char *name, int idx, int size) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "remove index %d is out of range for '%s' (size is %d)", idx, name, size);
    snprintf(hint, sizeof(hint),
        "valid indexes are 0 to %d", size - 1);
    nat_error(line, what, hint);
}

static inline void err_empty_array_op(int line, const char *op, const char *name) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "cannot '%s' from '%s' — array is empty", op, name);
    snprintf(hint, sizeof(hint),
        "add elements first with:  add <value> to %s.", name);
    nat_error(line, what, hint);
}

static inline void err_swap_range(int line, const char *name, int idx, int size) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "swap index %d is out of range for '%s' (size is %d)", idx, name, size);
    snprintf(hint, sizeof(hint),
        "valid indexes are 0 to %d", size - 1);
    nat_error(line, what, hint);
}

/* ── Personality system — random friendly word in errors ─────────── */
#include <time.h>

static inline const char *nat_personality(void) {
    static const char *words[] = {
        "mate", "bud", "dude", "pal", "chief",
        "boss", "champ", "higher mammal", "friend", "legend"
    };
    return words[rand() % 10];
}

/* geometry-specific humorous errors */
static inline void err_square_two_sides(int line) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "squares have equal sides, %s!", nat_personality());
    snprintf(hint, sizeof(hint),
        "use:  area_square(5).\n"
        "  or use:  area_rect(4, 6).  for different sides");
    nat_error(line, what, hint);
}

static inline void err_sphere_one_arg(int line) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "spheres only need radius, %s!", nat_personality());
    snprintf(hint, sizeof(hint),
        "use:  volume_sphere(5).");
    nat_error(line, what, hint);
}

static inline void err_circle_one_arg(int line) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "circles only need radius, %s!", nat_personality());
    snprintf(hint, sizeof(hint),
        "use:  area_circle(5).  or  circumference(5).");
    nat_error(line, what, hint);
}

static inline void err_rect_not_circumference(int line) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "rectangles have perimeter not circumference, %s!", nat_personality());
    snprintf(hint, sizeof(hint),
        "use:  perimeter_rect(4, 6).");
    nat_error(line, what, hint);
}

static inline void err_triangle_area_args(int line) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "triangle area needs base AND height, %s!", nat_personality());
    snprintf(hint, sizeof(hint),
        "use:  area_triangle(base, height).\n"
        "  or use:  area_triangle_heron(a, b, c).  if you have all 3 sides");
    nat_error(line, what, hint);
}

static inline void err_bad_conversion(int line, const char *from, const char *to) {
    char what[256], hint[256];
    snprintf(what, sizeof(what),
        "cannot convert '%s' to '%s', %s!", from, to, nat_personality());
    snprintf(hint, sizeof(hint),
        "check convert.tree for valid conversions");
    nat_error(line, what, hint);
}
