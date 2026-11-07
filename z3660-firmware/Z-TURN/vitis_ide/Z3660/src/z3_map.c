/*
 * z3_map.c -- the 060 bases of the Z3 RAM and the RTG board, and the AMIX-mode
 * registers that move them (see z3_map.h, rtg/zzregs.h REG_ZZ_AMIX_*, AMIX-MODE.md)
 */
#include <stdio.h>
#include "main.h"
#include "config_file.h"
#include "z3_map.h"
#include "rtg/zzregs.h"
#include "xc3_usleep.h"

uint32_t z3ram_base060 = Z3_SPACE_BASE060;
uint32_t z3ram_size    = Z3RAM_BOARD_SIZE;
uint32_t rtg_base060   = RTG_FIXED_BASE060;

static int real_060;            // the 060 drives the bus (no emulation)
static int committed;           // the loader's bases are active
static int32_t status=AMIX_ST_DEFAULT;
static uint32_t req_ram, req_rtg;

/* The Amiga-mode bases: AmigaOS configures the Z3 boards from Z3_SPACE_BASE060 on,
 * the RAM board first (the FPGA offers it first), so the RTG follows it when present. */
static void set_amiga_bases(void)
{
   z3ram_base060 = Z3_SPACE_BASE060;
   z3ram_size    = Z3RAM_BOARD_SIZE;
   if(config.autoconfig_rtg)
      rtg_base060 = Z3_SPACE_BASE060 + (config.autoconfig_ram ? z3ram_size : 0);
   else
      rtg_base060 = RTG_FIXED_BASE060;
}

void z3map_init(int real_cpu)
{
   real_060=real_cpu;
   committed=0;
   status=AMIX_ST_DEFAULT;
   req_ram=req_rtg=0;
   set_amiga_bases();
   if(config.amix_relocate)
      printf("[AMIX] relocation registers on (Z3 RAM 0x%08lX, RTG 0x%08lX until a loader commits)\n",
             z3ram_base060, rtg_base060);
}

static void back_to_amiga_bases(void)
{
   if(committed)
      rtg_mirror_core0_restore();
   committed=0;
   set_amiga_bases();
   data3d_follow_rtg();
}

void z3map_amiga_reset(void)
{
   if(!committed)
      return;
   back_to_amiga_bases();
   status=AMIX_ST_DEFAULT;
   printf("[AMIX] Amiga reset: Z3 RAM 0x%08lX, RTG 0x%08lX again\n", z3ram_base060, rtg_base060);
}

/* The FPGA decodes the RAM on A060[31:28] and the RTG on A060[31:27]: the bases must be
 * aligned to the board size.  Both go above the CPU RAM (0x08000000-0x17FFFFFF) and
 * below the AMIX kernel's virtual space (0x40000000).  Core 0 mirrors the RTG at its
 * 060 address (ARM VA == 060 address, for pointers the 060 hands over, e.g. MPEG,
 * soft3D), so the RTG cannot sit where core 0 needs its own DDR view: the Z3 RAM at
 * 0x20000000-0x2FFFFFFF (SCSI/floppy DMA) and core 1's region 0x38000000-0x3FFFFFFF. */
static int32_t validate(uint32_t ram, uint32_t rtg)
{
   if(!real_060)
      return AMIX_ST_ERR_NOT_060;
   if(!config.autoconfig_ram || !config.autoconfig_rtg)
      return AMIX_ST_ERR_BOARDS;
   if((ram & (Z3RAM_BOARD_SIZE-1)) || ram < 0x18000000UL || ram > 0x40000000UL-Z3RAM_BOARD_SIZE)
      return AMIX_ST_ERR_RAM;
   if((rtg & (RTG_BOARD_SIZE-1)) || rtg < 0x18000000UL || rtg > 0x40000000UL-RTG_BOARD_SIZE
      || (rtg >= Z3RAM_ARM_BASE && rtg < Z3RAM_ARM_BASE+Z3RAM_BOARD_SIZE)
      || rtg >= 0x38000000UL)
      return AMIX_ST_ERR_RTG;
   if(rtg < ram+Z3RAM_BOARD_SIZE && ram < rtg+RTG_BOARD_SIZE)
      return AMIX_ST_ERR_OVERLAP;
   return AMIX_ST_COMMITTED;
}

