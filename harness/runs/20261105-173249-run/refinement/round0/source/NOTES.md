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
- `--variant NAME` selects a diagnostic departure from the default model, and may be repeated. The names are `reserve-printed`, `updown-printed`, `updown-all-periods`, `tail-none`, `tail-printed`, `pw48-printed`, `no-hist-v` and `base`, along with names that restate the defaults. They exist only to measure the decisions above.

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
