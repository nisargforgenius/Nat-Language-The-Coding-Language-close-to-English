/* ═══════════════════════════════════════════════════════════════════
   nat_runtime.c — NAT Language Runtime Library Implementation
   v4.0 Phase 3
   ═══════════════════════════════════════════════════════════════════ */

#include "nat_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

/* ─────────────────────────────────────────────────────────────────
   CORE UTILITIES
   ───────────────────────────────────────────────────────────────── */

void nat_fmt_num(char *out, int sz, double v) {
    long long iv = (long long)v;
    if ((double)iv == v)
        snprintf(out, sz, "%lld", iv);
    else
        snprintf(out, sz, "%.10g", v);
}

int nat_is_number(const char *s) {
    return is_number(s);
}

double nat_var_as_num(const Variable *v) {
    return VAR_NUM(v);
}

void nat_var_as_str(const Variable *v, char *out, int sz) {
    var_to_str(v, out, sz);
}

void nat_var_set_str(Variable *v, const char *s) {
    cache_numeric(v, s);
}

void nat_var_set_num(Variable *v, double d) {
    v->type  = VAL_NUM;
    v->num   = d;
    v->str[0] = '\0';
}

/* ─────────────────────────────────────────────────────────────────
   COMPARISON HELPERS
   ───────────────────────────────────────────────────────────────── */

int nat_num_equal(double a, double b) {
    /* NAT intentional: format both sides then strcmp
       so 0.1+0.2 == 0.3 is TRUE by design */
    char sa[64]={0}, sb[64]={0};
    nat_fmt_num(sa, sizeof(sa), a);
    nat_fmt_num(sb, sizeof(sb), b);
    return strcmp(sa, sb) == 0;
}

int nat_num_notequal(double a, double b) {
    return !nat_num_equal(a, b);
}

/* ─────────────────────────────────────────────────────────────────
   MATH OPERATIONS
   ───────────────────────────────────────────────────────────────── */

double nat_abs(double v)          { return fabs(v); }
double nat_round(double v)        { return round(v); }
double nat_floor_val(double v)    { return floor(v); }
double nat_ceil_val(double v)     { return ceil(v); }
double nat_clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}
double nat_max2(double a, double b) { return a > b ? a : b; }
double nat_min2(double a, double b) { return a < b ? a : b; }
int    nat_is_even(double v)  { return ((long long)v % 2) == 0; }
int    nat_is_odd(double v)   { return ((long long)v % 2) != 0; }

double nat_larger(double *vals, int count) {
    if (count <= 0) return 0.0;
    double best = vals[0];
    for (int i = 1; i < count; i++)
        if (vals[i] > best) best = vals[i];
    return best;
}

double nat_smallest(double *vals, int count) {
    if (count <= 0) return 0.0;
    double best = vals[0];
    for (int i = 1; i < count; i++)
        if (vals[i] < best) best = vals[i];
    return best;
}

/* ─────────────────────────────────────────────────────────────────
   STRING OPERATIONS
   ───────────────────────────────────────────────────────────────── */

void nat_upper(char *out, int sz, const char *s) {
    int i;
    for (i = 0; s[i] && i < sz-1; i++)
        out[i] = (char)toupper((unsigned char)s[i]);
    out[i] = '\0';
}

void nat_lower(char *out, int sz, const char *s) {
    int i;
    for (i = 0; s[i] && i < sz-1; i++)
        out[i] = (char)tolower((unsigned char)s[i]);
    out[i] = '\0';
}

void nat_trim(char *out, int sz, const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    const char *end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t')) end--;
    int len = (int)(end - s + 1);
    if (len < 0) len = 0;
    if (len >= sz) len = sz - 1;
    strncpy(out, s, len);
    out[len] = '\0';
}

