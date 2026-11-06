#include "instance.h"
#include "json.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MW_TOL 1e-9

static void *xcalloc(size_t n, size_t sz) {
    void *p = calloc(n ? n : 1, sz);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

static char *xstrdup(const char *s) {
    char *d = xcalloc(strlen(s) + 1, 1);
    strcpy(d, s);
    return d;
}

static int seterr(char *err, unsigned long len, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, len, fmt, ap);
    va_end(ap);
    return 0;
}

static int get_num(const JsonValue *o, const char *key, const char *ctx, double *out,
                   char *err, unsigned long errlen) {
    const JsonValue *v = json_get(o, key);
    if (!v) return seterr(err, errlen, "%s: missing field '%s'", ctx, key);
    if (v->type != JSON_NUMBER && v->type != JSON_BOOL)
        return seterr(err, errlen, "%s: field '%s' is not a number", ctx, key);
    *out = v->number;
    return 1;
}

static int get_int(const JsonValue *o, const char *key, const char *ctx, int *out,
                   int *n_rounded, char *err, unsigned long errlen) {
    double d;
    if (!get_num(o, key, ctx, &d, err, errlen)) return 0;
    *out = (int)lround(d);
    if (fabs(d - *out) > 1e-9) (*n_rounded)++;
    return 1;
}

static int get_series(const JsonValue *o, const char *key, const char *ctx, int T, double *out,
                      char *err, unsigned long errlen) {
    const JsonValue *v = json_get(o, key);
    if (!v) return seterr(err, errlen, "%s: missing field '%s'", ctx, key);
    if (v->type != JSON_ARRAY || (int)v->count != T)
        return seterr(err, errlen, "%s: field '%s' is not an array of length %d", ctx, key, T);
    for (int t = 0; t < T; t++) {
        if (v->items[t].type != JSON_NUMBER)
            return seterr(err, errlen, "%s: '%s'[%d] is not a number", ctx, key, t);
        out[t] = v->items[t].number;
    }
    return 1;
}

/* Floor of a ratio that is exact up to rounding error in the data. */
static int floor_ratio(double num, double den) {
    return (int)floor(num / den + 1e-9);
}

/* Evaluate the file's cost curve at mw, extending the end segments if needed. */
static double curve_value(const double *mw, const double *cost, int n, double x) {
    if (n == 1) return cost[0];
    int k = 0;
    while (k < n - 2 && x > mw[k + 1]) k++;
    double d = mw[k + 1] - mw[k];
    if (d <= 0) return cost[k];
    return cost[k] + (cost[k + 1] - cost[k]) * (x - mw[k]) / d;
}

static int load_startup(Instance *inst, Gen *g, const JsonValue *jg, char *err, unsigned long errlen) {
    const JsonValue *js = json_get(jg, "startup");
    if (!js || js->type != JSON_ARRAY || js->count == 0)
        return seterr(err, errlen, "%s: 'startup' must be a non-empty array", g->name);
    int n = (int)js->count;
    int *lag = xcalloc((size_t)n, sizeof *lag);
    double *cost = xcalloc((size_t)n, sizeof *cost);
    for (int k = 0; k < n; k++) {
        int r = 0;
        if (!get_int(&js->items[k], "lag", g->name, &lag[k], &r, err, errlen) ||
            !get_num(&js->items[k], "cost", g->name, &cost[k], err, errlen)) {
            free(lag); free(cost);
            return 0;
        }
        inst->n_int_rounded += r;
        if (k > 0 && lag[k] <= lag[k - 1]) inst->n_lag_not_increasing++;
        if (k > 0 && cost[k] < cost[k - 1]) inst->n_scost_decreasing++;
    }
    if (lag[0] < g->dt) inst->n_lag_first_lt_dt++;
    if (lag[0] > g->dt) inst->n_lag_first_gt_dt++;

    /* Paper: T^1 = DT and T^S = TC. An off period shorter than DT cannot occur, so
     * a category whose whole range lies below DT is unreachable and is dropped.
     * The last category with lag <= DT becomes the hottest; if every lag exceeds
     * DT, the first category is extended down to DT. */
    int first = 0;
    for (int k = 0; k < n; k++) if (lag[k] <= g->dt) first = k;
    if (first > 0) inst->n_cat_merged++;
    g->ncat = n - first;
    g->lag = xcalloc((size_t)g->ncat, sizeof *g->lag);
    g->scost = xcalloc((size_t)g->ncat, sizeof *g->scost);
    for (int k = 0; k < g->ncat; k++) {
        g->lag[k] = lag[first + k];
        g->scost[k] = cost[first + k];
    }
    g->lag[0] = g->dt;
    g->tc = g->lag[g->ncat - 1];
    free(lag);
    free(cost);
    return 1;
}

