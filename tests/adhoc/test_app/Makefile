LIBNEWRELIC_INCLUDE := ../../../include
LIBNEWRELIC_LIB := ../../../libnewrelic.a

CFLAGS := -O0 -g3
CPPFLAGS := -I$(LIBNEWRELIC_INCLUDE) $(shell pcre-config --cflags)
LDFLAGS := -rdynamic
LIBS := $(LIBNEWRELIC_LIB) -lpcre -pthread

all: test_app

.PHONY: clean
clean:
	rm -r *.o test_app

test_app: test_app.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)
