#!/usr/bin/env bash
set -e
source $(dirname $0)/format_env.sh

files=$(find ${project_dir} -type f -name "*.cpp" -o -name "*.cc" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" |
  ${format_dir}/filter.sh)
for x in ${files}; do
  echo "format file: $x"
  ${CLANG_FORMAT} --style=file -i $x
done
