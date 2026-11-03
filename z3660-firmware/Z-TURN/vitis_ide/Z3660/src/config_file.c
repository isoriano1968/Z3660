/*
 * config_file.c
 *
 *  Created on: 28 feb. 2023
 *      Author: shanshe
 */
#include <ff.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xil_exception.h"
#include "config_file.h"
#include "amix_ram.h"
#include "overhead_switches.h"   /* the single copy of the perf_stats/emu_probes/core0_stats rule */
#include "prof_config.h"         /* the single copy of the prof_hz/prof_buckets rule (CYBERSTORM C1) */
/* WARP ENGINE: the OFF sentinel this file publishes unconditionally on every boot. Included
 * directly rather than mirrored, for debug_console.c's reason -- a mirrored copy is only a
 * second place for the sentinel to drift away from the one core1 tests against. */
#include "../../Z3660_emu/src/uae/z3660_warp.h"
#include "core0_stats.h"         /* core0_stats_set_enabled() -- the core0_stats key */
#include "main.h"
#include "ARM_ztop/checkbox.h"
CONFIG config,default_config,temp_config;
extern SHARED *shared;
extern uint8_t EmacPsMAC[6];

/* Deliberately UNSIZED: the completeness tripwire just below compares the
 * initializer count against CONFITEM_NUM, and an explicit [CONFITEM_NUM]
 * dimension would silently pad a missing name with NULL and make the check
 * vacuous. */
const char *config_item_names[] = {
      "NONE",
      "bootmode",
      "kickstart",
      "ext_kickstart",
      "scsiboot",
      "hdf0",
      "hdf1",
      "hdf2",
      "hdf3",
      "hdf4",
      "hdf5",
      "hdf6",
      "hdf7",
      "hdf8",
      "hdf9",
      "hdf10",
      "hdf11",
      "hdf12",
      "hdf13",
      "hdf14",
      "hdf15",
      "hdf16",
      "hdf17",
      "hdf18",
      "hdf19",
      "iso0",
      "iso1",
      "iso2",
      "iso3",
      "iso4",
      "iso5",
      "iso6",
      "iso7",
      "iso8",
      "iso9",
      "iso10",
      "iso11",
      "iso12",
      "iso13",
      "iso14",
      "iso15",
      "iso16",
      "iso17",
      "iso18",
      "iso19",
      "scsi0",
      "scsi1",
      "scsi2",
      "scsi3",
      "scsi4",
      "scsi5",
      "scsi6",
      "adf_file0",
      "adf_file1",
      "adf_file2",
      "adf_file3",
      "adf_file4",
      "adf_file5",
      "adf_file6",
      "adf_file7",
      "adf_file8",
      "adf_file9",
      "adf_file10",
      "adf_file11",
      "adf_file12",
      "adf_file13",
      "adf_file14",
      "adf_file15",
      "adf_file16",
      "adf_file17",
      "adf_file18",
      "adf_file19",
      "adf0",
      "adf1",
      "adf2",
      "adf3",
      "adf4",
      "adf5",
      "adf6",
      "adf7",
      "autoconfig_ram",
      "autoconfig_rtg",
      "cpu_ram",
      "mount_sd_0x76",
      "mount_sd_root",
      "resistor",
      "temperature",
      "cpufreq",
      "kickstart0",
      "kickstart1",
      "kickstart2",
      "kickstart3",
      "kickstart4",
      "kickstart5",
      "kickstart6",
      "kickstart7",
      "kickstart8",
      "kickstart9",
      "ext_kickstart0",
      "ext_kickstart1",
      "ext_kickstart2",
      "ext_kickstart3",
      "ext_kickstart4",
      "ext_kickstart5",
      "ext_kickstart6",
      "ext_kickstart7",
      "ext_kickstart8",
      "ext_kickstart9",
      "sound_language",
      "enable_test",
      "bootscreen_resolution",
      "doubled_cursor",
      "mac_address",
      "bp_ton",
      "bp_toff",
      "boot_delay",
      "sd_clock",
      "monitor_switch",
      "test_range0",
      "test_range1",
      "test_range2",
      "test_range3",
      "test_range4",
      "test_range5",
      "test_range6",
      "test_range7",
      "arm_frequency",
      "amix_mode",
      "service_cadence",
      "amix_ram",
      "perf_stats",
      "emu_probes",
      "core0_stats",
      "prof_hz",
      "prof_buckets",
      "eth_reraise",
      "eth_rxkick",
      "eth_taildrop",
      "eth_tx_poll",
      "eth_adv1000",
};
/* Tripwire: config_item_names[] is indexed by CONFITEM, so a new enum entry with
 * no matching string silently shifts every key after it -- the parser would then
 * assign the wrong config field for the whole rest of the file, with no error.
 * Compile-time check, spelled without _Static_assert so it holds under -std=gnu99. */
typedef char z3660_confitem_names_complete[
   (sizeof(config_item_names)/sizeof(config_item_names[0]) == CONFITEM_NUM) ? 1 : -1];
const char *bootmode_names[BOOTMODE_NUM] = {
      "MOBOCPU",
      "CPU",
      "MUSASHI",
      "UAE_030",
      "UAEJIT_030",
      "UAE_040",
      "UAEJIT_040",
      "UAE_030_MMU",
      "UAE_040_MMU",
};
const char *yesno_names[YESNO_NUM] = {
      "NO",
      "YES",
};
const char *yesnomin_names[YESNOMIN_NUM] = {
      "NO",
      "YES",
      "MIN",
};
const char *resolution_names[RES_NUM] = {
      "800x600",
      "1280x720",
      "1920x1080",
};
const char *arm_frequency_names[FREQ_NUM] = {
      "667",
      "767",
      "867",
      "900",
      "933",
      "967",
      "1000",
      "1033",
      "1067",
      "1100",
      "1133",
      "1167",
      "1200",
      "1233",
      "1267",
      "1300",
};
void print_line(FIL *fil, char* line_in, ...)
{
   va_list args;
   UINT NumBytesWritten;
   char line[100];
   va_start(args, line_in);
   sprintf(line,line_in, args);
   f_write(fil, line, strlen(line),&NumBytesWritten);
}
/* Resolve, validate and publish the AMIX main-RAM window size.
 *
 * Called from BOTH config paths (no-preset and preset) once the whole file is
 * parsed, so config.amix_ram is always a usable MiB value by the time core1 is
 * dispatched. The bounds and the fallback live in amix_ram.h -- core1 re-runs
 * exactly the same classification on the mailbox value before it builds the map,
 * so a corrupted or stale shared->amix_ram cannot produce a bad bank range.
 *
 * A refused value is NOT truncated to the bound. It falls back to the default and
 * shouts, because the operator must not end up on a 128 MiB machine believing it
 * is a 512 MiB one -- especially when the failure mode at the top of the range is
 * the guest producing no console output whatsoever.
 *
 * The resolved value is always published to the mailbox (so core1 can never read a
 * stale one), but it only MEANS anything under amix_mode -- core1 consults it solely
 * inside its amix_mode arm, so every non-AMIX config is bit-for-bit unaffected. */
/* CYBERSTORM C3: resolve and publish the three run-loop overhead switches.
 *
 * Called from BOTH config paths (no-preset and preset) once the whole file is parsed,
 * exactly like resolve_amix_ram(), so the values are settled before core1 is dispatched.
 *
 * -1 (no key in the file) resolves to 1 = ON = the behaviour this firmware has always
 * had. That is the whole contract of this feature: a config that does not mention these
 * keys must produce the loops it produced before they existed.
 *
 * perf_stats/emu_probes are published to the cross-core mailbox unconditionally, so core1
 * can never read a stale value left by a previous boot; core1 re-reads them from there
 * (z3660_knobs_sync in newcpu.cpp) rather than caching anything across a reset.
 * core0_stats is core0's own, so it is applied directly.
 *
 * NOTE these are DIAGNOSTIC-COST switches, not functional ones. Turning any of them off
 * removes measurement and tracing, never emulation: the guest sees the same machine, it
 * just gets more of the ARM's time. That is what makes them safe to sweep on metal. */
static void resolve_overhead_switches(int verbose)
{
   config.perf_stats  = z3660_ovh_resolve(config.perf_stats);    /* overhead_switches.h */
   config.emu_probes  = z3660_ovh_resolve(config.emu_probes);
   config.core0_stats = z3660_ovh_resolve(config.core0_stats);

   shared->perf_stats = config.perf_stats;   /* always published, so core1 never reads a stale mailbox */
   shared->emu_probes = config.emu_probes;
   core0_stats_set_enabled(config.core0_stats);   /* core0's own -- applied, not published */

   /* Silent when everything is at the historical default; loud the moment an
    * instrument is off, so a serial log always says which run it was. */
   if(verbose && !(z3660_ovh_is_default(config.perf_stats)
                && z3660_ovh_is_default(config.emu_probes)
                && z3660_ovh_is_default(config.core0_stats)))
      printf("[CFG] run-loop overhead: perf_stats %s, emu_probes %s, core0_stats %s "
             "(diagnostics only -- emulation is unaffected)\n",
             yesno_names[config.perf_stats],
             yesno_names[config.emu_probes],
             yesno_names[config.core0_stats]);
}

/* CYBERSTORM C1: resolve the profiler keys and publish them to the cross-core mailbox.
 *
 * Published UNCONDITIONALLY, exactly like perf_stats/emu_probes above and for the same
 * reason: the mailbox is DDR that survives a warm reset, so a boot that leaves prof_ctl
 * alone would let core1 act on the previous boot's command. Writing it every time makes
 * "no profile requested" an explicit instruction rather than the absence of one.
 *
 * The buckets ride the ARM command's argument field rather than a mailbox word of their
 * own. That is not a byte count -- SHARED is hand-maintained in two headers that a beta
 * resync has already mis-merged once (docs/piscsi-service-path.md 5), so every field NOT
 * added is one fewer chance to alias something. The command word already has a 16-bit
 * argument and this is what it is for.
 *
 * On a non-profiling core1 the whole thing is inert: the stub poll_cmd never reads it. */
static void resolve_prof_config(int verbose)
{
   config.prof_hz      = z3660_prof_cfg_hz(config.prof_hz);            /* prof_config.h */
   config.prof_buckets = z3660_prof_cfg_buckets(config.prof_buckets);

   shared->prof_hz  = (uint32_t)config.prof_hz;
   shared->prof_ctl = config.prof_hz
       ? Z3660_PROF_CFG_CTL_MAKE(Z3660_PROF_CFG_CTL_ARM, config.prof_buckets ? 1 : 0)
       : Z3660_PROF_CFG_CTL_NONE;

   if(verbose && !z3660_prof_cfg_is_default(config.prof_hz, config.prof_buckets))
      printf("[CFG] profiler: prof_hz %d, prof_buckets %s "
             "(PROFILING build only -- the default and lean images ignore this)\n",
             config.prof_hz, yesno_names[config.prof_buckets]);
}

/* WARP ENGINE: DISARM THE BLOCK ENGINE, ON EVERY BOOT, UNCONDITIONALLY.
 *
 * THE MAILBOX IS DDR THAT SURVIVES A WARM RESET -- resolve_prof_config() above says so about
 * prof_ctl, and the consequence is the same one: a boot that leaves this field alone would
 * let core1 act on the PREVIOUS boot's arm. For a switch between two proven machines
 * (blk_chunk / dopc_on / dfast_on, which are deliberately NOT published here) that is
 * harmless. For an engine that is not shipped behaviour until the M5 gate it would mean an
 * unproven emulator coming up after a reset nobody connected to a console. Writing the
 * sentinel every time makes "not armed" an explicit instruction rather than the absence of
 * one.
 *
 * DELIBERATELY NOT A CONFIG KEY, and this function takes no argument for that reason: there
 * is nothing to parse, nothing to resolve, and no way to ask for the armed state from a
 * file. The console command WARP is the only way in, and it lasts one boot.
 *
 * Called from BOTH config paths, exactly like resolve_amix_ram() and
 * resolve_overhead_switches(), so the value is settled before core1 is dispatched. */
