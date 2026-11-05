/*
 * config_file.h
 *
 *  Created on: 28 feb. 2023
 *      Author: shanshe
 */

#ifndef SRC_CONFIG_FILE_H_
#define SRC_CONFIG_FILE_H_

enum BOOTMODE{
   MOBOCPU,
   CPU,
   MUSASHI,
   UAE_030,
   UAEJIT_030,
   UAE_040,
   UAEJIT_040,
   UAE_030_MMU,        // real 68030 PMMU (AMIX). Append-only; index-aligned with
   UAE_040_MMU,        // real 68040 MMU. Append-only; index-aligned with
   BOOTMODE_NUM        // ../Z3660_emu/src/main.h, ZTop/Ztop.c, ARM_ztop.
};

/* Is this boot mode EMULATED (the 68k is executed by core1) rather than a real CPU
 * driving the bus? Single source of truth -- main.c, debug_console.c and rtg/rtg.c all
 * ask here.
 *
 * Deliberately a CLOSED list of the REAL-CPU modes. It used to be spelled the other way
 * round, as eight separate copies that each enumerated the emulator modes, so appending
 * UAE_040_MMU to BOOTMODE left every copy answering "not emulated". main.c's copy is the
 * one that starts core1 -- it guards the only site that writes core1's entry address to
 * 0xFFFFFFF0 and sev's it -- so UAE_040_MMU booted as if a real 68060 were fitted: core1
 * was never started, core0 fell through to its real-CPU path, relinquished the bus to a
 * CPU that is not on this board, and the Amiga watchdog-reset every ~3 s forever
 * (first 040 metal boot 2026-08-20, firmware 2CFA9216: "060 starting now..." instead of
 * "Starting CPU emulator on Core1", and not one line of core1 output).
 *
 * The inversion also changes what a FUTURE appended mode defaults to, from real-CPU to
 * emulated -- which is the safe direction: core1's own dispatch halts loudly with
 * "No emulator selected!!!" if it has not been taught the mode, whereas the old default
 * silently handed the bus to absent silicon. Add a new REAL-CPU mode to this list.
 */
static inline int boot_mode_is_emulated(int boot_mode)
{
   return !(boot_mode == MOBOCPU || boot_mode == CPU);
}

enum YESNO{
   NO,
   YES,
   YESNO_NUM
};
enum YESNOMIN{
   _NO,
   _YES,
   _MIN,
   YESNOMIN_NUM
};
enum BOOTSCREEN_RES{
   RES_800x600,
   RES_1280x720,
   RES_1920x1080,
   RES_NUM
};
enum ARM_FREQUENCY{
   FREQ_667,
   FREQ_767,
   FREQ_867,
   FREQ_900,
   FREQ_933,
   FREQ_967,
   FREQ_1000,
   FREQ_1033,
   FREQ_1067,
   FREQ_1100,
   FREQ_1133,
   FREQ_1167,
   FREQ_1200,
   FREQ_1233,
   FREQ_1267,
   FREQ_1300,
   FREQ_NUM
};
extern const char *arm_frequency_names[FREQ_NUM];
typedef struct {
   uint32_t start;
   uint32_t length;
   char name[150];
} range;

