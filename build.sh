#!/bin/sh
set -eu
export CC=${CC:-gcc}
export CFLAGS=${CFLAGS:--Wall -Wextra -Werror -std=c99 -Og -g}
b="build-$(uname | tr A-Z- a-z_)-$(uname -m | tr A-Z- a-z_)-$CC"
mkdir -p -- "$b"
cd "$(dirname "$0")/$b"
echo "Entering directory '$PWD'"
$CC $CFLAGS -o rmempty ../rmempty.c \
    -D "PROGVERSION=\"$(git describe --always --dirty)\""
