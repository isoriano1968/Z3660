/*
 * main.h
 *
 *  Created on: 8 jun. 2022
 *      Author: shanshe
 */

#ifndef SRC_MAIN_H_
#define SRC_MAIN_H_

#include "version.h"

#define INT_IPL_ON_THIS_CORE 0

#define NO_ARM_RESET_ON_AMIGA_RESET
//#define CPU_FIXED_FREQUENCY

#define M_PI 3.14159265358979323846

#include "platform.h"
#include "xgpiops.h"
#include "xil_io.h"
#include "xscugic.h"

#include "xil_cache.h"
#include "xil_cache_l.h"
#include "xil_exception.h"
//#define sleep sleep_seconds
//#define usleep sleep_microseconds
#include "xclk_wiz.h"
//#undef sleep
//#undef usleep
#include "xadcps.h"
#include "xuartps.h"

#include <ff.h>

#include "xaxivdma.h"
#include "sii9022_init/sii9022_init.h"
#include "xil_types.h"
#include "rtg/zz_video_modes.h"

#include "rtg/gfx.h"
#include "video.h"
#include "ethernet.h"
#include "interrupt.h"
#include "adc.h"
#include "ax.h"
#include "usb.h"
#include "xpseudo_asm.h"
#include <xparameters.h>

#define ETH_BACKLOG_NAG_COUNTER_MAX_060 1500
#define ETH_BACKLOG_NAG_COUNTER_MAX_EMU 1500

// comment out if you don't want to compile CPU emulators (Musashi and UAE)
#define CPU_EMULATOR

#define OP_PALETTE 3
#define OP_PALETTE_HI 19

#define PS_MIO_0      0 // MIO  0
#define PS_MIO_4      4 // MIO  4
#define PS_MIO_5      5 // MIO  5
#define PS_MIO_8      8 // MIO  8
#define PS_MIO_9      9 // MIO  9
#define PS_MIO_10    10 // MIO 10
#define PS_MIO_11    11 // MIO 11
#define PS_MIO_12    12 // MIO 12
#define PS_MIO_13    13 // MIO 13
#define PS_MIO_15    15 // MIO 15
#define PS_MIO_50    50 // MIO 50
#define PS_MIO_51    51 // MIO 51