typedef struct {
   int boot_mode;
   int scsiboot;
   int kickstart;
   int ext_kickstart;
   int scsi_num[7];
   int cd_target[7];   // per-SCSI-target CD-ROM flag: cd_target[id]!=0 => target id is a read-only 2048-byte CD-ROM (pdt 0x05).
                       // Set by the "isoN" config key, which indexes this by IMAGE SLOT N while piscsi_init() reads it by
                       // SCSI TARGET -- so a config only works when the iso slot number equals the target number (isoN + "scsiN N").
                       // Only slots 0..6 are representable here; see z3660cfg.txt and docs/AMIX.md.
   int adf_num[8];
   char hdf[20][150];
   char adf[20][150];
   int autoconfig_ram;
   int autoconfig_rtg;
   int cpu_ram;
   int amix_mode;
   int amix_ram;        // AMIX main-RAM window size in MiB; -1 = unset -> AMIX_RAM_MB_DEFAULT (see amix_ram.h)
   int service_cadence;
   /* CYBERSTORM C3 run-loop overhead switches. -1 = unset -> 1 = the historical
    * behaviour, so an absent key changes nothing. See docs/AMIX.md. */
   int perf_stats;      // core1 MMU run loops: per-instruction perf accounting
   int emu_probes;      // core1 MMU run loops: ISSUE-52-era diagnostic probes
   int core0_stats;     // core0 cooperative loop: C0STAT span stamping
   /* CYBERSTORM C1 profiler arming, config file only (NOT per preset -- see prof_config.h).
    * -1 = unset -> off, so an absent key produces a board that does not profile. */
   int prof_hz;         // core1 PROFILING build: guest-PC sampler rate in Hz, 0 = off
   int prof_buckets;    // core1 PROFILING build: interpreter stage buckets YES/NO
   int mount_sd_0x76;
   int mount_sd_root;
   float resistor;
   float temperature;
   int cpufreq;
   char kickstart0[150];
   char kickstart1[150];
   char kickstart2[150];
   char kickstart3[150];
   char kickstart4[150];
   char kickstart5[150];
   char kickstart6[150];
   char kickstart7[150];
   char kickstart8[150];
   char kickstart9[150];
   char ext_kickstart0[150];
   char ext_kickstart1[150];
   char ext_kickstart2[150];
   char ext_kickstart3[150];
   char ext_kickstart4[150];
   char ext_kickstart5[150];
   char ext_kickstart6[150];
   char ext_kickstart7[150];
   char ext_kickstart8[150];
   char ext_kickstart9[150];
   char sound_language[150];
   int enable_test;
   int bootscreen_resolution;
   int doubled_cursor;
   uint8_t mac_address[6];
   uint8_t pad[2];
   float bp_ton;
   float bp_toff;
   int boot_delay;
   int sd_clock;
   int monitor_switch;
   range test_range[8];
   int arm_frequency;
   /* ethernet keys (z3660cfg.txt only, read at boot; an absent key keeps its CFG_ETH_DEFAULT_*) */
   int eth_reraise;     // re-raise the Amiga interrupt on release while frames remain
   int eth_rxkick;      // interrupt on the next loop pass when a frame lands in an empty backlog
   int eth_taildrop;    // a full backlog drops only the arriving frame
   int eth_tx_poll;     // transmit wait polls the send count (bounded 1.1 ms) instead of usleep(100) steps
   int eth_adv1000;     // OS-time PHY setup also advertises 1000BASE-T
   int cpu_ram2;        // CPU RAM also at 0x10000000-0x17FFFFFF (FPGA REG0 bit 15); WHDLoad fails with it
} CONFIG;
typedef struct {
   int boot_mode;
   int scsiboot;
   int autoconfig_ram;
   int autoconfig_rtg;
   int cpu_ram;
   int amix_mode;
   int amix_ram;        // AMIX main-RAM window size in MiB; -1 = unset -> AMIX_RAM_MB_DEFAULT (see amix_ram.h)
   int service_cadence;
   /* CYBERSTORM C3 run-loop overhead switches. -1 = unset -> 1 = the historical
    * behaviour, so an absent key changes nothing. See docs/AMIX.md. */
   int perf_stats;      // core1 MMU run loops: per-instruction perf accounting
   int emu_probes;      // core1 MMU run loops: ISSUE-52-era diagnostic probes
   int core0_stats;     // core0 cooperative loop: C0STAT span stamping
   int mount_sd_0x76;
   int mount_sd_root;
   int kickstart;
   int ext_kickstart;
   int enable_test;
   int cpufreq;
   int bootscreen_resolution;
   int doubled_cursor;
   int scsi_num[7];
   int adf_num[8];
   uint8_t mac_address[6];
   char pad[2];
   float bp_ton;
   float bp_toff;
   char preset_name[50];
   int monitor_switch;
   range test_range[8];
   int arm_frequency;
} ENV_FILE_VARS;

