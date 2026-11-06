"""Runs validate_egret.py on each instance and writes egret_reference.json."""
import glob
import json
import os
import re
import subprocess
import sys
import time

here, data = sys.argv[1], sys.argv[2]
script = os.path.join(here, "validate_egret.py")


def solve(instance, *options):
    done = subprocess.run([sys.executable, script, *options, instance],
                          capture_output=True, text=True, check=True, cwd=here)
    values = dict(re.findall(r"^(objective|bound)=(\S+)$", done.stdout, re.M))
    return {key: float(value) for key, value in values.items()}


result = {"mip_gap": 1e-4}
for path in sorted(glob.glob(os.path.join(data, "*.json"))):
    name = os.path.basename(path)[:-5]
    start = time.time()
    lp = solve(path, "--relax")
    mip = solve(path, "--time-limit", "900")
    result[name] = {"lp": lp["objective"], "mip_objective": mip["objective"], "mip_bound": mip["bound"]}
    print(f"{name}: {result[name]} ({time.time() - start:.0f} s)", flush=True)

with open(os.path.join(here, "egret_reference.json"), "w") as file:
    json.dump(result, file, indent=1)