typedef struct {
   volatile uint32_t shared_data;         // 0xFFFF0000
   volatile uint32_t write_rtg;           // 0xFFFF0004
   volatile uint32_t write_rtg_addr;      // 0xFFFF0008
   volatile uint32_t write_rtg_data;      // 0xFFFF000C
   volatile uint32_t core0_hold;          // 0xFFFF0010
   volatile uint32_t core0_hold_ack;      // 0xFFFF0014
   volatile uint32_t irq;                 // 0xFFFF0018
   volatile uint32_t uart_semaphore;      // 0xFFFF001C
   volatile uint32_t jit_enabled;         // 0xFFFF0020
   volatile uint32_t reset_emulator;      // 0xFFFF0024
   volatile uint32_t load_rom_emu;        // 0xFFFF0028
   volatile uint32_t load_rom_addr;       // 0xFFFF002C
   volatile uint32_t int_available;       // 0xFFFF0030
   volatile uint32_t cfg_emu;             // 0xFFFF0034
   volatile uint32_t write_scsi;          // 0xFFFF0038
   volatile uint32_t write_scsi_addr;     // 0xFFFF003C
   volatile uint32_t write_scsi_data;     // 0xFFFF0040
   volatile uint32_t write_scsi_type;     // 0xFFFF0044
   volatile uint32_t read_scsi;           // 0xFFFF0048
   volatile uint32_t read_scsi_addr;      // 0xFFFF004C
   volatile uint32_t read_scsi_data;      // 0xFFFF0050
   volatile uint32_t read_scsi_type;      // 0xFFFF0054
   volatile uint32_t scsiboot_rom_loaded; // 0xFFFF0058
   volatile uint32_t write_scsi_in_progress; // 0xFFFF005C
   volatile uint32_t read_rtg;            // 0xFFFF0060
   volatile uint32_t read_rtg_addr;       // 0xFFFF0064
   volatile uint32_t read_rtg_data;       // 0xFFFF0068
   volatile uint32_t mmu_core1_add;       // 0xFFFF006C
   volatile uint32_t z3_enabled;          // 0xFFFF0070
   volatile uint32_t load_ext_rom_addr;   // 0xFFFF0074
   volatile uint32_t load_romext_emu;     // 0xFFFF0078
   volatile uint32_t nops_write;          // 0xFFFF007C
   volatile uint32_t nops_read;           // 0xFFFF0080
   volatile uint32_t disassemble;         // 0xFFFF0084
   volatile uint32_t musashi_step;        // 0xFFFF0088
   volatile uint32_t reset_emulator_dis;  // 0xFFFF008C
   volatile uint32_t z2_enabled;          // 0xFFFF0090
   volatile uint32_t printhist_dataabort; // 0xFFFF0094
   volatile uint32_t arm_freq_code;       // 0xFFFF0098
   volatile uint32_t write_floppy;        // 0xFFFF009C
   volatile uint32_t write_floppy_addr;   // 0xFFFF00A0
   volatile uint32_t write_floppy_data;   // 0xFFFF00A4
   volatile uint32_t write_floppy_type;   // 0xFFFF00A8
   volatile uint32_t read_floppy;         // 0xFFFF00AC
   volatile uint32_t read_floppy_addr;    // 0xFFFF00B0
   volatile uint32_t read_floppy_data;    // 0xFFFF00B4
   volatile uint32_t read_floppy_type;    // 0xFFFF00B8
   volatile uint32_t debug_emu;           // 0xFFFF00BC  emulator-core debug spam toggle (0=off,1=on): [PC]/[RTE-B-IF]/fixup (DEMU menu cmd)
   volatile uint32_t amix_mode;           // 0xFFFF00C0  AMIX memory contract on/off (control core sets from config.amix_mode)
   volatile uint32_t service_cadence;     // 0xFFFF00C4  030-MMU run loop: instructions between IPL/cross-core polls (1=every; perf knob, SERV cmd)
   volatile uint32_t perf_report;         // 0xFFFF00C8  030-MMU run loop: 1=print instr/sec to serial ~1Hz (PERF cmd)
   volatile uint32_t amix_ram;            // 0xFFFF00CC  AMIX main-RAM window size in MiB (16 = the historical default; bounds in Z3660/src/amix_ram.h)
   volatile uint32_t perf_stats;          // 0xFFFF00D0  MMU run loops: 1=per-instruction perf accounting ON (the historical default), 0=off (perf_stats key, PERFS cmd)
   volatile uint32_t emu_probes;          // 0xFFFF00D4  MMU run loops: 1=run-loop diagnostic probes ON (the historical default), 0=off (emu_probes key, PROBES cmd)
   volatile uint32_t prof_ctl;            // 0xFFFF00D8  CYBERSTORM C1 profiler command word: cmd in bits 15..0, arg in 31..16 (Z3660_PROF_CTL_*). Core1 consumes and clears it in do_specialties(). Inert unless core1 is the PROFILING build.
   volatile uint32_t prof_hz;             // 0xFFFF00DC  CYBERSTORM C1 sampler rate in Hz, read when prof_ctl says ARM (PROF cmd; 1..20000, default 1000)
   volatile uint32_t arm_wall_hz;         // 0xFFFF00E0  ARM global-timer rate in Hz AS RETUNED AT RUNTIME (= counts_per_second, main.c). Core0 publishes it beside arm_freq_code; core1's profiler scales its measured PMU:timer ratio by it. Exists because COUNTS_PER_SECOND is a COMPILE-TIME BSP constant frozen at the 666.67 MHz default while arm_frequency retunes the PLL -- which made every rate the C1 profiler printed low by 1.65x on an 1100 MHz board. 0 = not published (older core0) -> core1 falls back to the constant and says so.
   volatile uint32_t blk_chunk;           // 0xFFFF00E4  CYBERSTORM C2: iterations of a recognised block idiom (bzero/mcpy/CPUSHL loop) the 040 run loop services per pass. 0 = unset -> the built-in default; 0xFFFFFFFF = OFF (pure passthrough); anything else is the cap, clamped. The rule is z3660_blk.h's z3660_blk_resolve_chunk() and NOTHING else may re-implement it. Unset means DEFAULT, not off, so a core0 built before this field existed still runs the shipped behaviour.
   volatile uint32_t dopc_on;             // 0xFFFF00E8  CYBERSTORM C2 rung 1: the 040 run loop's decoded-op cache (4096 entries, 4-way, 64 KB). 0 = unset -> the built-in default (ON); 0xFFFFFFFF = OFF (pure passthrough: never consulted, never filled); anything else = ON. The rule is z3660_dopc.h's z3660_dopc_resolve() and NOTHING else may re-implement it. Unset means ON, not off, so a core0 built before this field existed still runs the shipped behaviour.
   volatile uint32_t dfast_on;            // 0xFFFF00EC  CYBERSTORM C2 rung 2: the 040 data accessors' fast path (inline TTR verdict + page-cache resolve + direct RAM access). 0 = unset -> the built-in default (ON); 0xFFFFFFFF = OFF (every access takes the original out-of-line accessor); anything else = ON. The rule is z3660_dfast.h's z3660_dfast_resolve() and NOTHING else may re-implement it. Unset means ON, not off, so a core0 built before this field existed still runs the shipped behaviour. OFF is the REFERENCE machine, which is what makes a PROFZ-fenced same-boot A/B possible without a second flash.
   volatile uint32_t warp_on;             // 0xFFFF00F0  WARP ENGINE (docs/WARP-ENGINE.md): the 68040 MMU block engine -- guest basic blocks compiled to ARM and executed in place of the interpreter. 0 = unset -> DISARMED, which is also the shipped state; 0x57415250 ("WARP") = ARMED; anything else = DISARMED. THE POLARITY IS THE OPPOSITE OF THE THREE SWITCHES ABOVE, on purpose (plan Q9): unset means the shipped behaviour, and the shipped behaviour of an engine that is not shipped until the M5 gate is OFF. Arming takes an EXACT MAGIC, not a truthy word, because SHARED is hand-maintained in two headers a beta resync has already mis-merged once -- a neighbour bleeding into this field must DISARM. The rule is z3660_warp.h's z3660_warp_resolve() and NOTHING else may re-implement it. core0 publishes the OFF sentinel UNCONDITIONALLY on every boot (config_file.c), because this mailbox is DDR that survives a warm reset and an arm must not outlive the boot that made it. No config-file key exists: the console command WARP is the only way in.
} SHARED;
extern SHARED *shared;
#define REG_BASE_ADDRESS XPAR_Z3660_0_BASEADDR
#define write_reg(Offset,Data) (*(volatile uint32_t *)(REG_BASE_ADDRESS+(Offset)))=(Data)
#define read_reg(Offset) (*(volatile uint32_t *)(REG_BASE_ADDRESS+(Offset)))

