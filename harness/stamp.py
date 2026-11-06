"""Adds an arrival time to each line of Claude Code's JSON event stream."""
import json
import sys
import time

for line in sys.stdin:
    line = line.strip()
    if line:
        print(json.dumps({"arrived": time.time(), "event": json.loads(line)}), flush=True)
