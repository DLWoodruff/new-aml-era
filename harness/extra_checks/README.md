# Two checks cited in the note

These were run by hand after the experiment, using `program/`, outside any Claude Code session
that wrote the program.

**Published (48).** `published_48_results.txt` holds the output for all seven instances from
`uc_solve --variant pw48-printed`, with and without `--relax` and a 600-second limit. Every value
equals the default model's in `program/RESULTS.md`, because no generator with `UT = 1` in these
instances has `SU != SD`, so the two forms of (48) are the same model here.

**EGRET's form of (35) at t = 1.** `egret_form_of_35.patch` changes what `--variant first-ramp-u0`
does so that it uses the row in EGRET 0.6.2 (`ramping_limits.py`), whose right-hand side exceeds
the paper's by `P_min w(1)`. Solved with `--relax` on the three RTS-GMLC instances, it gave:

| Instance | LP objective |
|---|---:|
| `rts_gmlc_2020-12-23` | 2696922.367712 |
| `rts_gmlc_2020-01-27` | 1226645.339956 |
| `rts_gmlc_2020-07-06` | 3722397.471090 |

All three equal EGRET's values in `harness/egret_reference.json`.
