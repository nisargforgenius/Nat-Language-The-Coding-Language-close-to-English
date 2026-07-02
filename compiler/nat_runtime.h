/* ═══════════════════════════════════════════════════════════════════
   nat_runtime.h — NAT Language Runtime Library
   v4.0 Phase 3

   Used by:
     1. The NAT interpreter (nat.exe) — linked directly
     2. Future: nat build generated C programs — #include this

   All functions operate on NatVal (typed double/string union)
   defined in nat.h. Zero string↔number conversion on numeric ops.
   ═══════════════════════════════════════════════════════════════════ */

#ifndef NAT_RUNTIME_H
#define NAT_RUNTIME_H

#include "nat.h"
#include <math.h>
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────
   CORE UTILITIES
   ───────────────────────────────────────────────────────────────── */

/* format a double for display — whole numbers show without .0 */
void   nat_fmt_num(char *out, int sz, double v);

/* check if string is purely numeric */
int    nat_is_number(const char *s);

/* read a Variable's value as double — no atof on hot path */
double nat_var_as_num(const Variable *v);

/* write a Variable's value as string into buffer */
void   nat_var_as_str(const Variable *v, char *out, int sz);

/* set a Variable from a string — auto-detects type */
void   nat_var_set_str(Variable *v, const char *s);

/* set a Variable from a known double */
void   nat_var_set_num(Variable *v, double d);

/* ─────────────────────────────────────────────────────────────────
   MATH OPERATIONS
   ───────────────────────────────────────────────────────────────── */

double nat_abs(double v);
double nat_round(double v);
double nat_floor_val(double v);
double nat_ceil_val(double v);
double nat_clamp(double v, double lo, double hi);
double nat_max2(double a, double b);
double nat_min2(double a, double b);
int    nat_is_even(double v);
int    nat_is_odd(double v);

/* variadic max/min — takes array of doubles and count */
double nat_larger(double *vals, int count);
double nat_smallest(double *vals, int count);

/* ─────────────────────────────────────────────────────────────────
   STRING OPERATIONS
   ───────────────────────────────────────────────────────────────── */

void   nat_upper(char *out, int sz, const char *s);
void   nat_lower(char *out, int sz, const char *s);
void   nat_trim(char *out, int sz, const char *s);
void   nat_replace(char *out, int sz,
                   const char *haystack,
                   const char *needle,
                   const char *replacement);
int    nat_contains(const char *haystack, const char *needle);
int    nat_str_length(const char *s);
void   nat_str_repeat(char *out, int sz, const char *s, int n);
void   nat_reverse_str(char *out, int sz, const char *s);
void   nat_first_n_chars(char *out, int sz, const char *s, int n);
void   nat_last_n_chars(char *out, int sz, const char *s, int n);
int    nat_is_num_str(const char *s);   /* alias for nat_is_number */
int    nat_is_text_str(const char *s);  /* always true for non-numeric */
void   nat_text_of(char *out, int sz, double v); /* num → string */

/* ─────────────────────────────────────────────────────────────────
   ARRAY OPERATIONS
   ───────────────────────────────────────────────────────────────── */

/* get first/last element as string into out */
void   nat_array_first(char *out, int sz, Variable *v);
void   nat_array_last(char *out, int sz, Variable *v);
int    nat_array_len(Variable *v);

/* push a string value onto array */
void   nat_array_push(Variable *v, const char *val);

/* pop last element from array */
void   nat_array_pop(Variable *v);

/* ─────────────────────────────────────────────────────────────────
   FILE I/O OPERATIONS
   ───────────────────────────────────────────────────────────────── */

int    nat_file_write(const char *path, const char *text);
int    nat_file_write_line(const char *path, const char *text, int line_no);
int    nat_file_append(const char *path, const char *text);
int    nat_file_insert(const char *path, const char *text, int line_no);
int    nat_file_remove_line(const char *path, int line_no);
int    nat_file_read_line(const char *path, int line_no,
                          char *out, int sz);
void   nat_file_read_all(const char *path);   /* prints with line N | */
int    nat_file_exists(const char *path);
int    nat_file_delete(const char *path);

/* ─────────────────────────────────────────────────────────────────
   COMPARISON HELPERS
   ───────────────────────────────────────────────────────────────── */

/* NAT intentional: == and != use fmt_num rounded comparison
   so that 0.1+0.2 == 0.3 is TRUE — by design, not accident */
int    nat_num_equal(double a, double b);
int    nat_num_notequal(double a, double b);

#endif /* NAT_RUNTIME_H */
