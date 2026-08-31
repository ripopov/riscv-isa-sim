#!/bin/bash
# Build the RVA22 Linux kernel with the BusyBox initramfs linked in.
source "$(dirname "$0")/env.sh"
K="$SRC/linux-6.19.14"
KB="$BLD/linux"
mkdir -p "$KB" "$OUT"
make -C "$K" ARCH=riscv mrproper >/dev/null 2>&1 || true

sed "s|@INITRAMFS@|$OUT/initramfs.cpio|" "$LT/rva22.config" > "$BLD/rva22.config"

make -C "$K" O="$KB" ARCH=riscv CROSS_COMPILE=$CROSS defconfig
"$K/scripts/kconfig/merge_config.sh" -m -O "$KB" "$KB/.config" "$BLD/rva22.config"
make -C "$K" O="$KB" ARCH=riscv CROSS_COMPILE=$CROSS olddefconfig
make -C "$K" O="$KB" ARCH=riscv CROSS_COMPILE=$CROSS -j$JOBS Image

cp "$KB/arch/riscv/boot/Image" "$OUT/Image"
grep -m1 "riscv-march\|CONFIG_RISCV_ISA_ZBB" "$KB/.config" || true
ls -l "$OUT/Image"
echo "LINUX OK"
