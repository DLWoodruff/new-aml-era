/* uc_verify instance.json solution.csv [claimed_objective]
 *
 * Checks a MIP solution CSV against the physical rules of the instance using only
 * the schedule itself, with no reference to the model's rows, and recomputes its
 * cost: production from the curve, start-up from the actual off time, and penalties
 * from the balance. Exits 1 on any violation or objective mismatch. */
#include "instance.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TOL 1e-4

static int nviol = 0;

static void viol(const char *what, const Gen *g, int t, double a, double b) {
    if (nviol < 25) printf("VIOLATION %s: %s t=%d (%.6f vs %.6f)\n", what, g ? g->name : "-", t, a, b);
    nviol++;
}

static double production_cost(const Gen *g, double p) {
    double c = g->cr, rem = p - g->pmin;
    for (int l = 1; l <= g->nseg && rem > 0; l++) {
        double d = g->pw[l] - g->pw[l - 1], q = rem < d ? rem : d;
        c += g->slope[l] * q;
        rem -= q;
    }
    return c;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: uc_verify instance.json solution.csv [objective]\n"); return 2; }
    char err[512];
    Instance *in = instance_load(argv[1], err, sizeof err);
    if (!in) { fprintf(stderr, "%s\n", err); return 2; }
    const int T = in->T, G = in->ngen;

    int *on = calloc((size_t)(G * (T + 2)), sizeof *on);
    double *pw = calloc((size_t)(G * (T + 2)), sizeof *pw);
    double *av = calloc((size_t)(G * (T + 2)), sizeof *av);
    int *seen = calloc((size_t)(G * (T + 2)), sizeof *seen);
    double *ren = calloc((size_t)(T + 2), sizeof *ren);
    int *ren_seen = calloc((size_t)(T + 2), sizeof *ren_seen);

    FILE *f = fopen(argv[2], "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    char line[1024];
    if (!fgets(line, sizeof line, f) || strncmp(line, "generator,period,on,power,available", 35) != 0) {
        printf("VIOLATION bad header\n");
        nviol++;
    }
    int rows = 0;
    while (fgets(line, sizeof line, f)) {
        char *fld[5];
        char *p = line;
        int n = 0;
        line[strcspn(line, "\r\n")] = '\0';
        while (n < 5) {
            fld[n++] = p;
            p = strchr(p, ',');
            if (!p) break;
            *p++ = '\0';
        }
        if (n != 5) { printf("VIOLATION bad row\n"); nviol++; continue; }
        rows++;
        int t = atoi(fld[1]);
        if (t < 1 || t > T) { printf("VIOLATION bad period %d\n", t); nviol++; continue; }
        if (!strcmp(fld[0], "RENEWABLE")) {
            if (fld[2][0] || fld[4][0]) { printf("VIOLATION renewable row has on/available\n"); nviol++; }
            ren[t] = atof(fld[3]);
            ren_seen[t]++;
            continue;
        }
        int gi = -1;
        for (int k = 0; k < G; k++) if (!strcmp(in->gen[k].name, fld[0])) { gi = k; break; }
        if (gi < 0) { printf("VIOLATION unknown generator %s\n", fld[0]); nviol++; continue; }
        if (strcmp(fld[2], "0") && strcmp(fld[2], "1")) viol("on not 0/1", &in->gen[gi], t, atof(fld[2]), 0);
        on[gi * (T + 2) + t] = atoi(fld[2]);
        pw[gi * (T + 2) + t] = atof(fld[3]);
        av[gi * (T + 2) + t] = atof(fld[4]);
        seen[gi * (T + 2) + t]++;
    }
    fclose(f);
    if (rows != G * T + T) { printf("VIOLATION row count %d, expected %d\n", rows, G * T + T); nviol++; }

    double cost_prod = 0, cost_run = 0, cost_start = 0;
    int nstart = 0;
    for (int gi = 0; gi < G; gi++) {
        const Gen *g = &in->gen[gi];
#define ON(t) on[gi * (T + 2) + (t)]
#define PW(t) pw[gi * (T + 2) + (t)]
#define AV(t) av[gi * (T + 2) + (t)]
        ON(0) = g->on0;
        PW(0) = g->on0 ? g->p0 : 0.0;
        int run = g->on0 ? g->up0 : 0;    /* periods on so far in the current run */
        int off = g->on0 ? 0 : g->down0;  /* periods off so far */
        for (int t = 1; t <= T; t++) {
            if (seen[gi * (T + 2) + t] != 1) viol("row count", g, t, seen[gi * (T + 2) + t], 1);
            int u = ON(t), up = ON(t - 1);
            if (g->must_run && !u) viol("must_run off", g, t, 0, 1);
            if (u) {
                if (PW(t) < g->pmin - TOL) viol("below Pmin", g, t, PW(t), g->pmin);
                if (PW(t) > g->pmax + TOL) viol("above Pmax", g, t, PW(t), g->pmax);
                if (AV(t) > g->pmax + TOL) viol("available above Pmax", g, t, AV(t), g->pmax);
                if (AV(t) < PW(t) - TOL) viol("available below power", g, t, AV(t), PW(t));
            } else if (fabs(PW(t)) > TOL || fabs(AV(t)) > TOL) {
                viol("output while off", g, t, PW(t), AV(t));
            }
            if (u && !up) {  /* start-up at t */
                if (AV(t) > g->su + TOL) viol("start-up limit", g, t, AV(t), g->su);
                if (off < g->dt) viol("min down", g, t, off, g->dt);
                int s = 0;
                while (s + 1 < g->ncat && g->lag[s + 1] <= off) s++;
                cost_start += g->scost[s];
                nstart++;
                run = 0;
            }
            if (!u && up) {  /* shut-down at t */
                if (PW(t - 1) > g->sd + TOL) viol("shut-down limit", g, t, PW(t - 1), g->sd);
                if (t > 1 && AV(t - 1) > g->sd + TOL) viol("shut-down limit (available)", g, t, AV(t - 1), g->sd);
                if (run < g->ut) viol("min up", g, t, run, g->ut);
                off = 0;
            }
            if (u && up) {
                if (AV(t) - PW(t - 1) > g->ru + TOL) viol("ramp up", g, t, AV(t) - PW(t - 1), g->ru);
                if (PW(t - 1) - PW(t) > g->rd + TOL) viol("ramp down", g, t, PW(t - 1) - PW(t), g->rd);
            }
            if (u) { run++; cost_run += g->cr; cost_prod += production_cost(g, PW(t)) - g->cr; }
            else off++;
        }
    }

    double cost_load = 0, cost_res_ren = 0, cost_res_printed = 0, max_mis = 0, wind_used = 0, wind_avail = 0;
    int short_ren = 0, short_printed = 0;
    for (int t = 1; t <= T; t++) {
        if (ren_seen[t] != 1) viol("renewable row count", NULL, t, ren_seen[t], 1);
        if (ren[t] < in->ren_min[t] - TOL) viol("renewable below min", NULL, t, ren[t], in->ren_min[t]);
        if (ren[t] > in->ren_max[t] + TOL) viol("renewable above max", NULL, t, ren[t], in->ren_max[t]);
        double sp = 0, sa = 0;
        for (int gi = 0; gi < G; gi++) { sp += PW(t); sa += AV(t); }
        double mis = in->demand[t] - sp - ren[t];
        cost_load += 10000.0 * fabs(mis);
        if (fabs(mis) > max_mis) max_mis = fabs(mis);
        double need = in->demand[t] + in->reserve[t];
        double s1 = need - sa - ren[t], s2 = need - sa;
        if (s1 > TOL) { cost_res_ren += 1000.0 * s1; short_ren++; }
        if (s2 > TOL) { cost_res_printed += 1000.0 * s2; short_printed++; }
        wind_used += ren[t];
        wind_avail += in->ren_max[t];
    }
    double base = cost_prod + cost_run + cost_start + cost_load;
    printf("cost: running=%.2f production=%.2f startup=%.2f (%d starts) load_penalty=%.2f (max mismatch %.4f MW)\n",
           cost_run, cost_prod, cost_start, nstart, cost_load, max_mis);
    printf("reserve penalty: with renewables=%.2f (%d periods), as printed=%.2f (%d periods)\n",
           cost_res_ren, short_ren, cost_res_printed, short_printed);
    printf("renewable used %.1f of %.1f MWh\n", wind_used, wind_avail);
    printf("recomputed objective: with renewables=%.4f, as printed=%.4f\n", base + cost_res_ren, base + cost_res_printed);
    if (argc > 3) {
        double claim = atof(argv[3]);
        double d1 = fabs(claim - (base + cost_res_ren)), d2 = fabs(claim - (base + cost_res_printed));
        double d = d1 < d2 ? d1 : d2;
        printf("claimed %.4f: difference %.4f (%.2e relative)\n", claim, d, d / (fabs(claim) + 1));
        if (d > 1e-6 * (fabs(claim) + 1)) { printf("VIOLATION objective mismatch\n"); nviol++; }
    }
    printf("violations=%d\n", nviol);
    instance_free(in);
    return nviol ? 1 : 0;
}
