# JOUST: Journal Of User-Scripted Tests[^1]

Copyright (C) 2020 Nathan Sidwell, nathan@acm.org

GNU Affero GPL v3.0
(but not yet ready for public consumption)

Joust is a testsuite infrastructure consisting of a few components.
These either interact directly, or via user scripts.  Typically you'll
create a file of variable assignments, point at it with the Joust
environment variable, then invoke `aloy` giving it the name of a
tester to invoke and a generator program.  Aloy will read from the
generator's stdout, with each word being a test program to run.

I accidentally wrote this when I realized the test harness I was
adding to another project, was a project in its own right.  I didn't
want to use `dejagnu`.  Grepping for test harnesses found lots of
confusing ones.  Finally llvm's test harness is not a separate
component, and so hard to use elsewhere.  You'll notice that Kratos
and Ezio have some comonalities with that harness.

* Aloy: Apply List, Observe Yield

A testsuite runner that takes a list of test files and command to run.
The command is run for each test file.  Results are provided in
summary and log files.  Aloy can run sub jobs in parallel, either
communicating with a GNUmake jobserver, or using a specified job
limit.  Usually the command to run will be kratos.

* Kratos: Kapture Run And Test Output Safely

A test executor.  Looks for RUN lines in the specified text file and
executes them, piping stdout &| stderr to checkers.  Ability to skip
tests.

* Ezio: Expect Zero Irregularities Observed

A pattern checker.  Looks for CHECK lines in the specified text file
and then matches them against the specified file or stdin.

* DRAKE: Dynamic Response And Keyboard Emulation

(Yet to be written)

## Example

I use Make to build, along with separating the build directory tree
from the source tree -- so the source directories are unaltered by the
build.  A project may consist of multiple source directories, but
tests will be somewhere under `tests` subdirectories within this heirarchy.

Here is a `Makefile` snippet to invoke the testsuite:

```make
# The list of test programs to compile, before invoking tests
TESTS/iblis := $(patsubst $(srcdir)/%.cc,%,\
	$(wildcard $(srcdir)/iblis/tests/*/*.cc))

TESTDIRS/iblis = $(shell cd $(srcdir)/${<D} ; echo *(/))
ifneq (,$(filter joust,$(SUBPROJS)))
# If Joust is itself a subproject, point PATH at it
check.iblis: PATH := ../../joust:$(PATH)
endif

# The testsuite.  `+` tells Make to provide a jobserver
check.iblis:: iblis/tests/iblis.defs $(TESTS/iblis)
	+cd ${<D} && JOUST=${<F} aloy -t kratos -o iblis \
	  -g ../../$(srcdir)/iblis/tests/jouster $(TESTDIRS/iblis)

# The variable definition file.  Add anything you need to refer to from
# kratos or ezio lines.
iblis/tests/iblis.defs: iblis/tests/Makesub
	echo '# Automatically generated by Make' >$@
	echo srcdir=../../${srcdir}/iblis/tests/ >>$@
	echo OBJCOPY=$(OBJCOPY) >>$@

# Build rule for test programs.
$(TESTS/iblis): %: %.o libiblis.a libnms.a
	$(CXX) $(LDFLAGS) $< -liblis -lnms $(LIBS) -o $@
```

You might notice, that's providing `../../$(srcdir)/iblis/tests/jouster` as a generator program.  It happens to be a script in the source directory:

```zsh
pushd ${0%/*}
setopt nullglob
for subdir in $@ ; do
    echo $subdir/*(.^*)
done
popd
```

The first test is `tests/00-basic/axiom.cc`, which is compiled by the
above Makefile before the testsuite is invoked.  The source file
contains the following fragment:

```c++
// Check backtrace dtrt
// RUN-SIGNAL:6 $subdir$stem | ezio -p FATAL $src
// FATAL: I am unwell
// FATAL-NEXT: 00-0x{:[0-9a-f]+}
// FATAL: InvokeHCF
// FATAL: main
// FATAL-NEXT: Version
// FATAL-END:

// Check that the ABI is optimal for passing Ref<T>'s the same as T &.
// RUN: $OBJCOPY -Obinary -jidentity1 $subdir$stem.o $tmp.1
// RUN: $OBJCOPY -Obinary -jidentity2 $subdir$stem.o $tmp.2
// RUN: cmp -s $tmp.1 $tmp.2

// RUN-END:
```

