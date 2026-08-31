# Linux (RVA22) on Spike — boot + `ls` benchmark

Boots a RISC-V 64 Linux image built for the **RVA22** profile on Spike, runs `ls`,
and powers off. Measures end-to-end wall clock and simulator throughput (MIPS).

## Repro

Prerequisites: `riscv64-linux-gnu-gcc` (Ubuntu 25.10 / GCC 15), `dtc`, `fakeroot`,
`cpio`, `bc`, `curl`, plus Spike's own build deps.

```sh
cd linux-test
./build-all.sh          # fetch + build everything + run once
```

Or step by step:

| step | what it does |
|---|---|
| `./00-fetch.sh` | Linux 6.19.14, BusyBox 1.38.0, musl 1.2.5, OpenSBI 1.7 |
| `./00-spike.sh` | Spike, `-O3 -march=native -mtune=native -flto -DNDEBUG` |
| `./01-busybox-initramfs.sh` | musl + static BusyBox for RVA22U64, initramfs cpio |
| `./02-linux.sh` | kernel (defconfig + `rva22.config`), initramfs linked in |
| `./03-opensbi.sh` | OpenSBI `FW_PAYLOAD` wrapping the kernel → `out/fw_payload.elf` |
| `./03b-dtb.sh` | Spike's DT + modern `riscv,isa-extensions` bindings |
| `./04-run.sh` | boots on Spike, prints wall clock / instructions / MIPS |

Repeat measurement only: `./04-run.sh` (rerun as often as you like).

## Configuration

* **RVA22U64 (user)** — `rv64imafdc_zicsr_zifencei_zicntr_zihpm_zihintpause_zfhmin_zba_zbb_zbs_zicbom_zicbop_zicboz_zkt`
* **RVA22S64 (Spike)** — the above `+ svpbmt svinval svnapot`, `--priv=MSU`, Sv39, 1 GiB RAM, 1 hart
* Userspace is built against a **musl rebuilt for RVA22U64**: Ubuntu's stock riscv64
  glibc/libgcc are compiled for an RVA23-class baseline (they contain `V`, `zicond`,
  `zimop`, `zcb` code), so they are not RVA22 binaries.
* Spike's auto-generated DT only exports the deprecated `riscv,isa` string, from which
  Linux parses single-letter extensions only. `03b-dtb.sh` adds `riscv,isa-base`,
  `riscv,isa-extensions` and the `cbom/cbop/cboz-block-size` properties so the kernel
  actually detects and patches in Zba/Zbb/Zbs/Zicbo*/Svpbmt/Svinval.
* Boot path: Spike → OpenSBI (M-mode) → Linux (S-mode) → `/init` → `ls -la /` →
  `rdinstret` → `poweroff -f` → SBI SRST → HTIF exit.

Run command:

```sh
spike --isa=rv64imafdc_zicsr_zifencei_zicntr_zihpm_zihintpause_zfhmin_zba_zbb_zbs_\
zicbom_zicbop_zicboz_zkt_svpbmt_svinval_svnapot \
      --priv=MSU -p1 -m0x80000000:0x40000000 \
      --dtb=out/spike-rva22.dtb out/fw_payload.elf
```

## Results

Host: Intel Core Ultra 7 265K (20 cores), Ubuntu 25.10, GCC 15.2.0.
Spike 1.1.1-dev @ `c09c0cce`. 5 consecutive runs.

| metric | value |
|---|---|
| wall clock, reset → `ls` → power off | **2.380 s** mean (2.348 – 2.400 s) |
| retired instructions (whole run) | **206 178 691** (bit-identical every run) |
| simulation throughput | **86.6 MIPS** mean, 87.8 MIPS best |
| Spike startup + 29 MB ELF load | 0.02 s (≈0.8 % of wall clock) |
| kernel-reported time at `Run /init` | 1.211 s simulated |

Instruction count is read in userspace with `rdinstret` right after `ls` completes
(`/init` sets `kernel.perf_user_access=2` so `scounteren` allows it), so it covers
OpenSBI + the entire kernel boot + `ls`. MIPS = that count / wall clock.

Artifacts: `out/Image` (27 MB, initramfs linked in), `out/fw_payload.elf` (29 MB),
`out/spike-rva22.dtb`, boot logs in `out/run*.log`.

### Notes

* The Spike ISA string contains **no** Zicond/V/Zimop, so the run completing without
  an illegal-instruction trap is itself the proof that nothing outside RVA22 executed.
* Kernel is `arch/riscv/configs/defconfig` plus `rva22.config` (Zba/Zbb/Zbs/Zicbom/
  Zicboz/Svpbmt/Svnapot/Svinval on, `RISCV_ISA_V` off — vector is not in RVA22,
  `HZ=100`, initramfs embedded).
