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
| wall clock, reset → `ls` → power off | **0.383 s** mean (0.379 s best) |
| retired instructions (whole run) | **116 152 768** (bit-identical every run) |
| simulation throughput | **303.6 MIPS** mean, 306.5 MIPS best |
| peak RSS | 95 MiB |
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

### 4 — the opcode map was a linear scan over 128 buckets

The dispatch loop's `refill_icache()` path calls `processor_t::decode_insn()`,
which walked a bucket of the opcode map keyed on `insn.bits() % 128` — i.e. on
the major opcode alone. Everything in the OP, OP-IMM, LOAD, STORE and BRANCH
spaces (base + M + all of Zba/Zbb/Zbs) lands in one bucket each, tens of entries
long, searched linearly. **23 % of all host time** was in that scan.

Fix: key the map on the major opcode *and* funct3 (bits 6:0 gathered with bits
14:12, 1024 buckets), and flatten it from 1024 separate `std::vector`s into one
contiguous array plus a start-offset table, so a decode is a single indirection
into memory it will find hot. Because the index is a pure bit gather,
`index(a & b) == index(a) & index(b)`, which makes working out the buckets an
encoding belongs in a two-line computation instead of the old stride arithmetic.
The chain scan disappears from the profile. **+4.3 %** (156.3 → 163.0 MIPS).

### 5 — the instruction cache was flushed 33 400 times, the hard way

Counting flush-triggering instructions in the target (`spike -g` PC histogram,
mnemonics resolved through `objdump`) — the boot executes **22 462 `sfence.vma`
and 10 937 `fence.i`**, one flush every ~3 500 instructions. Each one walked all
4096 `icache_entry_t` slots writing `tag = -1`: a strided scatter over 128 KB, so
33 400 flushes dirtied 4.3 GB of cache lines. And with the cache reset that often
it never warms up — only ~100 slots were live at each flush, so the target was
re-decoding an instruction every ~35 executed.

Two fixes, both exact:

* **Flush only what was filled.** `mmu_t` now records the slot index on every
  refill; `flush_icache()` invalidates just those (falling back to a full walk in
  the rare case the list overflows). Typical flush touches ~100 slots, not 4096.
* **Reuse the decode.** A flush invalidates tags but leaves the decoded handler
  in the slot, and an encoding always maps to the same handler — so a refill that
  finds the slot still holding the encoding it just fetched skips the opcode-map
  search entirely. Only a rebuild of the opcode map (a `misa`/`xlen` write, or
  enabling commit logging) invalidates that, which is what the new
  `mmu_t::flush_icache_decodes()` handles.

**+8.0 %** (163.0 → 176.0 MIPS).

### 6 — target memory was a red-black tree with a node per 4 KiB page

`mem_t` allocated target RAM one `calloc(4096)` at a time and indexed it with a
`std::map<reg_t, char*>`. For a 1 GiB target that is a quarter of a million tree
nodes on the address-translation path — `sim_t::addr_to_mem()` plus
`mem_t::contents()` were **5.1 %** of host time even behind the
`addr_to_mem_cache` hash table — and it scattered target memory over a quarter of
a million small heap blocks, none host-page aligned, so every target page
straddled two host pages.

Fix: allocate lazily in 2 MiB chunks (still sparse — untouched chunks are never
allocated, and `mmap(MAP_NORESERVE)` chunks are only backed as the target touches
them) and index them with a flat `std::vector<char*>`, so a lookup is one load.
Chunks are page aligned and large enough for the host to back with huge pages.
`addr_to_mem` drops to 1.5 %. **+0.9 %** mean, +2.4 % best (176.0 → 177.6 MIPS).

### 7 — `sfence.vma` threw away the whole TLB even when given one address

Resolving the operands of every executed `sfence.vma` in the boot: **22 453 of
22 462 name a single virtual address** (`sfence.vma a5`, Linux's
`flush_tlb_page`); only 9 are the global form. Spike discarded all 768 TLB
entries and the whole PTE cache for each one, so the data TLB never stayed warm
— which is where the page-walk, PMP-check and TLB-refill time was going.

`SFENCE.VMA` with `rs1 != x0` only has to order translations for that one
address. `mmu_t::flush_tlb_vaddr()` invalidates the one direct-mapped slot in
each TLB (found from the raw address — the index bits are below anything
pointer masking can alter, and clearing without a tag compare only ever
over-invalidates), clears the PTE cache (keyed by physical address, so the leaf
PTE for this address cannot be identified — but it is only 4 KiB), and flushes
the instruction cache, which is virtually tagged and, since change 5, cheap.
`rs2` names an ASID, which this MMU does not track, so its translations are
invalidated whatever the ASID — always permitted.

This makes Spike *stricter*, not laxer: it now keeps stale entries exactly as
long as the architecture permits, so target code that under-invalidates fails
here as it would on hardware. **+6.1 %** (177.6 → 188.5 MIPS).

### 8 — the instruction cache covered 8 KiB of text

`icache_index()` is `(pc / 2) % ICACHE_ENTRIES`, so with `ICACHE_ENTRIES = 4096`
the cache is direct mapped on PC bits 12:1 — **its whole reach is an 8 KiB window
of addresses**. Any two instructions 8 KiB apart collide, and a kernel with
megabytes of hot text spends its time evicting itself: a decode was still 18 % of
host time, and the dispatch loop's tag check another 11 %, almost all of it
conflict misses.

