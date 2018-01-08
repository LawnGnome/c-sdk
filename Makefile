#
# The top level Makefile
#
# For details on building the PHP agent, please refer to the documentation in
# the docs directory. Quick links:
#
# Development guide: https://source.datanerd.us/php-agent/php_agent/blob/master/docs/development_guide.md#build-the-php-agent
# Make reference: https://source.datanerd.us/php-agent/php_agent/blob/master/docs/make.md
#
# If all you want to do is build the agent against the PHP found in your PATH:
#     make
#
# You can also run some tests:
#     make check
#
# And you can clean up:
#     make clean
#
# For everything else, please see the links above.
#

GCOV  ?= gcov
GO    ?= go
SHELL = /bin/bash

include make/config.mk
include make/version.mk

# Configure an isolated workspace for the Go daemon.
export GOPATH=$(CURDIR)
export GO15VENDOREXPERIMENT=1

# GOBIN affects the behavior of go install, ensure it is unset.
unexport GOBIN

GOFLAGS += -ldflags '-X newrelic/version.Number=$(AGENT_VERSION) -X newrelic/version.Commit=$(GIT_COMMIT)'

#
# TODO(msl): Should we use override to force required flags to be set
# even when CFLAGS, CPPFLAGS and/or LDFLAGS is overidden by the make
# invocation? e.g. make CFLAGS=-Os
#

#
# Uniformly apply compiler and linker flags for the target architecture.
# This includes ensuring these flags are picked up by the agent's configure
# script. Otherwise, the agent build may fail if the agent's target
# architecture does not match axiom.
#
CFLAGS += $(MODEL_FLAGS)
LDFLAGS += $(MODEL_FLAGS)

ifeq (1,$(OPTIMIZE))
  CFLAGS += -O3 -g1
  LDFLAGS += -O3 -g1
else
  CFLAGS += -O0 -g3 -DENABLE_TESTING_API
  LDFLAGS += -O0 -g3
endif

ifeq (1,$(ENABLE_LTO))
  CFLAGS += -flto
  LDFLAGS += -flto
endif

#
# Code coverage
#
ifeq (1,$(ENABLE_COVERAGE))
  CPPFLAGS += -DDO_GCOV
  CFLAGS += -fprofile-arcs -ftest-coverage
  LDFLAGS += --coverage
endif

#
# Sanitizers
#
# Support for sanitizers varies by compiler and platform. Generally, it's best
# to use these on Linux and with the latest version of Clang, but GCC 4.9 or
# newer works as well. Compiling with -O1 or -Og with full debug info is
# strongly recommended for usable stack traces. See the Clang user manual
# for more information and options.
#
# Common sanitizers: address, integer, memory, thread, undefined
#
ifneq (,$(SANITIZE))
  CFLAGS += -fsanitize=$(SANITIZE) -fno-omit-frame-pointer
  LDFLAGS += -fsanitize=$(SANITIZE)
endif

#
# When the silent flag (-s) is given, configure and libtool should
# also be quiet.
#
ifneq (,$(findstring s,$(MAKEFLAGS)))
SILENT := --silent
endif

#
# At this point the following variables should have their final values,
# and can be safely exported to recipe commands.
#
export AR CC CFLAGS CPPFLAGS LDFLAGS

.PHONY: all
all: agent daemon

#
# Print some of the build specific variables so they can be computed
# once in this file, but shared with our Jenkins build scripts. Values
# should be formatted in a shell compatible NAME=VALUE format so the
# build scripts can safely eval the output of this target. NOTE: the
# build scripts are sensitive to the variable names. If you need
# to print something that is not in this format, or should otherwise
# be ignored by the build scripts, prefix it with '#' so it will be
# treated as a comment.
#
.PHONY: echo_config
echo_config:
	@echo '# Ignore me, Makefile example comment'
	@echo VERSION=$(AGENT_VERSION)
	@echo GIT_COMMIT=$(GIT_COMMIT)

#
# Let's build an agent! Building an agent is a three step process: using phpize
# to build a configure script, using configure to build a Makefile, and then
# actually using that Makefile to build the agent extension.
#
PHPIZE := phpize
PHP_CONFIG := php-config

.PHONY: agent
agent: agent/Makefile
	$(MAKE) -C agent

agent/configure: agent/config.m4 agent/Makefile.frag
	cd agent; $(PHPIZE) --clean && $(PHPIZE)

agent/Makefile: agent/configure | axiom
	cd agent; ./configure $(SILENT) --enable-newrelic --with-axiom=$(realpath axiom) --with-php-config=$(PHP_CONFIG)

#
# Installs the agent into the extension directory of the appropriate PHP
# installation. By default this is directory name formed by evaluating
# $(INSTALL_ROOT)$(EXTENSION_DIR). The former is empty by default, so to
# install the agent into the directory of your choice run the following.
#
#   make agent-install EXTENSION_DIR=...
#
agent-install: agent
	$(MAKE) -C agent install

.PHONY: agent-clean
agent-clean:
	cd agent; [ -f Makefile ] && $(MAKE) clean; $(PHPIZE) --clean

#
# Agent tests. These just delegate to the agent Makefile: all the smarts are in
# agent/Makefile.frag.
#
.PHONY: agent-tests
agent-tests: agent/Makefile
	$(MAKE) -C agent unit-tests

.PHONY: agent-check agent-run-tests
agent-check agent-run-tests: agent/Makefile
	$(MAKE) -C agent run-unit-tests

.PHONY:
agent-valgrind: agent/Makefile
	$(MAKE) -C agent valgrind

#
# Build the daemon and related utilities
#

