// Joust Test Suite			-*- mode:c++ -*-
// Copyright (C) 2019-2020 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

// Joust
#include "fatal.hh"
// C++
#include <exception>
#include <typeinfo>
// C
#include <cinttypes>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// OS
#include <cxxabi.h>
#if HAVE_BFD_H
#include <bfd.h>
#endif
#if HAVE_DEMANGLE_H
#include <demangle.h>
#elif HAVE_LIBIBERTY_DEMANGLE_H
#include <libiberty/demangle.h>
#endif
#if NMS_BACKTRACE
#include <execinfo.h>
#endif
#ifdef HAVE_UCONTEXT_T
// Used for segv stack probing
#include <ucontext.h>
#endif
#include <unistd.h>

#if HAVE_BFD_H
extern char __executable_start[];
extern char __etext[];
#endif

namespace Joust
{

char const *progname = "$PROG";

namespace
{

class Binfo
  : public SrcLoc
{
  using Parent = SrcLoc;

#if HAVE_BFD_H
  bfd *theBfd;
  asymbol **syms;
  bfd_vma pc;
#endif
public:
  char const *fn;

public:
  Binfo
    ()
    noexcept;
  ~Binfo
    ()
    noexcept;

public:
#if NMS_BACKTRACE
  bool FindSrcLoc
    (void *addr, bool is_return_address);
  bool InlineUnwind
    ();
  char *Demangle
    ()
    noexcept;
#endif
};

Binfo::Binfo
  ()
  noexcept
  :
#if HAVE_BFD_H
  Parent (nullptr, 0), theBfd (nullptr), syms (nullptr),
#endif
    fn (nullptr)
{
#if HAVE_BFD_H
  bfd_init ();
  bfd_set_error_handler ([] (char const *, va_list) {});

  // readlink in a loop so we can extend the buffer if needed
  unsigned prglen = 100;
  for (char *program_path = 0;
       (program_path = static_cast<char *> (malloc (prglen)));
       free (program_path), prglen *= 2)
    {
      ssize_t len = readlink ("/proc/self/exe", program_path, prglen);

      if (len < 0)
	break;
      if (len >= 0 && len < int (prglen))
	{
	  program_path[len] = 0;
	  theBfd = bfd_openr (program_path, 0);
	  break;
	}
    }

  if (theBfd)
    {
#if defined (BFD_DECOMPRESS)
      theBfd->flags |= BFD_DECOMPRESS;
#endif
      if (!bfd_check_format_matches (theBfd, bfd_object, 0))
	{
	  bfd_close (theBfd);
	  theBfd = nullptr;
	}
      else if (bfd_get_file_flags (theBfd) & HAS_SYMS)
	{
	  long storage = bfd_get_symtab_upper_bound (theBfd);
	  bool dynamic = false;
      
	  if (!storage)
	    {
	      storage = bfd_get_dynamic_symtab_upper_bound (theBfd);
	      dynamic = true;
	    }
	  if (storage >= 0)
	    syms = static_cast<asymbol **> (malloc (storage));

	  if (!syms)
	    ;
	  else if ((dynamic
		    ? (bfd_canonicalize_dynamic_symtab (theBfd, syms))
		    : bfd_canonicalize_symtab (theBfd, syms)) < 0)
	    {
	      free (syms);
	      syms = 0;
	    }
	}
    }
#endif
}

Binfo::~Binfo
  ()
  noexcept
{
#if HAVE_BFD_H
  free (syms);
  syms = 0;

  if (theBfd)
    {
      bfd_close (theBfd);
      theBfd = 0;
    }
#endif
}

#if NMS_BACKTRACE
bool Binfo::FindSrcLoc
  (void *addr [[maybe_unused]], bool is_return_address [[maybe_unused]])
  noexcept
{
  fn = nullptr;
  *static_cast <Parent *> (this) = SrcLoc (nullptr, 0);

#if HAVE_BFD_H
  pc = reinterpret_cast<bfd_vma> (addr) - is_return_address;

  auto find_line
    = [] (bfd *fd, asection *section, void *data_) -> void
      {
	Binfo *self = static_cast<Binfo *> (data_);

	if (self->File ())
	  return;

	if (!(bfd_section_flags (section) & SEC_ALLOC))
	  return;

	bfd_vma vma = bfd_section_vma (section);
	if (self->pc < vma)
	  return;

	bfd_size_type size = bfd_section_size (section);
	if (self->pc - vma >= size)
	  return;

	bfd_find_nearest_line (fd, section, self->syms,
			       self->pc - vma,
			       &self->file, &self->fn, &self->line);
      };

  if (theBfd && addr >= &__executable_start && addr < &__etext)
    bfd_map_over_sections (theBfd, find_line, this);
#endif
  return File () != nullptr;
}
#endif

#if NMS_BACKTRACE
bool Binfo::InlineUnwind
  ()
{
#if HAVE_BFD_H
  if (theBfd && bfd_find_inliner_info (theBfd, &file, &fn, &line))
    return true;
#endif

  return false;
}
#endif

#if NMS_BACKTRACE
char const *StripObjDir
  (char const *file)
{
  char const *obj = OBJDIR;
  unsigned ol = strlen (obj);

  if (!strncmp (obj, file, ol) && file[ol] == '/')
    file += ol + 1;
  return file;
}
#endif

#if NMS_BACKTRACE
char *Binfo::Demangle
  () noexcept
{
  char *demangled = 0;
#if HAVE_DEMANGLE_H || HAVE_LIBIBERTY_DEMANGLE_H
  if (theBfd)
    demangled = bfd_demangle (theBfd, fn, DMGL_ANSI | DMGL_PARAMS);

  if (demangled)
    {
      // Do some reformatting so it looks the one true way :)
      unsigned len = strlen (demangled);
      char *reformed = static_cast<char *> (malloc (len * 2));
      char *dst = reformed;
      char prev = 0;

      for (char const *src = demangled; char c = *src; prev = c, src++)
	{
	  switch (c)
	    {
	    case ' ':
	      // '* (' -> '*('
	      // '> >' -> '>>'
	      if ((prev == '*' && src[1] == '(')
		  || (prev == '>' && src[1] == '>'))
		{
		  c = prev;
		  continue;
		}

	      // 'unsigned int' -> 'unsigned'
	      if (dst - reformed >= 8
		  && src - demangled + 3 < len
		  && !memcmp (src - 8, "unsigned int", 12))
		{
		  src += 3;
		  c = 't';
		  continue;
		}

	      // '(anonymous namespace)' -> '{anon}'
	      if (dst - reformed >= 10
		  && src - demangled + 9 < len
		  && !memcmp (src - 10, "(anonymous namespace)", 20))
		{
		  src += 10;
		  dst -= 5;
		  dst[-5] = '{';
		  *dst++ = '}';
		  c = '}';
		  continue;
		}

	      break;

	    case '*': case '&': case '(':
	      // ([:alnum:_)])([*&(]) -> \1 \2
	      if ((prev >= '0' && prev <= '9')
		  || (prev >= 'a' && prev <= 'z')
		  || (prev >= 'A' && prev <= 'Z')
		  || prev == '_' || prev == '>' || prev == ')')
		*dst++ = ' ';
	      break;
	    }
	  *dst++ = c;
	}
      free (demangled);
      *dst = 0;
      demangled = reformed;
    }
#endif // HAVE_DEMANGLE_H || HAVE_LIBIBERTY_DEMANGLE_H
  return demangled;
}
#endif

// An unexpected signal.  Try and determine where the signal came
// from, to report a file and line number.  The following frames will
// be live:
// 0:SignalHandler -- on the altstack
// 1:OS trampoline -- on the altstack
// 2:Frame of interest -- on regular stack (probably)
// We try and detect stack overflow, and report that
// distinctly.
void SignalHandler
  [[gnu::optimize ("-fno-omit-frame-pointer")]]
  (int sig)
  noexcept
{
  static volatile bool stack_overflow = false;

  if (sig == SIGSEGV)
    {
      // We get a segv if we've run out of stack.  Distinguish that
      // from other segvs by probing the stack.  While the CFA is
      // canonical across architectures, _b_f_a returns bottom of
      // frame on some and top of frame on others.
      char *frame = reinterpret_cast <char *> (__builtin_frame_address (0));

      {
#if __x86_64__
	// frame is top of incoming stack.
	frame += 0x10;
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.gregs[REG_RSP])
#define UCTX_SET_PC(UCTX,PC) ((UCTX)->uc_mcontext.gregs[REG_RIP] = (PC))
#elif __aarch64__
	// frame is bottom of current frame, unwind once
	uint32_t opcode = *reinterpret_cast <uint32_t *> (SignalHandler);

	// stp r29,r30,[sp,#-N]!
	if ((opcode & 0xffe07fff) == 0xa9a07bfd)
	  {
	    unsigned frame_size = (0x40 - ((opcode >> 15) & 0x3f)) * 8;
	    frame += frame_size;
	    frame += 0x80;
	  }
	else
	  frame = 0;
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.sp)
#define UCTX_SET_PC(UCTX,PC) ((UCTX)->uc_mcontext.pc = (PC))
#elif __powerpc64__
	// frame is bottom of current frame, unwind once
	frame = *reinterpret_cast <char **> (frame);
	frame += 0x80;
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.gp_regs[1])
#define UCTX_SET_PC(UCTX,PC) ((UCTX)->uc_mcontext.gp_regs[32] = (PC))
#else
	frame = 0;
#define UCTX_GET_SP(UCTX) (void (UCTX),0)
#define UCTX_SET_PC(UCTX,PC) (void (UCTX),void (PC))
#endif
      }

