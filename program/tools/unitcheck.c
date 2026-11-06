/* uc_unitcheck [--variant NAME]... [--gens N] [--seeds K] instance.json
 *
 * Tests that formulation T's strengthened inequalities cut off no feasible schedule
 * and admit no infeasible one. For a single generator, with no system rows, it
 * optimizes the same random objective over T and over the basic physical model and
 * compares the integer optima; any difference is a bug in one of the two. */
#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double rnd(unsigned *state) {
    /* xorshift32, so results do not depend on the C library */
    unsigned x = *state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    *state = x;
    return 2.0 * ((double)x / 4294967296.0) - 1.0;
}

static int solve_one(GRBenv *env, const Instance *in, ModelOptions o, int gi, unsigned seed, double *val) {
    GRBmodel *model = NULL;
    VarMap *m = NULL;
    o.only_gen = gi;
    int err = model_build(env, in, &o, &model, &m);
    if (err) return err;
    int nv;
    GRBgetintattr(model, GRB_INT_ATTR_NUMVARS, &nv);
    for (int k = 0; k < nv; k++) GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, k, 0.0);
    const Gen *g = &in->gen[gi];
    const GenVars *x = &m->gen[gi];
    double scale = g->pmax > 1 ? g->pmax : 1;
    unsigned s = seed;
    for (int t = 1; t <= in->T; t++) {
        /* Mostly push output and availability up, which is where a bad bound shows. */
        GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, x->pp[t], rnd(&s) - 0.5);
        GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, x->pa[t], rnd(&s) - 0.5);
        GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, x->u[t], scale * rnd(&s));
        GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, x->v[t], 0.2 * scale * rnd(&s));
        GRBsetdblattrelement(model, GRB_DBL_ATTR_OBJ, x->w[t], 0.2 * scale * rnd(&s));
    }
    err = GRBoptimize(model);
    int status = 0;
    if (!err) err = GRBgetintattr(model, GRB_INT_ATTR_STATUS, &status);
    if (!err) {
        if (status == GRB_OPTIMAL) err = GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, val);
        else *val = NAN;
    }
    GRBfreemodel(model);
    varmap_free(m);
    return err;
}

int main(int argc, char **argv) {
    ModelOptions opt;
    model_default_options(&opt);
    int ngens = 0, nseeds = 3;
    const char *path = NULL;
    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--variant") && a + 1 < argc) {
            if (!model_apply_variant(&opt, argv[++a])) { fprintf(stderr, "unknown variant\n"); return 2; }
        } else if (!strcmp(argv[a], "--gens") && a + 1 < argc) ngens = atoi(argv[++a]);
        else if (!strcmp(argv[a], "--seeds") && a + 1 < argc) nseeds = atoi(argv[++a]);
        else path = argv[a];
    }
    if (!path) { fprintf(stderr, "usage: uc_unitcheck [--variant NAME] [--gens N] [--seeds K] instance.json\n"); return 2; }
    char err[512];
    Instance *in = instance_load(path, err, sizeof err);
    if (!in) { fprintf(stderr, "%s\n", err); return 2; }

    GRBenv *env = NULL;
    if (GRBemptyenv(&env)) return 1;
    GRBsetintparam(env, GRB_INT_PAR_OUTPUTFLAG, 0);
    if (GRBstartenv(env)) { fprintf(stderr, "cannot start Gurobi\n"); return 1; }
    GRBsetintparam(env, GRB_INT_PAR_THREADS, 1);
    GRBsetdblparam(env, GRB_DBL_PAR_MIPGAP, 0.0);
    GRBsetdblparam(env, GRB_DBL_PAR_MIPGAPABS, 0.0);
    GRBsetdblparam(env, GRB_DBL_PAR_INTFEASTOL, 1e-9);
    GRBsetdblparam(env, GRB_DBL_PAR_FEASIBILITYTOL, 1e-9);

    ModelOptions base;
    model_default_options(&base);
    base.base_model = 1;

    int total = 0, tighter = 0, looser = 0, shown = 0;
    int step = ngens > 0 && in->ngen > ngens ? in->ngen / ngens : 1;
    for (int gi = 0; gi < in->ngen; gi += step) {
        for (int k = 1; k <= nseeds; k++) {
            unsigned seed = 2166136261u ^ (unsigned)(gi * 7919 + k * 104729);
            double vt, vb;
            int e1 = solve_one(env, in, opt, gi, seed, &vt);
            int e2 = solve_one(env, in, base, gi, seed, &vb);
            if (e1 || e2) { fprintf(stderr, "Gurobi error %d %d: %s\n", e1, e2, GRBgeterrormsg(env)); return 1; }
            total++;
            /* Minimizing: T above base means T cut off a feasible schedule. */
            double scale = 1 + fabs(vb);
            int infeasible = isnan(vt) != isnan(vb);
            int cut = infeasible || (!isnan(vt) && vt > vb + 1e-6 * scale);
            int loose = !infeasible && !isnan(vt) && vt < vb - 1e-6 * scale;
            if (cut) tighter++;
            if (loose) looser++;
            if ((cut || loose) && shown < 12) {
                const Gen *g = &in->gen[gi];
                printf("%s %s seed %d: T=%.6f base=%.6f UT=%d DT=%d P_=%.3f Pbar=%.3f SU=%.3f SD=%.3f RU=%.3f RD=%.3f on0=%d up0=%d p0=%.3f\n",
                       cut ? "CUT" : "LOOSE", g->name, k, vt, vb, g->ut, g->dt, g->pmin, g->pmax, g->su, g->sd, g->ru, g->rd, g->on0, g->up0, g->p0);
                shown++;
            }
        }
    }
    printf("%s: %d comparisons, T cuts feasible schedules in %d, T admits infeasible schedules in %d\n", path, total, tighter, looser);
    GRBfreeenv(env);
    instance_free(in);
    return (tighter || looser) ? 1 : 0;
}
