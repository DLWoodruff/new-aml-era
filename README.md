# A New AML Era

Materials for the note "A New AML Era" by David L. Woodruff. The note asks whether a large
language model can take the place of an algebraic modeling language. Claude Code translated the
"Tight" unit commitment formulation of Knueven, Ostrowski, and Watson into C that calls Gurobi's C
API, working from their paper alone. The result was then compared with EGRET.

Claude Code wrote the program, the experiment scripts, and much of the note's text. The author
directed the work, edited the text, and is responsible for the final content.

## Contents

- `note/`: the note's LaTeX source, bibliography, and PDF.
- `program/`: the C program and its CMake file, with the session's own `NOTES.md` (every
  departure from the paper, with reasons) and `RESULTS.md`.
- `data/`: the seven pglib-uc instances (CC BY 4.0; see `data/README.md`).
- `harness/`: the prompts, the isolation and timing scripts, the checker, and EGRET's cached
  answers. `harness/README.md` describes the procedure.
- `harness/extra_checks/`: two checks cited in the note that were run by hand afterward.
- `harness/runs/20261105-173249-run/`: the records of the experiment reported in the note, with
  `creation/` for the session that wrote the program and `refinement/` for the verification rounds
  and the one refinement session.

## Building the program

You need CMake, a C compiler, and Gurobi 13 with a license:

    export GUROBI_HOME=/path/to/gurobi/linux64
    cmake -S program -B build
    cmake --build build
    build/uc_solve --solution out.csv data/rts_gmlc_2020-01-27.json

## What is not here

- **The paper.** The sessions worked from the Optimization Online version of Knueven, Ostrowski,
  and Watson. It is not redistributed here, so get it from Optimization Online. Its hash is in
  `harness/runs/20261105-173249-run/creation/inputs.sha256`. Repeating the experiment requires
  it, placed at `work/paper.pdf` next to `harness/`, with `data/` and `program/DATA_FORMAT.md`
  beside it.
- **Paper content in the session logs.** In both `events.jsonl` files, page images and text
  of the paper in tool results were replaced by a marked placeholder. All other events are
  as recorded, and the sizes and timings in each `summary.txt` are from the original logs.
- **The Gurobi license ID**, replaced by `REDACTED` in the solver logs.
- **Four short smoke-test runs** that only checked the scripts.

Gurobi, EGRET, and Claude Code are not included.

## Rerunning the experiment

Set `GUROBI_ROOT` to your Gurobi directory. By default the sandbox uses whichever Claude Code is
installed; set `CLAUDE_BIN` to use another binary (the experiment used version 2.1.318). A rerun
also needs `bwrap` and starts new paid sessions, and it may well produce a different program.

`harness/sandbox.sh` and `harness/session.sh` were changed after the experiment to use variables
for these paths, and nothing else in them changed. The versions that actually ran are in
`harness/as_run/`. The records and `PROMPT.md` are unchanged and show the original machine's paths.
Change the Gurobi path in `PROMPT.md` to yours before rerunning.
