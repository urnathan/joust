// Copyright (C) 2020 Nathan Sidwell, nathan@acm.org
// Not For Distribution

// NMS
#include "fatal.hh"
#include "option.hh"
// C
#include <stddef.h>

using namespace NMS;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
//Wattributes
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
