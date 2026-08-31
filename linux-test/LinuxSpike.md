# Linux (RVA22) on Spike — boot + `ls` benchmark

Boots a RISC-V 64 Linux image built for the **RVA22** profile on Spike, runs `ls`,
and powers off. Measures end-to-end wall clock and simulator throughput (MIPS).

## Repro

Prerequisites: `riscv64-linux-gnu-gcc` (Ubuntu 25.10 / GCC 15), `dtc`, `fakeroot`,
`cpio`, `bc`, `curl`, plus Spike's own build deps.

```sh
cd linux-test
./build-all.sh          # fetch + build everything + run once
./bench.sh 7            # repeat measurement, 7 runs, min/mean
```

Or step by step:

| step | what it does |
|---|---|
| `./00-fetch.sh` | Linux 6.19.14, BusyBox 1.38.0, musl 1.2.5, OpenSBI 1.7 |
| `./00-spike.sh` | Spike, `-O3 -march=native -flto -DNDEBUG -fno-stack-protector -fcf-protection=none` |
| `./01-busybox-initramfs.sh` | musl + static BusyBox for RVA22U64, initramfs cpio |
| `./02-linux.sh` | kernel (defconfig + `rva22.config`), initramfs linked in |
| `./03-opensbi.sh` | OpenSBI `FW_PAYLOAD` wrapping the kernel → `out/fw_payload.elf` |
| `./03b-dtb.sh` | Spike's DT + modern `riscv,isa-extensions` bindings |
| `./04-run.sh` | boots on Spike, prints instructions / sim time / MIPS |

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
  `poweroff -f` → SBI SRST → HTIF exit.

Run command:

```sh
spike --stats \
      --isa=rv64imafdc_zicsr_zifencei_zicntr_zihpm_zihintpause_zfhmin_zba_zbb_zbs_\
zicbom_zicbop_zicboz_zkt_svpbmt_svinval_svnapot \
      --priv=MSU -p1 -m0x80000000:0x40000000 \
      --dtb=out/spike-rva22.dtb out/fw_payload.elf
```

## Results

Host: Intel Core Ultra 7 265K (20 cores), Ubuntu 25.10, GCC 15.2.0.
Spike 1.1.1-dev @ `c09c0cce` + local changes. 7 consecutive runs.

| metric | value |
|---|---|
| wall clock, reset → `ls` → power off | **0.743 s** mean (0.731 s best) |
| retired instructions (whole run) | **116 152 768** (bit-identical every run) |
| simulation throughput | **156.3 MIPS** mean, 158.9 MIPS best |
| Spike startup + 25 MB ELF load | 0.011 s (≈1.5 % of process time) |

Instruction count comes from Spike's own `--stats`, which reports a monotonic
per-hart retired-instruction counter, so it covers OpenSBI + the whole kernel boot
+ `ls`. MIPS = that count / simulated wall clock.

Artifacts: `out/Image` (24 MB, initramfs linked in), `out/fw_payload.elf` (25 MB),
`out/spike-rva22.dtb`, boot log in `out/run.log`.

## Optimization log

Starting point of the exercise was a reported *86 MIPS*. Profiling (gperftools
`libprofiler` for the host side, `spike -g` PC histogram for the target side)
found that number to be wrong and the workload to be pathological.

### 1 — the instruction count was wrong (measurement bug)

The original harness read `rdinstret` from userspace at the end of `/init`.
Linux's SBI PMU driver (`drivers/perf/riscv_pmu_sbi.c`) programs and resets the
hardware counters during boot, so that read reports only the instructions since
the last reset — **206 M instead of the true 1 271 M**, a 6.2x undercount.

Fix: Spike now maintains `processor_t::insns_retired`, a monotonic counter the
target cannot write, bumped in `processor_t::step()` next to the existing
`minstret` bump (no hot-path cost — it is one add per `step()` batch, not per
instruction). New `spike --stats` prints retired instructions, wall time and MIPS
on exit. On the *original* image this reports **532.8 MIPS**, not 86.

### 2 — 79 % of the boot was ftrace event registration (workload bug)

`spike -g` PC histogram over the whole boot, resolved against `System.map`:

| share of retired instructions | symbol |
|---|---|
| 79.3 % | `trace_event_update_all` |
| 3.3 % | `strncmp_zbb` |
| 2.2 % | `strlen_zbb` |
| 1.3 % | `strchr` |
| 1.1 % | `strstr` |
| 0.9 % | `update_event_fields` |

riscv `defconfig` enables `CONFIG_FTRACE`, and registering the ~2090 built-in
trace events is quadratic string matching over event/field names. Roughly 89 % of
the boot (including the string helpers it calls) was that one initialisation step
— which also explains why `lbu` was 21 % of host time in the profile.

Fix: `# CONFIG_FTRACE is not set` (and `BPF_SYSCALL`, which pulls tracing back in)
in `rva22.config`. Guest work drops **1 271 M → 118 M instructions (10.8x)**.

### 3 — Ubuntu hardening flags on the interpreter hot path

Every instruction handler is a tiny function called indirectly from the dispatch
loop. Ubuntu's GCC defaults to `-fstack-protector-strong` and `-fcf-protection=full`,
so each one carried an `endbr64`, a `%fs:0x28` canary load, a stack frame, and a
canary compare against `__stack_chk_fail`:

```
fast_rv64i_lbu:
  endbr64
  sub    $0x38,%rsp
  mov    %fs:0x28,%rax          <- canary
  mov    %rax,0x28(%rsp)
  ...
  sub    %fs:0x28,%rax          <- check
  jne    __stack_chk_fail
```

Fix: `-fno-stack-protector -fcf-protection=none` in `00-spike.sh`. **+2.7 %**.

### Summary so far

| | instructions | sim time | MIPS |
|---|---|---|---|
| original harness, as reported | 206 M *(wrong)* | 2.38 s | 86.6 |
| original image, correctly counted | 1 271 M | 2.39 s | 532.8 |
| ftrace off + hardening off (current) | 116 M | 0.743 s | **156.3** |

The 533 MIPS figure is real but flattering: the ftrace loop is a tiny, perfectly
cache-resident hot spot. 160 MIPS on a full defconfig-class boot is the
representative number, and it is the baseline the remaining work is measured against.

### Notes

* The Spike ISA string contains **no** Zicond/V/Zimop, so the run completing without
  an illegal-instruction trap is itself the proof that nothing outside RVA22 executed.
* Kernel is `arch/riscv/configs/defconfig` plus `rva22.config` (Zba/Zbb/Zbs/Zicbom/
  Zicboz/Svpbmt/Svnapot/Svinval on, `RISCV_ISA_V` off — vector is not in RVA22,
  `HZ=100`, ftrace off, initramfs embedded).
