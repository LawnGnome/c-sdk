#
# Makefile for New Relic C-Agent Code Examples
#

OBJS = ex_notice_error.o ex_common.o

# This Makefile presumes that the locations of libnewrelic.h and libnewrelic.a are one level above this
# example directory.
LIBNEWRELIC_A_LOCATION = ../
LIBNEWRELIC_H_LOCATION = ../

all:	ex_notice_error.out

ex_notice_error.out:	$(OBJS) ../libnewrelic.a
	$(CC) -o $@ $(OBJS) -L$(LIBNEWRELIC_A_LOCATION) -lnewrelic $(PCRE_CFLAGS) -L/usr/local/lib/ -lpcre  -pthread

%.o: %.c
	$(CC) -c $< -o $@ -I$(LIBNEWRELIC_H_LOCATION) -DHAVE_BACKTRACE=1

clean:
	rm -f *.o *.out c_agent.log