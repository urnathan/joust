// RUN: $subdir$stem | ezio -p OUT $src |& ezio -p ERR -p OUT $src
// RUN-END:

// OUT-NEXT: PASS: {file:[^:]+}:{line1:[0-9]+}:test1
// ERR-NEXT: location {:.+}/tests/$file:$line1
// OUT-NEXT: FAIL: $file:{line2:[0-9]+}:test2
// OUT-NEXT: $EOF

// Joust
#include "joust.hh"
// C
#include <stddef.h>

int main (int, char *[])
{
  Joust::Logger log {};

  log.Result (Joust::Logger::PASS) << "test1";
  log.Log () << "location " << __FILE__ << ':' << __LINE__ - 1 << '\n';
  log.Result (Joust::Logger::FAIL) << "test2";

  return 0;
}
