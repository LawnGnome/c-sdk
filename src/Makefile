#
# This is not a standalone Makefile in its current form; it depends on a bunch
# of variables that are defined by the top level Makefile. We could revisit
# this later if we wanted to.
#

.PHONY: clean static
clean:
	rm -f *.o libnewrelic.a libnewrelic.so

static: libnewrelic.a

OBJS := \
	app.o \
	app_internal.o \
	attribute.o \
	config.o \
	libnewrelic.o \
	libnewrelic_internal.o \
	version.o

version.o: $(C_AGENT_ROOT)/VERSION

%.o: %.c
	$(CC) $(C_AGENT_CPPFLAGS) $(VERSION_FLAGS) $(CPPFLAGS) $(C_AGENT_CFLAGS) $(PCRE_CFLAGS) $(CFLAGS) -c $< -o $@

#
# Note that this rule only builds the C agent object files into the static
# library. In practice, users will also need to link against axiom, or bundle
# this static library with the axiom static library. (Spoilers for what we do
# in the parent Makefile.)
#
libnewrelic.a: $(OBJS)
	$(AR) rcs $@ $^
