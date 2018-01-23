#
# Makefile for New Relic C-Agent Code Examples
#

# This Makefile presumes that libnewrelic.h and libnewrelic.a are at the following
# locations:

LIBNEWRELIC_A_LOCATION = ..
LIBNEWRELIC_H_LOCATION = ../include

EXAMPLES := \
	ex_notice_error.out \

all: $(EXAMPLES)

ex_%.out: ex_%.o common.o ../libnewrelic.a
	$(CC) -o $@ $< common.o -rdynamic -L$(LIBNEWRELIC_A_LOCATION) -lnewrelic $(PCRE_CFLAGS) -L/usr/local/lib/ -lpcre  -pthread

%.o: %.c
	$(CC) -c $< -o $@ -I$(LIBNEWRELIC_H_LOCATION)

clean:
	rm -f *.o $(EXAMPLES) c_agent.log