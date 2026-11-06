# Clean rerun of the translation experiment

This is the working README from the experiment. Here, `work/` is not included: the paper is not
redistributed, and the program and data are in `../program` and `../data`. See the top-level README.

Stage 1: Claude Code translates formulation T from the paper into C and Gurobi, with no
access to any implementation of the model. Stage 2: the program is compared with the
authors' EGRET, and new sessions refine it using only a report of the numbers. Both are timed.

- `work/` holds what the sessions see: the paper, seven instances, and the data format.
- `harness/` holds the prompts, the isolation and timing scripts, and the checks. It does
  not exist inside the sessions.

## Steps

From `harness/`, in a plain terminal (not inside a Claude Code session):

    ./run.sh                          # stage 1, creation
    ./refine.sh runs/<timestamp>-run  # stage 2, validation and refinement

Nothing is asked of you during either one. Stage 2 stops when everything passes, after 5
refinement sessions, or when a round makes no progress. To see the verification results
without starting any session, add `--verify-only`.

Everything is recorded in `harness/runs/<timestamp>-run/`:

- `creation/`: the stage 1 session.
- `refinement/summary.txt`: why stage 2 stopped, and the results and times for each round.
- `refinement/roundN/source/`: the code and notes at the start of round N, for comparison.
- `refinement/roundN/verification/`: `report.md`, the numbers the next session saw, plus
  `verify.json` and the solution files.
- `refinement/roundN/session/`: the refinement session, if one was needed.

Each session directory has `timing.txt` (total wall-clock time), `summary.txt` (API time,
tokens, estimated cost, milestones, permission denials, and an audit of any tool call that
mentions other implementations or the network), `events.jsonl` (the complete record of the
session), and `prompt.md`, `settings.json`, `inputs.sha256`, and `versions.txt` (what was run).

Verification time is recorded separately and is not part of any session's time, because it
is the experimenter's check rather than Claude Code's work.

The sessions' program, `NOTES.md`, and `RESULTS.md` end up in `work/`. To repeat the run, first
move those and `build/` out of `work/`, because `run.sh` refuses to start otherwise.

## What a refinement session is told

It gets the code, `NOTES.md`, and `REFINEMENT_REPORT.md`, which holds each instance's LP and
MIP numbers for the program and for the reference, whether the MIP intervals overlap, and any
constraint violations the checker found, with counts and sizes. The report does not name a
cause or suggest a fix, and the session cannot see EGRET or the checker. It has no memory of
earlier rounds except what `NOTES.md` records.

A pass requires a clean build and, on every instance, overlapping intervals [bound, objective],
a clean check, and a recomputed cost that matches the reported objective. LP bounds are
reported but need not match, because a valid formulation can have a tighter relaxation.
`egret_reference.json` caches EGRET's answers, so EGRET runs once and never during a session.

## What the isolation does and does not do

- `/home`, `/tmp`, and the environment start empty, and only `work/`, Gurobi, the Claude
  Code binary, and its login file are put back. EGRET, the noAML project, conda
  environments, and earlier transcripts and memory do not exist inside.
- The network stays open, because Claude Code needs it. Web tools, `curl`, `wget`, `git`,
  `pip`, and `python` are denied by permission rules, and Bash is limited to an allowlist.
  This is enforced by Claude Code rather than the operating system, so check the isolation
  audit in `summary.txt`.
- `paper.pdf` itself names the EGRET and pglib-uc repositories, so the session knows that
  they exist.
- The prompt supplies C_LP, C_RP, and C^w. `DATA_FORMAT.md`, both prompts, the report format,
  and the checker were written by a Claude Code session that had already seen the data and EGRET. Both are worth disclosing in the note.
