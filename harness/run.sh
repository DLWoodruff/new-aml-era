#!/bin/bash
# Stage 1: creates the C program from the paper alone, and times it. Start it yourself:
#
#   ./run.sh           the real run, which needs an empty work/ apart from its inputs
#   ./run.sh --smoke   a short, cheap check of the isolation, which changes nothing in work/
#
# Nothing is asked of a person during the run, so wall-clock time is Claude Code's time,
# including the time spent compiling and solving. Stage 2 is refine.sh.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$HERE/../work"

if [[ "${1:-}" == "--smoke" ]]; then
  RUN="$HERE/runs/$(date +%Y%m%d-%H%M%S)-smoke"
  "$HERE/session.sh" "$RUN" "$HERE/SMOKE_PROMPT.md" --max-budget-usd 1
else
  for leftover in "$WORK/NOTES.md" "$WORK/RESULTS.md" "$WORK/CMakeLists.txt" "$WORK/build" "$WORK/REFINEMENT_REPORT.md"; do
    if [[ -e "$leftover" ]]; then
      echo "$leftover exists, so work/ is not clean. Refusing to start." >&2
      exit 1
    fi
  done
  RUN="$HERE/runs/$(date +%Y%m%d-%H%M%S)-run"
  "$HERE/session.sh" "$RUN/creation" "$HERE/PROMPT.md"
fi

SESSION="$RUN"
[[ -d "$RUN/creation" ]] && SESSION="$RUN/creation"
cat "$SESSION/summary.txt" "$SESSION/timing.txt"
echo "Run directory: $RUN"
