/* uc_solve [--relax] [--time-limit SECONDS] [--solution FILE] instance.json
 *
 * Two further options are documented in NOTES.md: --log FILE keeps the Gurobi log,
 * and --variant NAME selects a diagnostic departure from the default model. */
#include "model.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
            "usage: uc_solve [--relax] [--time-limit SECONDS] [--solution FILE] "
            "[--log FILE] [--variant NAME] instance.json\n");
}

static void csv_name(FILE *f, const char *s) {
    if (!strpbrk(s, ",\"\r\n")) { fputs(s, f); return; }
    fputc('"', f);
    for (; *s; s++) {
        if (*s == '"') fputc('"', f);
        fputc(*s, f);
    }
    fputc('"', f);
}

static double nonneg(double x) { return x > 0 ? x : 0; }

static int write_solution(const char *path, const Instance *in, const VarMap *m,
                          const double *x, int relax) {
    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot open %s for writing\n", path); return 1; }
    fprintf(f, "generator,period,on,power,available\n");
    for (int gi = 0; gi < in->ngen; gi++) {
        const Gen *g = &in->gen[gi];
        const GenVars *v = &m->gen[gi];
        for (int t = 1; t <= in->T; t++) {
            int on = x[v->u[t]] >= 0.5;
            /* In a MIP solution u is rounded to remove integrality noise. In an LP
             * solution p = p' + P_ u must use the fractional u. */
            double uval = relax ? x[v->u[t]] : (double)on;
            double power = nonneg(x[v->pp[t]]) + g->pmin * uval;
            double avail = nonneg(x[v->pa[t]]) + g->pmin * uval;
            if (avail < power) avail = power;
            csv_name(f, g->name);
            fprintf(f, ",%d,%d,%.6f,%.6f\n", t, on, power, avail);
        }
    }
    for (int t = 1; t <= in->T; t++)
        fprintf(f, "RENEWABLE,%d,,%.6f,\n", t, x[m->pw[t]]);
    if (fclose(f) != 0) { fprintf(stderr, "error writing %s\n", path); return 1; }
    return 0;
}

static const char *status_name(int s) {
    switch (s) {
        case GRB_OPTIMAL: return "OPTIMAL";
        case GRB_INFEASIBLE: return "INFEASIBLE";
        case GRB_INF_OR_UNBD: return "INF_OR_UNBD";
        case GRB_UNBOUNDED: return "UNBOUNDED";
        case GRB_TIME_LIMIT: return "TIME_LIMIT";
        case GRB_NUMERIC: return "NUMERIC";
        case GRB_SUBOPTIMAL: return "SUBOPTIMAL";
        case GRB_INTERRUPTED: return "INTERRUPTED";
        default: return "OTHER";
    }
}

#define CHECK(call)                                                                 \
    do {                                                                            \
        int err_ = (call);                                                          \
        if (err_) {                                                                 \
            fprintf(stderr, "Gurobi error %d at %s:%d: %s\n", err_, __FILE__,       \
                    __LINE__, env ? GRBgeterrormsg(env) : "(no environment)");      \
            return 1;                                                               \
        }                                                                           \
    } while (0)

