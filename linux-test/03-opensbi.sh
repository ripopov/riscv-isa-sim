#!/bin/bash
# Wrap the kernel Image in OpenSBI (FW_PAYLOAD) -> single ELF that Spike boots at 0x80000000.
source "$(dirname "$0")/env.sh"
OS="$SRC/opensbi-1.7"
mkdir -p "$BLD/opensbi" "$OUT"

make -C "$OS" O="$BLD/opensbi" PLATFORM=generic CROSS_COMPILE=$CROSS \
     PLATFORM_RISCV_ISA=rv64imafdc_zicsr_zifencei_zicntr_zihpm_zihintpause_zba_zbb_zbs \
     FW_PAYLOAD_PATH="$OUT/Image" -j$JOBS

cp "$BLD/opensbi/platform/generic/firmware/fw_payload.elf" "$OUT/fw_payload.elf"
ls -l "$OUT/fw_payload.elf"
echo "OPENSBI OK"
