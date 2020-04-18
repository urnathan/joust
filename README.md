# JOUST: Journal Of User-Scripted Tests[*]

Copyright (C) 2020 Nathan Sidwell, nathan@acm.org

GNU Affero GPL v3.0
(but not yet ready for public consumption)

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

## KRATOS: Kapture Run And Test Output Safely

## EZIO: Expect Zero Irregularities Observed

## DRAKE: Dynamic Response And Keyboard Emulation


[*] or 'Journal Of Utterly Stupid Tests'