#define BIT0  (1<<0)
#define BIT8  (1<<8)
#define BIT9  (1<<9)
#define BIT12 (1<<12)
#define BIT13 (1<<13)
#define BIT15 (1<<15)

//#define NBR_ARM(X)        *(volatile uint32_t*)(XPAR_PS7_GPIO_0_BASEADDR)=~((BIT15)<<16) & (0xFFFF0000U | ((X)*(BIT15)));
//#define CPLD_RESET_ARM(X) *(volatile uint32_t*)(XPAR_PS7_GPIO_0_BASEADDR)=~((BIT13)<<16) & (0xFFFF0000U | ((X)*(BIT13)));
#define NBR_ARM(X)        do{                                                  \
                              /*printf("--------------->NBR_ARM: %d\n", X); */ \
                              XGpioPs_WritePin(&GpioPs, PS_MIO_8, X);          \
                            } while(0)
#define CPLD_RESET_ARM(X) do{                                                        \
                              /*printf("--------------->CPLD_RESET_ARM: %d\n", X);*/ \
                              XGpioPs_WritePin(&GpioPs, PS_MIO_13, X);}              \
                              while(0)
unsigned int READ_NBG_ARM(void);

#define JP1_PIN      PS_MIO_4  // MIO 4  (ZTURN JP1)
#define JP2_PIN      PS_MIO_5  // MIO 5  (ZTURN JP2)
#define n040RSTI     PS_MIO_10 // MIO 10
#define LED1         PS_MIO_11 // MIO 11 (Z3660's green led)

//#define CPLD_RAM_EN  14 // MIO 14 used by CAN RX !!!!
#define USER_SW1     PS_MIO_50 // MIO 50
#define PS_MIO51_501 PS_MIO_51 // MIO 51 (ethernet reset)

