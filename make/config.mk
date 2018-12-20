#
# Platform specific configuration.
#

#
# OS-specific defines that axiom needs. These are:
#
# NR_SYSTEM_*:               The operating system being compiled on.
#

UNAME := $(shell uname)

#
# Detect platform specific features. Autotools is the right way to do
# this, but our needs are simple and the variation amoung our supported
# platforms is narrow.
#

# Whether the compiler is Clang/LLVM.
HAVE_CLANG := $(shell echo __clang__ | $(CC) -E - | tail -1)

# Whether you have alloca.h
HAVE_ALLOCA_H := $(shell test -e /usr/include/alloca.h && echo 1 || echo 0)

# Whether you have /dev/fd
HAVE_DEV_FD := $(shell test -d /dev/fd && echo 1 || echo 0)

# Whether you have /proc/self/fd
HAVE_PROC_SELF_FD := $(shell test -d /proc/self/fd && echo 1 || echo 0)

# Whether you have closefrom
HAVE_CLOSEFROM := 0

# Whether you have backtrace and backtrace_symbols_fd
HAVE_BACKTRACE := $(shell test -e /usr/include/execinfo.h && echo 1 || echo 0)

# Whether you have libexecinfo
HAVE_LIBEXECINFO := $(shell test -e /usr/lib/libexecinfo.so -o -e /usr/lib/libexecinfo.a && echo 1 || echo 0)

# Whether you have PTHREAD_MUTEX_ERRORCHECK
HAVE_PTHREAD_MUTEX_ERRORCHECK := $(CC) pthread_test.c -o /dev/null -pthread && echo 1 || echo 0)

ifeq (Darwin,$(UNAME))
  OS := Darwin
  ARCH := $(shell uname -m | sed -e 's/i386/x86/' -e 's/x86_64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_DARWIN=1
endif

ifeq (FreeBSD,$(UNAME))
  OS := FreeBSD
  ARCH := $(shell uname -p | sed -e 's/i386/x86/' -e 's/amd64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_FREEBSD=1
  HAVE_CLOSEFROM := 1
endif

ifeq (Linux,$(UNAME))
  OS := Linux
  ARCH := $(shell uname -m | sed -e 's/i[3456]86/x86/' -e 's/x86_64/x64/')
  PLATFORM_DEFS := -DNR_SYSTEM_LINUX=1
endif

ifeq (SunOS,$(UNAME))
  OS := Solaris
  # Solaris is a multi-architecture system, so we default to 64-bit.
  ARCH := x64
  PLATFORM_DEFS := -DNR_SYSTEM_SOLARIS=1 -D_POSIX_THREAD_SEMANTICS
  HAVE_CLOSEFROM := 1
endif

ifeq (x86,$(ARCH))
  # Minimum CPU requirement for the agent on x86 is SSE2.
  MODEL_FLAGS := -m32 -msse2 -mfpmath=sse

  ifeq (Darwin,$(UNAME))
    MODEL_FLAGS += -arch i386
  endif
endif

ifeq (x64,$(ARCH))
  MODEL_FLAGS := -m64

  ifeq (Darwin,$(UNAME))
    MODEL_FLAGS += -arch x86_64
  endif
endif

ifeq (1,$(HAVE_ALLOCA_H))
  PLATFORM_DEFS += -DHAVE_ALLOCA_H=1
endif

ifeq (1,$(HAVE_BACKTRACE))
  PLATFORM_DEFS += -DHAVE_BACKTRACE=1
endif

ifeq (1,$(HAVE_CLOSEFROM))
  PLATFORM_DEFS += -DHAVE_CLOSEFROM=1
endif

ifeq (1,$(HAVE_DEV_FD))
  PLATFORM_DEFS += -DHAVE_DEV_FD=1
endif

ifeq (1,$(HAVE_PROC_SELF_FD))
  PLATFORM_DEFS += -DHAVE_PROC_SELF_FD=1
endif

ifeq (1,$(HAVE_PTHREAD_MUTEX_ERRORCHECK))
  PLATFORM_DEFS += -DHAVE_PTHREAD_MUTEX_ERRORCHECK=1
endif
