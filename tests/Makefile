#
# The Makefile for the C-agent unit test suite.
#
# all:       Builds but does not run the tests.
# run_tests: Builds and runs the tests.

#
# Operating system detection.
#
include ../php_agent/make/config.mk

# These flags that are required to build the tests, and should be unioned with
# any flags inherited from the environment.
TEST_CFLAGS := -std=gnu99 -pthread $(CMOCKA_INCLUDE)

TEST_CFLAGS += -Wall
TEST_CFLAGS += -Werror
TEST_CFLAGS += -Wextra
TEST_CFLAGS += -Wcast-qual
TEST_CFLAGS += -Wdeclaration-after-statement
TEST_CFLAGS += -Wimplicit-function-declaration
TEST_CFLAGS += -Wmissing-declarations
TEST_CFLAGS += -Wmissing-prototypes
TEST_CFLAGS += -Wno-write-strings
TEST_CFLAGS += -Wpointer-arith
TEST_CFLAGS += -Wshadow
TEST_CFLAGS += -Wstrict-prototypes
TEST_CFLAGS += -Wswitch-enum

ifeq (1,$(HAVE_CLANG))
TEST_CFLAGS += -Wbool-conversion
TEST_CFLAGS += -Wempty-body
TEST_CFLAGS += -Wheader-hygiene
TEST_CFLAGS += -Wimplicit-fallthrough
TEST_CFLAGS += -Wlogical-op-parentheses
TEST_CFLAGS += -Wloop-analysis
TEST_CFLAGS += -Wsizeof-array-argument
TEST_CFLAGS += -Wstring-conversion
TEST_CFLAGS += -Wswitch
TEST_CFLAGS += -Wswitch-enum
TEST_CFLAGS += -Wuninitialized
TEST_CFLAGS += -Wunused-label
endif

TEST_CPPFLAGS := -I../php_agent/axiom -I../php_agent/axiom/tests -I..
TEST_LDFLAGS :=
TEST_LDLIBS := -L.. -lnewrelic $(CMOCKA_LIB)

# -pthread must be passed to the compiler, but not the linker when using Clang.
ifneq (1,$(HAVE_CLANG))
  TEST_LDFLAGS += -pthread
endif

ifeq (1,$(HAVE_LIBEXECINFO))
  TEST_LDLIBS += -lexecinfo
endif

# -ldl is required on GNU/Linux for dladdr. We'll test that via a very simple
# C compiler one-liner.
ifeq (Linux,$(UNAME))
  ifeq (1,$(shell (echo "int dladdr(void *, void *); int main() { dladdr(0, 0); return 0; }" | $(CC) -x c -o /dev/null - 1>/dev/null 2>/dev/null) && echo 0 || echo 1))
    TEST_LDLIBS += -ldl
  endif
endif

ifeq (SunOS,$(UNAME))
  TEST_LDLIBS += -lmalloc -lm -lsocket -lnsl -lrt
endif

#
# Flags required to link PCRE.
#
PCRE_CFLAGS := $(shell pcre-config --cflags)

# NRCAMP pcre-config --libs does not provide -L.. which causes the wrong
# pcre library to be linked if pcre is installed in the standard search
# paths, and linking to fail if pcre is not installed.
ifneq (,$(findstring /opt/nr/camp,$(shell pcre-config --prefix)))
  ifneq (SunOS,$(UNAME))
    PCRE_LDLIBS := -L/opt/nr/camp/lib -lnrpcre-pic
  else
    PCRE_LDLIBS := -lnrpcre-pic
  endif
else
  PCRE_LDLIBS := $(shell pcre-config --libs)
endif

all: tests

#
# Tests. Add a binary to this list to have it built by default. Note that the
# file name must start with test_.
#
TESTS := \
	test_config \
	test_destroy \
	test_end_transaction \
	test_txn \
	test_newrelic_start_transaction \
	test_newrelic_create_app \
	test_add_attribute \

#
# The list of tests to skip and tests to run.
#
SKIP_TESTS =
RUNNABLE_TESTS = $(filter-out $(SKIP_TESTS),$(TESTS))

#
# Implicit rule to build object files. We define them as precious to avoid
# recompilation; we're figuring out dependencies anyway, so we'll want to keep
# them.
#
.PRECIOUS: %.o
%.o: %.c Makefile .deps/compile_flags
	$(CC) $(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

#
# The top level phony rule to build the tests.
#
.PHONY: tests
tests: $(TESTS)

#
# All test binaries depend on libnewrelic.a.
#
# The shell portion of this pattern rule does the following:
# 1) extracts all symbols from the object file that begin with `__wrap`;
# 2) transforms the symbols to strings representing the gcc linker options necessary to link with libcmocka.
#
# For example, suppose the following object file has the following symbols:
#
# $ nm test_example.o | grep -o "__wrap\w*"
# __wrap_foo
# __wrap_bar
#
# These symbols would be transformed into these linker optioms:
#
# -Wl,--wrap=foo -Wl,--wrap=bar
#
#
test_%: test_%.o ../libnewrelic.a Makefile .deps/link_flags
	$(CC) $(TEST_LDFLAGS) $(LDFLAGS) -o $@ $< $(TEST_LDLIBS) $(PCRE_LDLIBS) $(LDLIBS) $(shell nm $< | grep -o "__wrap\w*" | sed 's/__wrap_/-Wl,--wrap=/g')

#
# The top level rule to run the tests.
#
.PHONY: check run_tests
check run_tests: tests Makefile
	@for T in $(RUNNABLE_TESTS); do ./$$T $(TESTARGS); done

#
# Track the flags passed to the compiler to force a rebuild when they change.
# These rules must kept in sync with the pattern rules used to perform the
# compilation and linking.
#
# The trick here is forcing the .deps/*_flags targets to be re-evaluated for
# each build while ensuring they are only out of date if their contents need
# to be updated. We use a PHONY dependency to do so.
#
.PHONY: force
.deps/compile_flags: force | .deps/
	@echo '$(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -MMD -MP' | cmp -s - $@ || echo '$(TEST_CPPFLAGS) $(CPPFLAGS) $(TEST_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -MMD -MP' > $@

.deps/link_flags: force | .deps/
	@echo '$(TEST_LDFLAGS) $(LDFLAGS) $(TEST_LDLIBS) $(PCRE_LDLIBS) $(LDLIBS)' | cmp -s - $@ || echo '$(TEST_LDFLAGS) $(LDFLAGS) $(TEST_LDLIBS) $(PCRE_LDLIBS) $(LDLIBS)' > $@

.deps/:
	@mkdir .deps

#
# Clean up build products: that's object files, dependency files,
# and any output files.
#
clean:
	rm -f *.gcov *.gcno *.gcda
	rm -f *.d *.o $(TESTS)
	rm -rf .deps *.dSYM

#
# Dependency handling. When we build a .o file, we also build a .d file
# containing that module's dependencies using -MM. Those files are in Makefile
# format, so we include them here to define dependency rules: this means that
# if we change a header, all affected modules will be recompiled automatically.
#
-include $(TESTS:%=%.d)

# vim: set noet ts=2 sw=2:
