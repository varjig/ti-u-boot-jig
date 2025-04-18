// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023-2025 Variscite Ltd. - https://www.variscite.com/
 */

#include <common.h>
#include <asm/global_data.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <log.h>
#include "am62x_eeprom.h"

DECLARE_GLOBAL_DATA_PTR;

#define AM64_DDRSS_SS_BASE  0x0F300000
#define SZ_6G	(SZ_2G + SZ_4G)
#define SZ_8G	(SZ_4G<<1)

typedef struct {
	uint64_t start;
	uint64_t max_size;
} ddr_bank_t;

#if defined(CONFIG_SOC_K3_AM625)
static ddr_bank_t am62x_ddr_banks[] = {
	{.start = 0x80000000, .max_size = SZ_2G},
	{.start = 0x880000000, .max_size = SZ_2G},
	{.start = 0x900000000, .max_size = SZ_4G},
	{.start = 0, .max_size = 0}
};
#else
static ddr_bank_t am62x_ddr_banks[] = {
	{.start = 0x80000000, .max_size = SZ_2G},
	{.start = 0x880000000, .max_size = SZ_6G},
	{.start = 0, .max_size = 0}
};
#endif

static int get_dram_size(uint64_t *size) {
	struct var_eeprom *ep = VAR_EEPROM_DATA;

	if (!size)
		return -EINVAL;

	var_eeprom_get_dram_size(ep, size);

	return 0;
}

int var_dram_init_mem_size_base(void) {
	uint64_t dram_size;
	int ret;
	ret = get_dram_size(&dram_size);
	if (ret) {
		printf("%s: Error: could not get dram size from EEPROM\n", __func__);
		return ret;
	}

#if defined(CONFIG_SOC_K3_AM625)
	/*
	 Update gd->ram_size according to EEPROM
	 Limit to 2GB for 32bit architecture (r5)
	*/
#ifdef CONFIG_PHYS_64BIT
	gd->ram_size = (phys_size_t) dram_size;
#else
	if ((uint64_t) dram_size > SZ_2G)
		gd->ram_size = (phys_size_t) SZ_2G;
	else
		gd->ram_size = (phys_size_t) dram_size;
#endif

	/* Set V2A_CTL_REG */
	switch ((long long unsigned int) dram_size) {
		case SZ_512M:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x1AD), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_1G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x1CE), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_2G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x1EF), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_4G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x210), AM64_DDRSS_SS_BASE + 0x020);
			break;
	}
#else
	if ((uint64_t) dram_size > SZ_2G)
		gd->ram_size = (phys_size_t) SZ_2G;
	else
		gd->ram_size = (phys_size_t) dram_size;

	/*
	 * Set V2A_CTL_REG
	 * Calculated as: (SDRAM_IDX << 5) | REGION_IDX
	 * Size SDRAM_IDX REGION_IDX V2A_CTL_REG
	 *  1G     14        17         0x1D1
	 *  2G     15        17         0x1F1
	 *  4G     16        17         0x211
	 *  8G     17        17         0x231
	 */
	switch ((long long unsigned int) dram_size) {
		case SZ_1G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x1D1), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_2G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x1F1), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_4G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x211), AM64_DDRSS_SS_BASE + 0x020);
			break;
		case SZ_8G:
			writel( ((readl(AM64_DDRSS_SS_BASE + 0x020) & ~0x3FF) | 0x231), AM64_DDRSS_SS_BASE + 0x020);
			break;
	}
#endif

	return ret;
}

int var_dram_init_banksize(void) {
	uint64_t dram_size, remaining_size;
	int ret, bank = 0;

	ret = get_dram_size(&dram_size);

	if (ret) {
		printf("%s: Error: could not get dram size from eeprom, reverting to device tree\n", __func__);
		ret = fdtdec_setup_memory_banksize();
		if (ret)
			printf("Error setting up memory banksize from device tree. %d\n", ret);

		return ret;
	}

	remaining_size = dram_size;
	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS && am62x_ddr_banks[bank].max_size != 0 && remaining_size; bank++) {
		/* Calculate bank size */
		if (remaining_size >= am62x_ddr_banks[bank].max_size) {
			gd->bd->bi_dram[bank].size = am62x_ddr_banks[bank].max_size;
			remaining_size -= am62x_ddr_banks[bank].max_size;
		} else {
			gd->bd->bi_dram[bank].size = remaining_size;
			remaining_size = 0;
		}

		/* Assign bank start*/
		gd->bd->bi_dram[bank].start = am62x_ddr_banks[bank].start;

		debug("%s: DRAM Bank #%d: start = 0x%llx, size = 0x%llx\n",
		      __func__, bank,
		      (unsigned long long)gd->bd->bi_dram[bank].start,
		      (unsigned long long)gd->bd->bi_dram[bank].size);
	}

	return 0;
}
