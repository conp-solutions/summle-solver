#!/usr/bin/env python3
#
# Copyright (C) 2022, Norbert Manthey <nmanthey@conp-solutions.com>

import argparse
import logging
import os
import resource  # process limits
import subprocess
import sys


# Create logger
log = logging.getLogger(__name__)

TIMEOUT = 30


# ../cbmc/src/cbmc/cbmc --property main.assertion.1 --trace --trace-hex --object-bits 16 --unwind 10 --depth 2000 -DTYPE="unsigned short" -DGOAL=4 -DINPUTS=2,2,4,5,8,8,11,25 -DSTEPS=4  main.c |& tee log.log | grep "   =    "


def generate_cbmc_cnf(
    output_file,
    cbmc,
    goal,
    inputs,
    steps,
    inttype="unsigned short",
    unwind=10,
    depth=2000,
):
    """Generate CBMC CNF for given summle puzzle"""
    call = [
        cbmc,
        "--property",
        "main.assertion.1",
        "--dimacs",
        "--outfile",
        f"{output_file}",
        "--trace",
        "--trace-hex",
        "--object-bits",
        "16",
        "--unwind",
        f"{unwind}",
        "--depth",
        f"{depth}",
        f'-DTYPE="{inttype}"',
        f"-DGOAL={goal}",
        f'-DINPUTS={",".join([str(x) for x in inputs])}',
        f"-DSTEPS={steps}",
        "main.c",
    ]
    try:
        callstr = " ".join(call)
        log.debug("Trying to run CBMC with %r", callstr)
        process = subprocess.run(
            callstr, shell=True, stderr=subprocess.DEVNULL, stdout=subprocess.DEVNULL
        )
    except Exception as e:
        log.warning("Received exception %r when running CBMC", e)
        return False
    return True


def generate_benchmark_cnf():

    filename = "test.cnf"
    cbmc = "../cbmc/src/cbmc/cbmc"  # TODO: turn into parameter!
    mergesat = "../mergesat/build/release/bin/mergesat"  # TODO: turn into parameter!

    inputs = [1, 2, 2, 4, 4, 8, 25, 100]  # TODO: set of candidates

    targetted_goals = [4031, 4052, 7490, 8643, 11110, 111111, 111113]
    targetted_goals = [11110, 111111, 111113]

    for basegoal in targetted_goals:
        for goal in range(basegoal - 10, basegoal + 10):
            for steps in 5, 6, 7, 8:

                filename = "summle_X{0}_steps{1}_I{2}.cnf".format(
                    goal, steps, "-".join([str(x) for x in inputs])
                )

                zipped_file = f"{filename}.gz"
                if os.path.exists(zipped_file):
                    log.info("Found file already, skipping")
                    continue
                else:
                    log.info("File does not already exist, solving (%s)", zipped_file)

                generate_cbmc_cnf(filename, cbmc, str(goal), inputs, str(steps))

                minisat_solver_call = ["minisat", filename]
                mergesat_solver_call = [mergesat, filename]
                zip_call = ["gzip", "-f", filename]

                minisat_results = measure_call(
                    minisat_solver_call, output_file=subprocess.DEVNULL
                )
                # mergesat_results = measure_call(mergesat_solver_call, output_file=subprocess.DEVNULL)

                if (
                    minisat_results["status_code"] == 10
                    or minisat_results["status_code"] == 20
                ):
                    log.info("Solved successfully within timeout, dropping")
                    os.remove(filename)
                else:
                    log.info("Unsolved file during timeout, keeping")
                    measure_call(zip_call, output_file=subprocess.DEVNULL)


def set_hour_timeout():
    """Set max runtime of the calling process to TIMEOUT seconds."""
    resource.setrlimit(resource.RLIMIT_CPU, (TIMEOUT, TIMEOUT))


def measure_call(call, output_file, container_id=None, expected_code=None):
    """Run the given command, return measured performance data."""

    log.debug("Run solver: %r", call)
    pre_stats = os.times()
    # Note: using a docker jail here reduces the precision of the measurement
    process = subprocess.run(
        call, stdout=output_file, stderr=subprocess.DEVNULL, preexec_fn=set_hour_timeout
    )
    post_stats = os.times()
    if expected_code is not None and process.returncode != expected_code:
        log.error("Detected unexpected solver behavior, printing output")
        print("STDOUT: ", process.stdout.decode("utf_8"))
        print("STDERR: ", process.stderr.decode("utf_8"))
    return {
        "cpu_time_s": (post_stats[2] + post_stats[3]) - (pre_stats[2] + pre_stats[3]),
        "wall_time_s": post_stats[4] - pre_stats[4],
        "status_code": process.returncode,
    }


def main():

    logging.basicConfig(
        format="%(asctime)s,%(msecs)d %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s",
        datefmt="%Y-%m-%d:%H:%M:%S",
        level=logging.DEBUG,
    )

    generate_benchmark_cnf()


if __name__ == "__main__":
    sys.exit(main())
