// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023, Texas Instruments Incorporated - https://www.ti.com/
 * Copyright (C) 2025, Variscite Ltd. - https://www.variscite.com/
 */

#include <fdt_support.h>
#include <dm/uclass.h>
#include <k3-ddrss.h>
#include <spl.h>

#include "k3-ddr-init.h"
#include "am62x_eeprom.h"
#include "am62x_dram.h"

int dram_init(void)
{
	int ret;

	read_eeprom_header();

	ret = fdtdec_setup_mem_size_base_lowest();

	if (ret)
		printf("Error setting up mem size and base. %d\n", ret);
	else
		/* Override fdtdec_setup_mem_size_base_lowest with memory size from EEPROM */
		ret = var_dram_init_mem_size_base();

	return ret;
}

int dram_init_banksize(void)
{
	return var_dram_init_banksize();
}

#if defined(CONFIG_SPL_BUILD)

void fixup_ddr_driver_for_ecc(struct spl_image_info *spl_image)
{
	struct udevice *dev;
	int ret, ctr = 1;

	dram_init_banksize();

	ret = uclass_get_device(UCLASS_RAM, 0, &dev);
	if (ret)
		panic("Cannnot get RAM device for ddr size fixup: %d\n", ret);

	ret = k3_ddrss_ddr_fdt_fixup(dev, spl_image->fdt_addr, gd->bd);
	if (ret)
		printf("Error fixing up ddr node for ECC use! %d\n", ret);

	dram_init_banksize();

	ret = uclass_next_device_err(&dev);

	while (!ret) {
		ret = k3_ddrss_ddr_fdt_fixup(dev, spl_image->fdt_addr, gd->bd);
		if (ret)
			printf("Error fixing up ddr node %d for ECC use! %d\n", ctr, ret);

		dram_init_banksize();
		ret = uclass_next_device_err(&dev);
		ctr++;
	}
}

void fixup_memory_node(struct spl_image_info *spl_image)
{
	u64 start[CONFIG_NR_DRAM_BANKS];
	u64 size[CONFIG_NR_DRAM_BANKS];
	int bank;
	int ret;

	dram_init();
	dram_init_banksize();

	for (bank = 0; bank < CONFIG_NR_DRAM_BANKS; bank++) {
		start[bank] = gd->bd->bi_dram[bank].start;
		size[bank] = gd->bd->bi_dram[bank].size;
	}

	ret = fdt_fixup_memory_banks(spl_image->fdt_addr, start, size,
				     CONFIG_NR_DRAM_BANKS);

	if (ret)
		printf("Error fixing up memory node! %d\n", ret);
}

#endif