static void publish_warp_default(void)
{
   shared->warp_on = Z3660_WARP_OFF;
}

static void resolve_amix_ram(int verbose)
{
   int requested = config.amix_ram;
   int status    = amix_ram_classify(requested);

   config.amix_ram = amix_ram_resolve(requested);
   shared->amix_ram = config.amix_ram;   /* always published, so core1 never reads a stale mailbox */

   if(!config.amix_mode)
   {
      /* Not an AMIX boot: the key is inert. Say so rather than silently ignoring
       * a line someone deliberately wrote. */
      if(verbose && requested != -1)
         printf("[CFG] amix_ram %d ignored: amix_mode is NO (no AMIX memory contract)\n", requested);
      return;
   }

   if(amix_ram_is_refused(status))
   {
      printf("\n");
      printf("[CFG] **************** amix_ram %d %s\n", requested, amix_ram_status_text(status));
      printf("[CFG] **************** valid range is %d..%d MiB; falling back to %d MiB\n",
             AMIX_RAM_MB_MIN, AMIX_RAM_MB_MAX, AMIX_RAM_MB_DEFAULT);
      printf("[CFG] **************** the AMIX window starts at guest $%08lX and must stop below\n",
             (unsigned long)AMIX_RAM_GUEST_BASE);
      printf("[CFG] **************** guest $%08lX, where the RTG/ethernet combo board lives\n",
             (unsigned long)(AMIX_RAM_BOARD_BANK << AMIX_RAM_BANK_SHIFT));
      printf("\n");
   }
   else if(status == AMIX_RAM_EXPERIMENTAL)
   {
      printf("[CFG] amix_ram %d MiB -- %s\n", config.amix_ram, amix_ram_status_text(status));
      printf("[CFG] amix_ram: %d MiB is the recommended cap for a shipping config\n",
             AMIX_RAM_MB_SHIP_MAX);
      if(config.boot_mode==UAE_030_MMU)
         printf("[CFG] amix_ram: the 2KB-page 68030 kernel has the LOWER ceiling of the two -- "
                "this size is 68040-line territory\n");
   }
   else if(verbose)
   {
      printf("[CFG] amix_ram %d MiB (guest $%08lX..$%08lX)\n",
             config.amix_ram,
             (unsigned long)AMIX_RAM_GUEST_BASE,
             (unsigned long)(AMIX_RAM_GUEST_BASE + amix_ram_bytes(config.amix_ram) - 1));
   }

   if(amix_ram_hides_a3000mem(config.amix_ram))
      printf("[CFG] amix_ram %d MiB > %d: a3000mem (guest $07000000) is dropped -- its host backing "
             "is inside the window\n", config.amix_ram, AMIX_RAM_A3000MEM_MB);
}
void load_default_config(void)
{
   config.boot_mode=UAEJIT_040;
   for(int i=0;i<20;i++)
      config.hdf[i][0]=0;
   for(int i=0;i<20;i++)
      config.adf[i][0]=0;
   for(int i=0;i<7;i++)
      config.scsi_num[i]=-1;
   for(int i=0;i<8;i++)
      config.adf_num[i]=-1;
   for(int i=0;i<7;i++)
      config.cd_target[i]=0;
   config.scsiboot=0;
   config.autoconfig_ram=0;
   config.autoconfig_rtg=0;
   config.cpu_ram=1;
   config.amix_mode=-1;
   config.amix_ram=-1;
   config.service_cadence=-1;
   config.perf_stats=Z3660_OVH_UNSET;    /* CYBERSTORM C3: unset -> YES (historical behaviour) */
   config.emu_probes=Z3660_OVH_UNSET;
   config.core0_stats=Z3660_OVH_UNSET;
   config.prof_hz=Z3660_PROF_CFG_UNSET;      /* CYBERSTORM C1: unset -> OFF (no profiler before there was one) */
   config.prof_buckets=Z3660_PROF_CFG_UNSET;
   config.mount_sd_0x76=0;
   config.mount_sd_root=0;
   config.resistor=800.0;
   config.temperature=27.0;
   config.cpufreq=CPUFREQ_MIN;
   config.kickstart=0;
   config.kickstart0[0]=0;
   config.kickstart1[0]=0;
   config.kickstart2[0]=0;
   config.kickstart3[0]=0;
   config.kickstart4[0]=0;
   config.kickstart5[0]=0;
   config.kickstart6[0]=0;
   config.kickstart7[0]=0;
   config.kickstart8[0]=0;
   config.kickstart9[0]=0;
   config.ext_kickstart=0;
   config.ext_kickstart0[0]=0;
   config.ext_kickstart1[0]=0;
   config.ext_kickstart2[0]=0;
   config.ext_kickstart3[0]=0;
   config.ext_kickstart4[0]=0;
   config.ext_kickstart5[0]=0;
   config.ext_kickstart6[0]=0;
   config.ext_kickstart7[0]=0;
   config.ext_kickstart8[0]=0;
   config.ext_kickstart9[0]=0;
   strcpy(config.sound_language,"eng");
   config.enable_test=0;
   config.bootscreen_resolution=RES_800x600;
   config.doubled_cursor=0;
   for(int i=0;i<6;i++)
      config.mac_address[i]=EmacPsMAC[i];
   config.bp_ton=0;
   config.bp_toff=0;
   config.boot_delay=2;
   config.sd_clock=50;
   memcpy(&default_config,&config,sizeof(CONFIG));
   config.monitor_switch=0;
   config.arm_frequency=FREQ_667;
   config.eth_reraise=CFG_ETH_DEFAULT_reraise;
   config.eth_rxkick=CFG_ETH_DEFAULT_rxkick;
   config.eth_taildrop=CFG_ETH_DEFAULT_taildrop;
   config.eth_tx_poll=CFG_ETH_DEFAULT_tx_poll;
   config.eth_adv1000=CFG_ETH_DEFAULT_adv1000;
}
void write_config_file(char *filename)
{
   static FIL fil;      /* File object */
   f_open(&fil,filename, FA_OPEN_ALWAYS | FA_WRITE);
   load_default_config();
   print_line(&fil,"## Z3660 config file ##\n");
   print_line(&fil,"\n");
   print_line(&fil,"##General Configuration\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select boot mode:  \"MOBOCPU\" for Mother Board CPU, \"CPU\" for 060 CPU, \"MUSASHI\" \"UAE\" or \"UAEJIT\" for emulator, \"UAE_030_MMU\" for 68030 with real MMU (AMIX)\n");
   print_line(&fil,"#bootmode MOBOCPU\n");
   print_line(&fil,"#bootmode CPU\n");
   print_line(&fil,"#bootmode MUSASHI\n");
   print_line(&fil,"#bootmode UAE\n");
   print_line(&fil,"#bootmode UAE_030_MMU\n");
   print_line(&fil,"bootmode UAEJIT\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select 060 CPU frequency in MHz\n");
   print_line(&fil,"cpufreq 80\n");
   print_line(&fil,"\n");
   print_line(&fil,"## Emulation Configuration\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Declare up to 9 kickstarts files. (1-9)\n");
   print_line(&fil,"kickstart1 kicks/A4kOS322.rom\n");
   print_line(&fil,"kickstart2 kicks/DiagROM.rom\n");
   print_line(&fil,"kickstart3 kicks/A4kOS31.rom\n");
   print_line(&fil,"kickstart4 kicks/A4kOS321.rom\n");
   print_line(&fil,"# Select the number (1-9) to map one of the above kickstarts on ARM's internal RAM, or 0 to use the installed kickstart on your Amiga\n");
   print_line(&fil,"kickstart 1\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Declare up to 9 extended kickstarts files. (1-9)\n");
   print_line(&fil,"ext_kickstart1 kicks/kick060.rom\n");
   print_line(&fil,"# Select the number (1-9) to map one of the above extended kickstarts on ARM's internal RAM, or 0 to use the installed extended kickstart on your Amiga (if any)\n");
   print_line(&fil,"ext_kickstart 1\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Load scsi ROM on boot (boot from SCSI hdf files on SD)\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"scsiboot NO\n");
   print_line(&fil,"#scsiboot YES\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Declare your hdf files (from hdf0 to hdf19)\n");
   print_line(&fil,"#hdf0 hdf/A4000.hdf\n");
   print_line(&fil,"#hdf1 hdf/ZDH0.hdf\n");
   print_line(&fil,"#hdf2 hdf/Programs.hdf\n");
   print_line(&fil,"# Select the scsi number (scsi0 to scsi6) to assign one of the above hdf files\n");
   print_line(&fil,"scsi0 0\n");
   print_line(&fil,"scsi1 1\n");
   print_line(&fil,"#scsi2 2\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Declare your adf files (from adf_file0 to adf_file19)\n");
   print_line(&fil,"#adf_file0 adf/Install3.2.adf\n");
   print_line(&fil,"#adf_file1 adf/Classes3.2.adf\n");
   print_line(&fil,"#adf_file2 adf/Locale.adf\n");
   print_line(&fil,"# Select the adf number (adf0 to adf7) to assign one of the above adf files\n");
   print_line(&fil,"adf0 0\n");
   print_line(&fil,"adf1 1\n");
   print_line(&fil,"# adf2 2\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Autoconfig RAM Enable (256 MB Zorro III RAM)\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"autoconfig_ram NO\n");
   print_line(&fil,"#autoconfig_ram YES\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Autoconfig RTG Enable (128 MB Zorro III space)\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"autoconfig_rtg NO\n");
   print_line(&fil,"#autoconfig_rtg YES\n");
   print_line(&fil,"\n");
   print_line(&fil,"# CPU RAM Enable (128 MB CPU RAM)\n");
   print_line(&fil,"# IMPORTANT NOTE: disabling CPU RAM, will also disable SCSIBOOT\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"#cpu_ram NO\n");
   print_line(&fil,"cpu_ram YES\n");
   print_line(&fil,"\n");
   print_line(&fil,"# AMIX memory contract (16MB cap, A3000-SCSI emulation, no $08 CPU RAM board)\n");
   print_line(&fil,"# Default when omitted: YES iff bootmode UAE_030_MMU, else NO\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"amix_mode NO\n");
   print_line(&fil,"#amix_mode YES\n");
   print_line(&fil,"#service_cadence 8\n");
   print_line(&fil,"\n");
   print_line(&fil,"# AMIX main-RAM window size in MiB (only meaningful with amix_mode YES)\n");
   print_line(&fil,"# Default 16. Valid 1..128; a larger value would collide with the combo board\n");
   print_line(&fil,"# window at guest $10000000 and is REFUSED (falls back to 16), never truncated.\n");
   print_line(&fil,"#amix_ram 16\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Run-loop overhead switches (diagnostics only; emulation is unaffected).\n");
   print_line(&fil,"# All default YES = the behaviour this firmware has always had. Set NO to buy\n");
   print_line(&fil,"# the ARM time back and measure what the guest gains. See docs/AMIX.md.\n");
   print_line(&fil,"#perf_stats YES     # core1: per-instruction instruction-rate accounting\n");
   print_line(&fil,"#emu_probes YES     # core1: run-loop diagnostic probes (ROMENT/RTE/URTE/USTEP)\n");
   print_line(&fil,"#core0_stats YES    # core0: C0STAT per-task span stamping\n");
   print_line(&fil,"\n");
   print_line(&fil,"# MOUNT SD 0x76 partition\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"#mount_sd_0x76 YES\n");
   print_line(&fil,"mount_sd_0x76 NO\n");
   print_line(&fil,"\n");
   print_line(&fil,"# MOUNT SD ROOT partition\n");
   print_line(&fil,"# (YES or NO, in capitals)\n");
   print_line(&fil,"#mount_sd_root YES\n");
   print_line(&fil,"mount_sd_root NO\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Temperature sensor calibration (THERM)\n");
   print_line(&fil,"# Theoretical value 780 Ohms @ 25 Celsius, but every 060 has a random offset.\n");
   print_line(&fil,"# Resistor R30 (measured from R30 right pad to ground, with 060 cooled to room temperature) in Ohms\n");
   print_line(&fil,"resistor 800.0\n");
   print_line(&fil,"# Room temperature in Celsius\n");
   print_line(&fil,"temperature 27.0\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select Test Amiga CHIP RAM access on start (it uses the ARM, not the CPU)\n");
   print_line(&fil,"enable_test YES\n");
   print_line(&fil,"#enable_test NO\n");
   print_line(&fil,"#enable_test MIN\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select a boot screen resolution (1920x1080, 1280x720 or 800x600)\n");
   print_line(&fil,"bootscreen_resolution 1920x1080\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select if you want a doubled cursor size or not (YES or NO)\n");
   print_line(&fil,"doubled_cursor YES\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select a mac address for Ethernet\n");
   print_line(&fil,"mac_address 00:80:10:00:01:00\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Set the BEEPER time on/off to simulate SCSI activity noise\n");
   print_line(&fil,"bp_ton 0.001\n");
   print_line(&fil,"bp_toff 0.010\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Set the reset sound language directory name (exFAT:/sound/XXX)\n");
   print_line(&fil,"sound_language eng\n");
   print_line(&fil,"#sound_language spa\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Set the selected boot delay (in seconds)\n");
   print_line(&fil,"boot_delay 2\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Set the selected sd clock (25 or 50 (MHz))\n");
   print_line(&fil,"sd_clock 50\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Set the selected monitor switch config (as a sum of values): 1 CTS, 2 SEL, 16 CTS level, 32 SEL level\n");
   print_line(&fil,"monitor_switch 17\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Select a frequency in MHz for the ARM system (667, 767, 867, 900, 933, 967, 1000, 1033, 1067, 1100, 1133, 1167, 1200, 1233, 1267, 1300)\n");
   print_line(&fil,"arm_frequency 667\n");
   print_line(&fil,"\n");
   print_line(&fil,"# Ethernet (YES or NO; a missing line keeps the default shown)\n");
   print_line(&fil,"# re-raise the Amiga interrupt while received frames remain\n");
   print_line(&fil,"eth_reraise YES\n");
   print_line(&fil,"# interrupt the Amiga at once when a frame arrives to an empty queue\n");
   print_line(&fil,"eth_rxkick YES\n");
   print_line(&fil,"# when the receive queue is full, drop only the arriving frame\n");
   print_line(&fil,"eth_taildrop YES\n");
   print_line(&fil,"# wait for a transmit by polling (bounded 1.1 ms) instead of 100 us sleeps\n");
   print_line(&fil,"eth_tx_poll YES\n");
   print_line(&fil,"# advertise 1000BASE-T at OS time\n");
   print_line(&fil,"eth_adv1000 NO\n");
   print_line(&fil,"\n");
   f_close(&fil);

   printf("File %s written OK\n",filename);
}
void trim_whitespace(char *str) {
   //trailing white spaces
  while (strlen(str) != 0 && (str[strlen(str) - 1] == ' ' || str[strlen(str) - 1] == '\t' || str[strlen(str) - 1] == 0x0A || str[strlen(str) - 1] == 0x0D)) {
    str[strlen(str) - 1] = '\0';
  }
}
void get_next_string(char *str, char *str_out, int *strpos, char separator) {
   int str_pos = 0, out_pos = 0, startquote = 0, endstring = 0;

   if (!str_out)
      return;

   if (strpos)
      str_pos = *strpos;

   while ((str[str_pos] == ' ' || str[str_pos] == '\t') && str_pos < (int)strlen(str)) {
      str_pos++;
   }

   if (str[str_pos] == '\"') {
      str_pos++;
      startquote = 1;
   }

   for (int i = str_pos; i < (int)strlen(str); i++) {
      str_out[out_pos] = str[i];

      if (startquote) {
         if (str[i] == '\"')
            endstring = 1;
      } else {
         if ((separator == ' ' && (str[i] == ' ' || str[i] == '\t')) || str[i] == separator) {
            endstring = 1;
         }
      }

      if (endstring) {
         str_out[out_pos] = '\0';
         if (strpos) {
            *strpos = i + 1;
         }
         break;
      }

      out_pos++;
      if (i + 1 == (int)strlen(str) && strpos) {
         *strpos = i + 1;
         str_out[out_pos] = '\0';
      }
   }
}
int get_config_item_type(char *cmd) {
   for (int i = 0; i < CONFITEM_NUM; i++) {
      if (strcmp(cmd, config_item_names[i]) == 0) {
         return i;
      }
   }
   return CONFITEM_NONE;
}
int get_bootmode_type(char *cmd) {
   for (int i = 0; i < BOOTMODE_NUM; i++) {
      if (strcmp(cmd, bootmode_names[i]) == 0) {
         return i;
      }
   }
   if (strcmp(cmd, "UAE") == 0)
      return(UAE_040);
   if (strcmp(cmd, "UAEJIT") == 0)
      return(UAEJIT_040);
   return UAEJIT_040;
}
int get_yesno_type(char *cmd) {
   for (int i = 0; i < YESNO_NUM; i++) {
      if (strcmp(cmd, yesno_names[i]) == 0) {
         return i;
      }
   }
   printf("\e[31m\e[103mERROR YES/NO expected, but got \"%s\"\e[0m\n",cmd);
   return NO;
}
int get_yesnomin_type(char *cmd) {
   for (int i = 0; i < YESNOMIN_NUM; i++) {
      if (strcmp(cmd, yesnomin_names[i]) == 0) {
         return i;
      }
   }
   return NO;
}
int get_resolution_type(char *cmd) {
   for (int i = 0; i < RES_NUM; i++) {
      if (strcmp(cmd, resolution_names[i]) == 0) {
         return i;
      }
   }
   return RES_800x600;
}
int get_arm_frequency_type(char *cmd) {
   for (int i = 0; i < FREQ_NUM; i++) {
      if (strcmp(cmd, arm_frequency_names[i]) == 0) {
         return i;
      }
   }
   return FREQ_667;
}
int get_mac_address_type(char *cmd) {
   if(cmd[2]!=':')
      return(0);
   if(cmd[5]!=':')
      return(0);
   if(cmd[8]!=':')
      return(0);
   if(cmd[11]!=':')
      return(0);
   if(cmd[14]!=':')
      return(0);
   for (int i = 0; i < 6; i++) {
      uint8_t hi=cmd[0+i*3];
      if(hi>='0' && hi<='9') hi-='0';
      if(hi>='a' && hi<='f') hi=-'a'+10;
      if(hi>='F' && hi<='F') hi=-'A'+10;
      uint8_t lo=cmd[1+i*3];
      if(lo>='0' && lo<='9') lo-='0';
      if(lo>='a' && lo<='f') lo=-'a'+10;
      if(lo>='F' && lo<='F') lo=-'A'+10;
      config.mac_address[i]=hi*16+lo;
   }
   return(1);
}
float get_float_type(char *cmd) {
   return atof(cmd);
}
uint32_t hextoi(char *str);
uint32_t get_hex_type(char *cmd) {
   return hextoi(cmd);
}
int get_int_type(char *cmd) {
   return atoi(cmd);
}
uint32_t get_intbytes_type(char *cmd) {
   uint32_t div=1;
   unsigned int i;
   char div_str[3]="", cmd2[30];
//   printf("get_intbytes()\n");
   div_str[0]=cmd[strlen(cmd)-2];
   div_str[1]=cmd[strlen(cmd)-1];
   div_str[2]=0;
//   printf("div_str %s\n",div_str);
   for(i=0;i<strlen(cmd)-2;i++)
      cmd2[i]=cmd[i];
   cmd2[i]=0;
//   printf("cmd2 %s\n",cmd2);
   if(strcmp(div_str,"KB")==0)
   {
      div=1024;
   }
   else if(strcmp(div_str,"MB")==0)
   {
      div=1024*1024;
   }
   else // bytes?
   {
      return atoi(cmd);
   }
   return atoi(cmd2)*div;
}

void read_config_file(int verbose)
{
   char Filename[]=DEFAULT_ROOT "z3660cfg.txt";
   static FIL fil;      /* File object */
   static FATFS fatfs;

   TCHAR *Path = DEFAULT_ROOT;

   Xil_ExceptionDisable();

   int ret;
retry:
   ret=f_clk_mount(&fatfs, Path, 1); // 1 mount immediately
   if(ret!=0)
   {
      if(verbose) printf("Error opening SD media\nRetry in 5 seconds\n");
      sleep(5);
      goto retry;
   }
   ret=f_open(&fil,Filename, FA_OPEN_EXISTING | FA_READ);
   if(ret!=0)
   {
//      if(verbose) printf("Error opening file \"%s\"\nCreating default file...\n",Filename);
//      write_config_file(Filename);
//      ret=f_open(&fil,Filename, FA_OPEN_EXISTING | FA_READ);
//      if(ret!=0)
      {
         if(verbose) printf("Error opening config file %s\nHALT!!!\n",Filename);
         while(1);
      }
   }
   if(verbose) printf("Reading %s file \n",Filename);
   int cur_line = 1;
   char parse_line[512];
   char cur_cmd[128];
   memset(&config, 0x00, sizeof(CONFIG));
//   sprintf(config.kickstart,""); // this produces a warning O_O
   config.boot_mode=UAEJIT_040;
   config.kickstart0[0]=0;
   config.kickstart1[0]=0;
   config.kickstart2[0]=0;
   config.kickstart3[0]=0;
   config.kickstart4[0]=0;
   config.kickstart5[0]=0;
   config.kickstart6[0]=0;
   config.kickstart7[0]=0;
   config.kickstart8[0]=0;
   config.kickstart9[0]=0;
   config.ext_kickstart0[0]=0;
   config.ext_kickstart1[0]=0;
   config.ext_kickstart2[0]=0;
   config.ext_kickstart3[0]=0;
   config.ext_kickstart4[0]=0;
   config.ext_kickstart5[0]=0;
   config.ext_kickstart6[0]=0;
   config.ext_kickstart7[0]=0;
   config.ext_kickstart8[0]=0;
   config.ext_kickstart9[0]=0;
   strcpy(config.sound_language,"eng");
   for(int i=0;i<20;i++)
      config.hdf[i][0]=0;
   for(int i=0;i<20;i++)
      config.adf[i][0]=0;
   for(int i=0;i<7;i++)
      config.scsi_num[i]=-1;
   for(int i=0;i<8;i++)
      config.adf_num[i]=-1;
   config.bootscreen_resolution=RES_800x600;
   config.doubled_cursor=0;
   for(int i=0;i<6;i++)
      config.mac_address[i]=EmacPsMAC[i];

   config.resistor=800.0;
   config.temperature=27.0;

   config.scsiboot=0;
   config.autoconfig_ram=0;
   config.autoconfig_rtg=0;
   config.cpu_ram=1;
   config.amix_mode=-1;
   config.amix_ram=-1;
   config.service_cadence=-1;
   config.perf_stats=Z3660_OVH_UNSET;    /* CYBERSTORM C3: unset -> YES (historical behaviour) */
   config.emu_probes=Z3660_OVH_UNSET;
   config.core0_stats=Z3660_OVH_UNSET;
   config.prof_hz=Z3660_PROF_CFG_UNSET;      /* CYBERSTORM C1: unset -> OFF (no profiler before there was one) */
   config.prof_buckets=Z3660_PROF_CFG_UNSET;
   config.mount_sd_0x76=0;
   config.mount_sd_root=0;
   config.resistor=800.0;
   config.temperature=27.0;
   config.cpufreq=CPUFREQ_MIN;
   config.kickstart=0;
   config.ext_kickstart=0;
   config.enable_test=0;
   config.bp_ton=0;
   config.bp_toff=0;
   config.boot_delay=2;
   config.sd_clock=50;
   config.monitor_switch=0;
   config.arm_frequency=FREQ_667;
   config.eth_reraise=CFG_ETH_DEFAULT_reraise;
   config.eth_rxkick=CFG_ETH_DEFAULT_rxkick;
   config.eth_taildrop=CFG_ETH_DEFAULT_taildrop;
   config.eth_tx_poll=CFG_ETH_DEFAULT_tx_poll;
   config.eth_adv1000=CFG_ETH_DEFAULT_adv1000;

   while (!f_eof(&fil))
   {
      int str_pos = 0;
      memset(parse_line, 0x00, 512);
      f_gets(parse_line, (s32)512, &fil);

      if (strlen(parse_line) <= 2 || parse_line[0] == '#' || parse_line[0] == '/')
         goto skip_line;

      trim_whitespace(parse_line);

      get_next_string(parse_line, cur_cmd, &str_pos, ' ');
      int item=get_config_item_type(cur_cmd);
      switch (item) {
      case CONFITEM_BOOTMODE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.boot_mode=get_bootmode_type(cur_cmd);
         if(verbose) printf("[CFG] Boot mode %s.\n", bootmode_names[config.boot_mode]);
         shared->cfg_emu=config.boot_mode;
         shared->jit_enabled=config.boot_mode==UAEJIT_030 || config.boot_mode==UAEJIT_040?1:0;
         break;

      case CONFITEM_KICKSTART:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.kickstart=get_int_type(cur_cmd);
         if(config.kickstart==0)
         {
            if(verbose) printf("[CFG] Kickstart file selected: mobo kickstart.\n");
         }
         else
         {
            if(verbose) printf("[CFG] Kickstart file selected: number %d.\n", config.kickstart);
         }
         break;

      case CONFITEM_EXT_KICKSTART:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.ext_kickstart=get_int_type(cur_cmd);
         if(config.ext_kickstart==0)
         {
            if(verbose) printf("[CFG] Extended Kickstart file selected: mobo extended kickstart (if any).\n");
         }
         else
         {
            if(verbose) printf("[CFG] Extended Kickstart file selected: number %d.\n", config.ext_kickstart);
         }
         break;

      case CONFITEM_SCSI_BOOT_ENABLE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.scsiboot=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] SCSI Boot %s.\n", yesno_names[config.scsiboot]);
         break;

      case CONFITEM_ENABLE_TEST:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.enable_test=get_yesnomin_type(cur_cmd);
         if(verbose) printf("[CFG] Enable Test %s.\n", yesnomin_names[config.enable_test]);
         break;

      case CONFITEM_SCSI0:
      case CONFITEM_SCSI1:
      case CONFITEM_SCSI2:
      case CONFITEM_SCSI3:
      case CONFITEM_SCSI4:
      case CONFITEM_SCSI5:
      case CONFITEM_SCSI6: {
         int index=item-CONFITEM_SCSI0;
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.scsi_num[index]=get_int_type(cur_cmd);
         if(config.scsi_num[index]>=0)
            if(verbose) printf("[CFG] SCSI%d assigned to %s\n",index,config.hdf[config.scsi_num[index]]);
         break;
      }
      case CONFITEM_ADF0:
      case CONFITEM_ADF1:
      case CONFITEM_ADF2:
      case CONFITEM_ADF3:
      case CONFITEM_ADF4:
      case CONFITEM_ADF5:
      case CONFITEM_ADF6:
      case CONFITEM_ADF7: {
         int index=item-CONFITEM_ADF0;
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.adf_num[index]=get_int_type(cur_cmd);
         if(config.adf_num[index]>=0)
            if(verbose) printf("[CFG] ADF%d assigned to %s\n",index,config.adf[config.adf_num[index]]);
         break;
      }
      case CONFITEM_HDF0:
      case CONFITEM_HDF1:
      case CONFITEM_HDF2:
      case CONFITEM_HDF3:
      case CONFITEM_HDF4:
      case CONFITEM_HDF5:
      case CONFITEM_HDF6:
      case CONFITEM_HDF7:
      case CONFITEM_HDF8:
      case CONFITEM_HDF9:
      case CONFITEM_HDF10:
      case CONFITEM_HDF11:
      case CONFITEM_HDF12:
      case CONFITEM_HDF13:
      case CONFITEM_HDF14:
      case CONFITEM_HDF15:
      case CONFITEM_HDF16:
      case CONFITEM_HDF17:
      case CONFITEM_HDF18:
      case CONFITEM_HDF19: {
         int index=item-CONFITEM_HDF0;
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         sprintf(config.hdf[index],"%s%s", DEFAULT_ROOT, cur_cmd);
         if(verbose) printf("[CFG] hdf%d file %s\n", index, config.hdf[index]);
         break;
      }
      case CONFITEM_ISO0:
      case CONFITEM_ISO1:
      case CONFITEM_ISO2:
      case CONFITEM_ISO3:
      case CONFITEM_ISO4:
      case CONFITEM_ISO5:
      case CONFITEM_ISO6:
      case CONFITEM_ISO7:
      case CONFITEM_ISO8:
      case CONFITEM_ISO9:
      case CONFITEM_ISO10:
      case CONFITEM_ISO11:
      case CONFITEM_ISO12:
      case CONFITEM_ISO13:
      case CONFITEM_ISO14:
      case CONFITEM_ISO15:
      case CONFITEM_ISO16:
      case CONFITEM_ISO17:
      case CONFITEM_ISO18:
      case CONFITEM_ISO19: {
         int index=item-CONFITEM_ISO0;
         if(index>=7){ // cd_target[] is 7 deep: iso7..iso19 would write past it into adf_num/hdf[0]
            printf("[CFG] iso%d ignored -- CD-capable slots are iso0..iso6 (cd_target[7])\n", index);
            break;
         }
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         sprintf(config.hdf[index],"%s%s", DEFAULT_ROOT, cur_cmd);
         if(verbose) printf("[CFG] iso%d file %s\n", index, config.hdf[index]);
         config.cd_target[index]=1;
         break;
      }
      case CONFITEM_ADF_FILE0:
      case CONFITEM_ADF_FILE1:
      case CONFITEM_ADF_FILE2:
      case CONFITEM_ADF_FILE3:
      case CONFITEM_ADF_FILE4:
      case CONFITEM_ADF_FILE5:
      case CONFITEM_ADF_FILE6:
      case CONFITEM_ADF_FILE7:
      case CONFITEM_ADF_FILE8:
      case CONFITEM_ADF_FILE9:
      case CONFITEM_ADF_FILE10:
      case CONFITEM_ADF_FILE11:
      case CONFITEM_ADF_FILE12:
      case CONFITEM_ADF_FILE13:
      case CONFITEM_ADF_FILE14:
      case CONFITEM_ADF_FILE15:
      case CONFITEM_ADF_FILE16:
      case CONFITEM_ADF_FILE17:
      case CONFITEM_ADF_FILE18:
      case CONFITEM_ADF_FILE19: {
         int index=item-CONFITEM_ADF_FILE0;
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         sprintf(config.adf[index],"%s%s", DEFAULT_ROOT, cur_cmd);
         if(verbose) printf("[CFG] adf_file%d file %s\n", index, config.adf[index]);
         break;
      }

      case CONFITEM_AUTOCONFIG_RAM_ENABLE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.autoconfig_ram=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] AutoConfig RAM %s.\n", yesno_names[config.autoconfig_ram]);
         break;

      case CONFITEM_AUTOCONFIG_RTG_ENABLE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.autoconfig_rtg=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] AutoConfig RTG %s.\n", yesno_names[config.autoconfig_rtg]);
         break;

      case CONFITEM_CPU_RAM_ENABLE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.cpu_ram=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] CPU RAM %s.\n", yesno_names[config.cpu_ram]);
         break;

      case CONFITEM_AMIX_MODE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.amix_mode=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] AMIX mode %s.\n", yesno_names[config.amix_mode]);
         break;

      case CONFITEM_SERVICE_CADENCE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.service_cadence=get_int_type(cur_cmd);
         if(verbose) printf("[CFG] service_cadence %d\n", config.service_cadence);
         break;

      /* CYBERSTORM C3 run-loop overhead switches. All three default to YES = what the
       * firmware has always done, so a config file without these lines is unchanged.
       * They exist so a metal calibration session can subtract one overhead at a time
       * and measure what the guest actually gains. See docs/AMIX.md. */
      case CONFITEM_PERF_STATS:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.perf_stats=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] perf_stats %s.\n", yesno_names[config.perf_stats]);
         break;

      case CONFITEM_EMU_PROBES:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.emu_probes=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] emu_probes %s.\n", yesno_names[config.emu_probes]);
         break;

      case CONFITEM_CORE0_STATS:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.core0_stats=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] core0_stats %s.\n", yesno_names[config.core0_stats]);
         break;

      /* CYBERSTORM C1 profiler arming. Both default to OFF -- see prof_config.h for why
       * that is the SAME "an absent key changes nothing" rule as the C3 switches above
       * and not its opposite. These exist for the one profile the serial console cannot
       * take: the boot itself. Config file only, not per preset. */
      case CONFITEM_PROF_HZ:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.prof_hz=get_int_type(cur_cmd);
         if(verbose) printf("[CFG] prof_hz %d\n", config.prof_hz);
         break;

      case CONFITEM_PROF_BUCKETS:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.prof_buckets=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] prof_buckets %s.\n", yesno_names[config.prof_buckets]);
         break;

      case CONFITEM_AMIX_RAM:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.amix_ram=get_int_type(cur_cmd);
         /* Validation is deferred to resolve_amix_ram(), which runs once the whole
          * file is parsed -- the advisory text depends on amix_mode and boot_mode,
          * and those lines may come after this one. */
         break;

      case CONFITEM_MOUNT_SD_0x76:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.mount_sd_0x76=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] MOUNT SD 0x76 %s.\n", yesno_names[config.mount_sd_0x76]);
         break;

      case CONFITEM_MOUNT_SD_ROOT:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.mount_sd_root=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] MOUNT SD ROOT %s.\n", yesno_names[config.mount_sd_root]);
         break;

      case CONFITEM_RESISTOR:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.resistor=get_float_type(cur_cmd);
         if(verbose) printf("[CFG] Calibrated Resistor %.1f.\n", config.resistor);
         break;

      case CONFITEM_TEMPERATURE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.temperature=get_float_type(cur_cmd);
         if(verbose) printf("[CFG] Calibrated Temperature %.1f.\n", config.temperature);
         break;

      case CONFITEM_CPUFREQ:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.cpufreq=(get_int_type(cur_cmd)/5)*5; // 5 MHz steps
         if(verbose) printf("[CFG] 060 CPU Frequency %d MHz\n", config.cpufreq);
         break;

      case CONFITEM_KICKSTART0:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         if(verbose) printf("[CFG] WARNING!!! Kickstart file 0 is reserved to internal kickstart!!!.\n");
         break;

