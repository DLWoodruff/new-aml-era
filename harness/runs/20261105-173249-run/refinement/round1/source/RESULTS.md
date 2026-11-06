# Results: formulation T on the seven instances

All seven MIPs reached the 0.01% gap well inside the 600 s limit, and all seven LP relaxations solved to optimality.

The model departs from the paper's printed text in a few places. One of them, the renewable term in the reserve row (67), changes the objective by up to 135%. See [Effect of the reserve equation](#effect-of-the-reserve-equation) and `NOTES.md`.

These results are from the rebuild after `REFINEMENT_REPORT.md`. **The review changed nothing in the default model**, so the objectives, bounds, node counts and model sizes are the same as in the previous round; only the times differ slightly. See [Comparison with the reference](#comparison-with-the-reference).

## Main results

Objectives are in dollars. Times are Gurobi's reported runtime and exclude reading the file and building the model. "Root gap" is (MIP objective − LP objective) / MIP objective.

| Instance | LP objective | LP time (s) | MIP objective | MIP bound | Gap | MIP time (s) | Nodes | Status | Root gap |
|---|---:|---:|---:|---:|---:|---:|---:|---|---:|
| `rts_gmlc_2020-01-27` | 1226645.339956 | 0.71 | 1230475.366858 | 1230407.984483 | 0.0055% | 39.38 | 20366 | OPTIMAL | 0.31% |
| `rts_gmlc_2020-07-06` | 3722397.471090 | 0.37 | 3729194.920899 | 3728825.770989 | 0.0099% | 4.25 | 1 | OPTIMAL | 0.18% |
| `rts_gmlc_2020-12-23` | 2697031.224634 | 0.39 | 2707509.117416 | 2707260.274187 | 0.0092% | 8.56 | 1 | OPTIMAL | 0.39% |
| `ca_2014-09-01_reserves_3` | 48399.526919 | 6.37 | 48409.123688 | 48404.470950 | 0.0096% | 20.56 | 1 | OPTIMAL | 0.02% |
| `ca_Scenario400_reserves_1` | 33585.584391 | 6.02 | 33590.742186 | 33587.387286 | 0.0100% | 25.23 | 1 | OPTIMAL | 0.02% |
| `ferc_2015-01-01_hw` | 41480391.463026 | 25.55 | 41483157.102869 | 41482399.553225 | 0.0018% | 53.95 | 1 | OPTIMAL | 0.01% |
| `ferc_2015-04-01_lw` | 56994290.752337 | 21.41 | 57001082.949048 | 56999603.005677 | 0.0026% | 57.85 | 1 | OPTIMAL | 0.01% |

OPTIMAL here means within the 0.01% gap, not proved exactly optimal.

Load shed, over-generation and reserve shortfall were zero in every MIP and LP solution above.

| Instance | Renewable used (MWh) | Available (MWh) | Variables | Binaries | Constraints | Nonzeros |
|---|---:|---:|---:|---:|---:|---:|
| `rts_gmlc_2020-01-27` | 127873.596 | 148361.000 | 39137 | 21425 | 44488 | 206976 |
| `rts_gmlc_2020-07-06` | 78694.790 | 78711.600 | 39137 | 21425 | 44488 | 206976 |
| `rts_gmlc_2020-12-23` | 80719.254 | 86297.500 | 39137 | 21425 | 44488 | 206976 |
| `ca_2014-09-01_reserves_3` | 0.000 | 0.000 | 228176 | 127280 | 453078 | 1245007 |
| `ca_Scenario400_reserves_1` | 647317.506 | 647317.506 | 228176 | 127280 | 453078 | 1245007 |
| `ferc_2015-01-01_hw` | 1989211.583 | 2106677.220 | 371631 | 181359 | 506007 | 2298488 |
| `ferc_2015-04-01_lw` | 123146.177 | 123146.177 | 378916 | 187252 | 536643 | 2375635 |

Solution files are in `results/`: `<instance>.mip.csv` and `<instance>.lp.csv`. In an LP file, `on` is `u` rounded to 0 or 1 while `power` and `available` use the fractional `u`.

### Setup

- Gurobi 13.0.2, with defaults except `MIPGap=0.0001` and `TimeLimit=600`.
- AMD Ryzen 7 7840HS, 8 cores and 16 threads, 14 GB of RAM. Gurobi used up to 16 threads.
- The solves ran one at a time, with no other solver running.

The commands, for each instance, were:

```
build/uc_solve --relax --time-limit 600 --solution results/NAME.lp.csv data/NAME.json
build/uc_solve --time-limit 600 --solution results/NAME.mip.csv --log results/NAME.mip.log data/NAME.json
```

All 14 solves above ran on the final binary. The `.mip.log` files are appended to on each run, so each holds several solves, with this round's last. Runs of the same model gave the same objective, bound and node count. The LP `.lp.log` files come from an earlier pass that gave the same objectives.

## Comparison with the reference

`REFINEMENT_REPORT.md` compares the program with a reference implementation. Both solved to a 0.01% gap. `NOTES.md` has the reasoning for each conclusion.

**LP.** Six of the seven agree to six decimals. On `rts_gmlc_2020-12-23` the program's LP is higher by 108.857. This traces to one row, (35) at t=1: the program uses the printed form, and the reference behaves as a weaker form of that row with the same integer solutions. `--variant first-ramp-u0` solves the weaker form, and gives the reference's LP on all seven:

| Instance | LP, default | LP, `first-ramp-u0` | LP, reference |
|---|---:|---:|---:|
| `rts_gmlc_2020-01-27` | 1226645.339956 | 1226645.339956 | 1226645.339956 |
| `rts_gmlc_2020-07-06` | 3722397.471090 | 3722397.471090 | 3722397.471090 |
| `rts_gmlc_2020-12-23` | **2697031.224634** | 2696922.367712 | 2696922.367712 |
| `ca_2014-09-01_reserves_3` | 48399.526919 | 48399.526919 | 48399.526919 |
| `ca_Scenario400_reserves_1` | 33585.584391 | 33585.584391 | 33585.584391 |
| `ferc_2015-01-01_hw` | 41480391.463026 | 41480391.463026 | 41480391.463026 |
| `ferc_2015-04-01_lw` | 56994290.752337 | 56994290.752337 | 56994290.752337 |

That the reference uses this form is an inference from these values; I have not seen its code. The default remains the printed row, which is valid and tighter.

**MIP.** All intervals overlap, and each objective difference is below the 0.01% gap. The largest relative difference is 0.0074%, on `ferc_2015-04-01_lw`. On `rts_gmlc_2020-01-27` and `rts_gmlc_2020-07-06` the objectives are identical.

On `rts_gmlc_2020-12-23`, a zero-gap solve gives 2707458.250827 with the default model and 2707458.250828 with the weaker row. That value lies inside both reported intervals, and equals the objective of the basic model below. These two solves used a temporary gap override that is not in the delivered program.

**Independent check.** The one failure, on `ferc_2015-04-01_lw`, is rounding in the six-decimal CSV, not a violated constraint. It will still appear if the check is rerun with the same tolerance, because neither the solution nor the format changed.

| Instance | Row in the solver: largest shortfall | `s_R` | Worst `R - sum(available - power)` from values rounded to six decimals | Periods above 1e-5 |
|---|---:|---:|---:|---:|
| `rts_gmlc_2020-01-27` | 4.5e-12 | 0 | 2.8e-14 | 0 |
| `rts_gmlc_2020-07-06` | 9.1e-13 | 0 | 0 | 0 |
| `rts_gmlc_2020-12-23` | 9.1e-13 | 0 | 0 | 0 |
| `ca_2014-09-01_reserves_3` | 0 | 0 | 0 | 0 |
| `ca_Scenario400_reserves_1` | 0 | 0 | 0 | 0 |
| `ferc_2015-01-01_hw` | 4.4e-11 | 0 | 8.0e-6 | 0 |
| `ferc_2015-04-01_lw` | 1.0e-10 | 0 | **1.1e-5 (t=7)** | **1** |

These figures came from temporary instrumentation, since removed. The last two columns can be recomputed from the `.mip.csv` files.

## Comparison with the paper

The paper reports no objective values, so only solve times can be compared. Its T times come from supplement Tables 17 to 19, on Gurobi 8.0.1 and a Xeon E5-2670. A different solver version and machine make the absolute times incomparable.

| Instance | Paper's name for it | Paper T (s) | This work (s) |
|---|---|---:|---:|
| `rts_gmlc_2020-01-27` | 2020-01-27 CP | 88.8 | 39.38 |
| `rts_gmlc_2020-07-06` | 2020-07-06 CP | 6.8 | 4.25 |
| `rts_gmlc_2020-12-23` | 2020-12-23 CP | 19.4 | 8.56 |
| `ca_2014-09-01_reserves_3` | 2014-09-01 3% | 83.5 | 20.56 |
| `ca_Scenario400_reserves_1` | Scenario400 1% | 66.5 | 25.23 |
| `ferc_2015-01-01_hw` | 2015-01-01 HW | 210.4 | 53.95 |
| `ferc_2015-04-01_lw` | 2015-04-01 LW | 411.6 | 57.85 |

## Effect of the reserve equation

This is the decision that matters most. Equation (67) as printed has no renewable term, so thermal capacity alone must cover load plus reserve. The default model adds the renewable output to the left side. `--variant reserve-printed` solves the printed form. This table is from the previous round and was not rerun; the default model has not changed. The reference's LP values agree with the default model and not with the printed form, for example 1226645.34 against 2886327.66 on `rts_gmlc_2020-01-27`.

| Instance | LP, as printed | MIP, as printed | MIP bound | Time (s) | Nodes | MIP change from default |
|---|---:|---:|---:|---:|---:|---:|
| `rts_gmlc_2020-01-27` | 2886327.663320 | 2895818.506145 | 2895818.506145 | 5.11 | 1 | +135.3% |
| `rts_gmlc_2020-07-06` | 4738304.251865 | 4744049.174369 | 4743596.078260 | 118.32 | 57703 | +27.2% |
| `rts_gmlc_2020-12-23` | 3420212.426006 | 3427596.725367 | 3427511.589515 | 6.67 | 1 | +26.6% |
| `ca_2014-09-01_reserves_3` | 48399.526919 | 48408.252585 | 48404.447154 | 19.46 | 1 | none (no renewables) |
| `ca_Scenario400_reserves_1` | 59131.628059 | 59294.766885 | 59289.559388 | 96.74 | 1959 | +76.5% |
| `ferc_2015-01-01_hw` | 70787813.609122 | 70853552.802373 | 70846642.488301 | 77.85 | 1 | +70.8% |
| `ferc_2015-04-01_lw` | 57415416.765581 | 57424327.867364 | 57422963.483430 | 103.42 | 1 | +0.7% |

All of these reached the 0.01% gap. On `rts_gmlc_2020-01-27`, the printed form uses only 57746 of the 148361 MWh of renewable energy (61% curtailed, against 14% by default), with no reserve shortfall: it curtails free energy to keep thermal units committed.

## Checks

The independent check in `REFINEMENT_REPORT.md` is the only separate test of these solutions against the data; it is covered under [Comparison with the reference](#comparison-with-the-reference). The tools written for that purpose here still could not be run in this environment; see "Verification" in `NOTES.md`.

**Comparison with an unstrengthened model.** `--variant base` replaces T's generation limits, ramping and piecewise bounds with the basic physical constraints (18), (19), (24), (25), (42). It has the same integer feasible set if T's strengthening is valid, so each model's objective should lie above the other's bound. This table and the other checks below are from the previous round and were not rerun.

| Instance | T objective | T bound | Base objective | Base bound | Consistent |
|---|---:|---:|---:|---:|---|
| `rts_gmlc_2020-01-27` | 1230475.366858 | 1230407.984483 | 1230475.366858 | 1230372.814177 | yes |
| `rts_gmlc_2020-07-06` | 3729194.920899 | 3728825.770989 | 3729194.920899 | 3728823.125816 | yes |
| `rts_gmlc_2020-12-23` | 2707509.117416 | 2707260.274187 | 2707458.250828 | 2707196.479605 | yes |
| `ca_2014-09-01_reserves_3` | 48409.123688 | 48404.470950 | 48407.906437 | 48404.148645 | yes |
| `ca_Scenario400_reserves_1` | 33590.742186 | 33587.387286 | 33590.159078 | 33587.075872 | yes |
| `ferc_2015-01-01_hw` | 41483157.102869 | 41482399.553225 | 41483616.907983 | 41482325.156730 | yes |
| `ferc_2015-04-01_lw` | 57001082.949048 | 56999603.005677 | 57004338.160063 | 56999545.483832 | yes |

This test has limits. It cannot see an error smaller than the gap, which is about $300 on the RTS instances and $5000 on FERC. The two models also share the objective, the up/down-time rows and the start-up rows, so it says nothing about errors in those.

**Hand checks on `rts_gmlc_2020-07-06`, period 18.**
- Thermal output 4861.93 MW plus renewable 1269.30 MW equals demand 6131.23 MW.
- Headroom on the units with spare capacity sums to 183.9369 MW, which equals the reserve requirement.
- Unit `101_STEAM_3` starts at 30 MW with `RU`=`RD`=40. Its `p(1)`=70 is exactly `p0+RU`, and its availability at t=9 is exactly `p(8)+RU`. Its largest drop, 28.71 MW, is within `RD`.

**File format.** On `rts_gmlc_2020-12-23` the MIP CSV has 74 rows for period 1: 73 generators and `RENEWABLE`.

**Independent review.** A separate agent compared `src/model.c` and `src/instance.c` with the paper by reading and hand derivation, without running anything. It found no defect in the default model, and its hand examples support each departure from the printed text.

**Command line.** A 1 s limit on `ferc_2015-04-01_lw` returned status `TIME_LIMIT`, an incumbent and `bound=-inf`. A non-JSON input gave a parse error and exit code 1.
