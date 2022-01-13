#!/usr/bin/env bash

# clang-format-13 wraps enum brace even though its option is set to false.
# Use clang-format-14 shipped with npm `clang-format` package
if [[ "$OSTYPE" == "darwin"* ]];
then
  formatter=/usr/local/lib/node_modules/clang-format/index.js
else
  formatter=clang-format
fi

find cmd src \( -name "*.h" -o -name "*.cpp" \) -exec $formatter -i --style=file {} \;
exit $?
