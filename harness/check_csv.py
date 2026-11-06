"""Check a uc_solve solution CSV directly against a pglib-uc instance.

Standard library only, and shares no code with the program under test. It recomputes
the cost from the commitment and power values, using the actual off-time of each
start-up, and tests the physical constraints rather than any MIP formulation.

    python check_csv.py instance.json solution.csv
"""
import csv
import json
import sys
from collections import Counter, defaultdict

TOLERANCE = 1e-5


def production_cost(curve, power):
    if len(curve) == 1 or power <= curve[0]["mw"]:
        return curve[0]["cost"]
    for low, high in zip(curve, curve[1:]):
        if power <= high["mw"] + TOLERANCE:
            width = high["mw"] - low["mw"]
            fraction = (power - low["mw"]) / width if width > 0 else 0.0
            return low["cost"] + fraction * (high["cost"] - low["cost"])
    return curve[-1]["cost"]


def main():
    data = json.load(open(sys.argv[1]))
    rows = defaultdict(dict)
    with open(sys.argv[2], newline="") as file:
        for row in csv.DictReader(file):
            rows[row["generator"]][int(row["period"])] = row

    T = data["time_periods"]
    periods = range(1, T + 1)
    violations = Counter()
    worst = {}

    def require(excess, label):
        if excess > TOLERANCE:
            violations[label] += 1
            worst[label] = max(worst.get(label, 0.0), excess)

    total_power = {t: 0.0 for t in periods}
    total_available = {t: 0.0 for t in periods}
    production, startup = 0.0, 0.0

    for name, gen in data["thermal_generators"].items():
        p_min, p_max = gen["power_output_minimum"], gen["power_output_maximum"]
        on = {0: gen["unit_on_t0"]}
        power = {0: gen["power_output_t0"] if gen["unit_on_t0"] else 0.0}
        available = {}
        for t in periods:
            row = rows[name][t]
            on[t] = round(float(row["on"]))
            require(abs(float(row["on"]) - on[t]), "on is integral")
            power[t] = float(row["power"])
            available[t] = float(row["available"])
            total_power[t] += power[t]
            total_available[t] += available[t]
            if gen["must_run"]:
                require(1 - on[t], "must run")
            require(p_min * on[t] - power[t], "minimum output")
            require(power[t] - available[t], "power above available")
            require(available[t] - p_max * on[t], "maximum output")
            if on[t]:
                production += production_cost(gen["piecewise_production"], power[t])

        for t in periods:
            if on[t] and on[t - 1]:
                require(available[t] - power[t - 1] - gen["ramp_up_limit"], "ramp up")
                require(power[t - 1] - power[t] - gen["ramp_down_limit"], "ramp down")
            elif on[t]:
                require(available[t] - gen["ramp_startup_limit"], "start-up limit")
            elif on[t - 1]:
                require(power[t - 1] - gen["ramp_shutdown_limit"], "shut-down limit")

        run_start = 1 - gen["time_up_t0"] if gen["unit_on_t0"] else None
        off_since = None if gen["unit_on_t0"] else 1 - gen["time_down_t0"]
        for t in periods:
            if on[t] and not on[t - 1]:
                off_time = t - off_since
                require(gen["time_down_minimum"] - off_time, "minimum down time")
                category = gen["startup"][0]
                for candidate in gen["startup"]:
                    if off_time >= candidate["lag"]:
                        category = candidate
                startup += category["cost"]
                run_start = t
            elif on[t - 1] and not on[t]:
                require(gen["time_up_minimum"] - (t - run_start), "minimum up time")
                off_since = t

    for t in periods:
        wind = float(rows["RENEWABLE"][t]["power"])
        gens = data["renewable_generators"].values()
        require(sum(g["power_output_minimum"][t - 1] for g in gens) - wind, "renewable minimum")
        require(wind - sum(g["power_output_maximum"][t - 1] for g in gens), "renewable maximum")
        require(abs(total_power[t] + wind - data["demand"][t - 1]), "demand")
        require(data["reserves"][t - 1] - (total_available[t] - total_power[t]), "reserve")

    print(f"recomputed_cost={production + startup:.6f}")
    if violations:
        for label, count in violations.most_common():
            print(f"VIOLATED {label}: {count} times, worst excess {worst[label]:.6f}")
    else:
        print("all checked constraints hold")


if __name__ == "__main__":
    main()