static int load_curve(Instance *inst, Gen *g, const JsonValue *jg, char *err, unsigned long errlen) {
    const JsonValue *jp = json_get(jg, "piecewise_production");
    if (!jp || jp->type != JSON_ARRAY || jp->count == 0)
        return seterr(err, errlen, "%s: 'piecewise_production' must be a non-empty array", g->name);
    int n = (int)jp->count;
    double *mw = xcalloc((size_t)n, sizeof *mw);
    double *cost = xcalloc((size_t)n, sizeof *cost);
    for (int k = 0; k < n; k++) {
        if (!get_num(&jp->items[k], "mw", g->name, &mw[k], err, errlen) ||
            !get_num(&jp->items[k], "cost", g->name, &cost[k], err, errlen)) {
            free(mw); free(cost);
            return 0;
        }
    }
    int dup = 0, nonconvex = 0;
    double prev = -INFINITY;
    for (int k = 1; k < n; k++) {
        double d = mw[k] - mw[k - 1];
        if (d <= MW_TOL) { dup = 1; continue; }
        double s = (cost[k] - cost[k - 1]) / d;
        if (s < prev - 1e-7) nonconvex = 1;
        prev = s;
    }
    if (dup) inst->n_pw_dup++;
    if (nonconvex) inst->n_pw_nonconvex++;
    if (fabs(mw[0] - g->pmin) > MW_TOL) inst->n_pw_first_ne_pmin++;
    if (fabs(mw[n - 1] - g->pmax) > MW_TOL) inst->n_pw_last_ne_pmax++;

    /* Paper: Pbar^0 = P_ and Pbar^l > P_ for every l. Restrict the curve to
     * [Pmin, Pmax], interpolating at the ends and dropping repeated points. */
    g->cr = curve_value(mw, cost, n, g->pmin);
    g->pw = xcalloc((size_t)n + 2, sizeof *g->pw);
    g->slope = xcalloc((size_t)n + 2, sizeof *g->slope);
    g->pw[0] = g->pmin;
    int L = 0;
    double last_cost = g->cr;
    for (int k = 0; k < n; k++) {
        if (mw[k] <= g->pw[L] + MW_TOL) continue;
        if (mw[k] >= g->pmax - MW_TOL) break;
        L++;
        g->pw[L] = mw[k];
        g->slope[L] = (cost[k] - last_cost) / (g->pw[L] - g->pw[L - 1]);
        last_cost = cost[k];
    }
    if (g->pmax > g->pw[L] + MW_TOL) {
        double c = curve_value(mw, cost, n, g->pmax);
        L++;
        g->pw[L] = g->pmax;
        g->slope[L] = (c - last_cost) / (g->pw[L] - g->pw[L - 1]);
    }
    g->nseg = L;
    free(mw);
    free(cost);
    return 1;
}

static int load_gen(Instance *inst, Gen *g, const char *key, const JsonValue *jg,
                    char *err, unsigned long errlen) {
    g->name = xstrdup(key);
    const JsonValue *jn = json_get(jg, "name");
    if (jn && jn->type == JSON_STRING && strcmp(jn->string, key) != 0) inst->n_name_mismatch++;
    if (jg->type != JSON_OBJECT) return seterr(err, errlen, "%s: not an object", key);

    int *r = &inst->n_int_rounded;
    if (!get_int(jg, "must_run", key, &g->must_run, r, err, errlen) ||
        !get_num(jg, "power_output_minimum", key, &g->pmin, err, errlen) ||
        !get_num(jg, "power_output_maximum", key, &g->pmax, err, errlen) ||
        !get_num(jg, "ramp_up_limit", key, &g->ru, err, errlen) ||
        !get_num(jg, "ramp_down_limit", key, &g->rd, err, errlen) ||
        !get_num(jg, "ramp_startup_limit", key, &g->su_raw, err, errlen) ||
        !get_num(jg, "ramp_shutdown_limit", key, &g->sd_raw, err, errlen) ||
        !get_int(jg, "time_up_minimum", key, &g->ut, r, err, errlen) ||
        !get_int(jg, "time_down_minimum", key, &g->dt, r, err, errlen) ||
        !get_int(jg, "unit_on_t0", key, &g->on0, r, err, errlen) ||
        !get_num(jg, "power_output_t0", key, &g->p0, err, errlen) ||
        !get_int(jg, "time_up_t0", key, &g->up0, r, err, errlen) ||
        !get_int(jg, "time_down_t0", key, &g->down0, r, err, errlen))
        return 0;

    if (g->pmax < g->pmin) return seterr(err, errlen, "%s: maximum power is below minimum", key);
    if (g->ru <= 0 || g->rd <= 0) return seterr(err, errlen, "%s: ramp rates must be positive", key);

    /* The formulation has no meaning for UT or DT below one period. */
    if (g->ut < 1) g->ut = 1;
    if (g->dt < 1) g->dt = 1;

    /* A limit above Pbar never binds; clipping keeps Pbar-SU and Pbar-SD nonnegative. */
    g->su = g->su_raw < g->pmax ? g->su_raw : g->pmax;
    g->sd = g->sd_raw < g->pmax ? g->sd_raw : g->pmax;

    g->tru = floor_ratio(g->pmax - g->su, g->ru);
    g->trd = floor_ratio(g->pmax - g->sd, g->rd);

    g->need_up = g->on0 && g->ut > g->up0 ? g->ut - g->up0 : 0;
    g->need_down = !g->on0 && g->dt > g->down0 ? g->dt - g->down0 : 0;

    return load_startup(inst, g, jg, err, errlen) && load_curve(inst, g, jg, err, errlen);
}

