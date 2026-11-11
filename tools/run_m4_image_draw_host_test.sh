#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc -std=c11 -Wall -Wextra -Werror -I"$root/main" \
  "$root/main/m4_image_draw.c" "$root/tools/m4_image_draw_host_test.c" \
  -o "$tmp/m4_image_draw_host_test"
"$tmp/m4_image_draw_host_test"