#
# Minimum required version of Go is 1.5.
#
# This is defined as a rule that other rules that require Go can depend upon.
# We don't want to require Go for a general build primarily to make the PHP
# agent easier to use as a component within the C agent.
#
.PHONY: go-minimum-version
go-minimum-version:
	@if $(GO) version | awk '/go1.[01234][\. ]/ { exit 1 }'; then \
		true; \
	else \
		echo -n 'Go 1.5 or newer required; found '; $(GO) version; false; \
	fi

DAEMON_TARGETS := $(addprefix bin/,client daemon integration_runner stressor)

# Delete Go binaries before each build to force them to be re-linked. This
# ensures the version and commit variables are set correctly by the linker.
#
# TODO(msl): The bin directory is also the target directory for the installer,
# which we need to be careful to leave in place. Therefore, the names of the
# Go binaries are made explicit. If we used a conventional `cmd` subdirectory
# for commands, we could use `go list` to determine the names.
.PHONY: daemon
daemon: go-minimum-version Makefile | bin/
	@rm -rf $(DAEMON_TARGETS)
	$(GO) install $(GOFLAGS) ./...

.PHONY: daemon_test
daemon_test: go-minimum-version
	$(GO) test $(GOFLAGS) ./...

.PHONY: daemon_bench
daemon_bench: go-minimum-version
	$(GO) test $(GOFLAGS) -bench=. ./...

.PHONY: daemon_integration
daemon_integration: go-minimum-version
	$(GO) test $(GOFLAGS) -tags integration ./...

DAEMON_COV_FILE = daemon_coverage.out
.PHONY: daemon_cover
daemon_cover: go-minimum-version
	@rm -f $(DAEMON_COV_FILE)
	$(GO) test -coverprofile=$(DAEMON_COV_FILE) $(GOFLAGS) newrelic
	$(GO) tool cover -html=$(DAEMON_COV_FILE)
	@rm -f $(DAEMON_COV_FILE)

# Note that this rule does not require the Go binary, and therefore doesn't
# depend on go-minimum-version.
.PHONY: daemon-clean
daemon-clean:
	rm -f $(DAEMON_TARGETS)
	rm -rf pkg/*

#
# Build the installer plus support utility. The support utility is run by
# the installer to provide some helpful operations missing from bash or
# missing from the $PATH.
#

installer: bin/newrelic-install bin/newrelic-iutil | bin/

bin/newrelic-install: agent/newrelic-install.sh Makefile VERSION | bin/
	sed -e "/nrversion:=/s,UNSET,$(AGENT_VERSION)," $< > $@
	chmod 755 $@

bin/newrelic-iutil: agent/install-util.c Makefile VERSION | bin/
	$(CC) -DNR_VERSION="\"$(AGENT_VERSION)\"" $(CPPFLAGS) $(CFLAGS) -o $@ $< $(LDFLAGS)


#
# Directories required during builds.
#

bin/:
	mkdir bin

#
# Build axiom and the axiom tests
#

.PHONY: axiom
axiom:
	$(MAKE) -C axiom

#
# TESTARGS is passed to every invocation of a test program.
# If you want to run each tests with 16 way thread parallelism, you would do:
#   make TESTARGS=-j16 run_tests
#
# TESTARGS =

.PHONY: axiom-tests
axiom-tests:
	$(MAKE) -C axiom tests

.PHONY: axiom-check axiom-run-tests
axiom-check axiom-run-tests: axiom/tests/cross_agent_tests
	$(MAKE) -C axiom run_tests

.PHONY: axiom-valgrind
axiom-valgrind: axiom/tests/cross_agent_tests
	$(MAKE) -C axiom valgrind

.PHONY: tests
tests: agent-tests axiom-tests

.PHONY: check run_tests
check run_tests: agent-run-tests axiom-run-tests

.PHONY: valgrind
valgrind: agent-valgrind axiom-valgrind

axiom/tests/cross_agent_tests:
	$(error Please run "git submodule update --init" to install the cross agent tests.)

.PHONY: axiom-clean
axiom-clean:
	$(MAKE) -C axiom clean

#
# Agent integration testing
#

.PHONY: integration
integration: Makefile daemon
	for PHP in $${PHPS:-7.2 7.1 7.0 5.6 5.5 5.4 5.3}; do \
          echo; echo "# PHP=$${PHP}"; \
	  env NRLAMP_PHP=$${PHP} bin/integration_runner $(INTEGRATION_ARGS) || exit 1; \
	  echo "# PHP=$${PHP}"; \
	done

#
# Code profiling
#

.PHONY: gcov
gcov: Makefile
	cd agent; $(GCOV) $(GCOV_FLAGS) *.gcno
	cd agent; $(GCOV) -o .libs $(GCOV_FLAGS) .libs/*.gcno
	cd axiom; $(GCOV) $(GCOV_FLAGS) *.gcno
	cd axiom/tests; $(GCOV) $(GCOV_FLAGS) *.gcno

# Reset code coverage line counts.
.PHONY: gcov_reset
gcov_reset:
	find . -name \*.gcda | xargs rm -f

#
# Clean up
#

.PHONY: clean
clean: agent-clean axiom-clean daemon-clean package-clean
	rm -rf releases
	rm -f agent/newrelic.map agent/LicenseData/license_errors.txt

.PHONY: package-clean
package-clean:
	rm -rf releases/debian releases/*.deb
	rm -rf releases/redhat releases/*.rpm
	rm -rf releases/newrelic-php5-*.tar.gz

#
# Extras
#

include make/release.mk

# vim: set noet ts=2 sw=2:
