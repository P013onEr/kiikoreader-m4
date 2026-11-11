#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" \
  "$root/main/m4_gray2.c" "$root/tools/m4_gray2_host_test.c" \
  -o "$tmp/m4_gray2_host_test"
"$tmp/m4_gray2_host_test"
