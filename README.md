summle-solver, Norbert Manthey, 2022

# Solving Summle.net

A simple math puzzle is presented on https://summle.net/. Given a target number
X, and available input numbers, the goal is to combine the inputs with the math
operations addition, substraction, multiplication and division. Each input, and
intermediate results, can be used exactly once. The task is to place the numbers
and operations so that the target number is the result of an operation.

summle.net was highlighted on https://news.ycombinator.com/item?id=30554994

## Implemented Solution

Similarly to puzzles like Sudoku, the math puzzle could be converted to a logic,
and a reasoner could be applied to a direct translation. This package uses a
different route, by allowing other tools to test their reasoning strength as
well.

The solution for a given summle.net puzzle is encoded in the C language. The
input number and goal numbers are represented by integer variables. For each
position in the puzzle grid, an array of integers stores the position of the
selected input number or intermediate result. Similarly, the operations used
for the each calculation is selected and stored in an array.

The rest of the program verifies whether the chosen position selection and
operations result in a calculation that reaches the goal in a given number of
steps. Only if a valid solution is reached, an assertion is triggered. For
release binaries, the same exit code is used in this case. Otherwise, the
exit code of the binary will be zero.

The values for the selected-position-array and operations are not defined, but
instead are initialized from stdin. This allows tools like fuzzers to select
place the numbers and operators in the puzzle and validate the result via the
exit code.

### Symmetries in Puzzle

The puzzle provides many symmetries. Addition and multiplication are
commutative, so that for each of these operations at least two solutions exist.
Furthermore, the order of calculations can be mixed, as the only requirement is
to calculate intermediate results before the result is needed. Any earlier
calculation could be used, multiplying the number of solutions further.

### Using CBMC to solve summle.net

The input file main.c has a single assertion. This assertion can only be reached
and falsified if the placed numbers in the puzzle describe a valid solution to
the puzzle. Hence, this assertion can be targetted by CBMC.

The board to solve has to be defined via compile time parameters. An example is
given below, together with a command to filter the relevant lines that would
represent the output:

```
cbmc --property main.assertion.1 \
     --trace --trace-hex \
     --object-bits 16 \
     --unwind 10 --depth 2000 \
         -DTYPE="unsigned short" \
         -DGOAL=1001 \
         -DINPUTS=2,2,4,5,8,8,25 \
         -DSTEPS=5
     main.c
|& grep "   =    "
```

Example output:

```
  format=" %d    %s    %d   =    %d\n" (0x16200000 0000000)
 8        4   =    2
  format=" %d    %s    %d   =    %d\n" (0x16200000 0000000)
 2        2   =    1
  format=" %d    %s    %d   =    %d\n" (0x16200000 0000000)
 5        8   =    40
  format=" %d    %s    %d   =    %d\n" (0x16200000 0000000)
 25        40   =    1000
  format=" %d    %s    %d   =    %d\n" (0x16200000 0000000)
 1        1000   =    1001
```

### Using AFL to solve summle.net

Similarly to CBMC, the puzzle can be fed with input by AFL. Note, before calling
afl-fuzz, the afl-in and afl-out directories need to be created and filled with
example input.

```
afl-gcc main.c -o afl-main \
    -DTYPE="unsigned short" \
    -DGOAL=1001 \
    -DINPUTS=2,2,4,5,8,8,25 \
    -DSTEPS=5
afl-fuzz -i afl-in -o afl-out ./afl-main
```
