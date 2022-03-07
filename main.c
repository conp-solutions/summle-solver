/*

summle.net

Spotted via https://news.ycombinator.com/item?id=30554994
Take input from https://summle.net/

Run like this:
```
../cbmc/src/cbmc/cbmc --property main.assertion.1 --trace --trace-hex --object-bits 16 --unwind 10 --depth 2000 main.c |& tee log.log | grep "   =    "
```

To solve more/new problems, add problems below (look for '  // FIXME: add more problems here!'


Example problems:

Problem 2022-03-05
  X = 323
  inputs: {3,4,7,8,25,100}
  8/4 = 2
  25 - 2 = 23
  3 * 100 = 300
  300 + 23 = 323
  In C language:
  T X = 323;
  T maxsteps = 5;
  #define M 6
  T inputs[M]= {3,4,7,8,25,100};
  T m = M;

  // to validate, example solution (where to put which input or intermediate value)
  // 1st column
  solution_positions_and_ops[0] = 4; // 4th input
  solution_positions_and_ops[1] = 5; // 5th input
  solution_positions_and_ops[2] = 1; // 1st input
  solution_positions_and_ops[3] = 1+maxsteps + 3; // solution from line 3
  // 2nd column
  solution_positions_and_ops[maxsteps+0] = 2; // 2nd input
  solution_positions_and_ops[maxsteps+1] = 1+maxsteps + 1; // solution from line 1
  solution_positions_and_ops[maxsteps+2] = 6; // 6th input
  solution_positions_and_ops[maxsteps+3] = 1+maxsteps + 2; // solution from line 2
  // operations
  solution_positions_and_ops[2*maxsteps+0] = 3; // division in line 1
  solution_positions_and_ops[2*maxsteps+1] = 2; // substraction in line 2
  solution_positions_and_ops[2*maxsteps+2] = 1; // multiplication in line 3
  solution_positions_and_ops[2*maxsteps+3] = 0; // addition in line 4


Problem 2022-03-06
  X = 420
  inputs: 1,4,5,6,6,50
  4x6
  24x5
  6*50
  120 + 320

*/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


typedef unsigned short T; // we operate below 64K (for now)

T op_add(T a, T b) {
    // could restrict that a <= b wrt symmetry breaking for this puzzle
    T c = a+b;
    if (c < a || c < b) { // overflow of unsigned type!
        printf("Multiplication resulted in overflow, abort!\n");
        assert (0); // allow to stop here for debugging
        exit (0);
    }
    return c;
}

T op_mult(T a, T b) {
    // could restrict that a <= b wrt symmetry breaking for this puzzle
    T c = a*b;
    if (c < a || c < b) { // overflow of unsigned type!
        printf("Multiplication resulted in overflow, abort!\n");
        assert (0); // allow to stop here for debugging
        exit (0);
    }
    return c;
}

T op_sub(T a, T b) {
    T c = a-b;
    if (c > a) { // overflow of unsigned type!
        printf("Substraction below 0, abort!\n");
        assert (0); // allow to stop here for debugging
        exit (0);
    }
    return c;
}

T op_div(T a, T b) {
    if (b == 0 || a < b) {
        printf("Division parameters invalid for integer division!\n");
        assert (0); // allow to stop here for debugging
        exit (0);        
    }
    T c = a / b;
    if (c * b != a) {
        printf("Division does not result in integers, abort!\n");
        assert (0); // allow to stop here for debugging
        exit (0);
    }
    return c;
}