This contains 4 test invocations, one for each `RUN:` line.  The
`RUN-SIGNAL:` line is checking that a failed assert produces a
backtrace and exits with a signal.  You'll see it's invoking Ezio to
check the backtrace, and tells it to use the `FATAL:` lines for that
checking.  The next 2 `RUN:` lines invoke objcopy to extract some
information from the object file, writing to two temporary files.  The
final `RUN:` line compares those temporaries and we only care it exits
with zero.

## Aloy: Apply List, Observe Yield

Aloy is the major driver of the testsuite.  It is expected that you'll
provide some scripts for its use.  Typical invocation is:

`aloy [options] [tester args --] [generator args]`

* `-C DIR`:  Change to `DIR` before doing anything else.
* `-j COUNT`:  Fixed job limit
* `-o STEM`  Output file stem, defaults to '-' (stdout/stderr)
* `-g GEN`:  Generator program, defaults to `kratos`
* `-t TESTER` Tester program, defaults to `kratos`

Additional arguments can be passed to the tester program, by using a
`--` separator after them.  The remaining arguments are passed to the
generator program.  If there is no generator program, they are used
directly as the names of tests to run.  Comments are introduced with
`#` and extend to end of line.

The number of concurrent jobs to run can be specified with `-j`, or
implicitly using a GNUmake jobserver specified via the `MAKEFLAGS`
environment variable.  You may need to prefix the Makerule with `+`,
so that Make knows the rule invokes a jobserver-aware program.
Otherwise, although `MAKEFLAGS` is set, the jobserver will be
unuseable.  Aloy will inform you of this happening.  Specifying `-j`
overrides any `MAKEFILE` variable.

## Kratos: Kapture Run And Test Output Safely

Kratos scans a source file for marked lines.  These are then executed,
with the output going to user-specified checker programs.  Several
separate tests can be specified in a single file, they are executed
sequentially.

`kratos [options] test-file`

* `-C DIR`:  Change to `DIR` before doing anything else.
* `-D VAR=VALUE`: Define variable
* `-d FILE`:  Specify file of variable definitions
* `-o STEM`  Output file stem, defaults to '-' (stdout/stderr)
* `-p PREFIX`: Command line prefix, defaults `RUN`, repeatable

The environment variable `$JOUST` can be set to specify another file
of variable definitions.

* RUN: A test pipeline to execute
* RUN-SIGNAL: A test pipeline, terminating via a signal
* RUN-REQUIRE: A predicate to evaluate
* RUN-END: Stop scanning test file

Both `RUN` and `RUN-SIGNAL` are similar, except the latter expects the
program to terminate via a signal.  In both cases the command may
specify an exit code or signal, together with `!` to indicate `not`.
It doesn't matter what characters appear before `RUN` on the line
(provided there is a non-alphanumeric just before).  For instance:

`# RUN: true`  
`// RUN:!0 false`

The first exits with a zero exit code, and the second exits with a
non-zero code.

The actual command to run is subject to `$` expansion, which is
similar, but not the same, as shell expansion.  In particular shell
quoting and substitution is different.  Arguments are space-separated,
use `{...}` braces to inhibit that -- the braces are dropped.
Variable expansion uses `$var` or `${var}`, spaces in the variable
expansion begin new arguments, unless the expansion itself is within
braces.  Variables are those specified via a definition file or on the
command line.

The program's stdin can be sourced from a file and its stdout can be
written to one.  Unlike the shell, these indirections _must_ be first
in the line, otherwise they will not be recognized.

`<$srcdir$src $srcdir$src`  
`>$tmp-1 $srcdir$src`

The first will read from `$srcdir$src` and the second will write to `$tmp-1`.

Commands may be continued to the next `RUN:` line by ending with a
single `\`.  This cannot appear in the middle of a word though.
Regardless of the kind of command started, all continuations must use
`RUN:`.

By default the program's stdout and stderr are buffered and forwarded
to kratos's stdout and stderr.  So they can be self-checking if
wanted.  Or the outputs can be piped to checking programs.  Often
`ezio` is used to check the output is as expected.  Use `|` to
indicate this.  If a line ends with `|` you do not also need a `\` to
continue to the next `RUN:` line.  Unlike the shell, which stream is
piped depends on ordering -- there's no explicit numbering of pipes.
There can be one or two checkers.  The last checker receives stderr,
and the one before that (if any) gets stdout.  For example:

`// 1 RUN: prog $srcdir$src |`  
`// 2 RUN: ezio $src`  
`// 3 RUN: prog $srcdir$src | ezio -p OUT $src | ezio $src`  
`// 4 RUN: >$tmp-1 prog $srcdir$src | ezio $src`  
`// 5 RUN: <$tmp-1 ezio -p OUT $src`

