#
# Makefile for New Relic C-Agent Code Examples
#

# This Makefile presumes that libnewrelic.h and libnewrelic.a are at the following
# locations:

LIBNEWRELIC_A_LOCATION = ..
LIBNEWRELIC_H_LOCATION = ../include

# Pull in PCRE config.
PCRE_CFLAGS := $(shell pcre-config --cflags)
PCRE_LIBS := $(shell pcre-config --libs)

EXAMPLES := \
	ex_notice_error.out \

all: $(EXAMPLES)

ex_%.out: ex_%.o common.o ../libnewrelic.a
	$(CC) -o $@ $< common.o -rdynamic -L$(LIBNEWRELIC_A_LOCATION) -lnewrelic $(PCRE_LIBS) -pthread

%.o: %.c
	$(CC) -c $< -o $@ -I$(LIBNEWRELIC_H_LOCATION) $(PCRE_CFLAGS)

clean:
	rm -f *.o $(EXAMPLES) c_agent.log
