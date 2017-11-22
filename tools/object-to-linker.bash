#!/bin/bash

# Given an object file that contains cmocka mocks:
# 1) extract all symbols from the object file that begin with `__wrap`;
# 2) transform the symbols to strings representing the gcc linker options necessary to link with libcmocka.
#
# For example, suppose the following object file has the following symbols:
#
# $ nm test_example.o | grep -o "__wrap\w*"
# __wrap_foo
# __wrap_bar
#
# An invocation of this script would do the following:
#
# $ object-to-linker test_example.o
# -Wl,--wrap=foo -Wl,--wrap=bar
#
SYMBOLS=$(nm $1 | grep -o "__wrap\w*")
echo $SYMBOLS | sed 's/__wrap_/-Wl,--wrap=/g'