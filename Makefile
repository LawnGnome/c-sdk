#
# "What's in the bag? A shark or something?"
#
# No, Nicolas. It's a C agent, and we're going to build it.
#
# Useful top level targets:
#
# - all:       Builds libnewrelic.a
# - clean:     Removes all build products
# - daemon:    Builds the daemon in php_agent/bin
# - dynamic:   Builds libnewrelic.so
# - static:    Builds libnewrelic.a
# - run_tests: Runs the unit tests
#

#
# The PHP agent's build system does a bunch of useful platform detection that
# we need both in general and to utilise axiom. Let's pull it in straightaway.
#
include php_agent/make/config.mk

#
# Set up the basic variables required to build the C agent. This is mostly
# knowing where we are in the filesystem and setting up the appropriate
# compiler flags.
#
C_AGENT_ROOT := $(shell pwd)

C_AGENT_CPPFLAGS := $(PLATFORM_DEFS)
C_AGENT_CPPFLAGS += -I$(C_AGENT_ROOT)/php_agent/axiom -I$(C_AGENT_ROOT)/include

C_AGENT_CFLAGS := -std=gnu99 -fPIC -DPIC -pthread
C_AGENT_CFLAGS += -Wall
C_AGENT_CFLAGS += -Werror
C_AGENT_CFLAGS += -Wextra
C_AGENT_CFLAGS += -Wbad-function-cast
C_AGENT_CFLAGS += -Wcast-qual
C_AGENT_CFLAGS += -Wdeclaration-after-statement
C_AGENT_CFLAGS += -Wimplicit-function-declaration
C_AGENT_CFLAGS += -Wmissing-declarations
C_AGENT_CFLAGS += -Wmissing-prototypes
C_AGENT_CFLAGS += -Wno-write-strings
C_AGENT_CFLAGS += -Wpointer-arith
C_AGENT_CFLAGS += -Wshadow
C_AGENT_CFLAGS += -Wstrict-prototypes
C_AGENT_CFLAGS += -Wswitch-enum

ifeq (1,$(HAVE_CLANG))
C_AGENT_CFLAGS += -Wbool-conversion
C_AGENT_CFLAGS += -Wempty-body
C_AGENT_CFLAGS += -Wheader-hygiene
C_AGENT_CFLAGS += -Wimplicit-fallthrough
C_AGENT_CFLAGS += -Wlogical-op-parentheses
C_AGENT_CFLAGS += -Wloop-analysis
C_AGENT_CFLAGS += -Wsizeof-array-argument
C_AGENT_CFLAGS += -Wstring-conversion
C_AGENT_CFLAGS += -Wswitch
C_AGENT_CFLAGS += -Wswitch-enum
C_AGENT_CFLAGS += -Wuninitialized
C_AGENT_CFLAGS += -Wunused-label
endif

#
# The OPTIMIZE flag should be set to 1 to enable a "release" build: that is,
# one with appropriate optimisation and minimal debugging information.
# Otherwise, the default is to make a "debug" build, with no optimisation and
# full debugging information.
#
ifeq (1,$(OPTIMIZE))
	C_AGENT_CFLAGS += -O3 -g1
	C_AGENT_LDFLAGS += -O3 -g1
else
	C_AGENT_CFLAGS += -O0 -g3
	C_AGENT_LDFLAGS += -O0 -g3 -rdynamic
endif

export C_AGENT_ROOT C_AGENT_CFLAGS C_AGENT_CPPFLAGS

#
# Set up the appropriate flags for cmocka, since we use that for unit tests.
#
CMOCKA_LIB = $(C_AGENT_ROOT)/vendor/cmocka/build/src/libcmocka.a
CMOCKA_INCLUDE = -I$(C_AGENT_ROOT)/vendor/cmocka/include

export CMOCKA_LIB
export CMOCKA_INCLUDE

# TODO(msl): OS X 10.11 (at least) does not provide pcre-config by default.
# Check whether it exists, and if not assume a sensible default.
PCRE_CFLAGS := $(shell pcre-config --cflags)

#
# We pull in the current agent version from the VERSION file, and expose it to
# the source code as the NEWRELIC_VERSION preprocessor define.
#
AGENT_VERSION := $(shell if test -f VERSION; then cat VERSION; fi)
VERSION_FLAGS += -DNEWRELIC_VERSION=$(AGENT_VERSION)

export AGENT_VERSION VERSION_FLAGS

all: libnewrelic.a

#
# This rule builds a static axiom library and a static C agent library, and
# then uses GNU ar's MRI support to smoosh them together into a single,
# beautiful library.
#
# TODO: consider linking PCRE in, if we can rename/hide it from symbol
#       collisions.
# TODO: maybe have a fallback that doesn't rely on GNU ar.
#
libnewrelic.a: combine.mri axiom src-static
	$(AR) -M < $<

.PHONY: static
static: libnewrelic.a

# Unlike axiom, we can't use a target-specific variable to send the CFLAGS and
# LDFLAGS, as those propagate to dependent rules and cmocka won't build with
# all the warnings we enabled. Instead, we'll use command line variables when
# invoking the sub-make to pass them in.
.PHONY: run_tests
run_tests: vendor libnewrelic.a
	$(MAKE) -C tests run_tests CFLAGS="$(C_AGENT_CFLAGS) -Wno-bad-function-cast" LDFLAGS="$(C_AGENT_LDFLAGS)"

.PHONY: vendor
vendor:
	$(MAKE) -C vendor

.PHONY: axiom
axiom: php_agent/axiom/libaxiom.a

php_agent/axiom/libaxiom.a: export CFLAGS := $(C_AGENT_CFLAGS)
php_agent/axiom/libaxiom.a: php_agent/Makefile
	$(MAKE) -C php_agent/axiom

.PHONY: axiom-clean
axiom-clean:
	$(MAKE) -C php_agent/axiom clean

.PHONY: daemon
daemon:
	$(MAKE) -C php_agent daemon

.PHONY: daemon-clean
daemon-clean:
	$(MAKE) -C php_agent daemon-clean

.PHONY: dynamic
dynamic: libnewrelic.so

#
# We build the shared library at the top level, since it's easiest to just take
# the static library and have gcc wrap it in the appropriate shared library
# goop.
#
# TODO: statically link to pcre instead
#
libnewrelic.so: libnewrelic.a
	$(CC) -shared -pthread $(PCRE_CFLAGS) -ldl -o $@ -Wl,--whole-archive $^  -Wl,--no-whole-archive

.PHONY: src-static
src-static:
	$(MAKE) -C src static

.PHONY: src-clean
src-clean:
	$(MAKE) -C src clean

.PHONY: tests-clean
tests-clean:
	$(MAKE) -C tests clean
	$(MAKE) -C vendor clean

.PHONY: clean
clean: axiom-clean daemon-clean src-clean tests-clean
	rm -f *.o test_app stress_app libnewrelic.a libnewrelic.so

# Implicit rule for top level test programs.
%.o: %.c
	$(CC) $(C_AGENT_CPPFLAGS) $(VERSION_FLAGS) $(CPPFLAGS) $(C_AGENT_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -c $< -o $@

# this can't be run when the .so is present or else it will use it
test_app: test_app.o libnewrelic.a
	$(CC) -rdynamic -o $@ -I$(C_AGENT_ROOT)/include $^ $(PCRE_CFLAGS) -L/usr/local/lib/ -lpcre  -pthread

test_app_dynamic: test_app.o libnewrelic.so
	$(CC) -o $@ $< -L. -lnewrelic