enum CONFITEM {
   CONFITEM_NONE,
   CONFITEM_BOOTMODE,
   CONFITEM_KICKSTART,
   CONFITEM_EXT_KICKSTART,
   CONFITEM_SCSI_BOOT_ENABLE,
   CONFITEM_HDF0,
   CONFITEM_HDF1,
   CONFITEM_HDF2,
   CONFITEM_HDF3,
   CONFITEM_HDF4,
   CONFITEM_HDF5,
   CONFITEM_HDF6,
   CONFITEM_HDF7,
   CONFITEM_HDF8,
   CONFITEM_HDF9,
   CONFITEM_HDF10,
   CONFITEM_HDF11,
   CONFITEM_HDF12,
   CONFITEM_HDF13,
   CONFITEM_HDF14,
   CONFITEM_HDF15,
   CONFITEM_HDF16,
   CONFITEM_HDF17,
   CONFITEM_HDF18,
   CONFITEM_HDF19,
   CONFITEM_ISO0,
   CONFITEM_ISO1,
   CONFITEM_ISO2,
   CONFITEM_ISO3,
   CONFITEM_ISO4,
   CONFITEM_ISO5,
   CONFITEM_ISO6,
   CONFITEM_ISO7,
   CONFITEM_ISO8,
   CONFITEM_ISO9,
   CONFITEM_ISO10,
   CONFITEM_ISO11,
   CONFITEM_ISO12,
   CONFITEM_ISO13,
   CONFITEM_ISO14,
   CONFITEM_ISO15,
   CONFITEM_ISO16,
   CONFITEM_ISO17,
   CONFITEM_ISO18,
   CONFITEM_ISO19,
   CONFITEM_SCSI0,
   CONFITEM_SCSI1,
   CONFITEM_SCSI2,
   CONFITEM_SCSI3,
   CONFITEM_SCSI4,
   CONFITEM_SCSI5,
   CONFITEM_SCSI6,
   CONFITEM_ADF_FILE0,
   CONFITEM_ADF_FILE1,
   CONFITEM_ADF_FILE2,
   CONFITEM_ADF_FILE3,
   CONFITEM_ADF_FILE4,
   CONFITEM_ADF_FILE5,
   CONFITEM_ADF_FILE6,
   CONFITEM_ADF_FILE7,
   CONFITEM_ADF_FILE8,
   CONFITEM_ADF_FILE9,
   CONFITEM_ADF_FILE10,
   CONFITEM_ADF_FILE11,
   CONFITEM_ADF_FILE12,
   CONFITEM_ADF_FILE13,
   CONFITEM_ADF_FILE14,
   CONFITEM_ADF_FILE15,
   CONFITEM_ADF_FILE16,
   CONFITEM_ADF_FILE17,
   CONFITEM_ADF_FILE18,
   CONFITEM_ADF_FILE19,
   CONFITEM_ADF0,
   CONFITEM_ADF1,
   CONFITEM_ADF2,
   CONFITEM_ADF3,
   CONFITEM_ADF4,
   CONFITEM_ADF5,
   CONFITEM_ADF6,
   CONFITEM_ADF7,
   CONFITEM_AUTOCONFIG_RAM_ENABLE,
   CONFITEM_AUTOCONFIG_RTG_ENABLE,
   CONFITEM_CPU_RAM_ENABLE,
   CONFITEM_MOUNT_SD_0x76,
   CONFITEM_MOUNT_SD_ROOT,
   CONFITEM_RESISTOR,
   CONFITEM_TEMPERATURE,
   CONFITEM_CPUFREQ,
   CONFITEM_KICKSTART0,
   CONFITEM_KICKSTART1,
   CONFITEM_KICKSTART2,
   CONFITEM_KICKSTART3,
   CONFITEM_KICKSTART4,
   CONFITEM_KICKSTART5,
   CONFITEM_KICKSTART6,
   CONFITEM_KICKSTART7,
   CONFITEM_KICKSTART8,
   CONFITEM_KICKSTART9,
   CONFITEM_EXT_KICKSTART0,
   CONFITEM_EXT_KICKSTART1,
   CONFITEM_EXT_KICKSTART2,
   CONFITEM_EXT_KICKSTART3,
   CONFITEM_EXT_KICKSTART4,
   CONFITEM_EXT_KICKSTART5,
   CONFITEM_EXT_KICKSTART6,
   CONFITEM_EXT_KICKSTART7,
   CONFITEM_EXT_KICKSTART8,
   CONFITEM_EXT_KICKSTART9,
   CONFITEM_SOUND_LANGUAGE,
   CONFITEM_ENABLE_TEST,
   CONFITEM_BOOTSCREEN_RESOLUTION,
   CONFITEM_DOUBLED_CURSOR,
   CONFITEM_MAC_ADDRESS,
   CONFITEM_BP_TON,
   CONFITEM_BP_TOFF,
   CONFITEM_BOOT_DELAY,
   CONFITEM_SD_CLOCK,
   CONFITEM_MONITOR_SWITCH,
   CONFITEM_TEST_RANGE0,
   CONFITEM_TEST_RANGE1,
   CONFITEM_TEST_RANGE2,
   CONFITEM_TEST_RANGE3,
   CONFITEM_TEST_RANGE4,
   CONFITEM_TEST_RANGE5,
   CONFITEM_TEST_RANGE6,
   CONFITEM_TEST_RANGE7,
   CONFITEM_ARM_FREQUENCY,
   CONFITEM_AMIX_MODE,
   CONFITEM_SERVICE_CADENCE,
   CONFITEM_AMIX_RAM,
   CONFITEM_PERF_STATS,
   CONFITEM_EMU_PROBES,
   CONFITEM_CORE0_STATS,
   /* CYBERSTORM C1. APPEND-ONLY, like everything above: config_item_names[] is indexed by
    * this enum, so a mid-enum insert silently reassigns every key after it and the parser
    * writes the wrong field for the rest of the file with no diagnostic at all. */
   CONFITEM_PROF_HZ,
   CONFITEM_PROF_BUCKETS,
   CONFITEM_ETH_RERAISE,
   CONFITEM_ETH_RXKICK,
   CONFITEM_ETH_TAILDROP,
   CONFITEM_ETH_TX_POLL,
   CONFITEM_ETH_ADV1000,
   CONFITEM_CPU_RAM2,
   CONFITEM_NUM
};
/* ethernet key defaults (config.eth_*; an absent z3660cfg.txt line keeps these) */
#define CFG_ETH_DEFAULT_reraise   1
#define CFG_ETH_DEFAULT_rxkick    1
#define CFG_ETH_DEFAULT_taildrop  1
#define CFG_ETH_DEFAULT_tx_poll   1
#define CFG_ETH_DEFAULT_adv1000   0
#define CPUFREQ_MIN 50
#define CPUFREQ_MAX 120

void read_config_file(int verbose);
void read_env_files(int verbose);
extern const char *bootmode_names[];
int write_env_files(ENV_FILE_VARS *env_file);
int write_env_files_boot(ENV_FILE_VARS *env_file);
int write_env_files_scsi(ENV_FILE_VARS *env_file);
int write_env_files_adf(ENV_FILE_VARS *env_file);
int write_env_files_misc(ENV_FILE_VARS *env_file);
int write_env_files_bootscres(ENV_FILE_VARS *env_file);
int write_env_files_preset(ENV_FILE_VARS *env_file);
int delete_env_files(void);
int remove_preset_file(void);
extern CONFIG config,default_config,temp_config;
extern ENV_FILE_VARS env_file_vars_temp[9]; // really size 8
extern int preset_selected;
#endif /* SRC_CONFIG_FILE_H_ */