void nat_replace(char *out, int sz,
                 const char *haystack,
                 const char *needle,
                 const char *replacement) {
    int nlen = (int)strlen(needle);
    int rlen = (int)strlen(replacement);
    int oi = 0;
    const char *p = haystack;
    while (*p && oi < sz-1) {
        if (strncmp(p, needle, nlen) == 0) {
            for (int i = 0; i < rlen && oi < sz-1; i++)
                out[oi++] = replacement[i];
            p += nlen;
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = '\0';
}

int nat_contains(const char *haystack, const char *needle) {
    return strstr(haystack, needle) != NULL;
}

int nat_str_length(const char *s) {
    return (int)strlen(s);
}

void nat_str_repeat(char *out, int sz, const char *s, int n) {
    int oi = 0, slen = (int)strlen(s);
    for (int i = 0; i < n && oi < sz-1; i++)
        for (int j = 0; j < slen && oi < sz-1; j++)
            out[oi++] = s[j];
    out[oi] = '\0';
}

void nat_reverse_str(char *out, int sz, const char *s) {
    int len = (int)strlen(s);
    if (len >= sz) len = sz - 1;
    for (int i = 0; i < len; i++)
        out[i] = s[len - 1 - i];
    out[len] = '\0';
}

void nat_first_n_chars(char *out, int sz, const char *s, int n) {
    int len = (int)strlen(s);
    if (n > len) n = len;
    if (n >= sz) n = sz - 1;
    strncpy(out, s, n);
    out[n] = '\0';
}

void nat_last_n_chars(char *out, int sz, const char *s, int n) {
    int len = (int)strlen(s);
    if (n > len) n = len;
    const char *start = s + (len - n);
    strncpy(out, start, sz-1);
    out[sz-1] = '\0';
}

int nat_is_num_str(const char *s)  { return is_number(s); }
int nat_is_text_str(const char *s) { return !is_number(s); }

void nat_text_of(char *out, int sz, double v) {
    nat_fmt_num(out, sz, v);
}

/* ─────────────────────────────────────────────────────────────────
   ARRAY OPERATIONS
   ───────────────────────────────────────────────────────────────── */

void nat_array_first(char *out, int sz, Variable *v) {
    if (!v->is_array || v->arr_len == 0) { out[0]='\0'; return; }
    strncpy(out, v->arr[0], sz-1);
}

void nat_array_last(char *out, int sz, Variable *v) {
    if (!v->is_array || v->arr_len == 0) { out[0]='\0'; return; }
    strncpy(out, v->arr[v->arr_len-1], sz-1);
}

int nat_array_len(Variable *v) {
    return v->is_array ? v->arr_len : 0;
}

void nat_array_push(Variable *v, const char *val) {
    if (!v->is_array || v->arr_len >= MAX_ARRAY_ELEM) return;
    strncpy(v->arr[v->arr_len], val, MAX_STR-1);
    v->arr_len++;
}

void nat_array_pop(Variable *v) {
    if (!v->is_array || v->arr_len <= 0) return;
    v->arr[v->arr_len-1][0] = '\0';
    v->arr_len--;
}

/* ─────────────────────────────────────────────────────────────────
   FILE I/O OPERATIONS
   ───────────────────────────────────────────────────────────────── */

int nat_file_write(const char *path, const char *text) {
    FILE *fp = fopen(path, "wb");
    if (!fp) return 0;
    fprintf(fp, "%s\n", text);
    fclose(fp);
    return 1;
}

int nat_file_write_line(const char *path, const char *text, int line_no) {
    if (line_no < 1) return 0;
    FILE *fp = fopen(path, "rb");
    char *lines[4096]; int lc = 0;
    char buf[MAX_STR];
    if (fp) {
        while (fgets(buf, MAX_STR, fp) && lc < 4096) {
            int bl = strlen(buf);
            while (bl > 0 && (buf[bl-1]=='\n'||buf[bl-1]=='\r')) buf[--bl]='\0';
            lines[lc++] = strdup(buf);
        }
        fclose(fp);
    }
    while (lc < line_no) lines[lc++] = strdup("");
    free(lines[line_no-1]);
    lines[line_no-1] = strdup(text);
    fp = fopen(path, "wb");
    if (!fp) { for(int i=0;i<lc;i++) free(lines[i]); return 0; }
    for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
    fclose(fp);
    for (int i = 0; i < lc; i++) free(lines[i]);
    return 1;
}

int nat_file_append(const char *path, const char *text) {
    FILE *fp = fopen(path, "ab");
    if (!fp) return 0;
    fprintf(fp, "%s\n", text);
    fclose(fp);
    return 1;
}

int nat_file_insert(const char *path, const char *text, int line_no) {
    if (line_no < 1) return 0;
    FILE *fp = fopen(path, "rb");
    char *lines[4096]; int lc = 0;
    char buf[MAX_STR];
    if (fp) {
        while (fgets(buf, MAX_STR, fp) && lc < 4095) {
            int bl = strlen(buf);
            while (bl > 0 && (buf[bl-1]=='\n'||buf[bl-1]=='\r')) buf[--bl]='\0';
            lines[lc++] = strdup(buf);
        }
        fclose(fp);
    }
    if (lc < 4095) {
        for (int i = lc; i >= line_no; i--) lines[i] = lines[i-1];
        lines[line_no-1] = strdup(text);
        lc++;
    }
    fp = fopen(path, "wb");
    if (!fp) { for(int i=0;i<lc;i++) free(lines[i]); return 0; }
    for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
    fclose(fp);
    for (int i = 0; i < lc; i++) free(lines[i]);
    return 1;
}

int nat_file_remove_line(const char *path, int line_no) {
    if (line_no < 1) return 0;
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    char *lines[4096]; int lc = 0;
    char buf[MAX_STR];
    while (fgets(buf, MAX_STR, fp) && lc < 4096) {
        int bl = strlen(buf);
        while (bl > 0 && (buf[bl-1]=='\n'||buf[bl-1]=='\r')) buf[--bl]='\0';
        lines[lc++] = strdup(buf);
    }
    fclose(fp);
    if (line_no > lc) { for(int i=0;i<lc;i++) free(lines[i]); return 0; }
    free(lines[line_no-1]);
    for (int i = line_no-1; i < lc-1; i++) lines[i] = lines[i+1];
    lc--;
    fp = fopen(path, "wb");
    if (!fp) { for(int i=0;i<lc;i++) free(lines[i]); return 0; }
    for (int i = 0; i < lc; i++) fprintf(fp, "%s\n", lines[i]);
    fclose(fp);
    for (int i = 0; i < lc; i++) free(lines[i]);
    return 1;
}

int nat_file_read_line(const char *path, int line_no, char *out, int sz) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return 0;
    char buf[MAX_STR];
    int ln = 1;
    while (fgets(buf, MAX_STR, fp)) {
        int bl = strlen(buf);
        while (bl > 0 && (buf[bl-1]=='\n'||buf[bl-1]=='\r')) buf[--bl]='\0';
        if (ln == line_no) {
            strncpy(out, buf, sz-1);
            fclose(fp);
            return 1;
        }
        ln++;
    }
    fclose(fp);
    return 0;
}

void nat_file_read_all(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return;
    char buf[MAX_STR];
    int ln = 1;
    while (fgets(buf, MAX_STR, fp)) {
        int bl = strlen(buf);
        while (bl > 0 && (buf[bl-1]=='\n'||buf[bl-1]=='\r')) buf[--bl]='\0';
        if (buf[0] == '\0' && feof(fp)) break;
        printf("line %d | %s\n", ln++, buf);
    }
    fclose(fp);
}

int nat_file_exists(const char *path) {
    FILE *fp = fopen(path, "rb");
    if (fp) { fclose(fp); return 1; }
    return 0;
}

int nat_file_delete(const char *path) {
    return remove(path) == 0;
}