      if (ucontext_t *uctx = reinterpret_cast<ucontext_t *> (frame))
	{
	  if (stack_overflow)
	    {
	      // We got a signal when probing the stack.  Resume the user
	      // after adjusting the return pointer to skip the overflow
	      // reset.
	      UCTX_SET_PC (uctx, uintptr_t (&&no_stack));
	      return;
	    }

	  if (void *sp = reinterpret_cast <void *> (UCTX_GET_SP (uctx)))
	    {
	      // Peek the page containing SP and the one before that.
	      // We don't know if the SP failed to decrement into the
	      // broken page.  We'll recurse, if the page isn't
	      // accessible
	      uintptr_t page_size = uintptr_t (sysconf (_SC_PAGESIZE));
	      uintptr_t aligned_sp = uintptr_t (sp) & ~(page_size - 1);
	      stack_overflow = true;
	      void (reinterpret_cast <char volatile *>(aligned_sp)[0]);
	      void (reinterpret_cast <char volatile *>(aligned_sp)[-1]);
	      stack_overflow = false;
	    no_stack:;
	    }
	}
#undef UCTX_SET_PC
#undef UCTX_GET_SP
    }

  Binfo binfo;
#if HAVE_BFD_H && NMS_BACKTRACE && NMS_CHECKING
  void *return_addrs[3];
  if (backtrace (return_addrs, 3) == 3)
    binfo.FindSrcLoc (return_addrs[2], false);
