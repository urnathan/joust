# JOUST: Journal Of User-Scripted Tests[^1]

Copyright (C) 2020 Nathan Sidwell, nathan@acm.org

GNU Affero GPL v3.0
(but not yet ready for public consumption)

JOUST is a testsuite infrastructure consisting of a few components.
These either interact directly, or via user scripts.  Typically you'll
create a file of variable assignments, point at it with the JOUST
environment variable, then invoke `aloy` giving it the name of a
tester to invoke and a generator program.

Aloy will read from the generator's stdout, and invoke the tester
program on each word that provides.  Aloy can parallelize the tester jobs.

* ALOY: Apply List, Observe Yield

A testsuite runner that takes a list of test files and command to run.
The command is run for each test file, in parallel.  Results are
provided in summary and log files.  Usually the command to run will be kratos.

* KRATOS: Kapture Run And Test Output Safely

A test executor.  Looks for RUN lines in the specified text file and
executes them, piping stdout &| stderr to checkers.  Ability to skip
tests.

* EZIO: Expect Zero Irregularities Observed

A pattern checker.  Looks for CHECK lines in the specified text file
and then matches them against the specified file or stdin.

* DRAKE: Dynamic Response And Keyboard Emulation

(Yet to be written)

## ALOY: Apply List, Observe Yield

ALOY is the major driver of the testsuite.  It is expected that you'll
provide some scripts for its use.

 * `-C $DIR`:  Change to `$DIR` before doing anything else.
 * `-j $COUNT`:  Fixed job limit
 * `-g $GEN`:  Generator program
 * `-o $STEM`  Output file stem.
 * `-t $TESTER` Tester program

## KRATOS: Kapture Run And Test Output Safely

## EZIO: Expect Zero Irregularities Observed

## DRAKE: Dynamic Response And Keyboard Emulation


[^1] or 'Journal Of Utterly Stupid Tests'
