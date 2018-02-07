#!/bin/bash

# Run clang-format against all the files (.h .c) that changed between local and diff
if [ "$#" -ne 1 ]; then
  echo "Usage: $0 DIFF"
  exit 1
fi

printed_header=false

files=(`git diff $1 --name-only --diff-filter=ACMRT | grep "\.[ch]$"`)

for file in "${files[@]}"
do
  clang-format -style=file -output-replacements-xml $file | grep "<replacement " >/dev/null

  if [ $? -ne 1 ]
  then
    if [ "$printed_header" = false ]; then
      printed_header=true;
      echo "The following file(s) failed clang-format checker. How to format: clang-format -i -style=file <FILE>"
    fi
    echo -e "\t-$file"
  fi
done

if [ "$printed_header" = true ]; then
  exit 1
fi

exit 0
