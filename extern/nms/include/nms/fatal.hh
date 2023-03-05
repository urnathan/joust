// NMS Utilities			-*- mode:c++ -*-
// Copyright (C) 2019-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#ifndef NMS_FATAL_HH

// NMS
#include "nms/srcloc.hh"
// C
#include <cstdio>

namespace NMS
{

#if NMS_CHECKING
#define NMS_HERE() SrcLoc::Here ()
#else
#define NMS_HERE() SrcLoc (nullptr, 0)
#endif

[[noreturn]]
void HCF (char const *msg, char const *optional = nullptr,
	  SrcLoc = NMS_HERE ()) noexcept;

[[noreturn]]
void AssertFailed (char const *msg = nullptr, SrcLoc = NMS_HERE ()) noexcept;

[[noreturn]]
void Unreachable (char const *msg = nullptr, SrcLoc = NMS_HERE ()) noexcept;

[[noreturn]]
void Unimplemented (char const *msg = nullptr, SrcLoc = NMS_HERE ()) noexcept;

#undef NMS_HERE

#if NMS_CHECKING
#define Unreachable(MSG) NMS::Unreachable (MSG)
#define Unimplemented(MSG) NMS::Unimplemented (MSG)

#define Assert(EXPR, ...)						\
		    (__builtin_expect (bool (EXPR), true)		\
		     ? void (0)	: NMS::AssertFailed (__VA_ARGS__))

#else

#if __clang__
#define Assert(EXPR, ...) __builtin_assume (EXPR)
#else
// Not asserting, use EXPR in an unevaluated context
#define Assert(EXPR, ...) (void (sizeof (bool (EXPR))))
#endif

#define Unreachable(...) __builtin_unreachable ()
#define Unimplemented(...) __builtin_trap ()
#endif

void SetBuild (char const *progname,
	       char const *projectName
#if defined (PROJECT_NAME) && defined (PROJECT_VERSION)
	       = PROJECT_NAME
#endif
	       ,
	       char const *projectVersion
#if defined (PROJECT_VERSION)
	       = PROJECT_VERSION
#endif
	       ,
	       char const *projectURL
#if defined (PROJECT_URL)
	       = PROJECT_URL
#else
	       = nullptr
#endif
	       ,
	       char const *sourceIdent
#if __has_include ("nms_ident.inc")
	       =
#include "nms_ident.inc"
#elif defined (NMS_IDENT)
	       = NMS_IDENT
#else
	       = nullptr
#endif
	       ,
	       bool isChecked
#if NMS_CHECKING
	       = true
#else
	       = false
#endif
	       ,
	       bool isOptimized
#if __OPTIMIZE__
	       = true
#else
	       = false
#endif
  ) noexcept;

void BuildNote (FILE *stream) noexcept;

[[noreturn]]
void Fatal (char const *, ...) noexcept;

// Install default set of handlers (which abort)
void SignalHandlers () noexcept;

// Default (aborting) handler
void SignalHandler (int sig, void * = nullptr, void * = nullptr) noexcept;

} // namespace NMS

#define NMS_FATAL_HH
#endif
