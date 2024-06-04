
// RUN: $subdir$stem >$tmp
// RUN: <$tmp
// RUN: ezio -p HELP $test
// HELP: Usage: $stem [options]
// HELP-NEXT: Inline
// HELP-NEXT: Nested
// HELP-NEXT: Optimized
// HELP-NEXT: Backtraced
// HELP-NEXT: $EOF
// HELP-END:

// RUN-REQUIRE:! $subdir$stem --optimized
// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE1 -p FATAL $test
// INLINE1-OPTION: matchSol
// INLINE1: $stem: burn it all down
// INLINE1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} nms::haltAndCatchFire (char const *,
// INLINE1-NEXT: 01-0x{:[0-9a-f]+} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:main|.anon.::InvokeHCF}
// INLINE1-NEXT: 01.1{: *} {:(extern/joust/)?}tests/$test:{:[0-9]+} main

// RUN-REQUIRE:! $subdir$stem --optimized
// RUN-REQUIRE: $subdir$stem --backtraced
// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED1 -p FATAL $test
// NESTED1-OPTION: matchSol
// NESTED1: $stem: go boom
// NESTED1-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} nms::haltAndCatchFire (char const *,
// NESTED1-NEXT: 01-0x{:[0-9a-f]+} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::NestedHCF (int)
// NESTED1-NEXT: 02-0x{ret1:[0-9a-f]+} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::TrampHCF (int)
// NESTED1-NEXT: 03-0x{ret2:[0-9a-f]+} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::NestedHCF (int)
// NESTED1-NEXT: 04-0x{:$ret1} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::TrampHCF (int)
// NESTED1-NEXT: 05-0x{:$ret2} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::NestedHCF (int)
// NESTED1-NEXT: 06-0x{:$ret1} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::TrampHCF (int)
// NESTED1-NEXT: 07-0x{:$ret2} {:(extern/joust/)?}tests/$test:{:[0-9]+} {:.}anon}::NestedHCF (int)
// NESTED1-NEXT: 08-0x{:[0-9a-f]+} {:(extern/joust/)?}tests/$test:{:[0-9]+} main

// FATAL-LABEL: joust-1: Joust
// FATAL-NEXT: {:  }Src:
// FATAL-NEXT: Inc: NMS
// FATAL-NEXT: {:  }Src:
// FATAL-NEXT: Build:
// FATAL-NEXT: $EOF
// FATAL-END:

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/option.hh"
// C
#include <stddef.h>

using namespace nms;

namespace {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
[[gnu::always_inline]] void
InvokeHCF ()
{ haltAndCatchFire ("burn it all down"); }
#pragma GCC diagnostic pop

void NestedHCF (int ix);

// Trampoline needed to avoid recursion warning (yes, I know, that't
// the point)
[[gnu::noinline]] void
TrampHCF (int ix)
{ NestedHCF (ix); }

[[gnu::noinline]] void
NestedHCF (int ix = 4)
{
  asm volatile ("");
  if (--ix)
    TrampHCF (ix);
  haltAndCatchFire ("go boom!");
}

} // namespace

int main (int argc, char *argv[])
{
#include "joust/project-ident.inc"
  setBuildInfo (JOUST_PROJECT_IDENTS);
  installSignalHandlers ();

  struct Flags 
  {
    bool do_inline = false;
    bool do_nested = false;
    bool is_backtraced = false;
    bool is_optimized = false;
  } flags;
  static constexpr Option const options[] =
    {
      {"inline", 0, OPTION_FLDFN (Flags, do_inline), "Inline"},
      {"nested", 0, OPTION_FLDFN (Flags, do_nested), "Nested"},
      {"optimized", 0, OPTION_FLDFN (Flags, is_optimized), "Optimized"},
      {"backtraced", 0, OPTION_FLDFN (Flags, is_backtraced), "Backtraced"},
      {}
    };
  options->parseArgs (argc, argv, &flags);

  if (flags.is_optimized)
    {
#if __OPTIMIZE__
      return 0;
#endif
      return 1;
    }

  if (flags.is_backtraced)
    return NMS_BACKTRACE == 0;

  if (flags.do_inline)
    InvokeHCF ();
  if (flags.do_nested)
    NestedHCF ();
  options->printUsage (stdout, "");
  return 0;
}