#define REG0 0x00
#define REG1 0x04
#define REG2 0x08
#define REG3 0x0C
#define REG4 0x10
#define REG5 0x14
#define REG6 0x18
#define REG7 0x1C
#define REG8 0x20

#define FPGA_RAM_EN                   (1L<< 0)  // SAXI REG0  0
#define FPGA_RAM_BURST_READ_EN        (1L<< 1)  // SAXI REG0  1
#define FPGA_RAM_BURST_WRITE_EN       (1L<< 2)  // SAXI REG0  2
#define FPGA_MAPROM_EN                (1L<< 3)  // SAXI REG0  3
#define FPGA_TSCONDITION0             (0L<< 4)  // SAXI REG0  6..4
#define FPGA_TSCONDITION1             (1L<< 4)  // SAXI REG0  6..4
#define FPGA_TSCONDITION2             (2L<< 4)  // SAXI REG0  6..4
#define FPGA_TSCONDITION3             (3L<< 4)  // SAXI REG0  6..4
#define FPGA_TSCONDITION4             (4L<< 4)  // SAXI REG0  6..4
#define FPGA_TSCONDITION5             (5L<< 4)  // SAXI REG0  6..4
#define FPGA_MAPROMEXT_EN             (1L<< 7)  // SAXI REG0  7
#define FPGA_ENCONDITION_PCLK0_CLKEN0 (0L<< 8)  // SAXI REG0  9..8
#define FPGA_ENCONDITION_PCLK1_CLKEN0 (1L<< 8)  // SAXI REG0  9..8
#define FPGA_ENCONDITION_PCLK0        (2L<< 8)  // SAXI REG0  9..8
#define FPGA_ENCONDITION_PCLK1        (3L<< 8)  // SAXI REG0  9..8
#define FPGA_ADVANCED_BURST           (3L<<10)  // SAXI REG0  10
#define FPGA_PREFETCH_ENABLE          (3L<<11)  // SAXI REG0  11
#define FPGA_256MB_AUTOCONFIG_EN      (1L<<12)  // SAXI REG0 12
#define FPGA_RTG_AUTOCONFIG_EN        (1L<<13)  // SAXI REG0 13
#define FPGA_SCSI_AUTOCONFIG_EN       (1L<<14)  // SAXI REG0 14
#define FPGA_RAM2_EN                  (1L<<15)  // SAXI REG0 15: CPU RAM at 0x10000000-0x17FFFFFF
#define FPGA_ENABLE_CPUCLK_OUTPUT     (1L<<16)  // SAXI REG0 16
#define FPGA_AUTOCONFIG_BOOT_EN       (1L<<27)  // SAXI REG0 27
#define FPGA_BP                       (1L<<28)  // SAXI REG0 28
#define FPGA_INT6                     (1L<<29)  // SAXI REG0 29
#define READ_WRITE_ACK                (1L<<30)  // SAXI REG0 30
#define FPGA_RESET                    (1L<<31)  // SAXI REG0 31

#define FPGA_CPUCLK_DETECTED          (1L<< 0)  // SAXI REG6 0
#define FPGA_CLK90_DETECTED           (1L<< 1)  // SAXI REG6 0

#define ENABLE_BURST_READ_FPGA do{DiscreteSet(REG0, FPGA_RAM_BURST_READ_EN);\
      }while(0)
#define DISABLE_BURST_READ_FPGA do{DiscreteClear(REG0, FPGA_RAM_BURST_READ_EN);\
      }while(0)

#define ENABLE_BURST_WRITE_FPGA do{DiscreteSet(REG0, FPGA_RAM_BURST_WRITE_EN);\
      }while(0)
#define DISABLE_BURST_WRITE_FPGA do{DiscreteClear(REG0, FPGA_RAM_BURST_WRITE_EN);\
      }while(0)

