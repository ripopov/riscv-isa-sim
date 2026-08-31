# Spike throughput on RVA22 — Linux boot and CoreMark

Two benchmarks, both built for the **RVA22** profile, that bracket Spike's
interpreter from opposite ends.

## Results

| benchmark | what it exercises | instructions | wall clock | throughput |
|---|---|---|---|---|
| **Linux/RVA22 boot + `ls`** | S-mode, Sv39 paging, megabytes of kernel text, constant TLB and I-cache invalidation | 116 152 768 | 0.394 s | **295 MIPS** |
| **CoreMark, bare metal** | M-mode, no paging, ~15 KB working set — almost pure fetch-dispatch-execute | 1 585 865 000 | 2.125 s | **746 MIPS** |

Host: Intel Core Ultra 7 265K (20 cores), Ubuntu 25.10, GCC 15.2.0. Spike
1.1.1-dev @ `c09c0cce` + the changes in this tree. Means of 7–12 consecutive
runs on an otherwise idle machine; run-to-run spread is under 2 % idle and
about 5 % on a busy desktop, so the two figures above were taken back to back.

Both come from Spike's own `--stats`: a monotonic per-hart retired-instruction
counter that the target cannot write, and wall clock measured from the top of
`main()`, so building the machine and loading the payload are included. MIPS is
one divided by the other.

## Why CoreMark is 2.5x faster

The interesting thing about the gap is that it is mostly *not* extra work. Host
nanoseconds per retired target instruction, split by where the profiler found
the time (gperftools, 9408 and 12704 samples):

| where the time goes | Linux boot | CoreMark |
|---|---|---|
| instruction handlers | 1.77 ns | 0.96 ns |
| dispatch loop (`processor_t::step`) | 1.00 ns | 0.37 ns |
| startup / teardown — build machine, load the 25 MB payload, release memory | 0.30 ns | 0.00 ns |
| MMU, page walks, device I/O | 0.22 ns | 0.00 ns |
| CSR access | 0.08 ns | 0.00 ns |
| other | 0.03 ns | 0.00 ns |
| **total** | **3.39 ns** | **1.34 ns** |

Only **0.62 ns of the 2.05 ns difference** is code CoreMark never executes — the
MMU, the CSRs, and the fixed cost of starting a big simulation. The other
**1.43 ns, 70 % of the gap, is the same dispatch loop and the same handlers
running 2.7x and 1.8x slower.** Four reasons, in rough order of size:

* **The instruction cache stops working.** The boot performs 5.54 M I-cache
  refills over 116 M instructions — one every 21 — because 22 462 `sfence.vma`
  and 10 937 `fence.i` keep throwing entries away and the kernel's hot text is
  megabytes wide. The CoreMark image contains **no `fence.i`, no `sfence.vma`
  and no `misa` write at all** — its only CSR write is the `mstatus.FS` in the
  startup code, which `base_status_csr_t::maybe_flush_tlb` correctly ignores —
  so `flush_icache()` never runs after reset. With 13 210 bytes of text the
  entire run can refill at most 6 605 times, one per 240 000 instructions. That
  is the dispatch-loop row: same code, but in the boot it is a fetch and a
  re-decode where in CoreMark it is a tag hit.
