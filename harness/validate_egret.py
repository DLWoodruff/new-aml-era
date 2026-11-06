"""Solve a pglib-uc instance with EGRET's formulation T, as a check on uc_tight.

EGRET is the authors' reference implementation. Usage:

    python validate_egret.py instance.json [--relax] [--mip-gap GAP] [--time-limit S]

Prints the same "objective=" line as uc_tight.
"""
import argparse
import time

import pyomo.environ as pyo
from egret.models.unit_commitment import create_tight_unit_commitment_model
from egret.parsers.pglib_uc_parser import create_ModelData


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("instance")
    parser.add_argument("--relax", action="store_true")
    parser.add_argument("--mip-gap", type=float, default=1e-4)
    parser.add_argument("--time-limit", type=float, default=None)
    args = parser.parse_args()

    # There is no network in pglib-uc, so copper-plate power balance is EGRET's
    # counterpart of the single-bus case.
    model = create_tight_unit_commitment_model(
        create_ModelData(args.instance),
        network_constraints="copperplate_power_flow",
        relaxed=args.relax,
    )
    solver = pyo.SolverFactory("gurobi_direct")
    solver.options["OutputFlag"] = 0
    if not args.relax:
        solver.options["MIPGap"] = args.mip_gap
    if args.time_limit is not None:
        solver.options["TimeLimit"] = args.time_limit

    start = time.perf_counter()
    results = solver.solve(model)
    elapsed = time.perf_counter() - start

    objective = next(model.component_data_objects(pyo.Objective, active=True))
    stats = model.nvariables(), model.nconstraints()
    print(f"egret columns={stats[0]} rows={stats[1]} "
          f"termination={results.solver.termination_condition} solve_seconds={elapsed:.2f}")
    print(f"objective={pyo.value(objective):.6f}")
    if not args.relax:
        print(f"bound={results.problem.lower_bound:.6f}")


if __name__ == "__main__":
    main()
