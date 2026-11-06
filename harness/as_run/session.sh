#!/bin/bash
# Runs one isolated, timed Claude Code session. Both stages of the experiment use this,
# so they share the same model, permissions, and isolation.
#
#   session.sh <output-directory> <prompt-file> [extra claude arguments]
#
# Writes prompt.md, settings.json, versions.txt, inputs.sha256, events.jsonl, stderr.txt,
# start.txt, end.txt, timing.txt, and summary.txt to the output directory.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$(cd "$HERE/../work" && pwd)"
MODEL=claude-opus-5-5
EFFORT=high
TIME_LIMIT_SECONDS=14400

OUT="$1"
PROMPT="$2"
shift 2
mkdir -p "$OUT"

cp "$PROMPT" "$OUT/prompt.md"
cp "$HERE/settings.json" "$OUT/settings.json"
(cd "$WORK" && find . -type f -not -path './build/*' -exec sha256sum {} +) | sort -k2 > "$OUT/inputs.sha256"
{
  "$HERE/sandbox.sh" claude --version
  "$HERE/sandbox.sh" gcc --version | head -1
  "$HERE/sandbox.sh" cmake --version | head -1
  grep -E "define GRB_VERSION_(MAJOR|MINOR|TECHNICAL)" /home/dlw/software/gurobi1302/linux64/include/gurobi_c.h
  grep -E "^(PRETTY_NAME)" /etc/os-release
  echo "model=$MODEL effort=$EFFORT"
} > "$OUT/versions.txt"

ALLOWED=(
  "Read" "Edit" "Write" "Glob" "Grep"
  "Bash(cmake *)" "Bash(make)" "Bash(make *)" "Bash(gcc *)" "Bash(cc *)"
  "Bash(build/uc_solve *)" "Bash(./build/uc_solve *)" "Bash($WORK/build/uc_solve *)"
  "Bash(pdftotext *)" "Bash(pdftoppm *)" "Bash(pdfinfo *)"
  "Bash(mkdir *)" "Bash(rm -rf build)" "Bash(time *)" "Bash(timeout *)"
)

date -Is > "$OUT/start.txt"
START=$(date +%s.%N)
set +e
timeout "$TIME_LIMIT_SECONDS" "$HERE/sandbox.sh" claude -p "$(cat "$PROMPT")" \
    --model "$MODEL" \
    --effort "$EFFORT" \
    --output-format stream-json \
    --verbose \
    --no-session-persistence \
    --strict-mcp-config \
    --permission-mode acceptEdits \
    --permission-prompts none \
    --settings "$(cat "$HERE/settings.json")" \
    --add-dir /home/dlw/software/gurobi1302 \
    --allowedTools "${ALLOWED[@]}" \
    "$@" \
  < /dev/null 2> "$OUT/stderr.txt" | python3 -I "$HERE/stamp.py" > "$OUT/events.jsonl"
STATUS=${PIPESTATUS[0]}
set -e
END=$(date +%s.%N)
date -Is > "$OUT/end.txt"

python3 -I - "$START" "$END" "$STATUS" > "$OUT/timing.txt" <<'EOF'
import sys
start, end, status = float(sys.argv[1]), float(sys.argv[2]), sys.argv[3]
print(f"wall_clock_seconds={end - start:.1f}")
print(f"wall_clock_minutes={(end - start) / 60:.2f}")
print(f"exit_status={status}")
EOF

python3 -I "$HERE/summarize.py" "$OUT/events.jsonl" > "$OUT/summary.txt"
