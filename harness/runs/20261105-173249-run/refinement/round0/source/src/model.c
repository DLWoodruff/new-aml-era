/* Equation numbers refer to Knueven, Ostrowski, Watson, "On Mixed Integer
 * Programming Formulations for the Unit Commitment Problem". NOTES.md explains
 * each place where the code differs from the printed text. */
#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COEF_EPS 1e-11

static double pos(double x) { return x > 0 ? x : 0; }
static int imin(int a, int b) { return a < b ? a : b; }

typedef struct {
    /* columns */
    int ncol, capcol;
    double *obj, *lb, *ub;
    char *vtype;
    /* rows, in compressed form */
    int nrow, caprow, nnz, capnz;
    int *beg, *ind;
    double *val, *rhs;
    char *sense;
    int relax;
} Builder;

static void *xrealloc(void *p, size_t n) {
    void *q = realloc(p, n ? n : 1);
    if (!q) { fprintf(stderr, "out of memory\n"); exit(1); }
    return q;
}

static int add_col(Builder *b, double obj, double lb, double ub, char type) {
    if (b->ncol == b->capcol) {
        b->capcol = b->capcol ? 2 * b->capcol : 4096;
        b->obj = xrealloc(b->obj, (size_t)b->capcol * sizeof(double));
        b->lb = xrealloc(b->lb, (size_t)b->capcol * sizeof(double));
        b->ub = xrealloc(b->ub, (size_t)b->capcol * sizeof(double));
        b->vtype = xrealloc(b->vtype, (size_t)b->capcol);
    }
    b->obj[b->ncol] = obj;
    b->lb[b->ncol] = lb;
    b->ub[b->ncol] = ub;
    b->vtype[b->ncol] = type;
    return b->ncol++;
}

static int add_binary(Builder *b, double obj, double lb, double ub) {
    return add_col(b, obj, lb, ub, b->relax ? GRB_CONTINUOUS : GRB_BINARY);
}

static int add_cont(Builder *b, double obj) {
    return add_col(b, obj, 0.0, GRB_INFINITY, GRB_CONTINUOUS);
}

static void row_begin(Builder *b) {
    if (b->nrow == b->caprow) {
        b->caprow = b->caprow ? 2 * b->caprow : 4096;
        b->beg = xrealloc(b->beg, (size_t)b->caprow * sizeof(int));
        b->rhs = xrealloc(b->rhs, (size_t)b->caprow * sizeof(double));
        b->sense = xrealloc(b->sense, (size_t)b->caprow);
    }
    b->beg[b->nrow] = b->nnz;
}

static void row_add(Builder *b, int col, double coef) {
    if (fabs(coef) < COEF_EPS) return;
    if (b->nnz == b->capnz) {
        b->capnz = b->capnz ? 2 * b->capnz : 65536;
        b->ind = xrealloc(b->ind, (size_t)b->capnz * sizeof(int));
        b->val = xrealloc(b->val, (size_t)b->capnz * sizeof(double));
    }
    b->ind[b->nnz] = col;
    b->val[b->nnz] = coef;
    b->nnz++;
}

static void row_end(Builder *b, char sense, double rhs) {
    b->sense[b->nrow] = sense;
    b->rhs[b->nrow] = rhs;
    b->nrow++;
}

static int *new_index(int T) {
    int *a = xrealloc(NULL, (size_t)(T + 2) * sizeof(int));
    for (int t = 0; t < T + 2; t++) a[t] = -1;
    return a;
}

/* Everything needed to write coef*v(tau) or coef*w(tau) for any tau: inside the
 * horizon it is a variable, before it a known value, after it zero (Section 3). */
typedef struct {
    Builder *b;
    const Gen *g;
    const GenVars *x;
    int T, no_hist_v;
    double rhs;   /* accumulates the constants of the current <= row */
} RowCtx;

static void add_v(RowCtx *c, int tau, double coef) {
    if (tau > c->T) return;
    if (tau >= 1) row_add(c->b, c->x->v[tau], coef);
    else if (!c->no_hist_v && gen_hist_v(c->g, tau)) c->rhs -= coef;
}