#define CASE_CONFITEM_KICKSTART(x)  case CONFITEM_KICKSTART ## x:\
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');\
         sprintf(config.kickstart ## x,"%s%s", DEFAULT_ROOT, cur_cmd);\
         if(verbose) printf("[CFG] kickstart%d file %s\n", x, config.kickstart ## x);\
         break;
      CASE_CONFITEM_KICKSTART(1)
      CASE_CONFITEM_KICKSTART(2)
      CASE_CONFITEM_KICKSTART(3)
      CASE_CONFITEM_KICKSTART(4)
      CASE_CONFITEM_KICKSTART(5)
      CASE_CONFITEM_KICKSTART(6)
      CASE_CONFITEM_KICKSTART(7)
      CASE_CONFITEM_KICKSTART(8)
      CASE_CONFITEM_KICKSTART(9)

      case CONFITEM_EXT_KICKSTART0:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         if(verbose) printf("[CFG] WARNING!!! Extended Kickstart file 0 is reserved to internal extended kickstart!!!.\n");
         break;

#define CASE_CONFITEM_EXT_KICKSTART(x)  case CONFITEM_EXT_KICKSTART ## x:\
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');\
         sprintf(config.ext_kickstart ## x,"%s%s", DEFAULT_ROOT, cur_cmd);\
         if(verbose) printf("[CFG] Extended kickstart%d file %s\n", x, config.ext_kickstart ## x);\
         break;
      CASE_CONFITEM_EXT_KICKSTART(1)
      CASE_CONFITEM_EXT_KICKSTART(2)
      CASE_CONFITEM_EXT_KICKSTART(3)
      CASE_CONFITEM_EXT_KICKSTART(4)
      CASE_CONFITEM_EXT_KICKSTART(5)
      CASE_CONFITEM_EXT_KICKSTART(6)
      CASE_CONFITEM_EXT_KICKSTART(7)
      CASE_CONFITEM_EXT_KICKSTART(8)
      CASE_CONFITEM_EXT_KICKSTART(9)

      case CONFITEM_SOUND_LANGUAGE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         sprintf(config.sound_language,"%s", cur_cmd);
         if(verbose) printf("[CFG] Sound Language: %s\n",config.sound_language);
         break;

      case CONFITEM_BOOTSCREEN_RESOLUTION:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.bootscreen_resolution=get_resolution_type(cur_cmd);
         if(verbose) printf("[CFG] Boot Screen Resolution %s\n", resolution_names[config.bootscreen_resolution]);
         break;

      case CONFITEM_DOUBLED_CURSOR:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.doubled_cursor=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] Doubled cursor %s\n", yesno_names[config.doubled_cursor]);
         break;

      case CONFITEM_MAC_ADDRESS:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         if(get_mac_address_type(cur_cmd)) {
            if(verbose) printf("[CFG] Mac Address %02X:%02X:%02X:%02X:%02X:%02X\n", config.mac_address[0],
                  config.mac_address[1],config.mac_address[2],
                  config.mac_address[3],config.mac_address[4],config.mac_address[5]);
            for(int i=0;i<6;i++)
               EmacPsMAC[i]=config.mac_address[i];
         }
         else
            if(verbose) printf("[CFG] BAD Mac Address %s\n",cur_cmd);
         break;
      case CONFITEM_BP_TON:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.bp_ton=get_float_type(cur_cmd);
         if(verbose) printf("[CFG] Beeper Ton %f\n", config.bp_ton);
         break;
      case CONFITEM_BP_TOFF:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.bp_toff=get_float_type(cur_cmd);
         if(verbose) printf("[CFG] Beeper Toff %f\n", config.bp_toff);
         break;
      case CONFITEM_BOOT_DELAY:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.boot_delay=get_int_type(cur_cmd);
         if(config.boot_delay<2) config.boot_delay=2;
         if(verbose) printf("[CFG] Boot Delay %d seconds\n", config.boot_delay);
         break;

      case CONFITEM_SD_CLOCK:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.sd_clock=get_int_type(cur_cmd);
         if(config.sd_clock<50)
            config.sd_clock=25;
         else
            config.sd_clock=50;
         if(verbose) printf("[CFG] SD Clock %d MHz\n", config.sd_clock);
         break;

      case CONFITEM_MONITOR_SWITCH:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.monitor_switch=get_int_type(cur_cmd);
         if(verbose) printf("[CFG] Monitor Switch 0x%02X\n", config.monitor_switch);
         break;

      case CONFITEM_TEST_RANGE0:
      case CONFITEM_TEST_RANGE1:
      case CONFITEM_TEST_RANGE2:
      case CONFITEM_TEST_RANGE3:
      case CONFITEM_TEST_RANGE4:
      case CONFITEM_TEST_RANGE5:
      case CONFITEM_TEST_RANGE6:
      case CONFITEM_TEST_RANGE7: {
         int index=item-CONFITEM_TEST_RANGE0;
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.test_range[index].start=get_hex_type(cur_cmd);
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.test_range[index].length=get_intbytes_type(cur_cmd);
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         strcpy(config.test_range[index].name,cur_cmd);
         // minimum unit 1 KB
         if(config.test_range[index].length<1024)
            config.test_range[index].length=1024;
//         if(config.test_range[index].length>=0) // always true
         {
            // Round to KB
            config.test_range[index].length&=~1023L;
            char div_str[3]="KB";
            int div=1024;
            if( config.test_range[index].length>1024*1024 &&   // if > 1MB
               (config.test_range[index].length&(1024*1024L-1))==0) // and has not remaining KBs
            {
               strcpy(div_str,"MB");
               div=1024*1024;
            }
            if(verbose) printf("[CFG] Test Range %d: start 0x%08lX, length %ld %s (\"%s\")\n",index,
                  config.test_range[index].start,
                  config.test_range[index].length/div,div_str,
                  config.test_range[index].name);
         }
         break;
      }

      case CONFITEM_ARM_FREQUENCY:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.arm_frequency=get_arm_frequency_type(cur_cmd);
         if(verbose) printf("[CFG] ARM Frequency %s Mhz\n", arm_frequency_names[config.arm_frequency]);
         break;

      case CONFITEM_ETH_RERAISE:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.eth_reraise=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] eth_reraise %s.\n", yesno_names[config.eth_reraise]);
         break;

      case CONFITEM_ETH_RXKICK:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.eth_rxkick=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] eth_rxkick %s.\n", yesno_names[config.eth_rxkick]);
         break;

      case CONFITEM_ETH_TAILDROP:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.eth_taildrop=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] eth_taildrop %s.\n", yesno_names[config.eth_taildrop]);
         break;

      case CONFITEM_ETH_TX_POLL:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.eth_tx_poll=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] eth_tx_poll %s.\n", yesno_names[config.eth_tx_poll]);
         break;

      case CONFITEM_ETH_ADV1000:
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         config.eth_adv1000=get_yesno_type(cur_cmd);
         if(verbose) printf("[CFG] eth_adv1000 %s.\n", yesno_names[config.eth_adv1000]);
         break;

      case CONFITEM_NONE:
      default:
         if(verbose) printf("[CFG] Unknown config item %s on line %d.\n", cur_cmd, cur_line);
         break;
      }

      skip_line:
      cur_line++;
   }
   goto load_successful;