Here lines 1 & 2 form a single pipeline, with `prog`'s stderr being
fed to `ezio`. Line 3 is similar, but also pipe's `prog`'s stdout to
the first invocation of `ezio`.  Lines 4 & 5 do the same checking, but
via a temporary file `$tmp-1`.  That file could also be used by a
subsequent program under test.  (`$tmp` is an automatically defined
variable.)

System resources can be constrained by use of variables:

* $cpulimit:  Maximum cpu time, in seconds (1 minute).
* $memlimit:  Maxiumum memory use, in GB (1 GB).
* $filelimit:  Maximum filesize, in GB (1 GB).
* $timelimit:  Maximum wall clock time, in seconds (1 minute).

## Ezio: Expect Zero Irregularities Observed

Ezio is a pattern matcher.  It scans a source file, extracting
patterns from it, and then matches the patterns against a test file.

`ezio [options] pattern-files+`

* `-C DIR`:  Change to `DIR` before doing anything else.
* `-D VAR=VALUE`: Define variable
* `-d FILE`:  Specify file of variable definitions
* `-i INPUT`  Input file, defaults to '-' (stdin)
* `-o STEM`  Output file stem, defaults to '-' (stdout/stderr)
* `-p PREFIX`: Command line prefix, defaults `CHECK`, repeatable

There are several kinds of check patterns.  Some are positive matches
(& fail if no match is found), others are negative matches (& fail if
they match).  Positive matches are never checked again, once they
match.  Negative matches might be checked multiple times (and produce
several fails).

* CHECK:  A synonym for MATCH.

* CHECK-MATCH: Match a pattern.  Is repeatedly checked until it
  matches, or we move to a next labelled block.

* CHECK-NEXT: Match the next line only.  Fails if it doesn't match.
  If this is the first match, it applies to the first line of the
  file.

* CHECK-DAG: A set of lines may represent a DAG, which implies a
  partial ordering.  This allows the lines of the DAG to be checked.

* CHECK-LABEL: Blocks of the input file can be separated with this
  pattern.  If the current pattern does not match, later LABELS are
  checked to see if we should advance.

* CHECK-NOT: A negative match.  It is expected not to match, until the
  next positive match advances.

* CHECK-NONE: A negative match that applies to an entire block.  Lines
  that do not make a positive match are expected to not match any of
  these lines either.

* CHECK-NEVER: A negative match that applies to the entire file.  As
  with NONE, lines that fail any positive match are expected to not
  match these either.  Only the first negative match that matches is
  checked -- a NONE and a NEVER that check the same pattern will not
  trigger twice.

* CHECK-OPTION: Set options for the following patterns.  Options are:

  * matchSol: The pattern is anchored to the start of line.
  * matchEol: The pattern is anchored to the end of the line.
  * matchLine: The pattern is anchored at both start & end.
  * matchSpace: Whitespace must match exactly.
  * xfail: The pattern is xfailed -- expected to fail.

  Options are space or `,` separated.  They may be preceded by `!` to
  turn the option off.  `matchLine` is equivalent to specifying both
  `matchSol` and `matchEol`.  An `xfail` only applies to the next
  pattern, unlike the other options it resets.

Patterns are literal matches, with the following extensions:

* Leading and trailing whitespace is elided.  If you must match such
  space, use a regexp escpe.

* A leading `^` will anchor a particlar pattern at the start of line
  (regardless of `matchSol`).

* A trailing `$` will anchor at the end of line.  (The options are
  useful if you need to anchor a whole set of lines.)

* Whitespace matches any sequence of whitespace.

* Variable expansions are denoted by `$VAR` or `${VAR}`.  These are
  matched literally.

* To match a literal `$` use `${}`.

* Captures are denoted by `{VAR:REGEXP}`, which if the match is
  successful set the variable to the text the regexp matched.  Posix
  extended regexps are used.

* Plain regexps are simply captures without a variable: `{:REGEXP}`.

* The regexp can contain a variable expansion, which is matched
  literally (not asa regexp itself).  The above `$VAR` or `${VAR}`
  syntax does that.  As before `${}` will obtain a literal `$`, but
  you might need to escape that with `\`, because of its meaning in a
  REGEXP.

* Patterns can contain variables whose value has not yet been
  determined.  These patterns will not match at that point.  If Ezio
  can determine the variable will never have a value set until too
  late, an error is given.  This is particularly signficant with DAG
  matching, where later patterns of the DAG contain expansion captured
  from earlier patterns.

## DRAKE: Dynamic Response And Keyboard Emulation

## Libjoust

You can use libjoust directly, to write unit tests.

```c++
#include "logger.hh"  // Yeah, I like .hh, bite me.

