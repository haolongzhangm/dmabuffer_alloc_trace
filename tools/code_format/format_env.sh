#!/usr/bin/env bash
project_dir="$(dirname $0)/../.."
format_dir="$(dirname $0)"

if [[ -z ${CLANG_FORMAT} ]]; then
  CLANG_FORMAT=${format_dir}/clang-format
fi