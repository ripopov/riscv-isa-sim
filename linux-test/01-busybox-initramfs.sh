#!/bin/bash
# Build musl + a static BusyBox, both strictly for RVA22U64, and pack the initramfs.
#
# Why musl: Ubuntu's riscv64 glibc is compiled for an RVA23-class baseline
# (it contains V / Zicond / Zimop / Zcb code), so a glibc-linked binary would
# not be an RVA22 binary.  musl is rebuilt from source with the RVA22U64 -march.
source "$(dirname "$0")/env.sh"

BB="$SRC/busybox-1.38.0"
MUSL="$SRC/musl-1.2.5"
ROOT="$BLD/rootfs"
mkdir -p "$OUT" "$BLD/busybox" "$BLD/musl-build"

# ---- musl (RVA22U64) ----------------------------------------------------
cd "$BLD/musl-build"
CROSS_COMPILE=$CROSS "$MUSL/configure" \
    --prefix="$BLD/musl" --target=riscv64 --enable-wrapper=gcc \
    CFLAGS="-march=$RVA22U64_MARCH -mabi=$RVA22_ABI -O2"
make -j$JOBS
make install
MUSLGCC="$BLD/musl/bin/musl-gcc"

# Linux UAPI headers next to musl (busybox needs linux/*.h)
mkdir -p "$BLD/linux-hdr"
make -C "$SRC/linux-6.19.14" O="$BLD/linux-hdr" ARCH=riscv INSTALL_HDR_PATH="$BLD/musl" headers_install >/dev/null
export REALGCC=${CROSS}gcc

# ---- busybox ------------------------------------------------------------
make -C "$BB" O="$BLD/busybox" ARCH=riscv CROSS_COMPILE=$CROSS defconfig
CFG="$BLD/busybox/.config"
sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/'          "$CFG"
sed -i 's/^CONFIG_TC=y/# CONFIG_TC is not set/'                  "$CFG"
sed -i 's/^CONFIG_FEATURE_WTMP=y/# CONFIG_FEATURE_WTMP is not set/' "$CFG"
sed -i 's/^CONFIG_FEATURE_UTMP=y/# CONFIG_FEATURE_UTMP is not set/' "$CFG"
sed -i "s|^CONFIG_EXTRA_CFLAGS=.*|CONFIG_EXTRA_CFLAGS=\"-march=$RVA22U64_MARCH -mabi=$RVA22_ABI\"|" "$CFG"
yes "" | make -C "$BB" O="$BLD/busybox" ARCH=riscv CROSS_COMPILE=$CROSS oldconfig >/dev/null
make -C "$BB" O="$BLD/busybox" ARCH=riscv CROSS_COMPILE=$CROSS CC="$MUSLGCC" -j$JOBS

# ---- rootfs -------------------------------------------------------------
rm -rf "$ROOT"
mkdir -p "$ROOT"/{bin,sbin,proc,sys,dev,etc/init.d,usr/bin,usr/sbin}
cp "$BLD/busybox/busybox" "$ROOT/bin/busybox"
for a in sh ls mount echo poweroff; do ln -sf /bin/busybox "$ROOT/bin/$a"; done

# /init: no busybox-init, just do the work and power the machine off
cat > "$ROOT/init" <<'EOF'
#!/bin/sh
/bin/busybox mount -t devtmpfs dev /dev
exec 0</dev/console 1>/dev/console 2>&1
/bin/busybox mount -t proc  proc  /proc
/bin/busybox mount -t sysfs sysfs /sys
echo "=== busybox: ls -la / ==="
/bin/busybox ls -la /
echo "=== ls done ==="
# Instruction count is reported by Spike itself (--stats), not from the target:
# Linux's SBI PMU driver reprograms the instret CSR during boot, so a userspace
# rdinstret undercounts the run by several times.
/bin/busybox poweroff -f
EOF
chmod +x "$ROOT/init"

# ---- cpio ---------------------------------------------------------------
# fakeroot so /dev/console can be a real char node inside the archive
fakeroot -- sh -c "cd '$ROOT' && mknod -m 622 dev/console c 5 1 && mknod -m 666 dev/null c 1 3 && find . | cpio -o -H newc --owner root:root" > "$OUT/initramfs.cpio" 2>/dev/null
riscv64-linux-gnu-readelf -A "$ROOT/bin/busybox" | grep Tag_RISCV_arch
ls -l "$OUT/initramfs.cpio"
echo "BUSYBOX+INITRAMFS OK"
