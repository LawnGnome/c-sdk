#
# This Makefile is supplied so that New Relic C-Agent developers may
# conveniently build the vendor-supplied libraries using this top-level
# make command:
#
# $ make vendor

all:	cmocka

cmocka: build_dir
	(cd cmocka/build && cmake ..)
	$(MAKE) -C cmocka/build
	(cd cmocka/build && sudo make install)
	export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

.PHONY: build_dir
build_dir:
	mkdir -p cmocka/build

# This clean rule is currently not available from the top-level make
# command.  It doesn't seem that developers want to rebuild a vendor
# library from scratch on a regular basis.
clean:
	rm -rf cmocka/build