int summle(T X, T* inputs, T m, T maxsteps, T *solution_positions_and_ops)
{
    // actual numbers/values during the computation
    T position_pool_size = 1 + m + maxsteps;
    T values[position_pool_size];  // use 0 as 'no position': m input positions, and maxsteps solutions
    
    // copy values from input into global array of values - note: index 1 to m will not be written from here on!
    values[0] = 0;
    for (T i = 0; i <= m; ++ i) {
      values[i+1] = inputs[i];
    }
    for (T i = m+1; i < position_pool_size; ++ i) {
      values[i] = 0;
    }
    

    // TODO: initialize nondet, or from fuzzers stdin!
    T A_n_pos[maxsteps]; // initialize for CBMC of fuzzer
    T B_n_pos[maxsteps]; // initialize for CBMC of fuzzer
    T C_n_ops[maxsteps]; // select operation to compute, can be duplicated, TODO: could be restricted to 0-3, we only have 4 ops at the moment

    // check a given solution
    if (solution_positions_and_ops) {
        T index = 0;
        for (T i = 0 ; i < maxsteps; ++i ) A_n_pos[i] = solution_positions_and_ops[index++];
        for (T i = 0 ; i < maxsteps; ++i ) B_n_pos[i] = solution_positions_and_ops[index++];
        for (T i = 0 ; i < maxsteps; ++i ) C_n_ops[i] = solution_positions_and_ops[index++];
    }

    for (T i = 0 ; i < maxsteps; ++ i) {
      if (C_n_ops[i] >= 4) {printf("operation[%d]=%d exceeds minimum (4)\n", i, C_n_ops[i]); exit(0); }
    }
      

    // build up conditions wrt the game
    T invalid = 0; // needs to remain 0 for valid game input
    
    // TODO: could be factored into a function!
    // check all A's use different inputs
    for (T i = 0 ; i < maxsteps && !invalid; ++ i) {
      if (A_n_pos[i] == 0) continue; // not using any value, continue
      
      // cannot use a value from a later equation
      invalid = invalid || !!(A_n_pos[i] > m + i); // TODO: check off-by-one!
      if (invalid) {printf("Failed A_pos[%d]=%d minimum (%d) check\n", i, A_n_pos[i], m + i); exit(0); }
      invalid = invalid || !!(A_n_pos[i] >= position_pool_size); // TODO: check off-by-one!
      if (invalid) {printf("Failed A_pos[%d]=%d maximum check\n", i, A_n_pos[i]); exit(0); }

      // avoid duplicates with b positions
      invalid = invalid || !!(A_n_pos[i] == B_n_pos[i]);
      if (invalid) {printf("Failed A-cross-B duplicate check A_pos[%d]=B_pos[%d], (%d == %d) maximum check\n", i,i, A_n_pos[i], B_n_pos[i]); exit(0); }
      
      for (T j=i+1; j < maxsteps && !invalid; ++j ) {
        if (A_n_pos[j] == 0) continue; // not using any value, continue
        // update invalid, unless it's false already
        invalid = invalid || !!(A_n_pos[i] == A_n_pos[j]);
        if (invalid) {printf("Failed A duplicate check A_pos[%d]=A_pos[%d], (%d == %d) maximum check\n", i,j, A_n_pos[i], A_n_pos[j]); exit(0); }
        
        // avoid duplicates with b positions
        invalid = invalid || !!(A_n_pos[i] == B_n_pos[j]);
        if (invalid) {printf("Failed A-cross-B duplicate check A_pos[%d]=B_pos[%d], (%d == %d) maximum check\n", i,j, A_n_pos[i], B_n_pos[j]); exit(0); }
      }
    }

    // check all B's use different inputs
    for (T i = 0 ; i < maxsteps && !invalid; ++ i) {
      if (B_n_pos[i] == 0) continue; // not using any value, continue
      
      invalid = invalid || !!(B_n_pos[i] > m + i); // TODO: check off-by-one!
      if (invalid) {printf("Failed B_pos[%d]=%d minimum check\n", i, B_n_pos[i]); exit(0); }
      invalid = invalid || !!(B_n_pos[i] >= position_pool_size); // TODO: check off-by-one!
      if (invalid) {printf("Failed B_pos[%d]=%d maximum check\n", i, B_n_pos[i]); exit(0); }
      
      for (T j=i+1; j < maxsteps && !invalid; ++j ) {
        if (B_n_pos[j] == 0) continue; // not using any value, continue
        // update invalid, unless it's false already
        invalid = invalid || !!(B_n_pos[i] == B_n_pos[j]);
        if (invalid) {printf("Failed B duplicate check B_pos[%d]=B_pos[%d], (%d == %d) maximum check\n", i,j, B_n_pos[i], B_n_pos[j]); exit(0); }

        // avoid duplicates with b positions (needs both, due to i<j
        invalid = invalid || !!(B_n_pos[i] == A_n_pos[j]);
        if (invalid) {printf("Failed B-cross-A duplicate check A_pos[%d]=B_pos[%d], (%d == %d) maximum check\n", i,j, A_n_pos[i], B_n_pos[j]); exit(0); }
      }
    }

    // invalid input locations selected
    if (invalid) {
        printf("failed position selection check, abort\n");
        return 1;
    }
    
    // compute given values - ops abort program for invalid input!
    T* C_n = &(values[1+m]);
    for( T i = 0; i < maxsteps; ++ i ) {
      if (A_n_pos[i] == 0 || B_n_pos[i] == 0 ) {
        C_n[i] = 0;
      } else {
        switch (C_n_ops[i]) {
          case 1:
            C_n[i] = op_mult(values[A_n_pos[i]], values[B_n_pos[i]]);
            break;
          case 2:
            C_n[i] = op_sub(values[A_n_pos[i]], values[B_n_pos[i]]);
            break;
          case 3:
            C_n[i] = op_div(values[A_n_pos[i]], values[B_n_pos[i]]);
            break;
          default:
            C_n[i] = op_add(values[A_n_pos[i]], values[B_n_pos[i]]);
            break;
        }
      }
    }
    
    // check whether some solution matches the given input
    T found_solution = 0;
    for (T i = 0; i < maxsteps; ++ i ) {
      if(A_n_pos[i] != 0 && B_n_pos[i] != 0)
        found_solution = found_solution || (C_n[i] == X);
    }
    
    if (found_solution) {
        printf("found a solution, visualizing ...\n");
      
        char *ops[4] = {"+", "x", "-", "/"};
        for(T i = 0 ; i < maxsteps; ++i) {
            if(A_n_pos[i] != 0 && B_n_pos[i] != 0) {
              printf (" %d    %s    %d   =    %d\n",
                values[A_n_pos[i]],
                ops[C_n_ops[i]],
                values[B_n_pos[i]],
                C_n[i]);
            }
        }
        printf ("X = %d\n", X);

        printf ("positions\n");
        printf ("As: ");
        for(T i = 0 ; i < maxsteps; ++i) printf (" %d", A_n_pos[i]);
        printf ("\n");

        printf ("Bs: ");
        for(T i = 0 ; i < maxsteps; ++i) printf (" %d", B_n_pos[i]);
        printf ("\n");

        printf ("OPs: ");
        for(T i = 0 ; i < maxsteps; ++i) printf (" %d", C_n_ops[i]);
        printf ("\n");
    }
    
    return found_solution == 0 ? 2 : 0;
}

