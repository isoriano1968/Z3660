/*
 * z3_map.h -- where the 060 sees the Z3 RAM and the RTG board, as variables
 *
 * The FPGA puts the two Zorro III boards into ARM DDR at fixed places (z3660.v:
 * Z3_RAM_DDR_OFFSET, RTG_RAM_DDR_OFFSET) and decodes them at whatever base the
 * 060 wrote during autoconfig.  The ARM cannot read those bases back from the
 * FPGA, so core 0 keeps its own copy here:
 *
 *   z3ram_base060  060 address of the Z3 RAM board (256 MB)
 *   z3ram_size     its size
 *   rtg_base060    060 address of the RTG board (128 MB)
 *
 * In Amiga mode the values are what AmigaOS' expansion.library gives the boards
 * (computed in z3map_init() from autoconfig_ram / autoconfig_rtg), exactly the
 * numbers the firmware used to hard-code.  With z3660cfg.txt "amix_relocate YES"
 * a loader that has re-autoconfigured both boards (pvmk, AMIX) tells core 0 the
 * new bases through the registers REG_ZZ_AMIX_* (rtg/zzregs.h; AMIX-MODE.md).
 */
#ifndef Z3_MAP_H_
#define Z3_MAP_H_

#include <stdint.h>

/* fixed by the FPGA (z3660.v) */
#define Z3RAM_ARM_BASE       0x20000000UL  // Z3_RAM_DDR_OFFSET: ARM DDR address of the Z3 RAM
#define Z3RAM_BOARD_SIZE     0x10000000UL  // 256 MB, decoded on A060[31:28]
#define RTG_BOARD_SIZE       0x08000000UL  // 128 MB, decoded on A060[31:27]
#define RTG_FIXED_BASE060    0x10000000UL  // RTG not in the autoconfig chain (autoconfig_rtg NO)
#define Z3_SPACE_BASE060     0x40000000UL  // where AmigaOS puts the first Z3 board

extern uint32_t z3ram_base060;
extern uint32_t z3ram_size;
extern uint32_t rtg_base060;

/* a 060 address inside the Z3 RAM board (caller still checks config.autoconfig_ram) */
static inline int z3ram_has060(uint32_t a)
{
   return (uint32_t)(a - z3ram_base060) < z3ram_size;
}
/* 060 address in the Z3 RAM -> ARM address (identity when z3ram_base060 == Z3RAM_ARM_BASE) */
static inline uint32_t z3ram_060_to_arm(uint32_t a)
{
   return a - z3ram_base060 + Z3RAM_ARM_BASE;
}

void z3map_init(int real_cpu);     // after the config is final; real_cpu: the 060 drives the bus
void z3map_amiga_reset(void);      // Amiga reset: back to the Amiga-mode bases
void z3map_poll(void);             // main loop, after rtg_thread (deferred FPGA reset, if built)
int  z3map_write_reg(uint32_t address, uint32_t zdata);   // 1: handled
int  z3map_read_reg(uint32_t address, uint32_t *data);    // 1: handled

/* main.c: core 0's mirror of the RTG at its 060 address (ARM VA == 060 address) */
void rtg_mirror_core0_move(uint32_t new_rtg_base060);
void rtg_mirror_core0_restore(void);
void data3d_follow_rtg(void);       // soft3D's data block at the RTG's 060 address

#endif /* Z3_MAP_H_ */
