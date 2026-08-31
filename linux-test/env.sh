# Shared environment for the Spike/Linux RVA22 experiment
set -e
LT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="$LT/src"
BLD="$LT/build"
OUT="$LT/out"
CROSS=riscv64-linux-gnu-
JOBS=$(nproc)

# RVA22U64 mandatory set, spelled out (Ubuntu GCC 15 has no -march=rva22u64 alias)
RVA22U64_MARCH="rv64imafdc_zicsr_zifencei_zicntr_zihpm_zihintpause_zfhmin_zba_zbb_zbs_zicbom_zicbop_zicboz_zkt"
RVA22_ABI="lp64d"
# RVA22S64 = RVA22U64 + Svpbmt + Svinval (+ Sv39 paging, S-mode)
SPIKE_ISA="${RVA22U64_MARCH}_svpbmt_svinval_svnapot"
