#!/usr/bin/env bash
grep -E '\.(h|cpp|hpp|c|cc)$'\
  |grep -v unwindstack\
  |grep -v build\
  |grep -v out


