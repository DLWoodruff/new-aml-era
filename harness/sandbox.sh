#!/bin/bash
# Runs a command in the isolated environment used for the rerun.
#
# /home, /tmp, /run/user, and the environment all start empty. Only these are put
# back: the working directory (read-write), Gurobi (read-only), the Claude Code
# binary (read-only), and the Claude Code login file. EGRET, the noAML project,
# conda environments, and earlier Claude Code transcripts and memory do not exist
# inside. The network stays open, because Claude Code needs its API; run.sh limits
# web access through permission rules, and summarize.py audits the commands used.
set -euo pipefail

WORK="$(cd "$(dirname "${BASH_SOURCE[0]}")/../work" && pwd)"
GUROBI_ROOT="${GUROBI_ROOT:?set GUROBI_ROOT to your Gurobi directory}"
CLAUDE_BIN="${CLAUDE_BIN:-$(readlink -f "$(command -v claude)")}"
CREDENTIALS="$HOME/.claude/.credentials.json"

exec bwrap \
  --ro-bind / / \
  --dev /dev \
  --proc /proc \
  --tmpfs /tmp \
  --tmpfs /run/user \
  --tmpfs /home \
  --bind "$WORK" "$WORK" \
  --ro-bind "$GUROBI_ROOT" "$GUROBI_ROOT" \
  --ro-bind "$CLAUDE_BIN" "$HOME/.local/bin/claude" \
  --bind "$CREDENTIALS" "$CREDENTIALS" \
  --chdir "$WORK" \
  --die-with-parent \
  env -i \
    HOME="$HOME" \
    USER="$USER" \
    LANG=C.UTF-8 \
    TMPDIR=/tmp \
    PATH="$HOME/.local/bin:/usr/bin:/bin" \
    GUROBI_HOME="$GUROBI_ROOT/linux64" \
    LD_LIBRARY_PATH="$GUROBI_ROOT/linux64/lib" \
    GRB_LICENSE_FILE="$GUROBI_ROOT/license/gurobi.lic" \
    CLAUDE_CODE_DISABLE_AUTO_MEMORY=1 \
    BASH_DEFAULT_TIMEOUT_MS=900000 \
    BASH_MAX_TIMEOUT_MS=3600000 \
  "$@"