#endif

  (HCF) (stack_overflow ? "stack overflow" : strsignal (sig)
#if NMS_CHECKING
	 , binfo
#endif
	 );
}

// An unexpected exception.  We don't try and locate the errant throw
// here.  That's a little fragile, and as we don't use exceptions,
// this never happens -- unlike seg faults :)

void TerminateHandler
  ()
  noexcept
{
  (HCF) (
#if NMS_CHECKING
	 "uncaught exception", SrcLoc (nullptr, 0)
#else
	 nullptr
#endif
	 );
}

void UnexpectedHandler
  ()
  noexcept
{
  (HCF) (
#if NMS_CHECKING
	 "unexpected exception", SrcLoc (nullptr, 0)
#else
	 nullptr
#endif
	 );
}

void OutOfMemory
  ()
  noexcept
{
  Fatal ("out of memory");
}

}

void SignalHandlers
  ()
  noexcept
{
  stack_t alt_stack;
  struct sigaction sig_action;

  // The default SIGSTKSZ is not big enough :( 3 additional pages
  // seems to suffice.
  alt_stack.ss_size = SIGSTKSZ + 3 * 4096;
  alt_stack.ss_flags = 0;
  alt_stack.ss_sp = malloc (alt_stack.ss_size);

  sigemptyset (&sig_action.sa_mask);
  sig_action.sa_flags = SA_NODEFER;
  if (alt_stack.ss_sp && !sigaltstack (&alt_stack, 0))
    sig_action.sa_flags |= SA_ONSTACK;

  sig_action.sa_handler = &SignalHandler;
  sigaction (SIGSEGV, &sig_action, 0);
  signal (SIGQUIT, &SignalHandler);
  signal (SIGILL, &SignalHandler);
  signal (SIGFPE, &SignalHandler);
  signal (SIGABRT, &SignalHandler);
  signal (SIGBUS, &SignalHandler);
  signal (SIGTRAP, &SignalHandler);

  std::set_unexpected (UnexpectedHandler);
  std::set_terminate (TerminateHandler);
  std::set_new_handler (OutOfMemory);
}

// Set the program name from, typically, argv[0]
void Progname
  (char const *prog)
{
  if (prog)
    {
      if (char const *name = strrchr (prog, '/'))
	if (name[1])
	  prog = name + 1;
      progname = prog;
    }
}

#if NMS_CHECKING
void (AssertFailed)
  (SrcLoc loc)
{
  (HCF) ("💥 assertion failed", loc);
}
void (Unreachable)
  (SrcLoc loc)
{
  (HCF) ("🛇 unreachable reached", loc);
}
void (Unimplemented)
  (SrcLoc loc)
{
  (HCF) ("✍  unimplemented functionality", loc);
}
#endif

