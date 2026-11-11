#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" -I"$root/main/m4_epdiy_compat" \
  "$root/tools/m4_refresh_policy_host_test.c" -o "$tmp/m4_refresh_policy_host_test"
"$tmp/m4_refresh_policy_host_test"
echo "m4 refresh policy host test: PASS"
