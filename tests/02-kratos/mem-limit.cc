// RUN:1 $subdir$stem |& ezio -p NEW $src
// NEW: fatal: out of memory
// NEW-END:

// RUN-SIGNAL:ABRT $subdir$stem -m |& ezio -p MAP $src
// MAP: Segmentation fault
// MAP-END:

// NMS
#include "fatal.hh"
#include "option.hh"
// C
#include <cstring>
// OS
#include <sys/mman.h>

using namespace NMS;

int main (int argc, char *argv[])
{
  bool map = false;
  static constexpr Option const options[] =
    {
      {"map", 'm', 0, nullptr, nullptr, "MMap"},
      {nullptr, 0, 0, nullptr, nullptr, nullptr}
    };
  options->Process (argc, argv, &map);

  size_t MB = 1024 * 1024;

  // Allocate at least 1GB, this should trigger an execution limit
  for (unsigned ix = 0; ix != 1024; ix++)
    {
      void *ptr = nullptr;

      if (map)
	{
	  ptr = mmap (nullptr, MB, PROT_READ | PROT_WRITE,
		      MAP_ANONYMOUS, -1, 0);
	  
	}
      else
	ptr = new char[MB];
      Assert (ptr);
      memset (ptr, 1, MB);
    }

  return 0;
}