static void add_w(RowCtx *c, int tau, double coef) {
    if (tau > c->T) return;
    if (tau >= 1) row_add(c->b, c->x->w[tau], coef);
    else if (gen_hist_w(c->g, tau)) c->rhs -= coef;
}

/* First period with a row (4) or (5), given the minimum time. */
static int updown_first(int min_time, int T, UpDownMode mode) {
    if (mode == UPDOWN_ALL_PERIODS) return 1;
    if (mode == UPDOWN_PRINTED) return min_time;
    return imin(min_time, T);
}

/* (2), (3) as bounds on u, (4), (5). */
static void add_updown(Builder *b, const Gen *g, const GenVars *x, int T, UpDownMode mode) {
    for (int t = 1; t <= T; t++) {
        row_begin(b);
        row_add(b, x->u[t], 1.0);
        if (t > 1) row_add(b, x->u[t - 1], -1.0);
        row_add(b, x->v[t], -1.0);
        row_add(b, x->w[t], 1.0);
        row_end(b, GRB_EQUAL, t == 1 ? (double)g->on0 : 0.0);
    }
    for (int t = updown_first(g->ut, T, mode); t <= T; t++) {
        double hist = 0;
        row_begin(b);
        for (int i = t - g->ut + 1; i <= t; i++) {
            if (i >= 1) row_add(b, x->v[i], 1.0);
            else if (gen_hist_v(g, i)) hist += 1.0;
        }
        row_add(b, x->u[t], -1.0);
        row_end(b, GRB_LESS_EQUAL, -hist);
    }
    for (int t = updown_first(g->dt, T, mode); t <= T; t++) {
        double hist = 0;
        row_begin(b);
        for (int i = t - g->dt + 1; i <= t; i++) {
            if (i >= 1) row_add(b, x->w[i], 1.0);
            else if (gen_hist_w(g, i)) hist += 1.0;
        }
        row_add(b, x->u[t], 1.0);
        row_end(b, GRB_LESS_EQUAL, 1.0 - hist);
    }
}

/* (17), (23), (38), (40), (41), stated in the p', pbar' variables by (12)-(15). */
static void add_generation_limits(Builder *b, const Gen *g, const GenVars *x, int T, const ModelOptions *o) {
    const double range = g->pmax - g->pmin;
    for (int t = 1; t <= T; t++) {
        RowCtx c = { b, g, x, T, o->no_hist_v, 0.0 };

        /* (17) */
        row_begin(b);
        row_add(b, x->pp[t], 1.0);
        row_add(b, x->pa[t], -1.0);
        row_end(b, GRB_LESS_EQUAL, 0.0);

        if (g->ut == 1) {
            /* (23a) */
            row_begin(b);
            c.rhs = 0;
            row_add(b, x->pa[t], 1.0);
            row_add(b, x->u[t], -range);
            add_v(&c, t, g->pmax - g->su);
            add_w(&c, t + 1, pos(g->su - g->sd));
            row_end(b, GRB_LESS_EQUAL, c.rhs);
            /* (23b) */
            row_begin(b);
            c.rhs = 0;
            row_add(b, x->pa[t], 1.0);
            row_add(b, x->u[t], -range);
            add_w(&c, t + 1, g->pmax - g->sd);
            add_v(&c, t, pos(g->sd - g->su));
            row_end(b, GRB_LESS_EQUAL, c.rhs);
        }

        /* (38) */
        row_begin(b);
        c.rhs = 0;
        row_add(b, x->pa[t], 1.0);
        row_add(b, x->u[t], -range);
        add_w(&c, t + 1, g->pmax - g->sd);
        for (int i = 0; i <= imin(g->ut - 2, g->tru); i++)
            add_v(&c, t - i, g->pmax - g->su - i * g->ru);
        row_end(b, GRB_LESS_EQUAL, c.rhs);

        /* (40) */
        if (g->tru > g->ut - 2) {
            row_begin(b);
            c.rhs = 0;
            row_add(b, x->pa[t], 1.0);
            row_add(b, x->u[t], -range);
            for (int i = 0; i <= imin(g->ut - 1, g->tru); i++)
                add_v(&c, t - i, g->pmax - g->su - i * g->ru);
            row_end(b, GRB_LESS_EQUAL, c.rhs);
        }

        /* (41): K^SD from (37); the printed K^SU refers to itself and is read with
         * [K^SD]^+ (NOTES.md). The left side is p', not pbar'. */
        int ksd = imin(imin(g->trd, g->ut - 1), T - t - 1);
        if (ksd > 0) {
            int ksu = imin(imin(g->tru, g->ut - 2 - ksd), t - 1);
            row_begin(b);
            c.rhs = 0;
            row_add(b, x->pp[t], 1.0);
            row_add(b, x->u[t], -range);
            for (int i = 0; i <= ksd; i++)
                add_w(&c, t + 1 + i, g->pmax - g->sd - i * g->rd);
            for (int i = 0; i <= ksu; i++)
                add_v(&c, t - i, g->pmax - g->su - i * g->ru);
            if (ksu < g->tru) {
                double a = g->su + (ksu + 1) * g->ru;
                double coef = 0;
                if (o->tail == TAIL_PRINTED) coef = pos(a - (g->sd + g->trd * g->rd));
                else if (o->tail == TAIL_CORRECTED) coef = pos((g->sd + ksd * g->rd) - a);
                add_v(&c, t - (ksu + 1), coef);
            }
            row_end(b, GRB_LESS_EQUAL, c.rhs);
        }
    }
}

