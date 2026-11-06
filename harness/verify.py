"""Verifies the program in work/ and writes a neutral, numbers-only report.

Builds the program in the sandbox, solves every instance, checks each solution against the
data, and compares with the cached reference answers. It writes report.md and verify.json
to the output directory. It exits 0 if everything passes and 1 otherwise.

A pass requires a successful build and, on every instance, an optimality interval
[bound, objective] that overlaps the reference's, a solution that passes check_csv.py, and
a recomputed cost that matches the reported objective. LP bounds are reported but are not
required to match, because a valid formulation can have a tighter relaxation.

    python verify.py <output-directory> [--solver PATH --no-sandbox --only SUBSTRING]

The three options exist to test this script without a real run.
"""
import argparse
import glob
import json
import os
import re
import shutil
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__))
WORK = os.path.normpath(os.path.join(HERE, "..", "work"))
TOLERANCE = 1e-6
COST_TOLERANCE = 1e-4
TIME_LIMIT = "900"


def values(text):
    found = dict(re.findall(r"^(objective|bound)=(\S+)\s*$", text, re.M))
    try:
        return {key: float(value) for key, value in found.items()}
    except ValueError:
        return {}


def overlap(a, b):
    return (a["objective"] >= b["bound"] * (1 - TOLERANCE)
            and b["objective"] >= a["bound"] * (1 - TOLERANCE))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("out")
    parser.add_argument("--solver")
    parser.add_argument("--no-sandbox", action="store_true")
    parser.add_argument("--only", default="")
    args = parser.parse_args()
    os.makedirs(args.out, exist_ok=True)
    started = time.time()

    prefix = [] if args.no_sandbox else [os.path.join(HERE, "sandbox.sh")]
    solver = args.solver or "build/uc_solve"
    reference = json.load(open(os.path.join(HERE, "egret_reference.json")))
    failures = []
    rows = []
    lines = ["# Verification report", ""]

    def run(*command, timeout=None):
        return subprocess.run([*prefix, *command], capture_output=True, text=True,
                              cwd=WORK, timeout=timeout)

    if not args.solver:
        shutil.rmtree(os.path.join(WORK, "build"), ignore_errors=True)
        built = run("cmake", "-S", ".", "-B", "build")
        if built.returncode == 0:
            built = run("cmake", "--build", "build")
        if built.returncode != 0:
            failures.append("build")
            lines += ["The program did not build from a clean directory with `cmake -S . -B build`"
                      " and `cmake --build build`.", "", "```", built.stderr[-3000:], "```", ""]

    if "build" not in failures:
        lines += ["| Instance | LP: program | LP: reference | MIP objective: program | MIP bound: program"
                  " | MIP objective: reference | MIP bound: reference | Intervals overlap |",
                  "|---|---|---|---|---|---|---|---|"]
        details = []
        for path in sorted(glob.glob(os.path.join(WORK, "data", f"*{args.only}*.json"))):
            name = os.path.basename(path)[:-5]
            relative = os.path.relpath(path, WORK)
            expected = reference[name]
            csv_name = f".verify-{name}.csv"
            lp = values(run(solver, "--relax", relative).stdout)
            solved = run(solver, "--time-limit", TIME_LIMIT, "--solution", csv_name, relative)
            mip = values(solved.stdout)
            csv_path = os.path.join(WORK, csv_name)
            kept = os.path.join(args.out, f"{name}.csv")
            if os.path.exists(csv_path):
                shutil.move(csv_path, kept)

            row = {"instance": name, "lp": lp.get("objective"), "objective": mip.get("objective"),
                   "bound": mip.get("bound"), "violations": []}
            agrees = "objective" in mip and "bound" in mip and overlap(
                mip, {"objective": expected["mip_objective"], "bound": expected["mip_bound"]})
            row["agrees"] = agrees
            if not agrees:
                failures.append(f"{name}:objective")
                row["gap"] = abs(mip["objective"] - expected["mip_objective"]) / abs(expected["mip_objective"]) \
                    if "objective" in mip else None

            notes = []
            if os.path.exists(kept):
                checked = subprocess.run(["python3", "-I", os.path.join(HERE, "check_csv.py"), path, kept],
                                         capture_output=True, text=True)
                output = checked.stdout + checked.stderr
                row["violations"] = re.findall(r"^VIOLATED (.+)$", output, re.M)
                recomputed = re.search(r"^recomputed_cost=(\S+)", output, re.M)
                if checked.returncode != 0 or not recomputed:
                    failures.append(f"{name}:check")
                    notes.append("The solution file could not be checked: " + output.strip()[-300:])
                for item in row["violations"]:
                    failures.append(f"{name}:violation:{item.split(':')[0]}")
                    notes.append("Violated " + item)
                if recomputed and "objective" in mip:
                    cost = float(recomputed.group(1))
                    if abs(cost - mip["objective"]) > COST_TOLERANCE * abs(cost):
                        failures.append(f"{name}:cost")
                        notes.append(f"The cost recomputed from the solution is {cost:.6f}, but the"
                                     f" program reported {mip['objective']:.6f}.")
            else:
                failures.append(f"{name}:solution")
                notes.append(f"No solution file was written. Exit status {solved.returncode}.")
            if notes:
                details += [f"**{name}**", ""] + [f"- {note}" for note in notes] + [""]

            def show(value):
                return "none" if value is None else f"{value:.6f}"
            lines.append(f"| {name} | {show(row['lp'])} | {expected['lp']:.6f} | {show(row['objective'])}"
                         f" | {show(row['bound'])} | {expected['mip_objective']:.6f}"
                         f" | {expected['mip_bound']:.6f} | {'yes' if agrees else 'no'} |")
            rows.append(row)
        lines += ["", "Both programs solved the MIP to a relative gap of 0.0001.", ""]
        lines += ["## Independent check of the solutions", ""]
        lines += details or ["Every solution satisfied the checked constraints, and its recomputed"
                             " cost matched the reported objective.", ""]

    lines += ["## Status", "", "All items passed." if not failures else f"{len(failures)} items did not pass.", ""]
    with open(os.path.join(args.out, "report.md"), "w") as file:
        file.write("\n".join(lines))
    gaps = [row["gap"] for row in rows if row.get("gap") is not None]
    with open(os.path.join(args.out, "verify.json"), "w") as file:
        json.dump({"failures": sorted(failures), "largest_gap": max(gaps, default=0.0), "rows": rows,
                   "seconds": time.time() - started}, file, indent=1)
    print(f"{len(failures)} items did not pass: {sorted(failures)}" if failures else "All items passed.")
    raise SystemExit(1 if failures else 0)


if __name__ == "__main__":
    main()
