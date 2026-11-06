/* Builds formulation T (Knueven, Ostrowski, Watson; Table 3) as a Gurobi model. */
#ifndef UC_MODEL_H
#define UC_MODEL_H

#include "gurobi_c.h"
#include "instance.h"

#define UC_C_LP 10000.0  /* load mismatch penalty, $/MWh */
#define UC_C_RP 1000.0   /* reserve shortfall penalty, $/MWh */

typedef enum { TAIL_NONE, TAIL_PRINTED, TAIL_CORRECTED } TailMode;
/* Rows (4), (5) for t from min(UT,T) / min(DT,T); from UT / DT exactly as printed
 * (no rows at all if UT > T); or from 1 with the sum truncated at the horizon. */
typedef enum { UPDOWN_CLAMPED, UPDOWN_PRINTED, UPDOWN_ALL_PERIODS } UpDownMode;

/* The defaults are formulation T as documented in NOTES.md. Every other setting
 * exists only to measure the effect of a documented decision. */
typedef struct {
    int relax;              /* LP relaxation: binaries become [0,1] */
    TailMode tail;          /* the extra v term of (41) */
    int pw48_printed;       /* (48) with its [.]^+ operands as printed */
    int reserve_printed;    /* (67) as printed, with no renewable term */
    UpDownMode updown;      /* which periods get rows (4), (5) */
    int no_hist_v;          /* ignore historical v(t), t<1, in (38), (40), (41) */
    int first_ramp_u0;      /* (35) at t=1 with RU u(0) instead of RU u(1) - RU v(1) */
    int base_model;         /* basic physical model instead of T; see NOTES.md */
    int only_gen;           /* >= 0: single generator with no system rows */
} ModelOptions;

typedef struct {
    int *u, *v, *w, *pp, *pa;  /* index by t=1..T */
} GenVars;

typedef struct {
    int T, ngen, numvars;
    GenVars *gen;      /* index by generator; only only_gen is filled if it is set */
    int *pw;           /* renewable output, t=1..T */
    int *splus, *sminus, *sr;
} VarMap;

void model_default_options(ModelOptions *o);
/* Returns 1 if name is a known diagnostic variant. */
int model_apply_variant(ModelOptions *o, const char *name);

/* Returns a Gurobi error code; on success *model_out and *map_out are set. */
int model_build(GRBenv *env, const Instance *in, const ModelOptions *o,
                GRBmodel **model_out, VarMap **map_out);
void varmap_free(VarMap *m);

#endif