/* (35), (36). */
static void add_ramps(Builder *b, const Gen *g, const GenVars *x, int T) {
    const double pp0 = g->on0 ? g->p0 - g->pmin : 0.0;
    for (int t = 1; t <= T; t++) {
        row_begin(b);
        row_add(b, x->pa[t], 1.0);
        if (t > 1) row_add(b, x->pp[t - 1], -1.0);
        row_add(b, x->v[t], -(g->su - g->pmin - g->ru));
        row_add(b, x->u[t], -g->ru);
        row_end(b, GRB_LESS_EQUAL, t == 1 ? pp0 : 0.0);

        double rhs = 0;
        row_begin(b);
        if (t > 1) {
            row_add(b, x->pp[t - 1], 1.0);
            row_add(b, x->u[t - 1], -g->rd);
        } else {
            rhs = g->rd * g->on0 - pp0;
        }
        row_add(b, x->pp[t], -1.0);
        row_add(b, x->w[t], -(g->sd - g->pmin - g->rd));
        row_end(b, GRB_LESS_EQUAL, rhs);
    }
}

/* C^v(l) or C^w(l) of (46): limit is SU or SD. */
static double seg_coef(const Gen *g, int l, double limit) {
    if (g->pw[l] <= limit) return 0.0;
    if (g->pw[l - 1] < limit) return g->pw[l] - limit;
    return g->pw[l] - g->pw[l - 1];
}

/* (43), (46) or (48); (44) is substituted into the objective. */
static void add_piecewise(Builder *b, const Gen *g, const GenVars *x, int **pl, int T, const ModelOptions *o) {
    for (int t = 1; t <= T; t++) {
        RowCtx c = { b, g, x, T, o->no_hist_v, 0.0 };
        row_begin(b);
        for (int l = 1; l <= g->nseg; l++) row_add(b, pl[l][t], 1.0);
        row_add(b, x->pp[t], -1.0);
        row_end(b, GRB_EQUAL, 0.0);

        for (int l = 1; l <= g->nseg; l++) {
            double d = g->pw[l] - g->pw[l - 1];
            double cv = seg_coef(g, l, g->su), cw = seg_coef(g, l, g->sd);
            if (g->ut > 1) {
                /* (46) */
                row_begin(b);
                row_add(b, pl[l][t], 1.0);
                row_add(b, x->u[t], -d);
                add_v(&c, t, cv);
                add_w(&c, t + 1, cw);
                row_end(b, GRB_LESS_EQUAL, 0.0);
            } else {
                /* (48). The printed operands of [.]^+ are reversed relative to (23);
                 * see NOTES.md. */
                double to_w = o->pw48_printed ? pos(cv - cw) : pos(cw - cv);
                double to_v = o->pw48_printed ? pos(cw - cv) : pos(cv - cw);
                row_begin(b);
                row_add(b, pl[l][t], 1.0);
                row_add(b, x->u[t], -d);
                add_v(&c, t, cv);
                add_w(&c, t + 1, to_w);
                row_end(b, GRB_LESS_EQUAL, 0.0);
                row_begin(b);
                row_add(b, pl[l][t], 1.0);
                row_add(b, x->u[t], -d);
                add_w(&c, t + 1, cw);
                add_v(&c, t, to_v);
                row_end(b, GRB_LESS_EQUAL, 0.0);
            }
        }
    }
}

