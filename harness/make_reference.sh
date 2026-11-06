#!/bin/bash
# Solves every instance once with the authors' EGRET and caches the answers in
# egret_reference.json. It runs outside the sandbox, and no Claude Code session can see
# the result, except through the numbers in a refinement report.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source ~/.bashrc 2>/dev/null
eval "$(conda shell.bash hook 2>/dev/null)"
conda activate mpisppy12

python -I "$HERE/make_reference.py" "$HERE" "$HERE/../work/data"
