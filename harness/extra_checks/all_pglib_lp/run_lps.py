"""LP relaxation of every pglib-uc instance: the program in both forms of (35), then EGRET.

    python -I run_lps.py program|egret PGLIB_DIR UC_SOLVE VALIDATE_EGRET RESULTS.jsonl

Solves run one at a time. Each result is appended as one JSON line, and a solve already in
the file is skipped, so an interrupted run can be resumed.
"""
import glob
import json
import os
import re
import subprocess
import sys
import time

stage, pglib, uc_solve, validate, results = sys.argv[1:6]
TIMEOUT = 7200

done = set()
if os.path.exists(results):
    with open(results) as file:
        for line in file:
            record = json.loads(line)
            done.add((record["instance"], record["solver"]))


def run(instance, solver, command):
    if (instance, solver) in done:
        return
    start = time.time()
    record = {"instance": instance, "solver": solver}
    try:
        finished = subprocess.run(command, capture_output=True, text=True, timeout=TIMEOUT)
        record["returncode"] = finished.returncode
        record["wall_seconds"] = round(time.time() - start, 2)
        for key, pattern in (("objective", r"^objective=(\S+)$"),
                             ("status", r"^status=(\S+)$"),
                             ("solver_seconds", r"^runtime=(\S+)$"),
                             ("termination", r"termination=(\S+)"),
                             ("size", r"^size=(.+)$"),
                             ("egret_size", r"^egret (columns=\S+ rows=\S+)"),
                             ("load_shed_mwh", r"^load_shed_mwh=(\S+)$"),
                             ("over_generation_mwh", r"^over_generation_mwh=(\S+)$"),
                             ("reserve_shortfall_mwh", r"^reserve_shortfall_mwh=(\S+)$")):
            match = re.search(pattern, finished.stdout, re.M)
            if match:
                record[key] = match.group(1)
        if finished.returncode != 0:
            record["stderr"] = finished.stderr[-2000:]
    except subprocess.TimeoutExpired:
        record["returncode"] = None
        record["wall_seconds"] = round(time.time() - start, 2)
        record["status"] = "HARNESS_TIMEOUT"
    with open(results, "a") as file:
        file.write(json.dumps(record) + "\n")
    print(json.dumps(record), flush=True)


paths = sorted(glob.glob(os.path.join(pglib, "*", "*.json")))
if len(paths) != 56:
    sys.exit(f"expected 56 instances, found {len(paths)}")
for path in paths:
    instance = os.path.relpath(path, pglib)[:-5]
    if stage == "program":
        run(instance, "program", [uc_solve, "--relax", "--time-limit", "3600", path])
        run(instance, "program-first-ramp-u0",
            [uc_solve, "--relax", "--time-limit", "3600", "--variant", "first-ramp-u0", path])
    else:
        run(instance, "egret", [sys.executable, validate, "--relax", path])