/* A basic physical model built from (18), (19), (24), (25), (43), (42): the same
 * integer feasible set as T, with none of T's strengthening. Used only to test T. */
static void add_base_physical(Builder *b, const Gen *g, const GenVars *x, int **pl, int T) {
    const double range = g->pmax - g->pmin, pp0 = g->on0 ? g->p0 - g->pmin : 0.0;
    for (int t = 1; t <= T; t++) {
        RowCtx c = { b, g, x, T, 1, 0.0 };
        row_begin(b);
        row_add(b, x->pp[t], 1.0);
        row_add(b, x->pa[t], -1.0);
        row_end(b, GRB_LESS_EQUAL, 0.0);
        row_begin(b);
        row_add(b, x->pa[t], 1.0);
        row_add(b, x->u[t], -range);
        add_w(&c, t + 1, g->pmax - g->sd);
        row_end(b, GRB_LESS_EQUAL, 0.0);

        /* (24) */
        double rhs = 0;
        row_begin(b);
        row_add(b, x->pa[t], 1.0);
        row_add(b, x->u[t], g->pmin);
        row_add(b, x->v[t], -g->su);
        if (t > 1) {
            row_add(b, x->pp[t - 1], -1.0);
            row_add(b, x->u[t - 1], -g->pmin - g->ru);
        } else {
            rhs = pp0 + g->on0 * (g->pmin + g->ru);
        }
        row_end(b, GRB_LESS_EQUAL, rhs);
        /* (25) */
        rhs = 0;
        row_begin(b);
        if (t > 1) {
            row_add(b, x->pp[t - 1], 1.0);
            row_add(b, x->u[t - 1], g->pmin);
        } else {
            rhs = -pp0 - g->on0 * g->pmin;
        }
        row_add(b, x->pp[t], -1.0);
        row_add(b, x->u[t], -g->pmin - g->rd);
        row_add(b, x->w[t], -g->sd);
        row_end(b, GRB_LESS_EQUAL, rhs);

        row_begin(b);
        for (int l = 1; l <= g->nseg; l++) row_add(b, pl[l][t], 1.0);
        row_add(b, x->pp[t], -1.0);
        row_end(b, GRB_EQUAL, 0.0);
        for (int l = 1; l <= g->nseg; l++) {
            row_begin(b);
            row_add(b, pl[l][t], 1.0);
            row_add(b, x->u[t], -(g->pw[l] - g->pw[l - 1]));
            row_end(b, GRB_LESS_EQUAL, 0.0);
        }
    }
}

/* Variables x(t',t) of (59)-(61), for shut-down at t' and start-up at t with
 * DT <= t-t' <= TC-1, and the rows (59), (60). Their objective coefficients are
 * those of (61) after substitution into (69). Only one historical shut-down
 * exists: t' = 1-down0 for a unit that is off at period 0. */
