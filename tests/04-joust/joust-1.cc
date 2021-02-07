
// RUN: $subdir$stem >$tmp
// RUN: <$tmp
// RUN: ezio -p HELP $test
// HELP: Usage: $stem [options]
// HELP-NEXT: Inline
// HELP-NEXT: Nested
// HELP-NEXT: Optimized
// HELP-NEXT: Backtraced
// HELP-NEXT: Demangled
// HELP-NEXT: $EOF
// HELP-END:

// RUN-REQUIRE:! $subdir$stem --optimized
// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE: $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE1 -p FATAL $test
// INLINE1-OPTION: matchSol
// INLINE1: $stem: burn it all down
// INLINE1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} NMS::HCF (char const *,
// INLINE1-NEXT: 01-0x{:[0-9a-f]+} tests/$test:{:[0-9]+} {:(main)|(InvokeHCF)}
// INLINE1-NEXT: 01{:(.1)|(-0x[0-9a-f]+)} tests/$test:{:[0-9]+} main

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE:! $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE2 -p FATAL $test
// INLINE2: ^$stem: burn it all down
// INLINE2-NEXT: ^00-0x{:[0-9a-f]+} $subdir$stem({:[^)]*})
// INLINE2: (main

// RUN-REQUIRE:! $subdir$stem --optimized
// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE: $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED1 -p FATAL $test
// NESTED1-OPTION: matchSol
// NESTED1: $stem: go boom
// NESTED1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} NMS::HCF (char const *,
// NESTED1-NEXT: 01-0x{:[0-9a-f]+} tests/$test:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 02-0x{ret:[0-9a-f]+} tests/$test:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 03-0x{:$ret} tests/$test:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 04-0x{:$ret} tests/$test:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 05-0x{:[0-9a-f]+} tests/$test:{:[0-9]+} main

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE:! $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED2 -p FATAL $test
// NESTED2: ^$stem: go boom
// NESTED2-NEXT: ^00-0x{:[0-9a-f]+} $subdir$stem({:[^)]*})

// FATAL-LABEL: Version
// FATAL-NEXT: See {:.*} for more information.
// FATAL-NEXT: Build is
// FATAL-NEXT: $EOF
// FATAL-END:

#include "config.h"
// NMS
#include "nms/fatal.hh"
#include "nms/option.hh"
// C
#include <stddef.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
static void InvokeHCF [[gnu::always_inlne]] ()
{
  NMS::HCF ("burn it all down");
}
#pragma GCC diagnostic pop

static void NestedHCF [[gnu::noinline]] (int ix = 3)
{
  if (ix--)
    NestedHCF (ix);
  NMS::HCF ("go boom!");
}

int main (int argc, char *argv[])
{
  NMS::SignalHandlers ();

  struct Flags 
  {
    bool do_inline = false;
    bool do_nested = false;
    bool is_backtraced = false;
    bool is_demangled = false;
    bool is_optimized = false;
  } flags;
  static constexpr NMS::Option const options[] =
    {
      {"inline", 0, OPTION_FLDFN (Flags, do_inline), "Inline"},
      {"nested", 0, OPTION_FLDFN (Flags, do_nested), "Nested"},
      {"optimized", 0, OPTION_FLDFN (Flags, is_optimized), "Optimized"},
      {"backtraced", 0, OPTION_FLDFN (Flags, is_backtraced), "Backtraced"},
      {"demangled", 0, OPTION_FLDFN (Flags, is_demangled), "Demangled"},
      {}
    };
  options->Parse (argc, argv, &flags);

  if (flags.is_optimized)
    {
#if __OPTIMIZE__
      return 0;
#endif
      return 1;
    }

  if (flags.is_backtraced)
    return NMS_BACKTRACE == 0;

  if (flags.is_demangled)
    return HAVE_DEMANGLE == 0;

  if (flags.do_inline)
    InvokeHCF ();
  if (flags.do_nested)
    NestedHCF ();
  options->Help (stdout, "");
  return 0;
}
