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


def generate_cbmc_smt(cbmc, cnf_file):

    # example: summle_X8651_steps8_I1-2-2-4-4-8-25-100.cnf.gz

    filename = cnf_file.split(".")[0].split("_")

    if filename[0] != "summle":
        raise Exception("no summly cnf")

    # extract goal
    goalstr = filename[1]
    if goalstr[0] != "X":
        raise Exception("no goal string")
    goal = int(goalstr[1:])
    log.info("retrieved goal: %d", goal)

    # extract steps
    stepstr = filename[2]
    if stepstr[0:5] != "steps":
        raise Exception("no steps string")
    steps = int(stepstr[5:])
    log.info("retrieved steps: %d", steps)

    inputstr = filename[3]
    if inputstr[0] != "I":
        raise Exception("no input string")
    inputs = inputstr[1:].split("-")
    log.info("retrieved input: %r", inputs)

    # used constants
    inttype = "unsigned short"
    unwind = 10
    depth = 2000

    for uf in ["never", "always", ""]:
        log.info("run with UF %s", uf)
        extra_params = []
        if uf != "":
            extra_params += [f"--arrays-uf-{uf}"]

        output_file = "summle_X{0}_steps{1}_I{2}_UF{3}.smt2".format(
            goal, steps, "-".join([str(x) for x in inputs]), uf
        )

        # Generate CBMC CNF for given summle puzzle
        call = (
            [
                cbmc,
                "--property",
                "main.assertion.1",
                "--smt2",
                "--outfile",
                f"{output_file}",
                "--trace",
                "--trace-hex",
                "--object-bits",
                "16",
            ]
            + extra_params
            + [
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
        )

        try:
            callstr = " ".join(call)
            log.debug("Trying to run CBMC with %r", callstr)
            process = subprocess.run(callstr, shell=True)
        except Exception as e:
            log.warning("Error when running CBMC", e)
            return False

    return True


def main():

    logging.basicConfig(
        format="%(asctime)s,%(msecs)d %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s",
        datefmt="%Y-%m-%d:%H:%M:%S",
        level=logging.DEBUG,
    )

    generate_cbmc_smt("../cbmc/src/cbmc/cbmc", sys.argv[1])


if __name__ == "__main__":
    sys.exit(main())
