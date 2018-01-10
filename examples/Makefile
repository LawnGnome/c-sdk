LIBNEWRELIC_INCLUDE = -I../

all:	ex_notice_error

ex_notice_error:	ex_notice_error.o ../libnewrelic.a
	$(CC) -o $@ $< -L. -lnewrelic $(PCRE_CFLAGS) -L/usr/local/lib/ -lpcre  -pthread

%.o: %.c
	$(CC) $(LIBNEWRELIC_INCLUDE) -c $< -o $@
