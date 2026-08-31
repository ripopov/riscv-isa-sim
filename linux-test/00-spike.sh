#!/bin/bash
# Build Spike with full optimization (-O3, host-native codegen, LTO).
source "$(dirname "$0")/env.sh"
mkdir -p "$BLD/spike"; cd "$BLD/spike"
# -fno-stack-protector / -fcf-protection=none undo Ubuntu's default hardening,
# which otherwise puts a canary load/compare and an endbr64 in every one of the
# thousands of tiny instruction-handler functions on the interpreter hot path.
OPT="-O3 -march=native -mtune=native -fno-semantic-interposition -DNDEBUG"
OPT="$OPT -fno-stack-protector -fcf-protection=none"
OPT="$OPT -flto=$JOBS -ffat-lto-objects"
"$LT/../configure" --prefix="$BLD/spike-install" CFLAGS="$OPT" CXXFLAGS="$OPT"
make -j$JOBS
make install
"$BLD/spike-install/bin/spike" --help 2>&1 | head -1
echo "SPIKE OK"
