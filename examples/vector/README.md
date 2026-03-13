# RISC-V Vector Extension (RVV 1.0) Demo

Bare-metal program demonstrating the RISC-V Vector extension on Spike.
Covers **111 tests** across **19 instruction categories**.

## Quick Start

```bash
./run_vector.sh
```

Requires `riscv64-unknown-elf-g++` and a built Spike (`../../build/spike`).

## What's Covered

| # | Category | Instructions Demonstrated |
|---|----------|--------------------------|
| 1 | Configuration | `vsetvli`, `vsetivli` (element widths e8/e16/e32/e64, LMUL m1/m2/m4) |
| 2 | Loads & Stores | `vle8/16/32`, `vse8/16/32`, `vlse32`, `vsse32`, `vluxei32`, `vlm`, `vsm`, `vl1re32`, `vs1r`, `vle32ff` |
| 3 | Integer Arithmetic | `vadd`, `vsub`, `vrsub`, `vmul`, `vdiv`, `vrem`, `vmacc`, `vmadd`, `vnmsac`, `vmulh`, `vwadd`, `vsadd`, `vsaddu` (.vv/.vx/.vi) |
| 4 | Bitwise & Logical | `vand`, `vor`, `vxor` (.vv/.vi), bitwise NOT via `vxor.vi -1` |
| 5 | Shifts | `vsll`, `vsrl`, `vsra` (.vi/.vv), `vnsrl` (narrowing) |
| 6 | Comparisons | `vmseq`, `vmsne`, `vmslt`, `vmsle`, `vmsgt` (.vv/.vi) |
| 7 | Min/Max | `vmin`, `vmax`, `vminu` (.vx) |
| 8 | Merge & Move | `vmerge.vvm`, `vmv.v.x`, `vmv.v.i`, `vmv.x.s`, `vmv.s.x` |
| 9 | Extension | `vsext.vf2`, `vzext.vf2` |
| 10 | Reductions | `vredsum`, `vredmax`, `vredmin`, `vredor`, `vredand`, `vredxor`, `vwredsum` (.vs) |
| 11 | Mask Ops | `vmand`, `vmor`, `vmxor`, `vmnand` (.mm), `vcpop.m`, `vfirst.m`, `vid.v`, `viota.m` |
| 12 | Permutation | `vrgather` (.vv/.vi), `vslideup`, `vslidedown`, `vslide1up`, `vslide1down`, `vcompress` |
| 13 | Carry/Borrow | `vadc.vvm`, `vmadc.vv`, `vsbc.vvm` |
| 14 | Fixed-Point | `vaadd`, `vasub`, `vssub` (.vv) |
| 15 | FP Arithmetic | `vfadd`, `vfsub`, `vfmul`, `vfdiv`, `vfsqrt`, `vfmacc`, `vfnmacc`, `vfmsac` (.vv/.vf), FP32 and FP64 |
| 16 | FP Compare/Misc | `vmfeq`, `vmflt`, `vfmin`, `vfmax`, `vfsgnj`, `vfsgnjn`, `vfsgnjx`, `vfclass` |
| 17 | FP Convert | `vfcvt.rtz.x.f`, `vfcvt.f.x`, `vfwcvt.f.f`, `vfncvt.f.f` |
| 18 | FP Reductions | `vfredusum`, `vfredosum`, `vfredmin`, `vfredmax` (.vs) |
| 19 | Register Move | `vmv1r.v`, `vmv2r.v` |

## Build Details

- **ISA**: `rv64gcv_zvl256b` (RV64 + General + Compressed + Vector, VLEN >= 256)
- **ABI**: `lp64d`
- **No standard library** — uses HTIF syscalls for output
- **Entry point**: `_start` (naked asm) sets up `mstatus.FS/VS`, stack, then calls `_main`
