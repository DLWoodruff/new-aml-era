"""Summarizes the refinement stage: outcome, and time for each round."""
import glob
import json
import os
import re
import sys

out, stop = sys.argv[1], sys.argv[2]
print(f"stopped: {stop}")
total = 0.0
for path in sorted(glob.glob(os.path.join(out, "round*")), key=lambda p: int(p.rsplit("round", 1)[1])):
    verify = json.load(open(os.path.join(path, "verification", "verify.json")))
    line = (f"{os.path.basename(path)}: {len(verify['failures'])} items failing,"
            f" largest objective difference {verify['largest_gap']:.6%},"
            f" verification {verify['seconds'] / 60:.1f} min")
    timing = os.path.join(path, "session", "timing.txt")
    if os.path.exists(timing):
        seconds = float(re.search(r"wall_clock_seconds=(\S+)", open(timing).read()).group(1))
        total += seconds
        line += f", session {seconds / 60:.1f} min"
    print(line)
print(f"total_refinement_session_minutes: {total / 60:.2f}")
