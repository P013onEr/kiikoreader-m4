#!/bin/sh
set -eu
out="${TMPDIR:-/tmp}/m4-project-ui-adapter-test"
cc -std=c11 -Wall -Wextra -Werror -I main \
  -I main/m4_epdiy_compat main/m4_project_ui_adapter.c main/m4_project_draw.c \
  main/m4_gray2.c main/m4_app_backend.c \
  tools/m4_project_ui_adapter_host_test.c -lm \
  -o "$out"
"$out"
rm -f "$out"
echo "m4 project UI adapter host test: PASS"
