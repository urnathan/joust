// RUN:2 $subdir$stem |& ezio -p NEW $test
// NEW: fatal: out of memory
// NEW-END:

// RUN-SIGNAL:ABRT $subdir$stem -m |& ezio -p MAP $test
// MAP: Segmentation fault
// MAP-END:

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
#include "nms/option.hh"
// C
#include <cstring>
// OS
#include <sys/mman.h>

int
main (int argc, char *argv[])
{
  using namespace nms;
#include "joust/project-ident.inc"
  setBuildInfo (JOUST_PROJECT_IDENTS);
  installSignalHandlers ();

  bool map = false;
  static constexpr Option const options[]
    = {{"map", 'm', 0, OPTION_FN (Option::Parser<bool>::parse), "MMap"},
       {}};
  options->parseArgs (argc, argv, &map);

  size_t MB = 1024 * 1024;

  // Allocate at least 1GB, this should trigger an execution limit
  for (unsigned ix = 0; ix != 1024; ix++)
    {
      void *ptr = nullptr;

      if (map)
	ptr = mmap (nullptr, MB, PROT_READ | PROT_WRITE, MAP_ANONYMOUS, -1, 0);
      else
	ptr = new char[MB];
      assert (ptr);
      memset (ptr, 1, MB);
    }

  return 0;
}