void (HCF)
  (char const *msg
#if NMS_CHECKING
   , SrcLoc const loc
#endif
   )
  noexcept
{ // HCF - you goofed!
  static int busy = 0;

  __asm__ volatile ("nop");  // HCF - you goofed!

  fflush (stdout);
  fflush (stderr);

#if !NMS_CHECKING
  SrcLoc loc (nullptr, 0);
#define NMS_CHECKING 0
#endif

  if (busy > NMS_CHECKING)
    msg = "recursive internal error";

  fprintf (stderr, "%s: %s", progname, msg ? msg : "internal error");
  if (busy++ <= NMS_CHECKING)
    {
      if (char const *file = loc.File ())
	{
	  char const *src = SRCDIR;

	  if (src[0])
	    {
	      size_t l = strlen (src);

	      if (!strncmp (src, file, l) && file[l] == '/')
		file += l + 1;
	    }
	  fprintf (stderr, " at %s:%u", file, loc.Line ());
	}
      fprintf (stderr, " 🤮\n");

#if NMS_BACKTRACE
      {
#define MAX_BACKTRACE 100
	void *return_addrs[MAX_BACKTRACE];
	unsigned depth = backtrace (return_addrs, MAX_BACKTRACE);
	Binfo binfo;

	uintptr_t max = 0;
	for (unsigned ix = 0; ix != depth; ix++)
	  if (uintptr_t (return_addrs[ix]) > max)
	    max = uintptr_t (return_addrs[ix]);
	int prec = 0;
	for (; max; max >>= 8)
	  prec += 2;
	for (unsigned ix = 0; ix != depth; ix++)
	  {
	    char pfx[24];
	    void *addr = return_addrs[ix];
	    int len = snprintf (pfx, sizeof (pfx), "%.2d-%#.*" PRIxPTR, ix,
				prec, uintptr_t (addr));

	    // Look for unbounded recursion
	    for (unsigned jx = 1; jx < ix; jx++)
	      if (return_addrs[ix - jx] == addr)
		{
		  // Found a potential loop.  Check the pattern
		  // repeats for the remainder of the call stack.
		  for (unsigned kx = ix; kx != depth; kx++)
		    if (return_addrs[kx - jx] != return_addrs[kx])
		      goto not_looped;
		  fprintf (stderr, "%s repeat frame %d...\n", pfx, ix - jx);
		  goto done;
		not_looped:;
		}

	    if (binfo.FindSrcLoc (addr, true))
	      {
		unsigned inliner = 0;
		do
		  {
		    if (inliner)
		      {
			int l = snprintf (pfx, sizeof (pfx), "%.2d.%u",
					  ix, inliner);
			pfx[len] = 0;
			memset (pfx + l, ' ', len - l);
		      }
		    
		    inliner++;
		    fprintf (stderr, "%s %s:%u",
			     pfx, StripObjDir (binfo.File ()), binfo.Line ());
		    if (binfo.fn)
		      {
			char *demangled = binfo.Demangle ();
			fprintf (stderr, " %s",
				 demangled ? demangled : binfo.fn);
			free (demangled);
		      }
		    fprintf (stderr, "\n");
		  }
		while (binfo.InlineUnwind ());

		if (binfo.fn && !strcmp (binfo.fn, "main"))
		  goto done;
	      }
	    else if (char **sym_ptr_ptr
		     = backtrace_symbols (return_addrs + ix, 1))
	      {
		char *sym = *sym_ptr_ptr;
		if (sym[0] == '.' && sym[1] == '/')
		  sym += 2;
		unsigned l = strlen (sym);
		if (char *e = (char *)memchr (sym, '[', l))
		  {
		    l = e - sym;
		    if (l && sym[l-1] == ' ')
		      l--;
		  }
		fprintf (stderr, "%s %.*s\n", pfx, l, sym);
		free (sym_ptr_ptr);
	      }
	  }

      done:;
      }
#endif // NMS_BACKTRACE

      BuildNote (stderr);
    }

  signal (SIGABRT, SIG_DFL);
  raise (SIGABRT);
  exit (2);
}

void BuildNote
  (FILE *stream)
  noexcept
{
  fprintf (stream, "Version %s.\n", PACKAGE_NAME " " PACKAGE_VERSION);
  fprintf (stream, "Report bugs to %s.\n", BUGURL[0] ? BUGURL : "you");
  if (PACKAGE_URL[0])
    fprintf (stream, "See %s for more information.\n", PACKAGE_URL);
  if (REVISION[0])
    fprintf (stream, "Source %s.\n", REVISION);

  fprintf (stream, "Build is %s & %s.\n",
#if !NMS_CHECKING
	   "un"
#endif
	   "checked",
#if !__OPTIMIZE__
	   "un"
#endif
	   "optimized");
}

// A fatal error
void Fatal
  (char const *format, ...)
  noexcept
{ // Fatal Error
  __asm__ volatile ("nop");  // Fatal Error
  va_list args;
  va_start (args, format);
  fprintf (stderr, "%s: fatal: ", progname);
  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");

  exit (1);
}

}
