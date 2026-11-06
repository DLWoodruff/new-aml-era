The current directory contains:

- `paper.pdf`: a paper on mixed-integer programming formulations for the unit commitment problem.
- `data/`: seven instances in JSON.
- `DATA_FORMAT.md`: a description of the instance format.

Implement the "Tight" formulation, denoted T and specified in Table 3 of the paper, as a C program that builds the model itself and passes it to Gurobi through Gurobi's C API. Do not use an algebraic modeling language. Gurobi is installed at `/home/dlw/software/gurobi1302/linux64`, which is also the value of `$GUROBI_HOME`. There is no JSON library available, so include your own reader.

Work only from the paper and the data. Do not look for or use any other implementation of this model, and do not use the internet.

The data contain no penalties or shut-down costs. Use C_LP = 10000, C_RP = 1000, and C^w = 0.

Requirements:

1. Provide a `CMakeLists.txt` that builds an executable named `uc_solve` in `build/`.
2. The command line is `build/uc_solve [--relax] [--time-limit SECONDS] [--solution FILE] instance.json`, where `--relax` solves the LP relaxation.
3. Use a MIP gap of 0.0001.
4. Print `objective=<value>` on a line of its own and, for a MIP solve, `bound=<value>` on another.
5. `--solution FILE` writes a CSV file with the header `generator,period,on,power,available`. Write one row for each thermal generator and each period 1 to T, where `generator` is the generator's name, `on` is 0 or 1, `power` is its total output in MW, and `available` is its maximum power available in MW. Also write one row for each period with `generator` set to `RENEWABLE`, `power` set to the total renewable output, and `on` and `available` left empty.
6. Build the program and run it on all seven instances, with a time limit of 600 seconds per solve. Write the results to `RESULTS.md`.
7. In `NOTES.md`, record every decision you made where the paper left something unspecified or where you departed from it, with your reason.

Finish when these are done.