static void add_startup(Builder *b, const Gen *g, const GenVars *x, int T) {
    const int S = g->ncat;
    if (S < 2 || g->tc <= g->dt) return;
    const double cold = g->scost[S - 1];
    int *xh = new_index(T);
    int *xm = xrealloc(NULL, (size_t)(T + 2) * (size_t)(T + 2) * sizeof(int));
    for (int k = 0; k < (T + 2) * (T + 2); k++) xm[k] = -1;

#define XM(tp, t) xm[(tp) * (T + 2) + (t)]
    for (int t = 1; t <= T; t++) {
        for (int tp = t - g->tc + 1; tp <= t - g->dt; tp++) {
            if (tp < 1 && !gen_hist_w(g, tp)) continue;
            int s = 0;
            while (s + 1 < S && g->lag[s + 1] <= t - tp) s++;
            int col = add_binary(b, -(cold - g->scost[s]), 0.0, 1.0);
            if (tp >= 1) XM(tp, t) = col;
            else xh[t] = col;
        }
    }
    /* (59) */
    for (int t = 1; t <= T; t++) {
        int any = 0;
        row_begin(b);
        for (int tp = 1; tp < t; tp++)
            if (XM(tp, t) >= 0) { row_add(b, XM(tp, t), 1.0); any = 1; }
        if (xh[t] >= 0) { row_add(b, xh[t], 1.0); any = 1; }
        if (any) {
            row_add(b, x->v[t], -1.0);
            row_end(b, GRB_LESS_EQUAL, 0.0);
        } else {
            b->nnz = b->beg[b->nrow];
        }
    }
    /* (60) */
    for (int tp = 1; tp <= T; tp++) {
        int any = 0;
        row_begin(b);
        for (int t = tp + 1; t <= T; t++)
            if (XM(tp, t) >= 0) { row_add(b, XM(tp, t), 1.0); any = 1; }
        if (any) {
            row_add(b, x->w[tp], -1.0);
            row_end(b, GRB_LESS_EQUAL, 0.0);
        } else {
            b->nnz = b->beg[b->nrow];
        }
    }
    /* (60) at the historical shut-down, where w = 1 */
    int any = 0;
    row_begin(b);
    for (int t = 1; t <= T; t++)
        if (xh[t] >= 0) { row_add(b, xh[t], 1.0); any = 1; }
    if (any) row_end(b, GRB_LESS_EQUAL, 1.0);
    else b->nnz = b->beg[b->nrow];
#undef XM
    free(xm);
    free(xh);
}

void model_default_options(ModelOptions *o) {
    memset(o, 0, sizeof *o);
    o->tail = TAIL_CORRECTED;
    o->only_gen = -1;
}

int model_apply_variant(ModelOptions *o, const char *name) {
    if (!strcmp(name, "tail-none")) o->tail = TAIL_NONE;
    else if (!strcmp(name, "tail-printed")) o->tail = TAIL_PRINTED;
    else if (!strcmp(name, "tail-corrected")) o->tail = TAIL_CORRECTED;
    else if (!strcmp(name, "pw48-printed")) o->pw48_printed = 1;
    else if (!strcmp(name, "pw48-corrected")) o->pw48_printed = 0;
    else if (!strcmp(name, "reserve-printed")) o->reserve_printed = 1;
    else if (!strcmp(name, "reserve-renewables")) o->reserve_printed = 0;
    else if (!strcmp(name, "updown-clamped")) o->updown = UPDOWN_CLAMPED;
    else if (!strcmp(name, "updown-printed")) o->updown = UPDOWN_PRINTED;
    else if (!strcmp(name, "updown-all-periods")) o->updown = UPDOWN_ALL_PERIODS;
    else if (!strcmp(name, "no-hist-v")) o->no_hist_v = 1;
    else if (!strcmp(name, "base")) o->base_model = 1;
    else return 0;
    return 1;
}