// this will never be executed
//load_failed:;
   if(verbose) printf("Error loading config file %s\n",Filename);
   if(verbose) printf("Loading default config\n");
   load_default_config();
load_successful:;

   f_close(&fil);
   f_umount(Path);
   if(verbose) printf("Config file read OK\n");

   // default_config is the preset template: keep amix_mode as the parsed value (incl.
   // -1 = "unset") so each preset resolves its own default against ITS own boot_mode
   // (in read_env_files). Resolve the ACTIVE config (no-preset path) separately below.
   memcpy(&default_config,&config,sizeof(CONFIG));

   // no-preset path: resolve the amix_mode default (-1 = unset) now that the whole
   // config file is parsed, so config.amix_mode is always 0/1 by the time anything
   // (here, the shared field below, or downstream) reads it. Default: AMIX iff UAE_030_MMU.
   if(config.amix_mode==-1) config.amix_mode=(config.boot_mode==UAE_030_MMU)?1:0; // default: AMIX iff UAE_030_MMU
   if(config.amix_mode) config.cpu_ram=0;   // AMIX forces a single contiguous window (no $08 CPU RAM board)
   shared->amix_mode=config.amix_mode;
   resolve_amix_ram(verbose);               /* AMIX RAM window size: default, bounds, loud refusal */
   if(config.service_cadence<1) config.service_cadence=1;   /* default/clamp: 1 = poll every instruction */
   shared->service_cadence=config.service_cadence;
   resolve_overhead_switches(verbose);      /* perf_stats / emu_probes / core0_stats */
   resolve_prof_config(verbose);            /* CYBERSTORM C1: prof_hz / prof_buckets */
   publish_warp_default();                  /* WARP ENGINE: never armed at boot */

   memcpy(&temp_config,&config,sizeof(CONFIG));

   for(int i=0;i<8;i++)
   {
#define COPY_DEFAULT(A) env_file_vars_temp[i].A=default_config.A
      COPY_DEFAULT(boot_mode);
      COPY_DEFAULT(scsiboot);
      COPY_DEFAULT(autoconfig_ram);
      COPY_DEFAULT(autoconfig_rtg);
      COPY_DEFAULT(cpu_ram);
      COPY_DEFAULT(amix_mode);
      COPY_DEFAULT(amix_ram);
      COPY_DEFAULT(service_cadence);
      COPY_DEFAULT(perf_stats);
      COPY_DEFAULT(emu_probes);
      COPY_DEFAULT(core0_stats);
      COPY_DEFAULT(mount_sd_0x76);
      COPY_DEFAULT(mount_sd_root);
      COPY_DEFAULT(kickstart);
      COPY_DEFAULT(ext_kickstart);
      COPY_DEFAULT(enable_test);
      COPY_DEFAULT(cpufreq);
      for(int j=0;j<7;j++)
         COPY_DEFAULT(scsi_num[j]);
      for(int j=0;j<8;j++)
         COPY_DEFAULT(adf_num[j]);
      for(int j=0;j<6;j++)
         COPY_DEFAULT(mac_address[j]);
      COPY_DEFAULT(bp_ton);
      COPY_DEFAULT(bp_toff);
      env_file_vars_temp[i].preset_name[0]=0;
      COPY_DEFAULT(monitor_switch);
      COPY_DEFAULT(arm_frequency);
   }

   Xil_ExceptionEnable();
}
void read_env_files(int verbose)
{
   static FIL fil;      /* File object */
   static FATFS fatfs;

   TCHAR *Path = DEFAULT_ROOT;

   Xil_ExceptionDisable();

   int ret;
retry:
   ret=f_clk_mount(&fatfs, Path, 1); // 1 mount immediately
   if(ret!=0)
   {
      if(verbose) printf("Error opening SD media\nRetry in 5 seconds\n");
      sleep(5);
      goto retry;
   }
   int cur_line = 1;
   char parse_line[512];
   char cur_cmd[128];

//   for(int preset=0;preset<8;preset++)
//      memset(&env_file_vars_temp[preset], 0x00, sizeof(ENV_FILE_VARS));

   ret=f_open(&fil,DEFAULT_ROOT "presets/preset.txt", FA_OPEN_EXISTING | FA_READ);
   if(ret==0)
   {
      preset_selected=-1;
      while (!f_eof(&fil))
      {
         int str_pos = 0;
         memset(parse_line, 0x00, 512);
         f_gets(parse_line, (s32)512, &fil);
         if (strlen(parse_line) <= 2 || parse_line[0] == '#' || parse_line[0] == '/')
            goto skip_line_preset;
         trim_whitespace(parse_line);
         get_next_string(parse_line, cur_cmd, &str_pos, ' ');
         if (strcmp(cur_cmd, "preset") == 0) {
            get_next_string(parse_line, cur_cmd, &str_pos, ' ');
            preset_selected=get_int_type(cur_cmd);
            if(preset_selected>=0 && preset_selected<PRESET_CB_MAX-1)
            {
               if(verbose) printf("[ENV] Preset %d selected\n",preset_selected);
            }
            else
            {
               preset_selected=PRESET_CB_MAX-1;
               if(verbose) printf("[ENV] Default preset selected (z3660cfg.txt file)\n");
            }
            break;
         }
         skip_line_preset:
         cur_line++;
         if(preset_selected==-1)
         {
            preset_selected=PRESET_CB_MAX-1;
            if(verbose) printf("[ENV] WARNING!!! preset NOT selected/loaded\n");
            f_close(&fil);
            return;
         }
      }
   }
   else
   {
      if(verbose) printf("[ENV] Warning!!! Can't open presets/preset.txt file\n");
       return;
   }
   char text[50];
   // Try to read all files
   for(int preset=0;preset<PRESET_CB_MAX-1;preset++)
   {
      cur_line = 1;
      sprintf(text,DEFAULT_ROOT "presets/preset%d.txt",preset);

      ret=f_open(&fil,text, FA_OPEN_EXISTING | FA_READ);
      if(ret==0)
      {
         memset(parse_line, 0x00, 512);
         f_gets(parse_line, (s32)512, &fil);
         if(parse_line[0] == '#')
         {
            int str_pos = 1;
            trim_whitespace(parse_line);
            get_next_string(parse_line, cur_cmd, &str_pos, ' ');
            char name[50];
            if (strcmp(cur_cmd,"preset_name") == 0)
            {
               get_next_string(parse_line, cur_cmd, &str_pos, ' ');
               strcpy(name,cur_cmd);
               if(strcmp(name,"")==0)
                  sprintf(name,"preset%d default name",preset);
            }
            else
            {
               sprintf(name,"preset%d default name",preset);
            }
            strcpy(env_file_vars_temp[preset].preset_name,name);
            goto skip_line;
         }
         while (!f_eof(&fil))
         {
            int str_pos = 0;
            memset(parse_line, 0x00, 512);
            f_gets(parse_line, (s32)512, &fil);
            if (strlen(parse_line) <= 2 || parse_line[0] == '#' || parse_line[0] == '/')
               goto skip_line;

            trim_whitespace(parse_line);

            get_next_string(parse_line, cur_cmd, &str_pos, ' ');
            int item=get_config_item_type(cur_cmd);

            switch (item) {
               case CONFITEM_BOOTMODE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].boot_mode=get_bootmode_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Boot mode %s.\e[0m\n", bootmode_names[env_file_vars_temp[preset].boot_mode]);
                  break;
               case CONFITEM_KICKSTART:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].kickstart=get_int_type(cur_cmd);
                  if(preset==preset_selected)
                  {
                     if(env_file_vars_temp[preset].kickstart==0)
                     {
                        if(verbose) printf("\e[30m\e[103m[ENV] Kickstart file selected: mobo kickstart.\e[0m\n");
                     }
                     else
                     {
                        if(verbose) printf("\e[30m\e[103m[ENV] kickstart selected: number %d\e[0m\n", env_file_vars_temp[preset].kickstart);
                     }
                  }
                  break;
               case CONFITEM_EXT_KICKSTART:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].ext_kickstart=get_int_type(cur_cmd);
                  if(preset==preset_selected)
                  {
                     if(env_file_vars_temp[preset].ext_kickstart==0)
                     {
                        if(verbose) printf("\e[30m\e[103m[ENV] Extended kickstart selected: mobo extended kickstart (if any).\e[0m\n");
                     }
                     else
                     {
                        if(verbose) printf("\e[30m\e[103m[ENV] Extended kickstart selected: number %d\e[0m\n", env_file_vars_temp[preset].ext_kickstart);
                     }
                  }
                  break;
               case CONFITEM_SCSI_BOOT_ENABLE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].scsiboot=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] SCSI Boot %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].scsiboot]);
                  break;
               case CONFITEM_ENABLE_TEST:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].enable_test=get_yesnomin_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Enable Test %s.\e[0m\n", yesnomin_names[env_file_vars_temp[preset].enable_test]);
                  break;
               case CONFITEM_SCSI0:
               case CONFITEM_SCSI1:
               case CONFITEM_SCSI2:
               case CONFITEM_SCSI3:
               case CONFITEM_SCSI4:
               case CONFITEM_SCSI5:
               case CONFITEM_SCSI6: {
                  int index=item-CONFITEM_SCSI0;
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  int num=get_int_type(cur_cmd);
                  if(num>=-1 && num<=19)
                     env_file_vars_temp[preset].scsi_num[index]=num;
                  else
                     if(verbose) printf("\e[31m\e[103mERROR SCSI%d assigned to %d\e[0m\n", index,num);
                  if(preset==preset_selected)
                  {
                     if(num>=0 && num<=19)
                        if(verbose) printf("\e[30m\e[103m[ENV] SCSI%d assigned to %s.\e[0m\n", index,config.hdf[env_file_vars_temp[preset].scsi_num[index]]);
                  }
                  break;
               }
               case CONFITEM_ADF0:
               case CONFITEM_ADF1:
               case CONFITEM_ADF2:
               case CONFITEM_ADF3:
               case CONFITEM_ADF4:
               case CONFITEM_ADF5:
               case CONFITEM_ADF6:
               case CONFITEM_ADF7: {
                  int index=item-CONFITEM_ADF0;
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  int num=get_int_type(cur_cmd);
                  if(num>=-1 && num<=19)
                     env_file_vars_temp[preset].adf_num[index]=num;
                  else
                     if(verbose) printf("\e[31m\e[103mERROR ADF%d assigned to %d\e[0m\n", index,num);
                  if(preset==preset_selected)
                  {
                     if(num>=0 && num<=19)
                        if(verbose) printf("\e[30m\e[103m[ENV] ADF%d assigned to %s.\e[0m\n", index,config.adf[env_file_vars_temp[preset].adf_num[index]]);
                  }
                  break;
               }
               case CONFITEM_AUTOCONFIG_RAM_ENABLE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].autoconfig_ram=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                  {
                     if(verbose) printf("\e[30m\e[103m[ENV] AutoConfig Ram %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].autoconfig_ram]);
                  }
                  break;
               case CONFITEM_AUTOCONFIG_RTG_ENABLE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].autoconfig_rtg=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                  {
                     if(verbose) printf("\e[30m\e[103m[ENV] AutoConfig RTG %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].autoconfig_rtg]);
                  }
                  break;
               case CONFITEM_CPU_RAM_ENABLE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].cpu_ram=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] CPU Ram %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].cpu_ram]);
                  break;
               case CONFITEM_AMIX_MODE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].amix_mode=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] AMIX mode %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].amix_mode]);
                  break;
               case CONFITEM_AMIX_RAM:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].amix_ram=get_int_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] amix_ram %d MiB\e[0m\n", env_file_vars_temp[preset].amix_ram);
                  /* Bounds/refusal happen in resolve_amix_ram() once the preset is applied. */
                  break;
               case CONFITEM_SERVICE_CADENCE:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].service_cadence=get_int_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] service_cadence %d\e[0m\n", env_file_vars_temp[preset].service_cadence);
                  break;
               /* CYBERSTORM C3 overhead switches, per preset. Absent -> the -1 inherited
                * from default_config -> YES in resolve_overhead_switches(). */
               case CONFITEM_PERF_STATS:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].perf_stats=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] perf_stats %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].perf_stats]);
                  break;
               case CONFITEM_EMU_PROBES:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].emu_probes=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] emu_probes %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].emu_probes]);
                  break;
               case CONFITEM_CORE0_STATS:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].core0_stats=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] core0_stats %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].core0_stats]);
                  break;
               case CONFITEM_MOUNT_SD_0x76:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].mount_sd_0x76=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] MOUNT SD 0x76 %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].mount_sd_0x76]);
                  break;
               case CONFITEM_MOUNT_SD_ROOT:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].mount_sd_root=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] MOUNT SD ROOT %s.\e[0m\n", yesno_names[env_file_vars_temp[preset].mount_sd_root]);
                  break;
               case CONFITEM_CPUFREQ:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].cpufreq=(get_int_type(cur_cmd)/5)*5; // 5 MHz steps
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] 060 CPU Frequency %d MHz\e[0m\n", env_file_vars_temp[preset].cpufreq);
                  break;
               case CONFITEM_BOOTSCREEN_RESOLUTION:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].bootscreen_resolution=get_resolution_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Boot Screen Resolution %s\e[0m\n", resolution_names[env_file_vars_temp[preset].bootscreen_resolution]);
                  break;
               case CONFITEM_DOUBLED_CURSOR:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].doubled_cursor=get_yesno_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Doubled Cursor %s\e[0m\n", yesno_names[env_file_vars_temp[preset].doubled_cursor]);
                  break;
               case CONFITEM_MAC_ADDRESS:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  if(get_mac_address_type(cur_cmd)) {
                     for(int i=0;i<6;i++)
                        EmacPsMAC[i]=env_file_vars_temp[preset].mac_address[i]=config.mac_address[i];
                     if(preset==preset_selected)
                        if(verbose) printf("\e[30m\e[103m[ENV] Mac Address %02X:%02X:%02X:%02X:%02X:%02X\e[0m\n", config.mac_address[0],
                        config.mac_address[1],config.mac_address[2],
                        config.mac_address[3],config.mac_address[4],config.mac_address[5]);
                  }
                  else
                  {
                     if(preset==preset_selected)
                        if(verbose) printf("\e[30m\e[103m[ENV] BAD Mac Address %s\e[0m\n",cur_cmd);
                  }
                  break;
               case CONFITEM_BP_TON:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].bp_ton=get_float_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Beeper Ton %f\e[0m\n", env_file_vars_temp[preset].bp_ton);
                  break;
               case CONFITEM_BP_TOFF:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].bp_toff=get_float_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Beeper Toff %f\e[0m\n", env_file_vars_temp[preset].bp_toff);
                  break;
               case CONFITEM_MONITOR_SWITCH:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].monitor_switch=get_int_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] Monitor Switch 0x%02X\e[0m\n", env_file_vars_temp[preset].monitor_switch);
                  break;
               case CONFITEM_ARM_FREQUENCY:
                  get_next_string(parse_line, cur_cmd, &str_pos, ' ');
                  env_file_vars_temp[preset].arm_frequency=get_arm_frequency_type(cur_cmd);
                  if(preset==preset_selected)
                     if(verbose) printf("\e[30m\e[103m[ENV] ARM Frequency %s MHz\e[0m\n", arm_frequency_names[env_file_vars_temp[preset].arm_frequency]);
                  break;
               case CONFITEM_NONE:
               default:
                  if(verbose) printf("\e[30m\e[103m[ENV] Unknown config item %s on line %d. (file %s)\e[0m\n", cur_cmd, cur_line,text);
                  break;
            }

            skip_line:
            cur_line++;
         }
      }
      f_close(&fil);
   }
   // Finally, set the presets we are going to use... (preset_selected)
   if(preset_selected<PRESET_CB_MAX-1)
   {
      config.boot_mode=env_file_vars_temp[preset_selected].boot_mode;
      shared->cfg_emu=config.boot_mode;
      shared->jit_enabled=config.boot_mode==UAEJIT_030 || config.boot_mode==UAEJIT_040?1:0;
      config.kickstart=env_file_vars_temp[preset_selected].kickstart;
      config.ext_kickstart=env_file_vars_temp[preset_selected].ext_kickstart;
      config.scsiboot=env_file_vars_temp[preset_selected].scsiboot;
      config.enable_test=env_file_vars_temp[preset_selected].enable_test;
      for(int i=0;i<7;i++)
         config.scsi_num[i]=env_file_vars_temp[preset_selected].scsi_num[i];
      for(int i=0;i<8;i++)
         config.adf_num[i]=env_file_vars_temp[preset_selected].adf_num[i];
      config.autoconfig_ram=env_file_vars_temp[preset_selected].autoconfig_ram;
      config.autoconfig_rtg=env_file_vars_temp[preset_selected].autoconfig_rtg;
      config.cpu_ram=env_file_vars_temp[preset_selected].cpu_ram;
      config.amix_mode=env_file_vars_temp[preset_selected].amix_mode;
      config.amix_ram=env_file_vars_temp[preset_selected].amix_ram;
      config.service_cadence=env_file_vars_temp[preset_selected].service_cadence;
      config.perf_stats=env_file_vars_temp[preset_selected].perf_stats;
      config.emu_probes=env_file_vars_temp[preset_selected].emu_probes;
      config.core0_stats=env_file_vars_temp[preset_selected].core0_stats;
      config.mount_sd_0x76=env_file_vars_temp[preset_selected].mount_sd_0x76;
      config.mount_sd_root=env_file_vars_temp[preset_selected].mount_sd_root;
      config.cpufreq=env_file_vars_temp[preset_selected].cpufreq;
      config.bootscreen_resolution=env_file_vars_temp[preset_selected].bootscreen_resolution;
      config.doubled_cursor=env_file_vars_temp[preset_selected].doubled_cursor;
      for(int i=0;i<6;i++)
         EmacPsMAC[i]=config.mac_address[i]=env_file_vars_temp[preset_selected].mac_address[i];
      config.bp_ton=env_file_vars_temp[preset_selected].bp_ton;
      config.bp_toff=env_file_vars_temp[preset_selected].bp_toff;
      config.monitor_switch=env_file_vars_temp[preset_selected].monitor_switch;
      config.arm_frequency=env_file_vars_temp[preset_selected].arm_frequency;
      // AMIX (UAE_030_MMU): force cpu_ram OFF so scsiboot_rom_loaded stays 0 (main.c:1430-1442):
      // no emulated PISCSI Z2 autoboot ROM (its driver needs $08000000 and stalled the AMIX boot
      // at the RDB - "[PISCSI] DRIVER ... not mapped in FPGA RAM"). AMIX instead autoboots via the
      // a3k204 ROM scsi.device from the A3000 SCSI @ $00DD0000 (a3000_scsi, gated only on enable_mmu).
      // scsiboot itself stays as the preset set it, so piscsi_init() still maps devs[6]/Amix.hdf for
      // the a3000_scsi cross-core backend. (This comment used to say the emulator presents NO RAM at
      // $08000000 and that AMIX runs from a3000mem @ $07000000 -- that describes an OLD map. It is
      // the other way round now: the AMIX window IS $08000000 (drct_bank, amix_ram MiB wide) and
      // a3000mem is vestigial. What is unchanged is WHY: exactly one contiguous region in the
      // memlist, nothing probeable adjacent to it, so no vatosde() coalesce. See amix_ram.h.)
      // amix_mode decouples the AMIX memory contract from UAE_030_MMU (the PMMU boot mode). Default
      // (amix_mode==-1, i.e. no amix_mode line in the preset): AMIX iff UAE_030_MMU, so every existing
      // preset stays byte-identical in behaviour. Only when amix_mode is ON do we force cpu_ram OFF.
      if(config.amix_mode==-1) config.amix_mode=(config.boot_mode==UAE_030_MMU)?1:0; // default: AMIX iff UAE_030_MMU
      if(config.amix_mode) config.cpu_ram=0;   // AMIX forces a single contiguous window (no $08 CPU RAM board)
      shared->amix_mode=config.amix_mode;
      resolve_amix_ram(verbose);            /* AMIX RAM window size: default, bounds, loud refusal */
      if(config.service_cadence<1) config.service_cadence=1;   /* default/clamp: 1 = poll every instruction */
      shared->service_cadence=config.service_cadence;
      resolve_overhead_switches(verbose);   /* perf_stats / emu_probes / core0_stats */
      resolve_prof_config(verbose);         /* CYBERSTORM C1: prof_hz / prof_buckets */
      publish_warp_default();               /* WARP ENGINE: never armed at boot */
      memcpy(&temp_config,&config,sizeof(CONFIG));
   }

   f_umount(Path);
   Xil_ExceptionEnable();
}
int write_env_files(ENV_FILE_VARS *env_file)
{
   static FIL fil;      /* File object */
   static FATFS fatfs;
   TCHAR text[50];
   TCHAR *Path = DEFAULT_ROOT;
   delete_env_files();
   Xil_ExceptionDisable();

   int ret;
retry:
   ret=f_clk_mount(&fatfs, Path, 1); // 1 mount immediately
   if(ret!=0)
   {
      printf("Error opening SD media\nRetry in 5 seconds\n");
      sleep(5);
      goto retry;
   }
   sprintf(text,DEFAULT_ROOT "presets");
   ret=f_stat(text,NULL);
   if(ret==FR_NO_FILE)
   {
      if(f_mkdir(text)==FR_OK)
         printf("Created directory %s\n",text);
      else
      {
         printf("Can't create directory %s\n",text);
         Xil_ExceptionEnable();
         return(0);
      }
   }
   sprintf(text,DEFAULT_ROOT "presets/preset.txt");
   ret=f_open(&fil,text, FA_CREATE_ALWAYS | FA_WRITE);
   if(ret==FR_OK)
   {
      f_printf(&fil,"preset %d\n",preset_selected);
      f_close(&fil);
   }
   else
   {
      printf("[Config] ERROR Write file presets/preset.txt\n");
      Xil_ExceptionEnable();
      return(0);
   }
   if(preset_selected<PRESET_CB_MAX-1) // if default preset selected, don't write the preset
   {
      sprintf(text,DEFAULT_ROOT "presets/preset%d.txt",preset_selected);
      printf("writing to file %s name %s\n",text,env_file->preset_name);
      ret=f_open(&fil,text, FA_CREATE_ALWAYS | FA_WRITE);
      if(ret==FR_OK)
      {
         env_file->preset_name[49]=0; // 50 max length
         f_printf(&fil,"#preset_name \"%s\"\n",env_file->preset_name);

         f_printf(&fil,"#############\n");
         f_printf(&fil,"#BOOT options\n");
         f_printf(&fil,"#############\n");
         f_printf(&fil,"bootmode %s\n",bootmode_names[env_file->boot_mode]);
         f_printf(&fil,"scsiboot %s\n",yesno_names[env_file->scsiboot]);
         f_printf(&fil,"autoconfig_ram %s\n",yesno_names[env_file->autoconfig_ram]);
         f_printf(&fil,"autoconfig_rtg %s\n",yesno_names[env_file->autoconfig_rtg]);
         f_printf(&fil,"cpu_ram %s\n",yesno_names[env_file->cpu_ram]);
         if(env_file->amix_mode!=-1)
            f_printf(&fil,"amix_mode %s\n",yesno_names[env_file->amix_mode]);
         if(env_file->amix_ram!=-1)
            f_printf(&fil,"amix_ram %d\n",env_file->amix_ram);
         if(env_file->service_cadence!=-1)
            f_printf(&fil,"service_cadence %d\n",env_file->service_cadence);
         f_printf(&fil,"mount_sd_0x76 %s\n",yesno_names[env_file->mount_sd_0x76]);
         f_printf(&fil,"mount_sd_root %s\n",yesno_names[env_file->mount_sd_root]);
         f_printf(&fil,"cpufreq %d\n",env_file->cpufreq);
         f_printf(&fil,"kickstart %d\n",env_file->kickstart);
         f_printf(&fil,"ext_kickstart %d\n",env_file->ext_kickstart);
         f_printf(&fil,"enable_test %s\n",yesno_names[env_file->enable_test]);
         f_printf(&fil,"bootscreen_resolution %s\n",resolution_names[env_file->bootscreen_resolution]);

         f_printf(&fil,"#############\n");
         f_printf(&fil,"#SCSI options\n");
         f_printf(&fil,"#############\n");
         for(int i=0;i<7;i++)
            f_printf(&fil,"scsi%d %d\n",i,env_file->scsi_num[i]);

         f_printf(&fil,"#############\n");
         f_printf(&fil,"# ADF options\n");
         f_printf(&fil,"#############\n");
         for(int i=0;i<8;i++)
            f_printf(&fil,"adf%d %d\n",i,env_file->adf_num[i]);

         f_printf(&fil,"#############\n");
         f_printf(&fil,"#MISC options\n");
         f_printf(&fil,"#############\n");
         f_printf(&fil,"mac_address %02X:%02X:%02X:%02X:%02X:%02X\n",env_file->mac_address[0],
               env_file->mac_address[1],env_file->mac_address[2],
               env_file->mac_address[3],env_file->mac_address[4],env_file->mac_address[5]);
         f_printf(&fil,"bp_ton %f\n",env_file->bp_ton);
         f_printf(&fil,"bp_toff %f\n",env_file->bp_toff);
         f_printf(&fil,"doubled_cursor %s\n",yesno_names[env_file->doubled_cursor]);
         f_printf(&fil,"monitor_switch %d\n",env_file->monitor_switch);
         f_printf(&fil,"arm_frequency %s\n",arm_frequency_names[env_file->arm_frequency]);

         f_printf(&fil,"\n"); // we really need this at the end of file...
         f_close(&fil);
      }
      else
      {
         printf("[Config] ERROR Write file presets/preset%d.txt\n",preset_selected);
         Xil_ExceptionEnable();
         return(0);
      }
   }
   usleep(10000);
   f_umount(Path);
   Xil_ExceptionEnable();
   usleep(10000);
   return(1);
}
int write_env_files_boot(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
//   env_file->boot_mode=config.boot_mode;
//   env_file->scsiboot=config.scsiboot;
//   env_file->autoconfig_ram=config.autoconfig_ram;
//   env_file->autoconfig_rtg=config.autoconfig_rtg;
//   env_file->enable_test=config.enable_test;
//   env_file->cpu_ram=config.cpu_ram;
//   env_file->amix_mode=config.amix_mode;
//   env_file->mount_sd_0x76=config.mount_sd_0x76;
//   env_file->mount_sd_root=config.mount_sd_root;
//   env_file->cpufreq=config.cpufreq;
//   env_file->kickstart=config.kickstart;
//   env_file->ext_kickstart=config.ext_kickstart;
   env_file->bootscreen_resolution=config.bootscreen_resolution;
   env_file->doubled_cursor=config.doubled_cursor;
   for(int i=0;i<7;i++)
      env_file->scsi_num[i]=config.scsi_num[i];
   for(int i=0;i<8;i++)
      env_file->adf_num[i]=config.adf_num[i];
   for(int i=0;i<6;i++)
      env_file->mac_address[i]=config.mac_address[i];
   env_file->bp_ton=config.bp_ton;
   env_file->bp_toff=config.bp_toff;
   env_file->monitor_switch=config.monitor_switch;
   env_file->arm_frequency=config.arm_frequency;

   return(write_env_files(env_file));
}
int write_env_files_scsi(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
   env_file->boot_mode=config.boot_mode;
   env_file->scsiboot=config.scsiboot;
   env_file->autoconfig_ram=config.autoconfig_ram;
   env_file->autoconfig_rtg=config.autoconfig_rtg;
   env_file->enable_test=config.enable_test;
   env_file->cpu_ram=config.cpu_ram;
   env_file->amix_mode=config.amix_mode;
   env_file->amix_ram=config.amix_ram;
   env_file->service_cadence=config.service_cadence;
   env_file->perf_stats=config.perf_stats;
   env_file->emu_probes=config.emu_probes;
   env_file->core0_stats=config.core0_stats;
   env_file->mount_sd_0x76=config.mount_sd_0x76;
   env_file->mount_sd_root=config.mount_sd_root;
   env_file->cpufreq=config.cpufreq;
   env_file->kickstart=config.kickstart;
   env_file->ext_kickstart=config.ext_kickstart;
   env_file->bootscreen_resolution=config.bootscreen_resolution;
   env_file->doubled_cursor=config.doubled_cursor;
//   for(int i=0;i<7;i++)
//      env_file->scsi_num[i]=config.scsi_num[i];
   for(int i=0;i<8;i++)
      env_file->adf_num[i]=config.adf_num[i];
   for(int i=0;i<6;i++)
      env_file->mac_address[i]=config.mac_address[i];
   env_file->bp_ton=config.bp_ton;
   env_file->bp_toff=config.bp_toff;
   env_file->monitor_switch=config.monitor_switch;
   env_file->arm_frequency=config.arm_frequency;

   return(write_env_files(env_file));
}
int write_env_files_adf(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
   env_file->boot_mode=config.boot_mode;
   env_file->scsiboot=config.scsiboot;
   env_file->autoconfig_ram=config.autoconfig_ram;
   env_file->autoconfig_rtg=config.autoconfig_rtg;
   env_file->enable_test=config.enable_test;
   env_file->cpu_ram=config.cpu_ram;
   env_file->mount_sd_0x76=config.mount_sd_0x76;
   env_file->mount_sd_root=config.mount_sd_root;
   env_file->cpufreq=config.cpufreq;
   env_file->kickstart=config.kickstart;
   env_file->ext_kickstart=config.ext_kickstart;
   env_file->bootscreen_resolution=config.bootscreen_resolution;
   env_file->doubled_cursor=config.doubled_cursor;
   for(int i=0;i<7;i++)
      env_file->scsi_num[i]=config.scsi_num[i];
//   for(int i=0;i<8;i++)
//      env_file->adf_num[i]=config.adf_num[i];
   for(int i=0;i<6;i++)
      env_file->mac_address[i]=config.mac_address[i];
   env_file->bp_ton=config.bp_ton;
   env_file->bp_toff=config.bp_toff;
   env_file->monitor_switch=config.monitor_switch;
   env_file->arm_frequency=config.arm_frequency;

   return(write_env_files(env_file));
}
int write_env_files_bootscres(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
   env_file->boot_mode=config.boot_mode;
   env_file->scsiboot=config.scsiboot;
   env_file->autoconfig_ram=config.autoconfig_ram;
   env_file->autoconfig_rtg=config.autoconfig_rtg;
   env_file->enable_test=config.enable_test;
   env_file->cpu_ram=config.cpu_ram;
   env_file->amix_mode=config.amix_mode;
   env_file->amix_ram=config.amix_ram;
   env_file->service_cadence=config.service_cadence;
   env_file->perf_stats=config.perf_stats;
   env_file->emu_probes=config.emu_probes;
   env_file->core0_stats=config.core0_stats;
   env_file->mount_sd_0x76=config.mount_sd_0x76;
   env_file->mount_sd_root=config.mount_sd_root;
   env_file->cpufreq=config.cpufreq;
   env_file->kickstart=config.kickstart;
   env_file->ext_kickstart=config.ext_kickstart;
//   env_file->bootscreen_resolution=config.bootscreen_resolution;
//   env_file->doubled_cursor=config.doubled_cursor;
//   env_file->monitor_switch=config.monitor_switch;
//   env_file->arm_frequency=config.arm_frequency;

   for(int i=0;i<7;i++)
      env_file->scsi_num[i]=config.scsi_num[i];
   for(int i=0;i<8;i++)
      env_file->adf_num[i]=config.adf_num[i];
   for(int i=0;i<6;i++)
      env_file->mac_address[i]=config.mac_address[i];
   env_file->bp_ton=config.bp_ton;
   env_file->bp_toff=config.bp_toff;

   return(write_env_files(env_file));
}
int write_env_files_misc(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
   env_file->boot_mode=config.boot_mode;
   env_file->scsiboot=config.scsiboot;
   env_file->autoconfig_ram=config.autoconfig_ram;
   env_file->autoconfig_rtg=config.autoconfig_rtg;
   env_file->enable_test=config.enable_test;
   env_file->cpu_ram=config.cpu_ram;
   env_file->amix_mode=config.amix_mode;
   env_file->amix_ram=config.amix_ram;
   env_file->service_cadence=config.service_cadence;
   env_file->perf_stats=config.perf_stats;
   env_file->emu_probes=config.emu_probes;
   env_file->core0_stats=config.core0_stats;
   env_file->mount_sd_0x76=config.mount_sd_0x76;
   env_file->mount_sd_root=config.mount_sd_root;
   env_file->cpufreq=config.cpufreq;
   env_file->kickstart=config.kickstart;
   env_file->ext_kickstart=config.ext_kickstart;
   env_file->bootscreen_resolution=config.bootscreen_resolution;
   env_file->doubled_cursor=config.doubled_cursor;
   env_file->monitor_switch=config.monitor_switch;
   env_file->arm_frequency=config.arm_frequency;
   for(int i=0;i<7;i++)
      env_file->scsi_num[i]=config.scsi_num[i];
   for(int i=0;i<8;i++)
      env_file->adf_num[i]=config.adf_num[i];
//   for(int i=0;i<6;i++)
//      env_file->mac_address[i]=config.mac_address[i];
//   env_file->bp_ton=config.bp_ton;
//   env_file->bp_toff=config.bp_toff;

   return(write_env_files(env_file));
}
int write_env_files_preset(ENV_FILE_VARS *env_file)
{
//   env_file->preset_name;
   env_file->boot_mode=config.boot_mode;
   env_file->scsiboot=config.scsiboot;
   env_file->autoconfig_ram=config.autoconfig_ram;
   env_file->autoconfig_rtg=config.autoconfig_rtg;
   env_file->enable_test=config.enable_test;
   env_file->cpu_ram=config.cpu_ram;
   env_file->amix_mode=config.amix_mode;
   env_file->amix_ram=config.amix_ram;
   env_file->service_cadence=config.service_cadence;
   env_file->perf_stats=config.perf_stats;
   env_file->emu_probes=config.emu_probes;
   env_file->core0_stats=config.core0_stats;
   env_file->mount_sd_0x76=config.mount_sd_0x76;
   env_file->mount_sd_root=config.mount_sd_root;
   env_file->cpufreq=config.cpufreq;
   env_file->kickstart=config.kickstart;
   env_file->ext_kickstart=config.ext_kickstart;
   env_file->bootscreen_resolution=config.bootscreen_resolution;
   env_file->doubled_cursor=config.doubled_cursor;
   for(int i=0;i<7;i++)
      env_file->scsi_num[i]=config.scsi_num[i];
   for(int i=0;i<8;i++)
      env_file->adf_num[i]=config.adf_num[i];
   for(int i=0;i<6;i++)
      env_file->mac_address[i]=config.mac_address[i];
   env_file->bp_ton=config.bp_ton;
   env_file->bp_toff=config.bp_toff;
   env_file->monitor_switch=config.monitor_switch;
   env_file->arm_frequency=config.arm_frequency;

   return(write_env_files(env_file));
}
int remove_preset_file(void)
{
   static FIL fil;
   f_unlink(DEFAULT_ROOT "presets/preset.txt");
   f_open(&fil,DEFAULT_ROOT "presets/preset.txt", FA_CREATE_ALWAYS | FA_WRITE);
   f_close(&fil);
   return(0);
}
int delete_env_files(void)
{
   static FIL fil;      /* File object */
   static FATFS fatfs;

   TCHAR *Path = DEFAULT_ROOT;

   Xil_ExceptionDisable();

   int ret;
retry:
   ret=f_clk_mount(&fatfs, Path, 1); // 1 mount immediately
   if(ret!=0)
   {
      printf("Error opening SD media\nRetry in 5 seconds\n");
      sleep(5);
      goto retry;
   }

#define DELETE_FILE(X) ret=f_unlink(DEFAULT_ROOT X);                         \
                       if(ret==FR_OK)                                        \
                       {                                                     \
                          printf("[Config] Delete file " X "\n");      \
                          f_close(&fil);                                     \
                       }                                                     \
                       else                                                  \
                       {                                                     \
                          /*printf("[Config] Can't Delete file " X "\n");*/\
                       }

   DELETE_FILE("env/bootmode")
   DELETE_FILE("env/scsiboot");
   DELETE_FILE("env/autoconfig_ram");
   DELETE_FILE("env/autoconfig_rtg");
   DELETE_FILE("env/cpu_ram");
   DELETE_FILE("env/amix_mode");
   DELETE_FILE("env/amix_ram");
   DELETE_FILE("env/service_cadence");
   DELETE_FILE("env/cpufreq");
   DELETE_FILE("env/kickstart");
   DELETE_FILE("env/ext_kickstart");
   DELETE_FILE("env/scsi");
   DELETE_FILE("env/enabletest");
   DELETE_FILE("env/bootscres");
   DELETE_FILE("env/macaddress");

   usleep(10000);
   f_umount(Path);
   Xil_ExceptionEnable();
   usleep(10000);
   return(1);
}
int delete_selected_preset(void)
{
   static FIL fil;      /* File object */
   static FATFS fatfs;

   TCHAR *Path = DEFAULT_ROOT;

   Xil_ExceptionDisable();

   int ret;
retry:
   ret=f_clk_mount(&fatfs, Path, 1); // 1 mount immediately
   if(ret!=0)
   {
      printf("Error opening SD media\nRetry in 5 seconds\n");
      sleep(5);
      goto retry;
   }

   char file[50];
   sprintf(file,DEFAULT_ROOT "presets/preset%d.txt",preset_selected);

   ret=f_unlink(file);
   if(ret==FR_OK)
   {
      printf("[Config] Delete file %s\n",file);
      f_close(&fil);
   }
   else
   {
      printf("[Config] Can't Delete file %s\n",file);
      return(0);
   }
   usleep(10000);
   f_umount(Path);
   Xil_ExceptionEnable();
   usleep(10000);
   return(1);
}
