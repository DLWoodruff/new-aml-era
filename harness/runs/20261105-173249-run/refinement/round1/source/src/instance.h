/* Unit commitment instance data and the preprocessing that maps it to the
 * paper's notation. See NOTES.md for why each preprocessing step exists. */
#ifndef UC_INSTANCE_H
#define UC_INSTANCE_H

#include <stdio.h>

typedef struct {
    char *name;
    int must_run;
    double pmin, pmax;       /* P_, Pbar */
    double ru, rd;           /* RU, RD */
    double su, sd;           /* SU, SD, clipped to Pbar */
    double su_raw, sd_raw;   /* SU, SD as in the file */
    int ut, dt;              /* UT, DT */
    int on0, up0, down0;     /* u(0), periods on / off as of period 0 */
    double p0;               /* p(0) */

    int ncat;                /* S: start-up categories after preprocessing */
    int *lag;                /* lag[s], s=0..S-1: T^s, with lag[0] = DT */
    double *scost;           /* scost[s]: C^s */
    int tc;                  /* TC = lag[S-1] */

    int nseg;                /* L: piecewise segments */
    double *pw;              /* pw[l], l=0..L: Pbar^l, with pw[0]=Pmin, pw[L]=Pmax */
    double *slope;           /* slope[l], l=1..L: C^l */
    double cr;               /* C^R: hourly cost at Pmin */

    int tru, trd;            /* T^RU, T^RD */
    int need_up, need_down;  /* U, D of constraint (3) */
} Gen;

typedef struct {
    int T;
    double *demand;       /* demand[t], t=1..T */
    double *reserve;      /* reserve[t], t=1..T */
    double *ren_min;      /* aggregate renewable minimum, t=1..T */
    double *ren_max;      /* aggregate renewable maximum, t=1..T */
    int ngen;
    Gen *gen;
    int nren;

    /* Raw-file facts that preprocessing would otherwise hide. */
    int n_name_mismatch, n_lag_first_lt_dt, n_lag_first_gt_dt, n_lag_not_increasing;
    int n_cat_merged, n_scost_decreasing;
    int n_pw_first_ne_pmin, n_pw_last_ne_pmax, n_pw_dup, n_pw_nonconvex;
    int n_int_rounded;
} Instance;

/* Load and preprocess. Returns NULL and fills err on failure. */
Instance *instance_load(const char *path, char *err, unsigned long errlen);
void instance_free(Instance *inst);
/* Print one "data warning" line for each way the data departs from what the
 * formulation assumes. Returns the number of lines printed. */
int instance_warnings(const Instance *inst, FILE *out);

/* Historical v(tau), w(tau) for tau <= 0. */
int gen_hist_v(const Gen *g, int tau);
int gen_hist_w(const Gen *g, int tau);

#endif
