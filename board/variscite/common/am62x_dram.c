// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Variscite Ltd.
 */

#include <common.h>
#include <asm/arch/sys_proto.h>
#include <asm/global_data.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <log.h>
#include "am62x_eeprom.h"

DECLARE_GLOBAL_DATA_PTR;

#define AM64_DDRSS_SS_BASE  0x0F300000

typedef struct {
	uint64_t start;
	uint64_t max_size;
} ddr_bank_t;

static ddr_bank_t am62x_ddr_banks[] = {
	{.start = 0x80000000, .max_size = SZ_2G},
	{.start = 0x880000000, .max_size = SZ_2G},
	{.start = 0x900000000, .max_size = SZ_4G},
	{.start = 0, .max_size = 0}
};

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

	return ret;
}

int var_dram_init_banksize(void) {
	uint64_t dram_size, remaining_size;
	int ret, bank = 0;

	ret = get_dram_size(&dram_size);

	if (ret) {
		printf("%s: Error: could not get dram size from eeprom, reverting to device tree\n", __func__);
		return fdtdec_setup_memory_banksize();
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