Instance *instance_load(const char *path, char *err, unsigned long errlen) {
    JsonValue *root = json_parse_file(path, err, errlen);
    if (!root) return NULL;
    Instance *inst = xcalloc(1, sizeof *inst);
    int ok = 0;

    do {
        if (root->type != JSON_OBJECT) { seterr(err, errlen, "top level is not an object"); break; }
        int r = 0;
        if (!get_int(root, "time_periods", "top level", &inst->T, &r, err, errlen)) break;
        int T = inst->T;
        if (T < 1) { seterr(err, errlen, "time_periods must be positive"); break; }
        inst->demand = xcalloc((size_t)T + 2, sizeof(double));
        inst->reserve = xcalloc((size_t)T + 2, sizeof(double));
        inst->ren_min = xcalloc((size_t)T + 2, sizeof(double));
        inst->ren_max = xcalloc((size_t)T + 2, sizeof(double));
        if (!get_series(root, "demand", "top level", T, inst->demand + 1, err, errlen)) break;
        if (!get_series(root, "reserves", "top level", T, inst->reserve + 1, err, errlen)) break;

        const JsonValue *jr = json_get(root, "renewable_generators");
        if (!jr || jr->type != JSON_OBJECT) { seterr(err, errlen, "missing 'renewable_generators'"); break; }
        inst->nren = (int)jr->count;
        double *lo = xcalloc((size_t)T, sizeof *lo), *hi = xcalloc((size_t)T, sizeof *hi);
        int good = 1;
        for (size_t k = 0; k < jr->count && good; k++) {
            good = get_series(&jr->items[k], "power_output_minimum", jr->keys[k], T, lo, err, errlen) &&
                   get_series(&jr->items[k], "power_output_maximum", jr->keys[k], T, hi, err, errlen);
            for (int t = 0; good && t < T; t++) {
                inst->ren_min[t + 1] += lo[t];
                inst->ren_max[t + 1] += hi[t];
            }
        }
        free(lo);
        free(hi);
        if (!good) break;

        const JsonValue *jt = json_get(root, "thermal_generators");
        if (!jt || jt->type != JSON_OBJECT) { seterr(err, errlen, "missing 'thermal_generators'"); break; }
        inst->ngen = (int)jt->count;
        inst->gen = xcalloc((size_t)inst->ngen, sizeof *inst->gen);
        for (int g = 0; g < inst->ngen && good; g++)
            good = load_gen(inst, &inst->gen[g], jt->keys[g], &jt->items[g], err, errlen);
        if (!good) break;
        ok = 1;
    } while (0);

    json_free(root);
    if (!ok) { instance_free(inst); return NULL; }
    return inst;
}

void instance_free(Instance *inst) {
    if (!inst) return;
    if (inst->gen) {
        for (int g = 0; g < inst->ngen; g++) {
            free(inst->gen[g].name);
            free(inst->gen[g].lag);
            free(inst->gen[g].scost);
            free(inst->gen[g].pw);
            free(inst->gen[g].slope);
        }
    }
    free(inst->gen);
    free(inst->demand);
    free(inst->reserve);
    free(inst->ren_min);
    free(inst->ren_max);
    free(inst);
}

static int warn(FILE *out, const char *what, int count, const char *example) {
    if (count == 0) return 0;
    fprintf(out, "data warning: %s: %d", what, count);
    if (example) fprintf(out, " (e.g. %s)", example);
    fputc('\n', out);
    return 1;
}