/* Runs while the 060 waits on its register write (the ack comes after this). */
static void commit(void)
{
   int32_t st=validate(req_ram,req_rtg);
   if(st!=AMIX_ST_COMMITTED)
   {
      status=st;
      printf("[AMIX] commit refused (%ld): RAM 0x%08lX RTG 0x%08lX\n", st, req_ram, req_rtg);
      return;
   }
   if(committed)
      rtg_mirror_core0_restore();
   z3ram_base060=req_ram;
   z3ram_size=Z3RAM_BOARD_SIZE;
   rtg_base060=req_rtg;
   rtg_mirror_core0_move(rtg_base060);
   data3d_follow_rtg();
   committed=1;
   status=AMIX_ST_COMMITTED;
   printf("[AMIX] committed: Z3 RAM 0x%08lX (ARM 0x%08lX), RTG 0x%08lX\n",
          z3ram_base060, Z3RAM_ARM_BASE, rtg_base060);
}

/* The FPGA bus reset for an AMIX loader (pvmk): pulses REG0 bit 31 (FPGA_RESET) the way
 * reset_thread does at every Amiga reset (10 ms, as shanshe's code): z3660.v clears the
 * autoconfig state (configured/shutup, both bases) and re-latches the board enables
 * from REG0.  It does not touch the video/audio IP or the ARM (shanshe 2026-11-07).
 * Done after the 060's write has been acked, so that the reset does not cut its own
 * cycle; the 060 must stay off the bus meanwhile, at least AMIX_BUSRESET_WAIT_US
 * (AMIX-MODE.md).  After the pulse both boards wait at 0xFF000000 for autoconfig, and
 * the RTG registers (this status too) are unreachable until the RTG is configured.
 * Only with amix_relocate YES and a real 060 (z3map_write_reg). */
#define AMIX_BUSRESET_DELAY_US  200     // after the ack, before the pulse
#define AMIX_BUSRESET_PULSE_US  10000   // as reset_thread and shanshe's code
static int busreset_pending;

void z3map_poll(void)
{
   if(!busreset_pending)
      return;
   busreset_pending=0;
   xc3_usleep(AMIX_BUSRESET_DELAY_US);
   DiscreteSet(REG0, FPGA_RESET);
   xc3_usleep(AMIX_BUSRESET_PULSE_US);
   DiscreteClear(REG0, FPGA_RESET);
   back_to_amiga_bases();     // nothing is configured now; the loader commits its bases next
   status=AMIX_ST_RESET_DONE;
   printf("[AMIX] FPGA bus reset done\n");
}

int z3map_write_reg(uint32_t address, uint32_t zdata)
{
   if(!config.amix_relocate)
      return 0;
   switch(address)
   {
      case REG_ZZ_AMIX_RAM_BASE:
         req_ram=zdata;
         return 1;
      case REG_ZZ_AMIX_RTG_BASE:
         req_rtg=zdata;
         return 1;
      case REG_ZZ_AMIX_COMMIT:
         if(zdata==AMIX_CMD_COMMIT)
            commit();
         else if(zdata==AMIX_CMD_DEFAULTS)
         {
            back_to_amiga_bases();
            status=AMIX_ST_DEFAULT;
         }
         else
            status=AMIX_ST_ERR_CMD;
         return 1;
      case REG_ZZ_AMIX_STATUS:
         return 1;
      case REG_ZZ_AMIX_FPGA_RESET:
         if(zdata==AMIX_CMD_BUSRESET && real_060)
         {
            busreset_pending=1;
            status=AMIX_ST_RESET_PENDING;
         }
         else
            status=real_060?AMIX_ST_ERR_CMD:AMIX_ST_ERR_NOT_060;
         return 1;
   }
   return 0;
}

int z3map_read_reg(uint32_t address, uint32_t *data)
{
   if(!config.amix_relocate)
      return 0;
   switch(address)
   {
      case REG_ZZ_AMIX_RAM_BASE:   *data=z3ram_base060;     return 1;
      case REG_ZZ_AMIX_RTG_BASE:   *data=rtg_base060;       return 1;
      case REG_ZZ_AMIX_COMMIT:     *data=AMIX_ID;           return 1;
      case REG_ZZ_AMIX_STATUS:     *data=(uint32_t)status;  return 1;
      case REG_ZZ_AMIX_FPGA_RESET:
         *data=1;                // the bus reset is there
         return 1;
   }
   return 0;
}
