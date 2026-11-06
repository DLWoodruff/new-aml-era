# Notes on the implementation

Equation numbers refer to the paper, and "supplement" to the online supplement bound into `paper.pdf`. The code is in `src/model.c` (the formulation), `src/instance.c` (data), `src/json.c` (the reader) and `src/main.c` (the command line).

The LP effects below were measured with `--variant`, on the instances named in each entry.

## Departures from the printed paper

### 1. Reserve constraint (67): renewable output added. This changes results substantially.

- **Printed:** `sum p̄(t) + s_R(t) >= D(t) + R(t)`.
- **Implemented:** `sum p̄(t) + p_W(t) + s_R(t) >= D(t) + R(t)`.

**Reason.**
- Section 3.9.3 presents (67) and (68) as two ways to write one requirement, chosen by which reserve variable is in use, and states that `r = p̄ - p`.
- Substituting that into (68) and using the balance (65) gives `sum p̄ + p_W + s + s_R >= D + R`. So the printed form matches (68) only when there are no renewables. The paper takes (67) from Carrion and Arroyo (2006) and Ostrowski et al. (2012).
- As printed, thermal capacity alone must cover the whole load, however much renewable energy is being used. On `rts_gmlc_2020-01-27` the model then curtails 61% of zero-cost renewable energy, against 14% otherwise, to keep thermal units on.

**Effect.** The MIP objective rises by 135%, 27%, 27%, 77%, 71% and 0.7% on the six instances with renewables. `ca_2014-09-01_reserves_3` has none and is unaffected. Full figures are in `RESULTS.md`.

**Supporting evidence, which is only suggestive.** The paper gives T times of 88.8, 19.4 and 6.8 s on the RTS days January, December and July. The default model takes 40.75, 8.79 and 4.46 s, the same order. The printed form takes 5.11, 6.67 and 118.32 s, the reverse order. The solver version and machine differ from the paper's.

**This is the one decision you may want to overrule.** It departs from an equation that is well defined as printed, on the ground that the printed form is inconsistent with the paper's own statements. `--variant reserve-printed` gives the literal form.

I added only `p_W`, not the load slacks `s+ - s-`, as the smallest change. The exact equivalent of (68) would include them. The two forms differ only when a slack is nonzero, and every slack was zero in all seven MIP and LP solutions.

### 2. Rows (4) and (5) when UT or DT exceeds T

- **Printed:** `t ∈ {UT,…,T}` and `t ∈ {DT,…,T}`.
- **Implemented:** `t ∈ {min(UT,T),…,T}` and `t ∈ {min(DT,T),…,T}`, with sums clipped to the horizon and any historical `v` or `w` moved to the right side. This is identical to the printed form when UT, DT ≤ T.

**Reason.** The FERC data has up to UT=168 and DT=168 on a 48-period horizon: 41 and 102 such units in `ferc_2015-01-01_hw`, 39 and 100 in `ferc_2015-04-01_lw`. The printed range is empty for them, so nothing enforces their minimum times. The single row at t=T restores it: a start at any period forces the unit on at T, and allows no second start.

**Effect.** With the rows exactly as printed (`--variant updown-printed`), `ferc_2015-01-01_hw` falls from 41483157 to 36221169 (−12.7%), and its LP from 41480391 to 36213647. With the clamp, the LP equals that of rows in all periods (`--variant updown-all-periods`) on the three instances tested: `rts_gmlc_2020-07-06`, `ca_2014-09-01_reserves_3`, `ferc_2015-01-01_hw`.

I did not add rows for t < UT in general, since the printed set is correct for integer solutions and adding them left those LP values unchanged. A spurious pair `v(t)=w(t)=1` with t below UT or DT is excluded by the rows at t=UT and t=DT.

### 3. K^SU in (41): typo corrected

- **Printed:** `K^SU(t) = min{T^RU, UT-2-[K^SU(t)]^+, t-1}`, which defines K^SU in terms of itself.
- **Implemented:** `[K^SD(t)]^+` in place of `[K^SU(t)]^+`.

**Reason.** A `v(t-i)` and a `w(t+1+j)` in the same row must not both be 1. That needs `i+j+1 < UT`, so the largest i is `UT-2-K^SD`. With this reading, (41) reduces to (39) when UT ≥ T^RU+T^RD+2, as the text says it should.

