#!/bin/zsh
set -euo pipefail
cd -- "${0:A:h}"
mkdir -p build/tests
for test in cadence pairs volume meters; do
 xcrun clang++ -std=c++17 "tests/$test.cpp" -o "build/tests/$test"
 "build/tests/$test"
done
for pair in A B; do
 xcrun clang -std=c11 -O2 -Wno-deprecated-declarations "tests/hal-$pair.c" -framework CoreAudio -framework CoreFoundation -lm -o "build/tests/hal-$pair"
 "build/tests/hal-$pair"
done