int main(int argc, char **argv) {
    ModelOptions opt;
    model_default_options(&opt);
    double time_limit = -1;
    const char *solution = NULL, *logfile = NULL, *path = NULL;

    for (int a = 1; a < argc; a++) {
        if (!strcmp(argv[a], "--relax")) {
            opt.relax = 1;
        } else if (!strcmp(argv[a], "--time-limit") && a + 1 < argc) {
            char *end;
            time_limit = strtod(argv[++a], &end);
            if (*end != '\0' || !(time_limit >= 0)) { usage(); return 2; }
        } else if (!strcmp(argv[a], "--solution") && a + 1 < argc) {
            solution = argv[++a];
        } else if (!strcmp(argv[a], "--log") && a + 1 < argc) {
            logfile = argv[++a];
        } else if (!strcmp(argv[a], "--variant") && a + 1 < argc) {
            if (!model_apply_variant(&opt, argv[++a])) {
                fprintf(stderr, "unknown variant %s\n", argv[a]);
                return 2;
            }
        } else if (argv[a][0] == '-' && argv[a][1] == '-') {
            usage();
            return 2;
        } else if (!path) {
            path = argv[a];
        } else {
            usage();
            return 2;
        }
    }
    if (!path) { usage(); return 2; }

    char err[512];
    Instance *in = instance_load(path, err, sizeof err);
    if (!in) { fprintf(stderr, "%s: %s\n", path, err); return 1; }
    instance_warnings(in, stderr);

    GRBenv *env = NULL;
    int rc = GRBemptyenv(&env);
    if (rc) { fprintf(stderr, "Gurobi: cannot create environment (error %d)\n", rc); return 1; }
    /* The solver log must not mix with the objective= and bound= lines on stdout. */
    CHECK(GRBsetintparam(env, GRB_INT_PAR_LOGTOCONSOLE, 0));
    if (logfile) CHECK(GRBsetstrparam(env, GRB_STR_PAR_LOGFILE, logfile));
    CHECK(GRBstartenv(env));
    CHECK(GRBsetdblparam(env, GRB_DBL_PAR_MIPGAP, 0.0001));
    if (time_limit >= 0) CHECK(GRBsetdblparam(env, GRB_DBL_PAR_TIMELIMIT, time_limit));

    GRBmodel *model = NULL;
    VarMap *map = NULL;
    CHECK(model_build(env, in, &opt, &model, &map));

    int numvars, numconstrs, numbin, numnz;
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_NUMVARS, &numvars));
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_NUMCONSTRS, &numconstrs));
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_NUMBINVARS, &numbin));
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_NUMNZS, &numnz));

    CHECK(GRBoptimize(model));

    int status, solcount = 0;
    double runtime = 0, obj = NAN, bound = NAN, gap = NAN, nodes = 0;
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_STATUS, &status));
    CHECK(GRBgetintattr(model, GRB_INT_ATTR_SOLCOUNT, &solcount));
    CHECK(GRBgetdblattr(model, GRB_DBL_ATTR_RUNTIME, &runtime));
    if (solcount > 0) CHECK(GRBgetdblattr(model, GRB_DBL_ATTR_OBJVAL, &obj));

    /* An LP stopped early has a primal point that is not the relaxation's value. */
    int have_objective = opt.relax ? status == GRB_OPTIMAL : solcount > 0;
    if (have_objective) printf("objective=%.6f\n", obj);
    else printf("objective=nan\n");

    if (!opt.relax) {
        /* Gurobi reports +-GRB_INFINITY until it has a finite bound or gap. */
        if (GRBgetdblattr(model, GRB_DBL_ATTR_OBJBOUND, &bound) != 0) printf("bound=nan\n");
        else if (bound <= -GRB_INFINITY) printf("bound=-inf\n");
        else printf("bound=%.6f\n", bound);
        if (solcount > 0 && GRBgetdblattr(model, GRB_DBL_ATTR_MIPGAP, &gap) == 0 && gap < GRB_INFINITY)
            printf("gap=%.8f\n", gap);
        if (GRBgetdblattr(model, GRB_DBL_ATTR_NODECOUNT, &nodes) == 0) printf("nodes=%.0f\n", nodes);
    }
    printf("status=%s\n", status_name(status));
    printf("runtime=%.2f\n", runtime);
    printf("size=%d vars, %d binaries, %d constraints, %d nonzeros\n", numvars, numbin, numconstrs, numnz);

    int exit_code = 0;
    if (have_objective) {
        double *x = malloc((size_t)numvars * sizeof *x);
        if (!x) { fprintf(stderr, "out of memory\n"); return 1; }
        CHECK(GRBgetdblattrarray(model, GRB_DBL_ATTR_X, 0, numvars, x));
        double shed = 0, over = 0, shortfall = 0, used = 0, avail = 0;
        for (int t = 1; t <= in->T; t++) {
            shed += x[map->splus[t]];
            over += x[map->sminus[t]];
            shortfall += x[map->sr[t]];
            used += x[map->pw[t]];
            avail += in->ren_max[t];
        }
        printf("load_shed_mwh=%.6f\nover_generation_mwh=%.6f\nreserve_shortfall_mwh=%.6f\n",
               shed, over, shortfall);
        printf("renewable_used_mwh=%.3f\nrenewable_available_mwh=%.3f\n", used, avail);
        if (solution) exit_code = write_solution(solution, in, map, x, opt.relax);
        free(x);
    } else {
        if (solution) fprintf(stderr, "no solution available; %s not written\n", solution);
        exit_code = 1;
    }

    varmap_free(map);
    GRBfreemodel(model);
    GRBfreeenv(env);
    instance_free(in);
    return exit_code;
}
