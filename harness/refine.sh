#!/bin/bash
# Stage 2: validation and refinement. Run it after run.sh:
#
#   ./refine.sh runs/<timestamp>-run                 up to 5 rounds of refinement
#   ./refine.sh runs/<timestamp>-run --verify-only   verification only, with no session
#
# Each round verifies the program outside any session. If something fails, a new isolated
# session gets the code, NOTES.md, and the numbers-only report, with no access to EGRET. It
# stops when everything passes, after MAX_ROUNDS sessions, or when a round makes no
# progress: the same items fail and the largest difference has not shrunk.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORK="$HERE/../work"
RUN="$(cd "${1:?usage: ./refine.sh runs/<timestamp>-run [--verify-only]}" && pwd)"
MAX_ROUNDS=5

if [[ ! -f "$HERE/egret_reference.json" ]]; then
  echo "Computing the reference answers once with EGRET."
  "$HERE/make_reference.sh" || exit 1
fi

OUT="$RUN/refinement"
mkdir -p "$OUT"
STOP="reached the limit of $MAX_ROUNDS refinement sessions"
PREVIOUS=""

for ((round = 0; round <= MAX_ROUNDS; round++)); do
  ROUND="$OUT/round$round"
  mkdir -p "$ROUND/source"
  (cd "$WORK" && find . -maxdepth 2 -type f -not -path './build/*' -not -path './data/*' -not -name 'paper.pdf' \
    -exec cp --parents {} "$ROUND/source/" \;)

  python3 -I "$HERE/verify.py" "$ROUND/verification"
  PASSED=$?
  if [[ $PASSED -eq 0 ]]; then
    STOP="all items passed after $round refinement sessions"
    break
  fi
  if [[ "${2:-}" == "--verify-only" ]]; then
    STOP="verification only"
    break
  fi
  if [[ $round -eq $MAX_ROUNDS ]]; then
    break
  fi

  CURRENT="$ROUND/verification/verify.json"
  if [[ -n "$PREVIOUS" ]] && python3 -I - "$PREVIOUS" "$CURRENT" <<'PY'
import json, sys
old, new = (json.load(open(path)) for path in sys.argv[1:3])
same = old["failures"] == new["failures"]
sys.exit(0 if same and new["largest_gap"] >= old["largest_gap"] else 1)
PY
  then
    STOP="no progress in round $round"
    break
  fi
  PREVIOUS="$CURRENT"

  cp "$ROUND/verification/report.md" "$WORK/REFINEMENT_REPORT.md"
  "$HERE/session.sh" "$ROUND/session" "$HERE/REFINE_PROMPT.md"
  rm -f "$WORK/REFINEMENT_REPORT.md"
done

python3 -I "$HERE/refine_summary.py" "$OUT" "$STOP" | tee "$OUT/summary.txt"
