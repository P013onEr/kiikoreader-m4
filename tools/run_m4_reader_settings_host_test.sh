#!/usr/bin/env bash
set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I"$repo_root/tools/m4_reader_settings_stubs" \
  -I"$repo_root/main" \
  "$repo_root/tools/m4_reader_settings_host_test.c" \
  "$repo_root/main/m4_reader_settings.c" \
  -o "$tmp_dir/m4_reader_settings_host_test"
"$tmp_dir/m4_reader_settings_host_test"
