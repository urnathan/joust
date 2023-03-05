// NMS Utilities			-*- mode:c++ -*-
// Copyright (C) 2019-2023 Nathan Sidwell, nathan@acm.org
// License: Affero GPL v3.0

#include "nms/cfg.h"
// NMS
#include "nms/fatal.hh"
// C++
#include <exception>
#include <typeinfo>
#include <vector>
// C
#include <cerrno>
#include <cinttypes>
#include <csignal>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
// OS
#include <cxxabi.h>
#if HAVE_BFD
#include <bfd.h>
#endif
#if NMS_BACKTRACE
#include <cxxabi.h>
#include <execinfo.h>
#endif
#ifdef HAVE_UCONTEXT
// Used for segv stack probing
#include <ucontext.h>
#endif
#include <unistd.h>

#if HAVE_BFD
extern char __executable_start[];
extern char __etext[];
#endif

namespace NMS
{

namespace
{

char const *progName = nullptr;
char const *projectName = nullptr;
char const *projectVersion = nullptr;
char const *projectURL = nullptr;
char const *sourceIdent = nullptr;
bool buildChecked = false;
bool buildOptimized = false;

class Binfo : public SrcLoc
{
  using Parent = SrcLoc;

#if HAVE_BFD
  bfd *theBfd = nullptr;
  asymbol **syms = nullptr;
  bfd_vma pc = 0;
#endif
public:
  char const *fn = nullptr;

public:
  Binfo () noexcept;
  ~Binfo () noexcept;

public:
  void Clear () noexcept
  {
    Parent::operator= (Parent ());
    fn = nullptr;
  }
  char const *Fn () const noexcept
  { return fn; }

public:
#if NMS_BACKTRACE
  bool FindSrcLoc (void *addr, bool is_return_address) noexcept;
  bool InlineUnwind () noexcept;
  char *Demangle () noexcept;
#endif
};

Binfo::Binfo () noexcept
{
#if HAVE_BFD
  bfd_init ();
  bfd_set_error_handler ([] (char const *, va_list)
			 {});

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
	  else if ((dynamic ? (bfd_canonicalize_dynamic_symtab (theBfd, syms))
		            : bfd_canonicalize_symtab (theBfd, syms)) < 0)
	    {
	      free (syms);
	      syms = 0;
	    }
	}
    }
#endif
}

