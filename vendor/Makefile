#
# This Makefile is supplied so that New Relic C-Agent developers may
# conveniently build the vendor-supplied libraries using this top-level
# make command:
#
# $ make vendor
CMOCKA_CMAKE_OPTIONS = -DWITH_STATIC_LIB=true

.PHONY: cmocka cmocka-clean

cmocka:	$(CMOCKA_LIB)

$(CMOCKA_LIB):	vendor/cmocka/build/Makefile
	$(MAKE) -C cmocka/build cmocka_static

vendor/cmocka/build/Makefile:
	mkdir -p cmocka/build
	cd cmocka/build && cmake .. $(CMOCKA_CMAKE_OPTIONS)

# This clean rule is currently not available from the top-level make
# command.  It doesn't seem that developers want to rebuild a vendor
# library from scratch on a regular basis.
cmocka-clean:
	rm -rf cmocka/build
