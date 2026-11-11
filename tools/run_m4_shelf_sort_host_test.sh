#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
c++ -std=c++17 -Wall -Wextra -Werror \
  -I"$repo_root/main" \
  "$repo_root/tools/m4_shelf_sort_host_test.cpp" \
  -o "$tmp_dir/m4_shelf_sort_host_test"
"$tmp_dir/m4_shelf_sort_host_test"
