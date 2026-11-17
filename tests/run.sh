#!/bin/sh
set -eu
cd "$(dirname "$0")"
bin=$(mktemp)
trap 'rm -f "$bin"' EXIT
${CXX:-c++} ${CXXFLAGS:-} -std=c++11 -Wall -Wextra -Werror -pedantic -I mock -I ../src ../src/flprogArrayExchange.cpp test_exchange.cpp -o "$bin"
"$bin"
