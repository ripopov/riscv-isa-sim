#!/bin/bash
# Build Spike with full optimization (-O3, host-native codegen, LTO).
source "$(dirname "$0")/env.sh"
mkdir -p "$BLD/spike"; cd "$BLD/spike"
OPT="-O3 -march=native -mtune=native -fno-semantic-interposition -DNDEBUG -flto=$JOBS -ffat-lto-objects"
"$LT/../configure" --prefix="$BLD/spike-install" CFLAGS="$OPT" CXXFLAGS="$OPT"
make -j$JOBS
make install
"$BLD/spike-install/bin/spike" --help 2>&1 | head -1
echo "SPIKE OK"