### 4. Extra term of (41): operands reversed and T^RD replaced

- **Printed:** `-[(SU+(K^SU+1)RU) - (SD+T^RD RD)]^+ v(t-(K^SU+1))`.
- **Implemented:** `-[(SD+K^SD RD) - (SU+(K^SU+1)RU)]^+ v(t-(K^SU+1))`.

**Reason for the operand order.** The printed order cuts off feasible schedules. For example, take P_=0, P̄=100, SU=40, RU=45, SD=50, RD=30, UT=3. The run 40, 80, 50 is feasible, but the printed term gives `p <= 75` at the middle period. The text calls this term "similar to (23b)", and (23b) has the reverse order. So does the supplement's (88a).

**Reason for K^SD.** (88a) only covers `K^SD = T^RD`. When `K^SD < T^RD`, both the tail `v` and `w(t+1+K^SD)` can equal 1, and using T^RD makes the right side too small. For example, SU=SD=RU=RD=10, P̄=100, UT=3 at a period where `K^SD=2`, `K^SU=-1`: the run 10, 20, 10 gets a right side of −60. With K^SD the right side is exactly `min(a, SD+K^SD RD)`.

**Effect.** None measured on the LP. The printed coefficient is zero on every row of `rts_gmlc_2020-07-06` and `ferc_2015-01-01_hw`, so on those two the printed form equals having no term. The implemented coefficient adds 4065 nonzeros on `ferc_2015-01-01_hw` and none on `rts_gmlc_2020-07-06` or `ca_2014-09-01_reserves_3`, and leaves the LP objective unchanged on all three.

### 5. Equation (48): operands of [·]^+ reversed

- **Printed:** (48a) has `[C^v-C^w]^+ w(t+1)` and (48b) has `[C^w-C^v]^+ v(t)`.
- **Implemented:** (48a) has `[C^w-C^v]^+ w(t+1)` and (48b) has `[C^v-C^w]^+ v(t)`.

**Reason.** The text says (48) follows (23). In (23a) the `w` coefficient is `[SU-SD]^+`, and `SU-SD` equals `C^w-C^v` on a segment both limits fall inside, so the printed order is the opposite of (23). It is also invalid. With P_=10, breakpoints 10, 30, 50, SU=20, SD=40, UT=1, printed (48a) gives `p^2 <= -10` for a one-period run, which makes that feasible run infeasible.

**Effect.** LP objective unchanged on `ferc_2015-01-01_hw` and `ca_2014-09-01_reserves_3`. MIP effect not measured. Both forms coincide whenever SU=SD.

### 6. SU and SD clipped to P̄

**Implemented.** `SU = min(SU, P̄)` and `SD = min(SD, P̄)`.

**Reason.** 574 of the 610 units in each CAISO instance have limits above P̄, such as SU=61.725 with P̄=5.75. Then `P̄-SU` is negative, which turns the reductions in (23) and (38) into increases and allows `p̄ > P̄`. A limit above P̄ never binds, so clipping removes no feasible schedule. The paper assumes `P̄-SU >= 0` throughout, in its definition of T^RU for instance. I did not measure the effect on the LP.

## Where the paper leaves something open

| Topic | Decision | Reason |
|---|---|---|
| `must_run` | `u(t) >= 1` for all t | Only the data format defines it; the paper mentions must-run units only in the instance description. |
| U and D in (3) | `U = max(UT - time_up_t0, 0)` if on at period 0, `D = max(DT - time_down_t0, 0)` if off | The nomenclature defines them only as the hours required on or off. |
| (3) | Implemented as bounds on u | Equivalent, and the paper expects presolve to remove these variables. |
| Historical `v` | `v(1-time_up_t0) = 1` for a unit on at period 0; otherwise 0 | Section 3 says to use historical data. Earlier starts cannot enter a row. `p(0)` is consistent with this on all seven instances, and LP objective on `ferc_2015-01-01_hw` is the same without it. |
| Historical `w` | `w(1-time_down_t0) = 1` for a unit off at period 0; otherwise 0 | Same reason. It is the only shut-down before the horizon that can pair with a start-up in it. |
| Off-time arcs | `x(t',t)` for `DT <= t-t' <= TC-1`, for `t'` in the horizon or at the historical shut-down. (60) at that shut-down has right side 1. | The nomenclature includes times before the horizon. An off time of n periods at period 0 and a start at t gives an off duration of `t+n-1`, matching "off for at least `lag` periods". |
| Rows at `t = T` | `w(T+1) = 0` in (23), (38), (46), (48) | Section 3 sets `v` and `w` beyond the horizon to 0, so no shut-down limit applies at T. |
| `(38)` and `(40)` for `UT = 1` | Included, with the sum empty or one term | Table 3 has no condition. These rows are dominated by (23) but harmless. |
| `(23)` for `UT > 1` | Not used | The paper defines it for G^1 only. (38) generalizes (20) for the others. |
| Left side of (41) | `p'`, not `p̄'` | It is printed with `p`, and its ramp-down terms do not limit available power. |
| `K^SD(t) <= 0` | (41) omitted | The paper says so. |
| `p'(0)` | `p(0) - P_` if on, else 0 | From (12). |
| Variables | `p`, `p̄`, `c^p`, `c^SU`, `c^SD`, and `s` are not created | (12), (13), (44), (61), (62) and (66) substitute them out, as the paper does for cost variables. `s` is `s+ - s-`. |
| Renewables | One aggregate variable per period, bounds the sum of the unit bounds | (63), for one bus. Only the total is reported. |
| Network | (64) dropped; flow sums in (65) empty | Section 3.9.2, for one bus. |
| `T^RU`, `T^RD` | Floor computed with an added 1e-9 | Protects against rounding in ratios that are exact in the data. |
| Solver settings | Gurobi defaults, 16 threads, except `MIPGap` and `TimeLimit` | Matches the paper's Section 6.3, which keeps defaults apart from the limit and gap. |
| `C_LP`, `C_RP` | 10000 and 1000 $/MWh (`src/model.h`) | Neither the paper, the supplement nor the data gives a value. The slacks are zero in all seven MIP and LP solutions, so the penalty terms add nothing to any objective here. A much smaller value would matter: a reserve penalty of 100 lowers the `rts_gmlc_2020-12-23` LP to 2696949.73. This row was missing until the refinement round. |

## Data preprocessing that is present but not used here

`uc_solve` prints a `data warning` to stderr whenever one of these applies. On the seven instances the only warnings are SU and SD above P̄ (CAISO) and UT and DT above T (FERC), so none of the following is used.

- **Start-up categories:** the paper sets `T^1 = DT`. A first lag above DT is extended down to DT, and categories wholly below DT are dropped.
- **Cost curves:** restricted to `[P_, P̄]` by interpolation, with repeated points removed, so that `P̄^l > P_` and `P̄^0 = P_`. `C^R` is the curve's cost at `P_`.
- **UT, DT below 1:** set to 1.
- **Not repaired, warned only:** decreasing start-up costs, which would break (61); non-convex curves; `p(0)` above `SU + (time_up_t0-1) RU`, which would make the historical `v` terms infeasible; and a must-run unit required to stay off, which is infeasible.

## Output choices the task left open

- `--relax --solution`: `on` is `u` rounded at 0.5, as the format requires 0 or 1. `power` and `available` use the fractional `u`.
- In a MIP solution, `u` is rounded before computing `power` and `available`, which removes integrality noise.
- Besides `objective=` and `bound=`, stdout has `gap=`, `nodes=`, `status=`, `runtime=`, `size=` and totals for slacks and renewables. The Gurobi log is not sent to the console.
- If there is no solution, it prints `objective=nan` and exits 1. An LP stopped by the limit counts as no solution, since its point is not the relaxation's value. A MIP with no finite bound yet prints `bound=-inf`.
- Generator names containing commas or quotes are quoted in the CSV. None of the seven instances has one.

## Options beyond the specified command line

- `--log FILE` keeps the Gurobi log.
- `--variant NAME` selects a diagnostic departure from the default model, and may be repeated. The names are `reserve-printed`, `updown-printed`, `updown-all-periods`, `tail-none`, `tail-printed`, `pw48-printed`, `no-hist-v`, `first-ramp-u0` and `base`, along with names that restate the defaults. They exist only to measure the decisions above.

## Verification

**What was run.** The checks reported in `RESULTS.md`: T against the `base` model on all seven instances, hand checks of one period and one unit, an independent read-through of the code against the paper, and tests of the early-stop and bad-input paths.

**What could not be run.** This session's permissions allowed `uc_solve` but denied other programs:

- `build/uc_verify` checks a solution CSV against the physical rules and recomputes its objective from the schedule.
- `build/uc_unitcheck` compares T with the `base` model, one generator at a time, under random objectives at zero gap. It would catch an invalid inequality far more sensitively than the 0.01% comparison in `RESULTS.md`.
- Scripts in Python and `awk` for the same purposes were also denied, as was a batch script for the runs.

So **both tools compile but have never been executed, and may contain bugs.** No solution's objective has been recomputed by separate code. To run them:

```
build/uc_verify data/NAME.json results/NAME.mip.csv OBJECTIVE
build/uc_unitcheck data/NAME.json
build/uc_unitcheck --variant pw48-printed data/NAME.json
build/uc_unitcheck --variant tail-printed data/NAME.json
```

Because of the same restriction, the data checks that I first wrote as a separate tool became the `data warning` lines in `uc_solve`. A solver should report these anyway, but it is also why they live there.

The paper's reference implementation (EGRET) and the benchmark library were not consulted, as instructed.

## Refinement round: `REFINEMENT_REPORT.md`

**Outcome.** I found no error in the model, so the default model is unchanged. Its objectives, bounds and node counts on all seven instances are the same as before. If the check is rerun with the same tolerance, it will report the same reserve line, for the reason in item 3.

| Item in the report | Conclusion | Changed | Reason, in brief |
|---|---|---|---|
| 1. LP differs on `rts_gmlc_2020-12-23` | Acceptable. The program matches the printed row. | Default model: nothing. Added `--variant first-ramp-u0`. | One row, (35) at t=1, explains the difference. The printed form is valid and tighter, and has the same integer solutions. |
| 2. LPs agree on the other six | Correct | Nothing | They also support departures 1 and 2 above. |
| 3. MIP objectives and bounds differ; all intervals overlap | Acceptable | Nothing | Each difference is below the 0.01% gap. |
| 4. Reserve "violated" once by 0.000011 on `ferc_2015-04-01_lw` | Not a violation by the model. Six-decimal rounding in the CSV. | Nothing | The row holds to 1.0e-10 in the solver. The check's figure is reproduced exactly from the rounded values. |

### Item 1: LP on `rts_gmlc_2020-12-23`

The program gives 2697031.224634 and the reference 2696922.367712. The program is higher by 108.857, so it is the tighter one here. That is what a row cutting off feasible points would look like, so I tested for that first.

**Cause.** (35) at t=1 reads `p̄'(1) - p'(0) <= (SU-P_) v(1) - RU v(1) + RU u(1)`. By (2), `u(1) - v(1) = u(0) - w(1)`, so this is

`p̄'(1) <= p'(0) + (SU-P_) v(1) + RU (u(0) - w(1))`.

Leaving out the `-RU w(1)` gives a weaker row, with `RU u(0)` in its place. `--variant first-ramp-u0` solves that form.

**Evidence.**
- With the weaker row, the December LP is 2696922.367712, equal to the reference in all six decimals.
- The other six LPs do not move, and all equal the reference. So this one row accounts for all seven LP values.
- The closest other candidates did not match. Examples are (24) at t=1 (2696909.55), and availability at t=1 freed on the 76 MW units only (2696922.96).

That the reference uses this form is an inference from these values. I have not seen its code, and other forms could give the same values.

**Why only this instance.** The weaker row only matters if some `w(1)` is above zero and availability at t=1 has value. The reserve row's dual at t=1 is 2.93 on December and 0 on January and July. Load falls in December's first hours, and the LP partly shuts down on-units: `323_CC_2` and `313_CC_1` are fractional at t=1. The LP values on the CAISO and FERC instances do not change with the weaker row either.

**Why this is not an error.**
- *The program matches the paper.* Table 3 lists (35), with no exception for t=1, and `p'(0)` is defined from the data.
- *The row is valid at t=1.* If `u(0)=u(1)=1`, it says `p̄'(1) <= p'(0)+RU`. If the unit shuts down at 1, then `u(1)=0` and `p̄'(1)=0`, while the right side is `p'(0) >= 0`. A start at 1 gives `p̄'(1) <= SU-P_`.
- *Both forms have the same integer solutions.* When `w(1)=0` the two rows are identical. When `w(1)=1`, (38) or (23) already forces `p̄'(1)=0`, so neither row binds. A spurious `v(1)=w(1)=1` is excluded as in departure 2.
- *The optimum agrees.* At zero gap, December gives 2707458.250827 with the printed row and 2707458.250828 with the weaker one. The basic model reached the same objective in the previous round. The value lies inside both MIP intervals in the report and below the reference's objective of 2707468.09, so nothing in the report shows the printed row removing a better solution. This is consistency, not a test of the reference's own schedule, which I do not have.

