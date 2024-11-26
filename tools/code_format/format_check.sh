#!/usr/bin/env bash
set -e
source $(dirname $0)/format_env.sh

bash ${format_dir}/format_all.sh
files=$(git diff --name-only --diff-filter=ACMRT)
if [[ ! -z ${files} ]]; then
  echo
  echo "** ERROR:"
  echo "please format the following files before commit by using <tools/code_format/format_all.sh>:"
  for file in ${files}; do
    echo " $file"
  done
  echo "*******************************************************************************************"
  echo "detail:"
  git diff --color
  exit 1
fi
