#! /bin/bash

#
# Sed trickiness to replace blanks with backslash escaped newlines,
# as needed to make one dependency per source line for make.
#

sed 's/ / \\\
&/g' | grep -v '^  *\\$'
