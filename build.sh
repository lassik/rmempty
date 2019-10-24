#!/bin/sh
set -eu
export CC="${CC:-gcc}"
export CFLAGS="${CFLAGS:--Wall -Wextra -Werror -std=gnu99 -Og -g}"
b="build-$(uname | tr A-Z- a-z_)-$(uname -m | tr A-Z- a-z_)-$CC"
cd "$(dirname "$0")"
mkdir -p -- "$b"
cd "$b"
echo "Entering directory '$PWD'"
set -x
$CC $CFLAGS -o rmempty ../rmempty.c \
    -D "PROGVERSION=\"$(git describe --always --dirty)\""
