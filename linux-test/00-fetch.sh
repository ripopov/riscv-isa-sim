#!/bin/bash
# Download and unpack the upstream sources.
source "$(dirname "$0")/env.sh"
mkdir -p "$SRC"; cd "$SRC"
[ -d linux-6.19.14 ] || { curl -LO https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.19.14.tar.xz && tar xf linux-6.19.14.tar.xz; }
[ -d busybox-1.38.0 ] || { curl -LO https://busybox.net/downloads/busybox-1.38.0.tar.bz2 && tar xf busybox-1.38.0.tar.bz2; }
[ -d musl-1.2.5 ]     || { curl -LO https://musl.libc.org/releases/musl-1.2.5.tar.gz && tar xf musl-1.2.5.tar.gz; }
[ -d opensbi-1.7 ]    || { curl -LO https://github.com/riscv-software-src/opensbi/archive/refs/tags/v1.7.tar.gz && tar xf v1.7.tar.gz; }
echo "FETCH OK"