#ifdef XPARAMETERS_H // FIXME: this is not needed, but eclipse complaints if not defined here
//#define XPAR_Z3660_0_BASEADDR 0x83C00000
#define XPAR_XI2STX_0_DEVICE_ID XPAR_PROCESSING_AV_SYSTEM_AUDIO_VIDEO_ENGINE_I2S_TRANSMITTER_0_DEVICE_ID
#define XPAR_PROCESSING_AV_SYSTEM_AUDIO_VIDEO_ENGINE_I2S_TRANSMITTER_0_DEVICE_ID 0
#define XPAR_XAUDIOFORMATTER_0_DEVICE_ID XPAR_PROCESSING_AV_SYSTEM_AUDIO_VIDEO_ENGINE_AUDIO_FORMATTER_0_DEVICE_ID
#define XPAR_PROCESSING_AV_SYSTEM_AUDIO_VIDEO_ENGINE_AUDIO_FORMATTER_0_DEVICE_ID 0
//#define XPAR_XAUDIOFORMATTER_0_BASEADDR 0x78C40000
#endif

#define REG_BASE_ADDRESS_S01 0x7FC70000
#define REG_BASE_ADDRESS_S00 XPAR_Z3660_0_BASEADDR

#define read_reg_s01(Offset) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S01+(Offset)))
#define write_reg_s01(Offset,Data) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S01+(Offset)))=(Data)
//#define write_reg64_s01(Offset,Data) (*(volatile uint64_t *)(REG_BASE_ADDRESS_S01+(Offset)))=(Data)

#define write_reg_s00(Offset,Data) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S00+(Offset)))=(Data)
#define read_reg_s00(Offset) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S00+(Offset)))
#define write_reg64_s00(Offset,Data) (*(volatile uint64_t *)(REG_BASE_ADDRESS_S00+(Offset)))=(Data)
#define write_mem32(Offset,Data) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S00+0x04000000+((Offset&0x00FFFFFF)<<2)))=(Data)
#define write_mem16(Offset,Data) (*(volatile uint32_t *)(REG_BASE_ADDRESS_S00+0x08000000+((Offset&0x00FFFFFF)<<2)))=(Data)
#define write_mem8(Offset,Data)  (*(volatile uint32_t *)(REG_BASE_ADDRESS_S00+0x0C000000+((Offset&0x00FFFFFF)<<2)))=(Data)

#define DiscreteSet(Offset,Mask) write_reg_s01(Offset,read_reg_s01(Offset)|(Mask))
#define DiscreteClear(Offset,Mask) write_reg_s01(Offset,read_reg_s01(Offset)&(~(Mask)))

void reset_video(int reset_frame_buffer);
void loop2(void);
void Display2(void);
void rtg_loop(void);
void rtg_init(void);
void hard_reboot(void);

void video_mode_init(int mode, int scalemode, int colormode);
int init_vdma_vid(int hsize, int vsize, int hdiv, int vdiv, uint32_t buspos);
int init_vdma_ovl(int hsize, int vsize, int hdiv, int vdiv, uint32_t buspos);
int stop_vdma_ovl(void);
int start_vdma_ovl(void);
uint32_t arm_read_amiga(uint32_t address, uint32_t size);
void arm_write_amiga(uint32_t address, uint32_t data, uint32_t size);
void arm_write_nowait(uint32_t address, uint32_t data);

#define WRITE_ (0<<1)
#define READ_  (1<<1)
#define LONG_  (0<<2)
#define BYTE_  (1<<2)
#define WORD_  (2<<2)
#define LINE_  (3<<2)

#define NO_RESET_FRAMEBUFFER 0
#define    RESET_FRAMEBUFFER 1

#define M68K_RUNNING 0
#define M68K_RESET   1
#include "defines.h"

//#define ACTIVITY_LED_OFF do{XGpioPs_WritePin(&GpioPs, LED1, 1);}while(0)
//#define ACTIVITY_LED_ON do{XGpioPs_WritePin(&GpioPs, LED1, 0);}while(0)
#define ACTIVITY_LED_OFF do{*(uint32_t *)0xE000A000=0xF7FF0800;\
      }while(0)
#define ACTIVITY_LED_ON do{*(uint32_t *)0xE000A000=0xF7FF0000;\
      DiscreteSet(REG0,FPGA_BP);\
      DiscreteClear(REG0,FPGA_BP);\
}while(0)

FRESULT f_clk_mount (FATFS* fs, const TCHAR* path, BYTE opt);
FRESULT f_umount(const TCHAR* path);

void xc3_usleep(unsigned int usec);
void set_cpu_freq_for_usleep(int cpufreq);

#endif /* SRC_MAIN_H_ */