Binfo::~Binfo () noexcept
{
#if HAVE_BFD
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
bool
Binfo::FindSrcLoc (void *addr [[maybe_unused]],
		   bool is_return_address [[maybe_unused]]) noexcept
{
  Clear ();

#if HAVE_BFD
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
bool
Binfo::InlineUnwind () noexcept
{
#if HAVE_BFD
  if (theBfd && bfd_find_inliner_info (theBfd, &file, &fn, &line))
    return true;
#endif

  return false;
}
#endif

std::vector<char const *> rootDirs;

char const *
StripRootDirs (char const *file) noexcept
{
  for (auto dir : rootDirs)
    {
      size_t l = strlen (dir);

      if (!strncmp (dir, file, l) && file[l] != '/')
	{
	  auto trail = file + l;
	  unsigned depth = 0;

	try_remove_dots:;
	  auto probe = trail;

	  for (auto slash = probe; auto c = *slash++;)
	    if (c == '/')
	      {
		if (slash - probe == 3 && probe[0] == '.' && probe[1] == '.')
		  {
		    if (!depth)
		      goto not_this_prefix;
		    if (!--depth)
		      {
			trail = slash;
			goto try_remove_dots;
		      }
		  }
		else
		  depth++;
		probe = slash;
	      }
	  file = trail;
	  break;
	}
    not_this_prefix:;
    }

  return file;
}

#if NMS_BACKTRACE
char *
Binfo::Demangle () noexcept
{
  char *demangled = abi::__cxa_demangle (fn, nullptr, nullptr, nullptr);

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
	      if (dst - reformed >= 8 && src - demangled + 3 < len
		  && !memcmp (src - 8, "unsigned int", 12))
		{
		  src += 3;
		  c = 't';
		  continue;
		}

	      // '(anonymous namespace)' -> '{anon}'
	      if (dst - reformed >= 10 && src - demangled + 9 < len
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

	    case '*':
	    case '&':
	    case '(':
	      // ([:alnum:_)])([*&(]) -> \1 \2
	      if ((prev >= 'a' && prev <= 'z') || (prev >= 'A' && prev <= 'Z')
		  || (prev >= '0' && prev <= '9') || prev == '_' || prev == '>'
		  || prev == ')')
		*dst++ = ' ';
	      break;
	    }
	  *dst++ = c;
	}
      free (demangled);
      *dst = 0;
      demangled = reformed;
    }

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
void
SignalHandlerImpl (int sig, siginfo_t *, void *ucontext) noexcept
{
  static volatile bool stack_overflow = false;

  if (sig == SIGSEGV)
    {
      // We get a segv if we've run out of stack.  Distinguish that
      // from other segvs by probing the stack.
      {
#if __x86_64__
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.gregs[REG_RSP])
#define UCTX_SET_PC(UCTX, PC) ((UCTX)->uc_mcontext.gregs[REG_RIP] = (PC))
	// mov $0xf,%rax; syscall
#define SNIFF 0x48, 0xc7, 0xc0, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x05
#elif __aarch64__
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.sp)
#define UCTX_SET_PC(UCTX, PC) ((UCTX)->uc_mcontext.pc = (PC))
#elif __powerpc64__
#define UCTX_GET_SP(UCTX) ((UCTX)->uc_mcontext.gp_regs[1])
#define UCTX_SET_PC(UCTX, PC) ((UCTX)->uc_mcontext.gp_regs[32] = (PC))
#else
	ucontext = nullptr;
#define UCTX_GET_SP(UCTX) (void (UCTX), 0)
#define UCTX_SET_PC(UCTX, PC) (void (UCTX), void (PC))
#endif
      }

      if (ucontext_t *uctx = reinterpret_cast<ucontext_t *> (ucontext))
	{
	  if (stack_overflow)
	    {
	      // We got a signal when probing the stack.  Resume the user
	      // after adjusting the return pointer to skip the overflow
	      // reset.
	      UCTX_SET_PC (uctx, uintptr_t (&&no_stack));
	      return;
	    }

	  if (void *sp = reinterpret_cast<void *> (UCTX_GET_SP (uctx)))
	    {
	      // Peek the page containing SP and the one before that.
	      // We don't know if the SP failed to decrement into the
	      // broken page.  We'll recurse, if the page isn't
	      // accessible
	      uintptr_t page_size = uintptr_t (sysconf (_SC_PAGESIZE));
	      uintptr_t aligned_sp = uintptr_t (sp) & ~(page_size - 1);
	      stack_overflow = true;
	      void (reinterpret_cast<char volatile *> (aligned_sp)[0]);
	      void (reinterpret_cast<char volatile *> (aligned_sp)[-1]);
	      stack_overflow = false;
	    no_stack:;
	    }
	}
#undef UCTX_SET_PC
#undef UCTX_GET_SP
    }

  Binfo binfo;
#if NMS_BACKTRACE && NMS_CHECKING && HAVE_BFD && defined (SNIFF)
  {
    // Find the frame that invoked the signal by discovering the
    // restorer function. We find this by sniffing bytes at the
    // returned-to location.
    void *return_addrs[10];
    if (auto count = backtrace (return_addrs, 10))
      for (int ix = 0; ix < count - 1; ix++)
	{
	  static unsigned char const sniff[] = {SNIFF};
	  if (!std::memcmp (return_addrs[ix], sniff, sizeof (sniff)))
	    {
	      binfo.FindSrcLoc (return_addrs[++ix], false);
	      break;
	    }
	}
  }
#endif
#undef SNIFF

  (HCF) (stack_overflow ? "stack overflow" : "signal",
	 stack_overflow ? nullptr : strsignal (sig), binfo);
}

// An unexpected exception.  We don't try and locate the errant throw
// here.  That's a little fragile, and as we don't use exceptions,
// this never happens -- unlike seg faults :)

void
TerminateHandler () noexcept
{
  (HCF) ("uncaught exception");
}

void
OutOfMemory () noexcept
{
  Fatal ("out of memory");
}

} // namespace

// A stub to avoid exposing <signal.h> in the header file
void SignalHandler (int sig, void *info, void *ucontext) noexcept
{
  SignalHandlerImpl (sig, static_cast<siginfo_t *> (info), ucontext);
}

