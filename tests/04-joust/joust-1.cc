
// RUN: $subdir$stem >$tmp
// RUN: <$tmp
// RUN: ezio -p HELP $src
// HELP: Usage: $stem [options]
// HELP-NEXT: Inline
// HELP-NEXT: Nested
// HELP-NEXT: Backtraced
// HELP-NEXT: Demangled
// HELP-NEXT: $EOF
// HELP-END:

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE: $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE1 -p FATAL $src
// INLINE1-OPTION: matchSol
// INLINE1: $stem: burn it all down
// INLINE1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} Joust::HCF (char const *,
// INLINE1-NEXT: 01-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} {:(main)|(InvokeHCF)}
// INLINE1-NEXT: 01{:(.1)|(-0x[0-9a-f]+)} tests/$src:{:[0-9]+} main

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE:! $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE2 -p FATAL $src
// INLINE2: ^$stem: burn it all down
// INLINE2-NEXT: ^00-0x{:[0-9a-f]+} $subdir$stem({:[^)]*})
// INLINE2: (main

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE: $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED1 -p FATAL $src
// NESTED1-OPTION: matchSol
// NESTED1: $stem: go boom
// NESTED1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} Joust::HCF (char const *,
// NESTED1-NEXT: 01-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 02-0x{ret:[0-9a-f]+} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 03-0x{:$ret} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 04-0x{:$ret} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED1-NEXT: 05-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} main

// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-REQUIRE:! $subdir$stem --demangled
// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED2 -p FATAL $src
// NESTED2: ^$stem: go boom
// NESTED2-NEXT: ^00-0x{:[0-9a-f]+} $subdir$stem({:[^)]*})

// FATAL-LABEL: Version
// FATAL-NEXT: Report bugs
// FATAL-NEXT: Source
// FATAL-NEXT: Build is
// FATAL-NEXT: $EOF
// FATAL-END:

// Joust
#include "fatal.hh"
#include "option.hh"
// C
#include <stddef.h>

using namespace Joust;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

static void InvokeHCF [[gnu::always_inlne]] ()
{
  HCF ("burn it all down");
}
#pragma GCC diagnostic pop

static void NestedHCF [[gnu::noinline]] (int ix = 3)
{
  if (ix--)
    NestedHCF (ix);
  HCF ("go boom!");
}

int main (int argc, char *argv[])
{
  SignalHandlers ();

  struct Flags 
  {
    bool do_inline = false;
    bool do_nested = false;
    bool is_backtraced = false;
    bool is_demangled = false;
  } flags;
  static constexpr Option const options[] =
    {
      {"inline", 0, offsetof (Flags, do_inline), nullptr, nullptr, "Inline"},
      {"nested", 0, offsetof (Flags, do_nested), nullptr, nullptr, "Nested"},
      {"backtraced", 0, offsetof (Flags, is_backtraced), nullptr,
       nullptr, "Backtraced"},
      {"demangled", 0, offsetof (Flags, is_demangled), nullptr,
       nullptr, "Demangled"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);

  if (flags.is_backtraced)
    {
#if JOUST_BACKTRACE
      return 0;
#else
      return 1;
#endif
    }

  if (flags.is_demangled)
    {
#if HAVE_DEMANGLE_H || HAVE_LIBIBETERY_DEMANGLE_H
      return 0;
#else
      return 1;
#endif
    }

  if (flags.do_inline)
    InvokeHCF ();
  if (flags.do_nested)
    NestedHCF ();
  options->Help (stdout, "");
  return 0;
}