* **The working set does not fit in the host's caches.** The boot touches about
  70 MiB of target memory (95 MiB peak RSS against CoreMark's 25 MiB, 9 MiB of
  which is Spike's own instruction cache and its fill list). CoreMark's entire
  state is 2 KB of benchmark data and 13 KB of text, resident in the host's L1
  for the whole run — so a target load that is a host cache miss in the boot is
  a hit in CoreMark. That is the handler row.
* **Address translation.** The boot runs S-mode under Sv39: every fetch, load
  and store probes a TLB, and a miss costs a page walk plus a PMP check.
  CoreMark runs M-mode with paging off, where translation is the identity — the
  MMU row is zero, and the TLB probe inlined into every handler always hits.
* **Traps.** Timer interrupts, SBI `ecall`s and page faults each break out of
  the fast dispatch batch and take the slow path through `step()`. CoreMark
  takes none for its entire run.

The corollary is in *None of the optimization work shows up on CoreMark* below:
everything in the optimization log attacks the first three items, so it doubles
the boot and does nothing at all for CoreMark. 1.34 ns per target instruction —
about 7 host cycles at this machine's clock — is roughly what this interpreter
costs when nothing is in its way, and closing that further means changing how
dispatch works, not what it does.

## Repro

Prerequisites: `riscv64-linux-gnu-gcc` (Ubuntu 25.10 / GCC 15) for the Linux side,
`riscv64-unknown-elf-gcc` + `picolibc-riscv64-unknown-elf` for the bare-metal side,
plus `dtc`, `fakeroot`, `cpio`, `bc`, `curl` and Spike's own build deps.

```sh
cd linux-test
./build-all.sh          # fetch + build everything + run both benchmarks
./bench.sh 7            # Linux boot, 7 runs, min/mean
./06-coremark-bench.sh 7   # CoreMark, 7 runs, min/mean
```

Or step by step:

| step | what it does |
|---|---|
| `./00-fetch.sh` | Linux 6.19.14, BusyBox 1.38.0, musl 1.2.5, OpenSBI 1.7, CoreMark |
| `./00-spike.sh` | Spike, `-O3 -march=native -flto -DNDEBUG -fno-stack-protector -fcf-protection=none` |
| `./01-busybox-initramfs.sh` | musl + static BusyBox for RVA22U64, initramfs cpio |
| `./02-linux.sh` | kernel (defconfig + `rva22.config`), initramfs linked in |
| `./03-opensbi.sh` | OpenSBI `FW_PAYLOAD` wrapping the kernel → `out/fw_payload.elf` |
| `./03b-dtb.sh` | Spike's DT + modern `riscv,isa-extensions` bindings |
| `./04-run.sh` | boots on Spike, prints instructions / sim time / MIPS |
| `./05-coremark.sh` | bare-metal CoreMark image → `out/coremark.elf` |
| `./06-coremark-bench.sh` | runs CoreMark on Spike, prints instructions / sim time / MIPS |

## Linux boot

### Configuration

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

### Results

12 consecutive runs.

| metric | value |
|---|---|
| wall clock, process start → `ls` → power off | **0.394 s** mean (0.388 s best) |
| retired instructions (whole run) | **116 152 768** (bit-identical every run) |
| simulation throughput | **295 MIPS** mean, 299 MIPS best |
| peak RSS | 95 MiB |
| of which: build the machine, load the 25 MB ELF, release memory at exit | ≈9 % of the run, by profile attribution |

The instruction count covers OpenSBI + the whole kernel boot + `ls`.

Artifacts: `out/Image` (24 MB, initramfs linked in), `out/fw_payload.elf` (25 MB),
`out/spike-rva22.dtb`, boot log in `out/run.log`.

## CoreMark

### The port

CoreMark upstream has no Spike target, so `coremark-port/` supplies one. It is
small because Spike's front end already provides everything a bare-metal image
needs: the target leaves a request in the `tohost` word, the front end takes it,
zeroes it and answers in `fromhost`. Device 1 command 1 writes a character —
that is `ee_printf`'s sink — and device 0 with an odd payload stops the
simulation with an exit status.

| file | what it is |
|---|---|
| `spike.ld` | flat image at `0x80000000`, `.tohost` in ordinary memory |
| `crt.S` | park non-zero harts, set `gp`/`sp`, enable the FPU, zero `.bss`, call `main` |
| `spike_port.c` | the HTIF mailbox: `spike_putchar`, `spike_exit` |
| `core_portme.c/.h` | CoreMark's port hooks: seeds, `rdcycle` timing, init |

Two things differ from CoreMark's stock `barebones` port, both forced by rv64:
`ee_ptr_int` must be pointer-sized (`barebones` uses `ee_u32`, which would
truncate every pointer the matrix benchmark aligns), and the image is built
`-mcmodel=medany` because `medlow`'s `lui`/`addi` pair cannot reach
`0x80000000`. Upstream's `ee_printf.c` ships an `#error` where the character
sink belongs, which `05-coremark.sh` substitutes at build time rather than
vendoring a copy of the other 700 lines.

Guest build: `-O2 -march=<RVA22U64> -mabi=lp64d`, 5000 iterations of the 2K
performance run, static memory. CoreMark is upstream `eembc/coremark` at
`1f483d5b` (2025-05-01); it has no tagged releases.

```sh
spike --stats --isa=<RVA22 string> out/coremark.elf
```

### Results

Seven consecutive runs.

| metric | value |
|---|---|
| wall clock, process start → HTIF exit | **2.125 s** mean (2.119 s best) |
| retired instructions | **1 585 865 000** (5000 iterations, 317 173 each) |
| simulation throughput | **746 MIPS** mean, 748 MIPS best |
| peak RSS | 25 MiB (9 MiB of it Spike's instruction cache and fill list) |

CoreMark's own report is at the bottom of `out/coremark.log`. The three data
CRCs — `crclist 0xe714`, `crcmatrix 0x1fd7`, `crcstate 0x8e3a` — are the
canonical values for the 2K performance run, and `06-coremark-bench.sh` checks
all three on every run; that is what validates the result here. CoreMark's own
`Errors detected` line is expected: the only check it fails is the rule that a
valid *submission* must run for at least ten seconds of target time, which at
1 GHz nominal would be 10 G instructions and about 13 s of host time per run.

The score CoreMark prints (3159 iterations/s, i.e. 3.16 CoreMark/MHz) is a
statement about the guest compiler, not about Spike: `rdcycle` in Spike advances
once per retired instruction, so it is the score an IPC=1 machine would get. The
number that means something here is the 746 MIPS.

### None of the optimization work shows up on CoreMark

Interleaved A/B against a pristine build of upstream `c09c0cce`, 15 runs each,
same binary image, median of each set:

| build | time | MIPS |
|---|---|---|
| upstream `c09c0cce` | 2.104 s | 753.7 |
| this tree | 2.108 s | 752.4 |

That is a wash, and it is the expected result. Every change in the log below is
in the MMU, the instruction cache or the opcode map, and CoreMark exercises none
of them: no paging, no `sfence.vma`, no `fence.i`, and a hot loop that fits in a
few hundred instruction-cache slots and stays there for the whole run. Spike's
8 MiB instruction cache is pure overhead for it, and does not measurably cost
anything either.

The profile says the same thing (gperftools, six runs merged, 12 704 samples):

| flat share | symbol |
|---|---|
| 27.4 % | `processor_t::step()` — fetch, tag check, indirect call |
| 9.4 % | `fast_rv64i_lh` |
| 7.3 % | `fast_rv64i_c_ld` |
| 5.1 % | `fast_rv64i_lbu` |
| 3.0 % | `fast_rv64i_c_lw` |
| … | ~40 more handlers, none above 2.9 % |

Not one MMU, page-walk, opcode-map or icache-refill symbol appears anywhere in
the profile. What is left is the dispatch loop and the handlers themselves —
the same ceiling described under *What is left*, with nothing in front of it.

## Optimization log

Every figure in this section is the **Linux boot** benchmark; see above for what
the same changes do to CoreMark (nothing).

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

### 11 — diminishing returns

The profile is flat now. `processor_t::step()` is 29 % of host time, of which the
dispatch loop — icache tag check, indirect call, next-entry chase — is 22 % and
refill/decode only 6 %. Load and store handlers are another ~20 %, and MMU slow
paths ~15 %. At 116 M instructions in 0.38 s that is about 16 host cycles per
target instruction.

Things tried in this round:

* **Fast load path kept in registers.** `mmu_t::load()` declared its result
  buffer outside the fast/slow split, and the slow path takes its address, so
  every load handler stored the value it had just fetched into a stack slot and
  read it straight back. Scoping the buffer to the slow path removes the round
  trip and the stack frame. Strictly better code; no measurable time change, the
  store-to-load forward was hidden behind the target load's own latency.
* **TLB entries padded to 32 bytes** (`alignas(32)` on `dtlb_entry_t`), so
  indexing is a shift instead of a multiply by 24 and entries stop straddling
  host cache lines. Back-to-back A/B, 15 runs each: 299.3 → 302.6 MIPS,
  **+1.1 %**.
* **Profile-guided optimization** of the Spike build (`-fprofile-generate`, boot
  once, `-fprofile-use`): 303.2 vs 301.9 MIPS — within noise, and not worth a
  two-pass build. What is left is cache misses and indirect-branch mispredicts,
  which PGO does not help.
* **ISA string fidelity**: RVA22U64 also mandates Zicclsm, Ziccif, Ziccrse,
  Ziccamoa and Za64rs, which Spike models and the benchmark was not asking for.
  Added — Zicclsm decides whether misaligned accesses are handled by the hardware
  or trap out to firmware. No behavioural change here (identical instruction
  count and commit log), so this target never issues a misaligned access, but the
  configuration is now actually RVA22.

Also tried: `-flto-partition=one` instead of GCC's default 20 LTO partitions, in
case the dispatch loop and the handlers were landing in different partitions —
299.3 vs 302.7 MIPS, within noise.

### 12 — the throughput metric was hiding startup cost

Re-tuning `ICACHE_ENTRIES` looked like it wanted to go higher: 256 K → 1 M
measured 302.7 → 313.4 MIPS. It was an artifact. `--stats` timed `sim_t::run()`,
but the cache is allocated and initialised in `sim_t`'s *constructor*, before
that — so making the cache bigger moved work out of the measured window. Total
process time went the other way, 0.398 s → 0.412 s.

`--stats` now times from the top of `main()`, so building the machine is counted.
Re-measured honestly, and 256 K is the real optimum:

| entries | size | MIPS (end to end) |
|---|---|---|
| 64 K | 2 MiB | 275.7 |
| 128 K | 4 MiB | 286.7 |
| **256 K** | **8 MiB** | **298.4** |
| 512 K | 16 MiB | 295.1 |
| 1 M | 32 MiB | 283.0 |

The headline number drops from ~302 to ~295 MIPS because it now includes the
~10 ms of building the machine. Every figure in this document is on the honest
metric.

### What is left

The profile is flat and the remaining items are each a percent or two:

* **The dispatch loop, 22 %.** An indirect call per instruction, whose target
  the branch predictor cannot learn. Reducing it means threaded dispatch — every
  handler tail-calling the next so each gets its own branch site — which is a
  rewrite of the handler ABI, not an incremental change.
* **`fence.i`, ~5 %.** 10 937 of them, from the kernel patching its own text, and
  with no address operand each must invalidate the whole instruction cache; they
  now account for 83 % of the 5.54 M refills. Avoiding them needs tracking writes
  to the *physical* pages that hold cached instructions, and every path that can
  write target memory — MMU fast path, MMU slow path, HTIF chunk writes, device
  DMA — would have to participate. That is a correctness surface not worth
  expanding for a few percent in a golden reference model.
* **Everything else** is target memory access and page walks: inherent work.

CoreMark corroborates the first item from the other direction: with paging, the
instruction cache and the opcode map all out of the picture, `processor_t::step()`
is still 28 % of host time and the rest is the handlers. Dispatch is the floor
for both workloads, and it is the same floor upstream Spike has.

### Verification

Every change above is meant to be semantics-preserving, checked against a
pristine build of upstream `c09c0cce`:

* `--log-commits` output for the first **7 389 902 instructions** of the boot is
  byte-for-byte identical.
* The full boot console output is identical.
* The retired-instruction count is identical (116 152 768) on every run.
* `make check-riscv` (opcode overlap) passes.
* On CoreMark, `--log-commits` for the first **4 000 000 instructions** is
  byte-for-byte identical between the two builds, as is the full CoreMark report
  — including its three data CRCs, which match the canonical 2K performance-run
  values.

### Summary

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
| + 2048-entry TLB, bulk payload load | 116 M | 0.383 s | 303.6 |
| + register-resident load path, padded TLB entries | 116 M | 0.384 s | 302 |
| honest timing (startup included) — **current** | 116 M | 0.394 s | **295** |

The 533 MIPS figure is real but flattering: the ftrace loop is a tiny, perfectly
cache-resident hot spot. The ftrace-free boot is the representative workload, and
156 MIPS is the baseline the rest of the work is measured against — **295 MIPS is
1.9x that**, on a target-instruction stream that is identical to upstream Spike's,
instruction for instruction.

Across both benchmarks:

| workload | upstream `c09c0cce` | this tree |
|---|---|---|
| Linux/RVA22 boot + `ls` | 156 MIPS | **295 MIPS** (1.9x) |
| CoreMark, bare metal | 754 MIPS | **752 MIPS** (unchanged) |

The gap between the two columns is the whole point: what was slow was never the
interpreter core, it was everything the boot does that CoreMark does not.

### Notes

* The Spike ISA string contains **no** Zicond/V/Zimop, so the run completing without
  an illegal-instruction trap is itself the proof that nothing outside RVA22 executed.
* Kernel is `arch/riscv/configs/defconfig` plus `rva22.config` (Zba/Zbb/Zbs/Zicbom/
  Zicboz/Svpbmt/Svnapot/Svinval on, `RISCV_ISA_V` off — vector is not in RVA22,
  `HZ=100`, ftrace off, initramfs embedded).
