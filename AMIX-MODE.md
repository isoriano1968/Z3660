# AMIX mode: Z3 RAM and RTG below 0x40000000

AMIX's kernel uses 0x40000000-0x7FFFFFFF as virtual space. Under AmigaOS the Z3660's
Zorro III boards sit right there (Z3 RAM 0x40000000, RTG 0x50000000). In AMIX mode
a loader (pvmk) that has stopped AmigaOS configures both boards again, lower down,
and tells core 0 where it put them. The ARM cannot read the autoconfig bases back
from the FPGA, so core 0 keeps its own copy (`z3ram_base060`, `z3ram_size`,
`rtg_base060` in `src/z3_map.c`).

Target layout (060 addresses):

| range                   | what                                           |
|-------------------------|------------------------------------------------|
| 0x08000000-0x17FFFFFF   | CPU RAM (0x10000000- with `cpu_ram2 YES`)      |
| 0x20000000-0x2FFFFFFF   | Z3 RAM: same as its ARM DDR address, so the 060->ARM translation is the identity |
| 0x30000000-0x37FFFFFF   | RTG (128 MB autoconfig window)                 |

## Config key

`z3660cfg.txt`: `amix_relocate YES|NO`, default `NO`. It is read from z3660cfg.txt
only (not from presets), like `cpu_ram2`.

The older key `amix_mode` (fork 1.0, emulated UAE_030_MMU memory contract) is
something else: it forces `cpu_ram` off. That is why this key has its own name.

AMIX mode also needs these settings: a real 060 (boot mode CPU), `autoconfig_ram YES`
and `autoconfig_rtg YES`.

With `amix_relocate NO`, the firmware behaves as before. The five registers below act
like unused offsets: writes are ignored and reads return 0xFFFFFFFF. All bases keep
the values the firmware used to hard-code, now computed in `set_amiga_bases()`:

- Z3 RAM = 0x40000000
- RTG = 0x40000000 + (autoconfig_ram ? 256 MB : 0)
- RTG = 0x10000000 with `autoconfig_rtg NO`

## Registers (RTG board offsets, `src/rtg/zzregs.h`)

Longwords, big-endian from the 060, as every ZZ register. The firmware services them
only with `amix_relocate YES`.

| offset | name                     | write                              | read                         |
|--------|--------------------------|------------------------------------|------------------------------|
| 0x4E0  | REG_ZZ_AMIX_RAM_BASE     | requested Z3 RAM base (060)        | active `z3ram_base060`       |
| 0x4E4  | REG_ZZ_AMIX_RTG_BASE     | requested RTG base (060)           | active `rtg_base060`         |
| 0x4E8  | REG_ZZ_AMIX_COMMIT       | 0x434F4D54 "COMT" commit, 0x44454654 "DEFT" back to Amiga bases | 0x414D5831 "AMX1" (ID) |
| 0x4EC  | REG_ZZ_AMIX_STATUS       | ignored                            | status, signed (below)       |
| 0x4F0  | REG_ZZ_AMIX_FPGA_RESET   | 0x42525354 "BRST": FPGA bus reset (REG0 bit 31, 10 ms) | 1 (the reset is there) |

Status codes:

| code | meaning                                                    |
|------|------------------------------------------------------------|
| 0    | Amiga-mode bases active                                    |
| 1    | committed: the written bases are active                    |
| 2    | bus reset accepted, not done yet                           |
| 3    | bus reset done                                             |
| -2   | not a real 060                                             |
| -3   | `autoconfig_ram` or `autoconfig_rtg` is not YES            |
| -4   | RAM base invalid                                           |
| -5   | RTG base invalid                                           |
| -6   | RAM and RTG overlap                                        |
| -7   | unknown command, or bus reset not built in                 |

A refused COMMIT changes nothing: the previous bases stay active.

### Validation on COMMIT

- **Z3 RAM base.** It must be 256 MB aligned, because the FPGA decodes A060[31:28].
  It must be at or above 0x18000000 (above the CPU RAM), and the board must end at or
  below 0x40000000. That leaves 0x20000000 or 0x30000000.