Changing the default would make the model weaker and less faithful to the printed row, solely to match a number. I did not do that.

**What this does not show.** The zero-gap solve tests one objective on one instance. It is not a proof that T is valid in general. The per-generator test that would be stronger, `uc_unitcheck`, still could not be run.

### Item 2: the six LPs that agree

They agree to six decimals, which is good evidence that the two models match on the rows that bind there. They also bear on two earlier decisions:
- **Departure 1, renewable term in (67).** The reference's LP is 1226645.34 on `rts_gmlc_2020-01-27`, equal to the default model; the printed form gives 2886327.66. The check also treats the requirement as one that these solutions meet. So the reference and the check both agree with this reading. That is corroboration, and still not a statement from the paper, so it remains your decision.
- **Departure 2, clamped rows for UT or DT above T.** The reference's LP on `ferc_2015-01-01_hw` is 41480391.46, equal to the default model; the rows as printed give 36213647.

Agreement says nothing about rows that do not bind in any of these LPs, such as the extra term of (41) or departure 5, which has no effect when SU=SD.

### Item 3: MIP objectives and bounds

All seven intervals overlap, and both programs stopped at a 0.01% gap. The objective differences are 0 on two RTS days, and otherwise from 0.00025% (`ferc_2015-01-01_hw`) to 0.0074% (`ferc_2015-04-01_lw`), all below the gap. Where the two differ, they are different points within the tolerance. On `ferc_2015-04-01_lw` the program's objective is the lower of the two.

### Item 4: reserve on `ferc_2015-04-01_lw`

**Cause.** Rounding when the solution is written to six decimals.

**Evidence, from the same solution as the report** (same objective and bound):
- In the solver's values, the reserve row's largest shortfall is 1.0e-10, `s_R` is 0 in every period, and every `u` is exactly 0 or 1. So rounding `u` in the writer plays no part.
- From values rounded as in the CSV, `R - sum(available - power)` has a worst value of 1.100e-05, at t=7. Exactly one period is above 1e-5. That matches the report's "1 times, worst excess 0.000011".
- The same calculation puts no period above 1e-5 on the other six instances, which also matches the report. The next largest is 8.0e-6 on `ferc_2015-01-01_hw`.

So the check appears to test `sum(available - power) >= R` with a tolerance of 1e-5. This is an inference from the match. The other form, `sum available + renewable >= D + R`, has a worst value of 7.0e-6 on the same data, and does not exceed 1e-5.

**Why nothing was changed.** The format is fixed at six decimals, and no model change affects this. Each of the 978 generators contributes two rounded values, so rounding alone can reach `2 x 978 x 5e-7`, about 9.8e-4, against the check's 1e-5. Biasing the rounding in the writer would remove the line without correcting anything, and would disturb the power balance. A tolerance of about 1e-3 on this sum would be appropriate for a CSV at this precision, or the check could use more digits if the format ever allows.

### How this was measured

- **Finding the row.** I temporarily instrumented `uc_solve` to drop row families by period and generator, print duals on the system rows, change the penalties and start-up category boundaries, override the gap, and print reserve residuals. All of that has been removed. Only `first-ramp-u0` remains.
- **Reproducible with the delivered program:** the LP values in `RESULTS.md`, including those with `first-ramp-u0`.
- **Not reproducible without that instrumentation:** the zero-gap solves, the duals and the solver-side residuals. The rounded residuals can be recomputed from `results/*.mip.csv`.
- **The reference's numbers were used only as a target for diagnosis.** No model change was made to match them.

### Not run

`build/uc_verify` and `build/uc_unitcheck` were denied again, so **they have still never been executed**, and the commands under "Verification" remain the way to run them. I did not add their function to `uc_solve` to get around that. The new variant was built with the rest but not tested under `uc_unitcheck`.

The tables in `RESULTS.md` for the printed reserve form and the basic model, and the hand checks, are from the previous round and were not rerun.
