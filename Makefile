include php_agent/make/config.mk

AGENT_SDK_CPPFLAGS := $(PLATFORM_DEFS)
AGENT_SDK_CPPFLAGS += -Iphp_agent/axiom

AGENT_SDK_CFLAGS := -std=gnu99 -fPIC -DPIC -pthread
AGENT_SDK_CFLAGS += -Wall
AGENT_SDK_CFLAGS += -Werror
AGENT_SDK_CFLAGS += -Wextra
AGENT_SDK_CFLAGS += -Wbad-function-cast
AGENT_SDK_CFLAGS += -Wcast-qual
AGENT_SDK_CFLAGS += -Wdeclaration-after-statement
AGENT_SDK_CFLAGS += -Wimplicit-function-declaration
AGENT_SDK_CFLAGS += -Wmissing-declarations
AGENT_SDK_CFLAGS += -Wmissing-prototypes
AGENT_SDK_CFLAGS += -Wno-write-strings
AGENT_SDK_CFLAGS += -Wpointer-arith
AGENT_SDK_CFLAGS += -Wshadow
AGENT_SDK_CFLAGS += -Wstrict-prototypes
AGENT_SDK_CFLAGS += -Wswitch-enum

ifeq (1,$(HAVE_CLANG))
AGENT_SDK_CFLAGS += -Wbool-conversion
AGENT_SDK_CFLAGS += -Wempty-body
AGENT_SDK_CFLAGS += -Wheader-hygiene
AGENT_SDK_CFLAGS += -Wimplicit-fallthrough
AGENT_SDK_CFLAGS += -Wlogical-op-parentheses
AGENT_SDK_CFLAGS += -Wloop-analysis
AGENT_SDK_CFLAGS += -Wsizeof-array-argument
AGENT_SDK_CFLAGS += -Wstring-conversion
AGENT_SDK_CFLAGS += -Wswitch
AGENT_SDK_CFLAGS += -Wswitch-enum
AGENT_SDK_CFLAGS += -Wuninitialized
AGENT_SDK_CFLAGS += -Wunused-label
endif

# TODO(msl): OS X 10.11 (at least) does not provide pcre-config by default.
# Check whether it exists, and if not assume a sensible default.
PCRE_CFLAGS := $(shell pcre-config --cflags)

AGENT_VERSION := $(shell if test -f VERSION; then cat VERSION; fi)
VERSION_FLAGS += -DNEWRELIC_VERSION=$(AGENT_VERSION)

OBJS := \
	libnewrelic.o \
	version.o

all: axiom libnewrelic.a

PHONY: run_tests
run_tests:
	$(MAKE) -C tests run_tests

.PHONY: axiom
axiom: php_agent/axiom/libaxiom.a

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

libnewrelic.a: $(OBJS) php_agent/axiom/libaxiom.a
	cp php_agent/axiom/libaxiom.a libnewrelic.a
	$(AR) rcs $@ $(OBJS)

# TODO: statically link to pcre instead
libnewrelic.so: libnewrelic.a
	$(CC) -shared -pthread $(PCRE_CFLAGS) -ldl -o $@ -Wl,--whole-archive $^  -Wl,--no-whole-archive

version.o: VERSION

%.o: %.c
	$(CC) $(AGENT_SDK_CPPFLAGS) $(VERSION_FLAGS) $(CPPFLAGS) $(AGENT_SDK_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean: axiom-clean daemon-clean
	rm -f *.o libnewrelic.a libnewrelic.so test_app
	$(MAKE) -C tests clean

dynamic: libnewrelic.so

# this can't be run when the .so is present or else it will use it
test_app: test_app.o libnewrelic.a
	$(CC) -o test_app test_app.o -L. -lnewrelic $(PCRE_CFLAGS) -L/usr/local/lib/ -lpcre  -pthread

test_app_dynamic: test_app.o libnewrelic.so
	$(CC) -o test_app test_app.o -L. -lnewrelic 
