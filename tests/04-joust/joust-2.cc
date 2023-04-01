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

int
main (int, char *argv[])
{
#include "joust/project-ident.inc"
  nms::SetBuild (argv[0], JOUST_PROJECT_IDENTS);
  nms::SignalHandlers ();

  joust::Tester log {};

  log.Result (joust::Tester::PASS) << "test1";
  log.Log () << "location " << __FILE__ << ':' << __LINE__ - 1 << '\n';
  log.Result (joust::Tester::FAIL) << "test2";

  log.Result (joust::Tester::MSG) << "message1";
  log.Message () << "message2";

  return 0;
}