- **RTG base.** It must be 128 MB aligned (A060[31:27]), at or above 0x18000000, and
  end at or below 0x40000000. It must not lie in ARM 0x20000000-0x2FFFFFFF, where
  core 0 reaches the Z3 RAM for SCSI/floppy DMA. It must not lie in
  0x38000000-0x3FFFFFFF, core 1's region. That leaves 0x18000000 or 0x30000000.
  - Why the address matters: core 0 maps the RTG a second time at its 060 address
    (ARM VA == 060 address) for pointers the 060 hands over (MPEG shared block,
    soft3D).
- **No overlap.** The RAM and the RTG must not overlap.

These rules are stricter than "64 KB aligned": the boards' own address decoding
needs natural alignment.

### What COMMIT does in core 0

COMMIT runs while the 060 waits for the ack of its write.

1. Sets `z3ram_base060`, `rtg_base060`.
2. Moves core 0's RTG mirror to the new VA (`rtg_mirror_core0_move()` in main.c):
   - It first saves the 128 section entries there.
   - Cache maintenance covers only the translation table, with a range flush.
   - It never flushes the whole L2, because core 1 may be running.
3. Points soft3D's `data3d` at the new address.

From then on, SCSI/floppy direct DMA translates with the new bases
(`z3ram_has060()`, `z3ram_060_to_arm()`).

An Amiga reset (`reset_thread`) puts everything back to the Amiga-mode bases. It also
restores the saved section entries, because AmigaOS autoconfigs the boards at the old
places again. "DEFT" does the same on request.

## Sequence for pvmk

1. Under AmigaOS, find the Z3660 RTG board (manufacturer 0x144B, product 1).
   - Read board+0x4E8. 0x414D5831 means AMIX mode is available.
   - 0xFFFFFFFF means the firmware has no AMIX mode, or `amix_relocate` is NO: boot the
     old way.
2. Load the kernel from disk (Z3660 SCSI) as now. Then stop AmigaOS: interrupts off,
   caches pushed.
3. Unconfigure both boards with the FPGA bus reset. This is the open point below.
   - After the reset nothing answers at the old addresses. The RTG registers,
     including the status, are unreachable until the RTG is configured again.
4. Autoconfig at 0xFF000000. The FPGA offers the Z3 RAM first (256 MB, product 2):
   - Write base 0x2000 (A31-A16, upper word of a longword) to 0xFF000044.
   - Then the RTG appears (128 MB, product 1): write 0x3000 to 0xFF000044.
   - Check manufacturer/product before each write.
5. Through the RTG at its new base 0x30000000:
   - Write 0x20000000 to +0x4E0 and 0x30000000 to +0x4E4.
   - Write "COMT" to +0x4E8.
   - Read +0x4EC: it must be 1. Read +0x4E0/+0x4E4 back.
6. No SCSI or floppy request between steps 3 and 5. Core 0 still translates with the
   old bases until the COMMIT.
7. Hand the bases to the kernel in bootinfo. The kernel maps the RTG and the mailbox
   noncacheable (dev_kvmap), never through the cacheable 1:1 window.

## Open question: the FPGA bus reset

shanshe has not yet said whether this needs a new bitstream. Findings from this tree:

### REG0 bit 31 (FPGA_RESET) already exists

`main.h`: SAXI REG0 at 0x7FC70000 is `GPIO_IN` in `z3660.v`.

- In `z3660.v:374-409`, `RESET_IN || GPIO_s[31]` resets the bus state machine and the
  autoconfig state:
  - `configured_z2/z3` and `shutup_z2/z3`
  - `autoConfigBaseFastRam/RTG/SCSI`, with RTG back to 0x1000 (0x10000000) only when
    REG0 bit 13 is 0
  - the AXI master state, `GPIO_OUT`
  - the board enables, latched again from REG0: bits 0, 3, 7, 12, 13, 14
- The firmware already pulses this bit:
  - at every Amiga reset (`reset_thread`: 10 ms)
  - at boot
  - from the debug console
- Video (VDMA, video formatter) and audio are separate IP with their own resets. This
  matches shanshe's answer that the framebuffer and the video mode survive.