int main()
{
  // spotted via https://news.ycombinator.com/item?id=30554994
  // take input from https://summle.net/

#ifndef problem
#define problem 2
#endif

  // FIXME: add more problems here!

#if problem==0
  T X = 323;
  T maxsteps = 5;
  #define M 6
  T inputs[M]= {3,4,7,8,25,100};
  T m = M;
#elif problem==1
  T X = 420;
  T maxsteps = 5;
  #define M 6
  T inputs[M]= {1,4,5,6,6,50};
  T m = M;
#elif problem==2
  T X = 432;
  T maxsteps = 5;
  #define M 6
  T inputs[M]= {1,3,5,5,25,50};
  T m = M;
#else
#error no problem specified
#endif


  // This is the solution to test problem 0
  // validate a known/given solution?
  T solution_positions_and_ops[3 * maxsteps];
  memset(solution_positions_and_ops, 0, sizeof(solution_positions_and_ops));
  // 1st column
  solution_positions_and_ops[0] = 4; // 4th input
  solution_positions_and_ops[1] = 5; // 5th input
  solution_positions_and_ops[2] = 1; // 1st input
  solution_positions_and_ops[3] = 1+maxsteps + 3; // solution from line 3
  // 2nd column
  solution_positions_and_ops[maxsteps+0] = 2; // 2nd input
  solution_positions_and_ops[maxsteps+1] = 1+maxsteps + 1; // solution from line 1
  solution_positions_and_ops[maxsteps+2] = 6; // 6th input
  solution_positions_and_ops[maxsteps+3] = 1+maxsteps + 2; // solution from line 2
  // operations
  solution_positions_and_ops[2*maxsteps+0] = 3; // division in line 1
  solution_positions_and_ops[2*maxsteps+1] = 2; // substraction in line 2
  solution_positions_and_ops[2*maxsteps+2] = 1; // multiplication in line 3
  solution_positions_and_ops[2*maxsteps+3] = 0; // addition in line 4

  // could check solution
  T *test_solution = NULL;
  
  if (0) {
      test_solution = solution_positions_and_ops; // activate this line to verify a known solution
  }

  // play the game
  int failed_game = summle(X, inputs, m, maxsteps, test_solution);
  assert (failed_game);
  if (failed_game) return 134; // allow same exit code as assert in release mode
  
  return 0;
}
