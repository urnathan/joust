// RUN: $subdir$stem | ezio -p OUT $test |& ezio -p ERR -p OUT $test
// RUN-END:

// OUT-NEXT: PASS: {file:[^:]+}:{line1:[0-9]+}:test1
// ERR-NEXT: location {:.+}/$file:$line1
// OUT-NEXT: FAIL: $file:{line2:[0-9]+}:test2
// OUT-NEXT: $EOF

#include "config.h"
// Joust
#include "joust/tester.hh"
// C
#include <stddef.h>

int main (int, char *[])
{
  Joust::Tester log {};

  log.Result (Joust::Tester::PASS) << "test1";
  log.Log () << "location " << __FILE__ << ':' << __LINE__ - 1 << '\n';
  log.Result (Joust::Tester::FAIL) << "test2";

  return 0;
}
