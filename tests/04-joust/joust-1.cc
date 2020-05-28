// RUN: $subdir$stem >$tmp
// RUN: <$tmp
// RUN: ezio -p HELP $src
// HELP: Usage: $stem [options]
// HELP-NEXT: Inline
// HELP-NEXT: Nested
// HELP-NEXT: $EOF
// HELP-END:

// RUN-SIGNAL:ABRT $subdir$stem --inline |& ezio -p INLINE $src
// INLINE-OPTION: matchSol
// INLINE: $stem: burn it all down
// INLINE-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} Joust::HCF (char const *,
// INLINE-NEXT: 01-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} {:(main)|(InvokeHCF)}
// INLINE-NEXT: 01{:(.1)|(-0x[0-9a-f]+)} tests/$src:{:[0-9]+} main
// INLINE-NEXT: Version
// INLINE-NEXT: Report bugs
// INLINE-NEXT: Source
// INLINE-NEXT: Build is
// INLINE-NEXT: $EOF
// INLINE-END:

// RUN-SIGNAL:ABRT $subdir$stem --nested |& ezio -p NESTED $src

// NESTED-OPTION: matchSol
// NESTED: $stem: go boom
// NESTED-NEXT: 00-0x{:[0-9a-f]+} {:([^ ]*/)?}fatal.cc:{:[0-9]+} Joust::HCF (char const *,
// NESTED-NEXT: 01-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED-NEXT: 02-0x{ret:[0-9a-f]+} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED-NEXT: 03-0x{:$ret} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED-NEXT: 04-0x{:$ret} tests/$src:{:[0-9]+} NestedHCF (int)
// NESTED-NEXT: 05-0x{:[0-9a-f]+} tests/$src:{:[0-9]+} main
// NESTED-NEXT: Version
// NESTED-NEXT: Report bugs
// NESTED-NEXT: Source
// NESTED-NEXT: Build is
// NESTED-NEXT: $EOF
// NESTED-END:

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
  struct Flags 
  {
    bool do_inline = false;
    bool do_nested = false;
  } flags;
  static constexpr Option const options[] =
    {
      {"inline", 0, offsetof (Flags, do_inline), nullptr, nullptr, "Inline"},
      {"nested", 0, offsetof (Flags, do_nested), nullptr, nullptr, "Nested"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  int argno = options->Process (argc, argv, &flags);

  if (flags.do_inline)
    InvokeHCF ();
  if (flags.do_nested)
    NestedHCF ();
  options->Help (stdout, "");
  return 0;
}
