// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AM62Px platforms
 *
 * Copyright (C) 2023 Texas Instruments Incorporated - https://www.ti.com/
 *
 */

#include <asm/arch/hardware.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <dm/uclass.h>
#include <env.h>
#include <fdt_support.h>
#include <fdt_simplefb.h>
#include <spl.h>
#include <splash.h>
#include <video.h>

#include "../common/am62x_eeprom.h"
#include "../common/am62x_dram.h"
#include "../common/am62x_eth.h"
#ifdef CONFIG_BOARD_LATE_INIT
#include "../common/am62x_mmc.h"
#endif

#include "../common/k3-ddr-init.h"

DECLARE_GLOBAL_DATA_PTR;

int read_eeprom_header(void) {
	struct var_eeprom *ep = VAR_EEPROM_DATA;
	struct var_eeprom eeprom = {0};
	int ret = 0;

	if (!var_eeprom_is_valid(ep)) {
		ret = var_eeprom_read_header(&eeprom);
		if (ret) {
			printf("%s EEPROM read failed.\n", __func__);
			return -1;
		}
		memcpy(ep, &eeprom, sizeof(*ep));
	}

	return ret;
}

#if CONFIG_IS_ENABLED(SPLASH_SCREEN)
static struct splash_location default_splash_locations[] = {
	{
		.name = "sf",
		.storage = SPLASH_STORAGE_SF,
		.flags = SPLASH_STORAGE_RAW,
		.offset = 0x700000,
	},
	{
		.name		= "mmc",
		.storage	= SPLASH_STORAGE_MMC,
		.flags		= SPLASH_STORAGE_FS,
		.devpart	= "1:1",
	},
};

int splash_screen_prepare(void)
{
	return splash_source_load(default_splash_locations,
				ARRAY_SIZE(default_splash_locations));
}
#endif

int board_init(void)
{
	return 0;
}

#if IS_ENABLED(CONFIG_BOARD_LATE_INIT)
#define ENV_STR_SIZE 10

void set_bootdevice_env(void) {
	int * boot_device = (int *) VAR_SCRATCH_BOOT_DEVICE;
	char env_str[ENV_STR_SIZE];

	snprintf(env_str, ENV_STR_SIZE, "%d", *boot_device);
	env_set("boot_dev", env_str);

	switch(*boot_device) {
	case BOOT_DEVICE_MMC2:
		printf("Boot Device: SD\n");
		env_set("boot_dev_name", "sd");
		break;
	case BOOT_DEVICE_MMC1:
		printf("Boot Device: eMMC\n");
		env_set("boot_dev_name", "emmc");
		break;
	default:
		printf("Boot Device: Unknown\n");
		env_set("boot_dev_name", "unknown");
		break;
	}
}

#define SDRAM_SIZE_STR_LEN 5

/* configure AUDIO_EXT_REFCLK1 pin as an output*/
static void audio_refclk1_ctrl_clkout_en(void) {
	volatile uint32_t *audio_refclk1_ctrl_ptr = (volatile uint32_t *)0x001082E4;
	uint32_t audio_refclk1_ctrl_val = *audio_refclk1_ctrl_ptr;
	audio_refclk1_ctrl_val |= (1 << 15);
	*audio_refclk1_ctrl_ptr = audio_refclk1_ctrl_val;
}

int board_late_init(void)
{
	struct var_eeprom *ep = VAR_EEPROM_DATA;
	char sdram_size_str[SDRAM_SIZE_STR_LEN];

	audio_refclk1_ctrl_clkout_en();

	env_set("board_name", "VAR-SOM-AM62P");

	read_eeprom_header();
	var_eeprom_print_prod_info(ep);

	set_bootdevice_env();

	snprintf(sdram_size_str, SDRAM_SIZE_STR_LEN, "%d",
			(int) (gd->ram_size / 1024 / 1024));
	env_set("sdram_size", sdram_size_str);

#ifdef CONFIG_ENV_IS_IN_MMC
	board_late_mmc_env_init();
#endif

#ifdef CONFIG_TI_AM65_CPSW_NUSS
	var_setup_mac(ep);
	var_eth_get_rgmii_id_quirk(ep);
#endif

	return 0;
}
#endif

#if defined(CONFIG_SPL_BUILD)
void spl_perform_fixups(struct spl_image_info *spl_image)
{
	if (IS_ENABLED(CONFIG_K3_DDRSS)) {
		if (IS_ENABLED(CONFIG_K3_INLINE_ECC))
			fixup_ddr_driver_for_ecc(spl_image);
	} else {
		fixup_memory_node(spl_image);
	}
}

void spl_board_init(void)
{
	u32 val;

#ifndef CONFIG_CPU_V7R
	/* Save boot_device for U-Boot */
	int * boot_device = (int *) VAR_SCRATCH_BOOT_DEVICE;
	*boot_device = spl_boot_device();
#endif

	/* We have 32k crystal, so lets enable it */
	val = readl(MCU_CTRL_LFXOSC_CTRL);
	val &= ~(MCU_CTRL_LFXOSC_32K_DISABLE_VAL);
	writel(val, MCU_CTRL_LFXOSC_CTRL);
	/* Add any TRIM needed for the crystal here.. */
	/* Make sure to mux up to take the SoC 32k from the crystal */
	writel(MCU_CTRL_DEVICE_CLKOUT_LFOSC_SELECT_VAL,
	       MCU_CTRL_DEVICE_CLKOUT_32K_CTRL);

	enable_caches();
	if (IS_ENABLED(CONFIG_SPL_SPLASH_SCREEN) && IS_ENABLED(CONFIG_SPL_BMP))
		splash_display();
}
#endif

#if defined(CONFIG_OF_BOARD_SETUP)
int ft_board_setup(void *blob, struct bd_info *bd)
{
	int ret = -1;

	if (IS_ENABLED(CONFIG_FDT_SIMPLEFB))
		ret = fdt_simplefb_enable_and_mem_rsv(blob);

	/* If simplefb is not enabled and video is active, then at least reserve
	 * the framebuffer region to preserve the splash screen while OS is booting
	 */
	if (IS_ENABLED(CONFIG_VIDEO) && IS_ENABLED(CONFIG_OF_LIBFDT)) {
		if (ret && video_is_active())
			return fdt_add_fb_mem_rsv(blob);
	}

	return 0;
}
#endif