int instance_warnings(const Instance *in, FILE *out) {
    enum {
        UT_GT_T, DT_GT_T, SU_LT_PMIN, SD_LT_PMIN, SU_GT_PMAX, SD_GT_PMAX, MUST_OFF, MUST_DOWN,
        P0_LOW, P0_HIGH, ON_NO_UP, ON_DOWN, OFF_P0, OFF_NO_DOWN, OFF_UP,
        HIST_TRAJ, CANNOT_STAY, NKIND
    };
    static const char *label[NKIND] = {
        "generators with UT > T",
        "generators with DT > T",
        "generators with SU below minimum power (cannot start)",
        "generators with SD below minimum power (cannot stop)",
        "generators with SU above maximum power (clipped)",
        "generators with SD above maximum power (clipped)",
        "must-run generators off in period 0",
        "must-run generators required to stay off at the start",
        "generators on in period 0 below minimum power",
        "generators on in period 0 above maximum power",
        "generators on in period 0 with time_up_t0 <= 0",
        "generators on in period 0 with time_down_t0 != 0",
        "generators off in period 0 with nonzero output",
        "generators off in period 0 with time_down_t0 <= 0",
        "generators off in period 0 with time_up_t0 != 0",
        "generators whose p(0) exceeds SU + (time_up_t0-1) RU, contradicting historical v",
        "generators on in period 0 that can neither shut down nor reach minimum power",
    };
    int count[NKIND] = { 0 };
    const char *first[NKIND] = { 0 };
#define FLAG(kind) do { if (!count[kind]++) first[kind] = g->name; } while (0)
    for (int i = 0; i < in->ngen; i++) {
        const Gen *g = &in->gen[i];
        if (g->ut > in->T) FLAG(UT_GT_T);
        if (g->dt > in->T) FLAG(DT_GT_T);
        if (g->su_raw < g->pmin - MW_TOL) FLAG(SU_LT_PMIN);
        if (g->sd_raw < g->pmin - MW_TOL) FLAG(SD_LT_PMIN);
        if (g->su_raw > g->pmax + MW_TOL) FLAG(SU_GT_PMAX);
        if (g->sd_raw > g->pmax + MW_TOL) FLAG(SD_GT_PMAX);
        if (g->must_run && !g->on0) FLAG(MUST_OFF);
        if (g->must_run && g->need_down) FLAG(MUST_DOWN);
        if (g->on0) {
            if (g->p0 < g->pmin - MW_TOL) FLAG(P0_LOW);
            if (g->p0 > g->pmax + MW_TOL) FLAG(P0_HIGH);
            if (g->up0 <= 0) FLAG(ON_NO_UP);
            if (g->down0 != 0) FLAG(ON_DOWN);
            if (g->up0 > 0 && g->p0 > g->su + (g->up0 - 1) * g->ru + 1e-7) FLAG(HIST_TRAJ);
            if (g->p0 > g->sd + MW_TOL && g->p0 + g->ru < g->pmin - MW_TOL) FLAG(CANNOT_STAY);
        } else {
            if (g->p0 != 0) FLAG(OFF_P0);
            if (g->down0 <= 0) FLAG(OFF_NO_DOWN);
            if (g->up0 != 0) FLAG(OFF_UP);
        }
    }
#undef FLAG
    int lines = 0;
    for (int k = 0; k < NKIND; k++) lines += warn(out, label[k], count[k], first[k]);
    lines += warn(out, "generators whose name field differs from their key", in->n_name_mismatch, NULL);
    lines += warn(out, "generators whose first start-up lag is below DT", in->n_lag_first_lt_dt, NULL);
    lines += warn(out, "generators whose first start-up lag is above DT", in->n_lag_first_gt_dt, NULL);
    lines += warn(out, "generators with start-up categories unreachable because of DT", in->n_cat_merged, NULL);
    lines += warn(out, "start-up lags not strictly increasing", in->n_lag_not_increasing, NULL);
    lines += warn(out, "start-up costs that decrease with lag", in->n_scost_decreasing, NULL);
    lines += warn(out, "curves whose first point is not at minimum power", in->n_pw_first_ne_pmin, NULL);
    lines += warn(out, "curves whose last point is not at maximum power", in->n_pw_last_ne_pmax, NULL);
    lines += warn(out, "curves with repeated mw points", in->n_pw_dup, NULL);
    lines += warn(out, "curves that are not convex", in->n_pw_nonconvex, NULL);
    lines += warn(out, "integer fields with non-integer values (rounded)", in->n_int_rounded, NULL);
    return lines;
}

int gen_hist_v(const Gen *g, int tau) {
    return g->on0 && g->up0 > 0 && tau == 1 - g->up0;
}

int gen_hist_w(const Gen *g, int tau) {
    return !g->on0 && g->down0 > 0 && tau == 1 - g->down0;
}
