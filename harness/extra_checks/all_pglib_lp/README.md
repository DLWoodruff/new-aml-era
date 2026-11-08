# LP relaxations of all of pglib-uc

Section 2.3 of the note. The program in `../../../program` was run unchanged on the LP
relaxation of all 56 pglib-uc instances (commit 39a7f38), and compared with EGRET 0.6.2 with
pull request https://github.com/grid-parity-exchange/Egret/pull/342 applied (commit `d684afd`).
The pull request was merged into EGRET's main branch as `c2a3c63`, whose tree is identical. Gurobi 13.0.2 for the program, gurobipy 12.0.2 for EGRET. Times are not reported.

## Results

- `results/lp.csv`: one row per instance. 43 of 56 LP bounds agree (largest difference 8e-6).
  On the other 13 the program's bound is higher, by at most 0.008%.
- `program_egret_form_35_lp`: the program with `../egret_form_of_35.patch` applied and run with
  `--variant first-ramp-u0`, which uses EGRET's form of (35) at t=1. It agrees with EGRET on
  all 56, so the 13 differences all come from that row. The program's row is valid and tighter.
- `results/mip.csv`: MIPs at a 0.01% gap on the 13 differing instances, the original seven, and
  `rts_gmlc/2020-11-25`. All intervals [bound, objective] overlap. The program's values for the
  seven are those in the experiment's `RESULTS.md`.
- `results/` also holds the raw outputs of each run.

## How to repeat

With pglib-uc checked out at 39a7f38 in `PGLIB`, the program built as `UC_SOLVE`, and EGRET at
`c2a3c63` or later on `PYTHONPATH` (do not use `python -I`, which ignores `PYTHONPATH`):

    python run_lps.py program PGLIB UC_SOLVE ../../validate_egret.py program_lp.jsonl
    python run_lps.py egret   PGLIB UC_SOLVE ../../validate_egret.py egret_lp.jsonl

For EGRET's form of (35), apply `../egret_form_of_35.patch` to a copy of the program with
`patch -p2`, build it, and solve each instance with `--relax --variant first-ramp-u0`. MIPs use
`validate_egret.py --time-limit 900` and `uc_solve --time-limit 600`.
