#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$root/tools/m4_sd_adapter_stubs" \
  -I"$root/main" -I"$root/main/m4_epdiy_compat" \
  "$root/main/m4_book_store_adapter.c" "$root/tools/m4_sd_adapter_host_test.c" \
  -o "$tmp/m4_sd_adapter_host_test"
"$tmp/m4_sd_adapter_host_test"
echo "m4 SD adapter host test: PASS"
