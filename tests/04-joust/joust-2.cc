// RUN: $subdir$stem | ezio -p OUT $test |& ezio -p ERR -p OUT $test
// RUN-END:

// OUT-NEXT: PASS: {pfx:(\.\./)?}{file:[^:]+}:{line1:[0-9]+}:test1
// ERR-NEXT: location {:.+}/$file:$line1
// OUT-NEXT: FAIL: $pfx$file:{line2:[0-9]+}:test2
// OUT-NEXT: MSG: $pfx$file:{:[0-9]+}:message1
// OUT-NEXT: MSG: $pfx$file:{:[0-9]+}:message2
// OUT-NEXT: $EOF

#include "joust/cfg.h"
// NMS
#include "nms/fatal.hh"
// Joust
#include "joust/tester.hh"

using namespace nms;

int
main (int, char *[])
{
#include "joust/project-ident.inc"
  setBuildInfo (JOUST_PROJECT_IDENTS);
  installSignalHandlers ();

  joust::Tester log {};

  log.result (joust::Tester::PASS) << "test1";
  log.log () << "location " << __FILE__ << ':' << __LINE__ - 1 << '\n';
  log.result (joust::Tester::FAIL) << "test2";

  log.result (joust::Tester::MSG) << "message1";
  log.message () << "message2";

  return 0;
}