int model_build(GRBenv *env, const Instance *in, const ModelOptions *o,
                GRBmodel **model_out, VarMap **map_out) {
    const int T = in->T;
    Builder bb, *b = &bb;
    memset(b, 0, sizeof *b);
    b->relax = o->relax;

    VarMap *m = xrealloc(NULL, sizeof *m);
    memset(m, 0, sizeof *m);
    m->T = T;
    m->ngen = in->ngen;
    m->gen = xrealloc(NULL, (size_t)in->ngen * sizeof *m->gen);
    memset(m->gen, 0, (size_t)in->ngen * sizeof *m->gen);

    for (int gi = 0; gi < in->ngen; gi++) {
        if (o->only_gen >= 0 && gi != o->only_gen) continue;
        const Gen *g = &in->gen[gi];
        GenVars *x = &m->gen[gi];
        x->u = new_index(T);
        x->v = new_index(T);
        x->w = new_index(T);
        x->pp = new_index(T);
        x->pa = new_index(T);
        int **pl = xrealloc(NULL, (size_t)(g->nseg + 1) * sizeof *pl);
        for (int l = 1; l <= g->nseg; l++) pl[l] = new_index(T);

        for (int t = 1; t <= T; t++) {
            double lb = 0.0, ub = 1.0;
            if (t <= imin(g->need_up, T)) lb = 1.0;          /* (3a) */
            if (t <= imin(g->need_down, T)) ub = 0.0;        /* (3b) */
            if (g->must_run) lb = 1.0;
            x->u[t] = add_binary(b, g->cr, lb, ub);
            x->v[t] = add_binary(b, g->scost[g->ncat - 1], 0.0, 1.0);
            x->w[t] = add_binary(b, 0.0, 0.0, 1.0);          /* C^w = 0 in (62) */
            x->pp[t] = add_cont(b, 0.0);
            x->pa[t] = add_cont(b, 0.0);
            for (int l = 1; l <= g->nseg; l++) pl[l][t] = add_cont(b, g->slope[l]);
        }

        add_updown(b, g, x, T, o->base_model ? UPDOWN_ALL_PERIODS : o->updown);
        if (o->base_model) {
            add_base_physical(b, g, x, pl, T);
        } else {
            add_generation_limits(b, g, x, T, o);
            add_ramps(b, g, x, T);
            add_piecewise(b, g, x, pl, T, o);
        }
        add_startup(b, g, x, T);

        for (int l = 1; l <= g->nseg; l++) free(pl[l]);
        free(pl);
    }

    if (o->only_gen < 0) {
        m->pw = new_index(T);
        m->splus = new_index(T);
        m->sminus = new_index(T);
        m->sr = new_index(T);
        for (int t = 1; t <= T; t++) {
            m->pw[t] = add_col(b, 0.0, in->ren_min[t], in->ren_max[t], GRB_CONTINUOUS);  /* (63) */
            m->splus[t] = add_cont(b, UC_C_LP);    /* (66) */
            m->sminus[t] = add_cont(b, UC_C_LP);
            m->sr[t] = add_cont(b, UC_C_RP);
        }
        for (int t = 1; t <= T; t++) {
            /* (65) with one bus: (64) is dropped and the flow sums are empty. */
            row_begin(b);
            for (int gi = 0; gi < in->ngen; gi++) {
                row_add(b, m->gen[gi].pp[t], 1.0);
                row_add(b, m->gen[gi].u[t], in->gen[gi].pmin);
            }
            row_add(b, m->pw[t], 1.0);
            row_add(b, m->splus[t], 1.0);
            row_add(b, m->sminus[t], -1.0);
            row_end(b, GRB_EQUAL, in->demand[t]);

            /* (67), by default with the renewable term (NOTES.md). */
            row_begin(b);
            for (int gi = 0; gi < in->ngen; gi++) {
                row_add(b, m->gen[gi].pa[t], 1.0);
                row_add(b, m->gen[gi].u[t], in->gen[gi].pmin);
            }
            if (!o->reserve_printed) row_add(b, m->pw[t], 1.0);
            row_add(b, m->sr[t], 1.0);
            row_end(b, GRB_GREATER_EQUAL, in->demand[t] + in->reserve[t]);
        }
    }

    GRBmodel *model = NULL;
    int err = GRBnewmodel(env, &model, "uc_T", b->ncol, b->obj, b->lb, b->ub, b->vtype, NULL);
    if (!err) err = GRBsetintattr(model, GRB_INT_ATTR_MODELSENSE, GRB_MINIMIZE);
    if (!err) err = GRBaddconstrs(model, b->nrow, b->nnz, b->beg, b->ind, b->val, b->sense, b->rhs, NULL);
    if (!err) err = GRBupdatemodel(model);
    m->numvars = b->ncol;

    free(b->obj); free(b->lb); free(b->ub); free(b->vtype);
    free(b->beg); free(b->ind); free(b->val); free(b->rhs); free(b->sense);

    if (err) {
        if (model) GRBfreemodel(model);
        varmap_free(m);
        return err;
    }
    *model_out = model;
    *map_out = m;
    return 0;
}

void varmap_free(VarMap *m) {
    if (!m) return;
    for (int g = 0; g < m->ngen; g++) {
        free(m->gen[g].u); free(m->gen[g].v); free(m->gen[g].w);
        free(m->gen[g].pp); free(m->gen[g].pa);
    }
    free(m->gen);
    free(m->pw); free(m->splus); free(m->sminus); free(m->sr);
    free(m);
}
