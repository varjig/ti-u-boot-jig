// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2023 Variscite Ltd.
 */
#include <common.h>
#include <command.h>
#include <asm/arch/sys_proto.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <stdbool.h>
#include <mmc.h>
#include <env.h>
#include <asm/arch/hardware.h>
#include <spl.h>
#include "am62x_eeprom.h"

static int am6_boot_dev(void) {
    int * boot_device = (int *) VAR_SCRATCH_BOOT_DEVICE;
    return *boot_device;
}

int mmc_get_env_dev(void) {
    int mmc_dev, boot_device = am6_boot_dev();

	switch(boot_device) {
	case BOOT_DEVICE_MMC2:
		mmc_dev = 1; /* SD */
		break;
	case BOOT_DEVICE_MMC1:
		mmc_dev = 0; /* eMMC */
		break;
	default:
        printf("%s: Warning: Unknown boot device, defaulting to CONFIG_SYS_MMC_ENV_DEV\n", __func__);
		mmc_dev = CONFIG_SYS_MMC_ENV_DEV;
		break;
	}

    return mmc_dev;
}

/* This should be defined for each board */
__weak int mmc_map_to_kernel_blk(int dev_no)
{
	return dev_no;
}

void board_late_mmc_env_init(void)
{
    char cmd[32];
    int dev_no = mmc_get_env_dev();

    env_set_ulong("mmcdev", dev_no);

	/* Set mmcblk env */
	env_set_ulong("mmcblk", mmc_map_to_kernel_blk(dev_no));

	sprintf(cmd, "mmc dev %d", dev_no);
	run_command(cmd, 0);
}