That constant has presumably been 4096 since the days when 128 KiB was a lot of
memory. Raising it was not worth doing before change 5, because every flush
walked the entire cache; now that a flush only touches the slots that were
actually filled, the cache can be as large as it is worth being. Measured:

| entries | size | MIPS |
|---|---|---|
| 4 096 | 128 KiB | 188.5 |
| 16 384 | 512 KiB | 226.8 |
| 65 536 | 2 MiB | 259.0 |
| 131 072 | 4 MiB | 279.1 |
| **262 144** | **8 MiB** | **289.0** |
| 1 048 576 | 32 MiB | 286.8 |

256 K entries is the knee. It costs 8 MiB (plus a 1 MiB fill list) per hart —
peak RSS for this benchmark is 95 MiB — which is worth flagging for
many-hart simulations, but is a good trade at the default of one or a few harts.
**+53 %** (188.5 → 289.0 MIPS).

### 9 — `sfence.vma` still flushed the instruction cache for data pages

Counters in a throwaway build: with everything above, the boot does **6.60 M
instruction-cache refills** — one per 17.6 instructions — against only 24 625
flushes, i.e. essentially every refill exists because a flush threw the entry
away. 87 % of refills skip the decode (change 5), but they still re-fetch.

`flush_tlb_vaddr()` was flushing the whole instruction cache for every
`sfence.vma`, even though the cache holds nothing from most of the pages being
invalidated — they are data pages. `mmu_t` now keeps a bitmap of the pages it has
filled instruction-cache slots from, aliased onto 8192 bits: bits are only ever
set, never cleared for an individual page, so aliasing can cause a needless flush
but never a missed one, and the bitmap is reset whenever the cache is emptied.
Straddling instructions mark both pages they read from.

This drops the `sfence.vma`s that flush the instruction cache from 22 443 to
**26**, and refills from 6.60 M to 5.54 M. Only **+0.4 %** in the end, because
what remains is dominated by `fence.i`, which has no address operand and so must
flush everything (290.2 MIPS).

### 10 — the data TLB covered 1 MiB, and loading the payload was byte-at-a-time

Two smaller things, once the instruction side stopped dominating.

`TLB_ENTRIES` was 256, so like the instruction cache the data TLB was direct
mapped over a window far smaller than the target's working set — 1 MiB. Raising
it is only affordable now that `sfence.vma` no longer flushes the whole TLB
(2182 full flushes left, down from 22 462). Measured: 256 → 290.2 MIPS,
512 → 296.3, 1024 → 297.5, **2048 → 298.1**, 4096 → 292.0. 2048 it is; past that
the arrays stop fitting alongside everything else.

Starting the simulation was **4 %** of the run, all of it getting the 25 MB
`fw_payload.elf` into target memory:

* `sim_t::chunk_max_size()` was 8, so the payload went in as 3.2 M virtual calls,
  each storing one doubleword through the MMU. It now transfers a page at a time,
  copying directly into target memory where the range is backed by it and
  falling back to the MMU otherwise. The two conversions in the old path were
  inverse endianness swaps, so a chunk transfer is a verbatim byte copy either
  way.
* `memif_t::write()` decided whether a range was all zeroes with a byte-at-a-time
  loop over the whole buffer **and no early exit** — 25 MB scanned one byte at a
  time to discover that byte 0 is not zero. Now `std::all_of`, which
  short-circuits.

**+4.6 %** together (290.2 → 303.6 MIPS).

### Verification

Every change above is meant to be semantics-preserving, checked against a
pristine build of upstream `c09c0cce`:

* `--log-commits` output for the first **7 389 902 instructions** of the boot is
  byte-for-byte identical.
* The full boot console output is identical.
* The retired-instruction count is identical (116 152 768) on every run.
* `make check-riscv` (opcode overlap) passes.

### Summary so far

| | instructions | sim time | MIPS |
|---|---|---|---|
| original harness, as reported | 206 M *(wrong)* | 2.38 s | 86.6 |
| original image, correctly counted | 1 271 M | 2.39 s | 532.8 |
| ftrace off + hardening off | 116 M | 0.743 s | 156.3 |
| + wider, flattened opcode map | 116 M | 0.713 s | 163.0 |
| + cheap icache flush, decode reuse | 116 M | 0.660 s | 176.0 |
| + chunked, mmap-backed target memory | 116 M | 0.654 s | 177.6 |
| + address-selective `sfence.vma` | 116 M | 0.616 s | 188.5 |
| + 256 K-entry instruction cache | 116 M | 0.402 s | 289.0 |
| + icache page tracking | 116 M | 0.400 s | 290.2 |
| + 2048-entry TLB, bulk payload load (current) | 116 M | 0.383 s | **303.6** |

The 533 MIPS figure is real but flattering: the ftrace loop is a tiny, perfectly
cache-resident hot spot. 160 MIPS on a full defconfig-class boot is the
representative number, and it is the baseline the remaining work is measured against.

### Notes

* The Spike ISA string contains **no** Zicond/V/Zimop, so the run completing without
  an illegal-instruction trap is itself the proof that nothing outside RVA22 executed.
* Kernel is `arch/riscv/configs/defconfig` plus `rva22.config` (Zba/Zbb/Zbs/Zicbom/
  Zicboz/Svpbmt/Svnapot/Svinval on, `RISCV_ISA_V` off — vector is not in RVA22,
  `HZ=100`, ftrace off, initramfs embedded).