- So core 0 can do the reset with no bitstream change, **if** the bitstream in use
  (shanshe's cpu_ram2 build of 2026-11-05, from his newer tree, not in this repo) still
  wires GPIO_s[31] the same way. That is the question for shanshe.

### The SLCR PL reset (FPGA_RST_CTRL 0xF8000240, FCLK_RESET0_N) is not an option

In `design_1.v`, FCLK_RESET0_N feeds `proc_sys_reset_0/1` and through them reaches:

- `z3660`'s RESET_IN
- its AXI slave resets (S00/S01_ARESETN), which would clear REG0 itself: CPU RAM,
  MAPROM and autoconfig enables lost
- the AXI interconnects
- the video VDMA and the audio formatter

That would lose the video and kill the CPU RAM. An AXI access from core 0 or core 1
in flight while its slave is in reset hangs that core. This is the same kind of board
freeze the SLCR reset of core 1 caused.

### Risks of the REG0 bit 31 path

- **The 060's own cycle.** The 060 asks for the reset with a write that the FPGA holds
  until core 0 acks it. A reset during that cycle would drop it, and the 060 would hang.
  `z3map_poll()` therefore pulses only after the ack: it waits 200 µs, then holds
  bit 31 for 10 ms, the same pulse reset_thread gives at every Amiga reset (shanshe's
  code: `DiscreteSet(REG0, FPGA_RESET); xc3_usleep(10000); DiscreteClear(...)`).
- **Every 060 bus cycle goes through the FPGA.** That includes CPU RAM, chip RAM and
  ROM, because the FPGA either serves the cycle or passes TS to the CPLD. Any cycle in
  flight during the pulse is lost. pvmk must stay off the bus for at least 20 ms
  (200 µs + 10 ms pulse + core 0's main loop latency, with margin), timed by the 060
  itself, since a CIA read is a bus cycle too:
  - a short loop running from the instruction cache
  - no data accesses
  - write buffer drained (`nop`)
  - interrupts off
- **MAPROM and the CPU RAM enables are latched again from REG0.** They keep their boot
  values, so this is harmless as long as nothing has changed REG0.
- **Core 0 itself must not master the Amiga bus during the pulse.** It does not: the
  pulse runs inline in the main loop.
- **Built in since 2026-11-07, untested on the board.** It acts only with
  `amix_relocate YES` and a real 060; otherwise a write of "BRST" sets an error status
  and nothing happens.  shanshe confirmed (2026-11-07) that bit 31 needs no new
  bitstream and leaves video, framebuffer and the ARM alone.

## Other constant sites

These were not changed:

- `main.c` `for(i=0x200;i<0x300;...)` (TLB for the Z3 RAM) is an ARM DDR address and
  stays the same in both modes.
- `main.c` mobotest restore, emulation branch: `ini=0x100` is used only by emulation
  modes, so it was left as it was.
- `z3660_debug_arm.c:46` accepts only 060 pointers 0x08000000-0x1FFFFFFF.
- `rtg/codec37.c`, `codec47.c` use ARM 0x30000000-0x32FFFFFF as scratch (SMUSH, AmigaOS
  only). In AMIX mode that VA shows the RTG. The saved entries are restored on an
  Amiga reset.
- `cpu_emulator.c` / `amix_ram.h` (0x10000000 for the emulated RTG) belong to the
  emulation modes only.

## Test plan

1. **`amix_relocate NO`** (no key): AmigaOS must behave exactly as with c1fix:
   - SCSI boot, file copies to and from Z3 RAM buffers (direct DMA)
   - ADF floppy
   - RTG/P96 screen, MPEG/vplay ARM mode, AHI
   - the reads of 0x4E0-0x4F0 return 0xFFFFFFFF
   - then AMIX as now (old layout).
2. **`amix_relocate YES` without COMMIT**: the same as test 1. In addition:
   - +0x4E8 reads "AMX1", +0x4EC reads 0, and +0x4E0/+0x4E4 read 0x40000000/0x50000000.
   - A COMMIT with bad bases (e.g. 0x48000000) is refused with status -4/-5, and
     nothing changes.
   - DEFT keeps status 0.
3. **pvmk** (later): bus reset ("BRST", wait 20 ms),
   autoconfig at 0x20000000/0x30000000, COMMIT, status 1.
   - Then AMIX with bootinfo bases: SCSI root mount, RTG.
   - Then reboot: the Amiga reset must bring AmigaOS back with the boards at
     0x40000000/0x50000000.