int main ()
{
  Joust::Logger logger;

  int error_code = PerformSomeTest ();
  if (error_code)
    // Emit arbitrary explanatory text to the log stream
    logger.Log () << "Foo test produced " << error_code;

  // Inform of results:
  logger.Result (Joust::Logger::PassFail (!error_code)) << "Foo test";
  // or with some location information:
  logger.Result (Joust::Logger::PassFail (!error_code),
                 __FILE__, __LINE))) << "Foo test";
}

```

If you're driving this from Kratos, the test program should return
zero, except in the case of configuration error or other failure
external to the test itself.

## Commonalities

As you might expect for a set of related programs, they share some
semantics and syntax.

### Variables

Variables can be specfied in:
* `-Dvar=val` command line options
* `-d file` file specified on the command line
* File specified in a `$JOUST` environment variable.

They are initialized in that order, and the first initializer wins.
These are unrelated to environment variables.  (Although environment
variables can influence tests, because they are visible to the
programs being tested.)  Typically setup code will create a file of
values and initialize `$JOUST` to point at it.

All the programs expect a `$srcdir` variable to be specified pointing
at the source directory.  They will automatically prepend it to
certain arguments.  The following variables will be automatically created:

* `$src`: The source being tested.

* `$stem`: The basename of `$src`, stripping both directory and suffix
  components.

* `$subdir`: The directory fragment of `$src` including a final `/`.
  If there isno directory component, an empty string is assigned.

* `$tmp`: A temporary name constructed from `$src` by replacing every
  `/` with `-` and appending `.tmp`.

### Input and Output

Ezio, by default, will read the file to check from `stdin`.  You can
use `-i FILE` to provide a file to read.  If `FILE` is `-`, it
signifies `stdin` anyway.

Aloy, Kratos & Ezio report their results to `stdout` and `stderr`.
`stdout` provides summary results -- just pass/fail of each tests.
`stderr` provides logging, which along with the pas/fail information
shows informative test output so you can see why it failed, and
information about how to reexecute just that test, either inside the
test harness, or from the shell.  All of them allow you to redirect
that output to two files with the same basename with the `-o STEM`
option.  Summary resuls go to `STEM.sum` and logging output goes to
`STEM.log`.  Usually you'll give Aloy a `-o mytests` option.

Note that if output is going to `stdout` and `stderr`, and they refer
to the same file, output will be generally be undecipherably
duplicated.  Use the shell to redirect to separate files (`>sum 2>log`).

## Future

* Kratos offloading to a remote execution system.  Add $wrapper
variable or something?

* Kratos copying files to/from remote system.  Add $cpto $cp from
variables along with RUN-AUX: or similar.

* Kratos iteration over a set of flags.  Add RUN-ITERATE: along with
ability to defer some toplevel variable expansion to runtime.
RUN-REQUIRE inside a loop would continue to the next iteration of the
loop.

## Building Joust

Building Joust is reasonably straight forwards.  The only(?) quirk is
that it relies upon my utility project, NMS.  That provides Makefile &
autoconf pieces along with an assert, backtrace and option processing
library.  You'll see there's an `nms` symlink in the top directory.
You can install NMS in a sibling directory, or replace the symlink
with the NMS project itself.  The former is easier and how I build it.

You will need a C++20 compiler, as I use some features of C++20.  The
configure script allows you to point at special build tools with
`--with-tools=TOOLDIR`.  I used this to point at a prerelease C++20
toolchain.

Although building in the source tree probably works, I do not build
and test that configuration.  I create a sibling `obj` directory, and
build there.

Here's a recipe

```zsh
mkdir src obj
git clone git@github.com:urnathan/joust.git src/joust
git clone git@github.com:urnathan/nms.git src/nms
cd obj
../src/joust/configure
make
make check
```

The `configure` script supports a number of options, you can disable
checking, and backtrace for instance.  When building you can override
`CXXFLAGS` if needs be -- I often used `make CXXFLAGS=-g3` if I need to
debug it.

The Makefile will automatically enable parallelism, trying to keep
each available CPU busy.  You can override this by adding
`PARALLELISM=` on the command line.  Set it to empty, or to the
desired parallelism.

### Install

I've not yet written install rules, sorry.

[^1]: or 'Journal Of Utterly Stupid Tests'