void
SignalHandlers () noexcept
{
  stack_t alt_stack;

  // The default SIGSTKSZ is not big enough :( 3 additional pages
  // seems to suffice.
  alt_stack.ss_size = SIGSTKSZ + 3 * 4096;
  alt_stack.ss_flags = 0;
  alt_stack.ss_sp = malloc (alt_stack.ss_size);

  struct sigaction sig_action;
  sig_action.sa_sigaction = &SignalHandlerImpl;
  sigemptyset (&sig_action.sa_mask);
  sig_action.sa_flags = SA_NODEFER | SA_SIGINFO;
  if (alt_stack.ss_sp && !sigaltstack (&alt_stack, 0))
    sig_action.sa_flags |= SA_ONSTACK;

  static constinit int const signals[]
    = {SIGSEGV, SIGQUIT, SIGILL, SIGFPE, SIGABRT, SIGBUS, SIGTRAP};
  for (auto sig : signals)
    sigaction (sig, &sig_action, 0);

  std::set_terminate (TerminateHandler);
  std::set_new_handler (OutOfMemory);
}

void
(AssertFailed) (char const *msg, SrcLoc loc) noexcept
{
  (HCF) ("💥 assertion failed", msg, loc);
}
void
(Unreachable) (char const *msg, SrcLoc loc) noexcept
{
  (HCF) ("🛇 unreachable reached", msg, loc);
}
void
(Unimplemented) (char const *msg, SrcLoc loc) noexcept
{
  (HCF) ("✍  unimplemented functionality", msg, loc);
}

void
(HCF) (char const *msg, char const *opt, SrcLoc loc) noexcept
{ __asm__ volatile ("nop"); // HCF - you goofed!
  static int busy = 0;

  fflush (stdout);
  fflush (stderr);

  if (busy > 1)
    {
      msg = "recursive internal error";
      opt = nullptr;
    }

  fprintf (stderr, "%s: %s%s%s%s", program_invocation_short_name,
	   msg ? msg : "internal error",
	   &" ("[2 * !opt], opt ? opt : "", &")"[!opt]);
  if (busy++ <= 1)
    {
      if (char const *file = loc.File ())
	fprintf (stderr, " at %s:%u", StripRootDirs (file), loc.Line ());
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
		    fprintf (stderr, "%s %s:%u", pfx,
			     StripRootDirs (binfo.File ()), binfo.Line ());
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
		    if (l && sym[l - 1] == ' ')
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

  {
    signal (SIGABRT, SIG_DFL);
    sigset_t set;
    sigemptyset (&set);
    sigaddset (&set, SIGABRT);
    sigprocmask (SIG_UNBLOCK, &set, nullptr);
    raise (SIGABRT);
  }
  exit (2);
}

void
AddPrefixDir (char const *src, char const *bin) noexcept
{
  rootDirs.emplace_back (src);
  if (bin)
    rootDirs.emplace_back (bin);
}

// Set the program name from, typically, argv[0]
void
SetBuild (char const *prog,
	  char const *name, char const *version, char const *url,
	  char const *ident, bool isChecked, bool isOptimized) noexcept
{
  AddPrefixDir (NMS_PREFIX_DIRS);
  if (prog)
    {
      if (char const *name = std::strrchr (prog, '/'))
	if (name[1])
	  prog = name + 1;
      progName = prog;
    }

  projectName = name;
  projectVersion = version;
  projectURL = url;
  sourceIdent = ident;
  buildChecked = isChecked;
  buildOptimized = isOptimized;
}

void
BuildNote (FILE *stream) noexcept
{
  if (projectName)
    {
      if (progName)
	fprintf (stream, "%s ", progName);
      fprintf (stream, "%s", projectName);
      if (projectVersion)
	fprintf (stream, " version %s", projectVersion);
      fprintf (stream, ".");
    }
  if (projectURL && projectURL[0])
    fprintf (stream, " See %s for more information.", projectURL);
  fprintf (stream, "\n");
  if (sourceIdent && sourceIdent[0])
    fprintf (stream, "Ident %s\n", sourceIdent);
  fprintf (stream, "Build is %s & %s.\n",
	   buildChecked ? "checked" : "unchecked",
	   buildOptimized ? "optimized" : "unoptimized");
}

// A fatal error
void
Fatal (char const *format, ...) noexcept
{  __asm__ volatile ("nop"); // Fatal Error
  va_list args;
  va_start (args, format);
  fprintf (stderr, "%s: fatal: ",
	   progName ? progName : program_invocation_short_name);
  vfprintf (stderr, format, args);
  va_end (args);
  fprintf (stderr, "\n");

  exit (1);
}

} // namespace NMS
