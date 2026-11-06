# Instance file format

Each file in `data/` is a JSON object describing one unit commitment instance.
Power is in MW, time in hourly periods, and costs in dollars.

## Top level

| Field | Meaning |
|---|---|
| `time_periods` | Number of periods in the planning horizon, T |
| `demand` | Array of length T: system load in each period |
| `reserves` | Array of length T: spinning reserve requirement in each period |
| `thermal_generators` | Object mapping each generator's name to its data |
| `renewable_generators` | Object mapping each renewable generator's name to its data |

The instances have no transmission network.

## Each thermal generator

| Field | Meaning |
|---|---|
| `name` | The generator's name |
| `must_run` | 1 if the generator must be on in every period, otherwise 0 |
| `power_output_minimum`, `power_output_maximum` | Minimum and maximum output when on |
| `ramp_up_limit`, `ramp_down_limit` | Ramp-up and ramp-down rates, per period |
| `ramp_startup_limit`, `ramp_shutdown_limit` | Start-up and shut-down rates |
| `time_up_minimum`, `time_down_minimum` | Minimum up time and minimum down time, in periods |
| `unit_on_t0` | 1 if the generator is on in period 0, otherwise 0 |
| `power_output_t0` | Output in period 0 |
| `time_up_t0`, `time_down_t0` | Number of periods the generator has been on, or off, as of period 0 |
| `startup` | Array of `{lag, cost}`, ordered by increasing `lag`: the start-up cost `cost` applies when the generator has been off for at least `lag` periods |
| `piecewise_production` | Array of `{mw, cost}`, ordered by increasing `mw`: the points of the piecewise linear production cost curve, where `cost` is the hourly cost of producing `mw` |

## Each renewable generator

| Field | Meaning |
|---|---|
| `name` | The generator's name |
| `power_output_minimum`, `power_output_maximum` | Arrays of length T: minimum and maximum output in each period |
