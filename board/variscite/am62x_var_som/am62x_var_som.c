// SPDX-License-Identifier: GPL-2.0+
/*
 * Board specific initialization for AM62x platforms
 *
 * Copyright (C) 2020-2022 Texas Instruments Incorporated - https://www.ti.com/
 *	Suman Anna <s-anna@ti.com>
 * Copyright (C) 2023 Variscite Ltd. - https://www.variscite.com/
 *
 */

#include <common.h>
#include <asm/io.h>
#include <spl.h>
#include <dm/uclass.h>
#include <k3-ddrss.h>
#include <fdt_support.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <env.h>

#include "../common/am62x_eeprom.h"
#include "../common/am62x_dram.h"
#ifdef CONFIG_BOARD_LATE_INIT
#include "../common/am62x_mmc.h"
#endif

int var_setup_mac(struct var_eeprom *eeprom);

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

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

int dram_init(void)
{
	int ret;
	read_eeprom_header();

	ret = fdtdec_setup_mem_size_base();

	/* Override fdtdec_setup_mem_size_base with memory size from EEPROM */
	if (!ret)
		ret = var_dram_init_mem_size_base();

	return ret;
}

int dram_init_banksize(void)
{
	return var_dram_init_banksize();
}

#if defined(CONFIG_SPL_LOAD_FIT)
int board_fit_config_name_match(const char *name)
{
	return 0;
}
#endif

#if defined(CONFIG_SPL_BUILD)
#if defined(CONFIG_K3_AM64_DDRSS)
static void fixup_ddr_driver_for_ecc(struct spl_image_info *spl_image)
{
	struct udevice *dev;
	int ret;

	dram_init_banksize();

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("Cannot get RAM device for ddr size fixup: %d\n", ret);

	ret = k3_ddrss_ddr_fdt_fixup(dev, spl_image->fdt_addr, gd->bd);
	if (ret)
		printf("Error fixing up ddr node for ECC use! %d\n", ret);
}
#else
static void fixup_memory_node(struct spl_image_info *spl_image)
{
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int bank;
	int ret;

	dram_init();
	dram_init_banksize();

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		start[bank] =  gd->bd->bi_dram[bank].start;
		size[bank] = gd->bd->bi_dram[bank].size;
	}

	/* dram_init functions use SPL fdt, and we must fixup u-boot fdt */
	ret = fdt_fixup_memory_banks(spl_image->fdt_addr,
				     start, size, CONFIG_NR_DRAM_BANKS);
	if (ret)
		printf("Error fixing up memory node! %d\n", ret);
}
#endif

void spl_perform_fixups(struct spl_image_info *spl_image)
{
#if defined(CONFIG_K3_AM64_DDRSS)
	fixup_ddr_driver_for_ecc(spl_image);
#else
	fixup_memory_node(spl_image);
#endif
}
#endif

#define ENV_STR_SIZE 10

#ifdef CONFIG_BOARD_LATE_INIT
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

	env_set("board_name", "VAR-SOM-AM62");

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
#endif

	return 0;
}
#endif

#define CTRLMMR_USB0_PHY_CTRL	0x43004008
#define CTRLMMR_USB1_PHY_CTRL	0x43004018
#define CORE_VOLTAGE		0x80000000

#ifdef CONFIG_SPL_BOARD_INIT
void spl_board_init(void)
{
	u32 val;

#ifndef CONFIG_CPU_V7R
	/* Save boot_device for U-Boot */
	int * boot_device = (int *) VAR_SCRATCH_BOOT_DEVICE;
	*boot_device = spl_boot_device();
#endif

	/* Set USB0 PHY core voltage to 0.85V */
	val = readl(CTRLMMR_USB0_PHY_CTRL);
	val &= ~(CORE_VOLTAGE);
	writel(val, CTRLMMR_USB0_PHY_CTRL);

	/* Set USB1 PHY core voltage to 0.85V */
	val = readl(CTRLMMR_USB1_PHY_CTRL);
	val &= ~(CORE_VOLTAGE);
	writel(val, CTRLMMR_USB1_PHY_CTRL);

	/* We have 32k crystal, so lets enable it */
	val = readl(MCU_CTRL_LFXOSC_CTRL);
	val &= ~(MCU_CTRL_LFXOSC_32K_DISABLE_VAL);
	writel(val, MCU_CTRL_LFXOSC_CTRL);
	/* Add any TRIM needed for the crystal here.. */
	/* Make sure to mux up to take the SoC 32k from the crystal */
	writel(MCU_CTRL_DEVICE_CLKOUT_LFOSC_SELECT_VAL,
	       MCU_CTRL_DEVICE_CLKOUT_32K_CTRL);

	/* Init DRAM size for R5/A53 SPL */
	dram_init_banksize();
}
#endif
