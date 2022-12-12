/*
 * Copyright (C) 2023 Variscite Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#include <common.h>
#include <command.h>
#include <dm.h>
#include <i2c.h>
#include <asm/io.h>
#include <cpu_func.h>
#include <u-boot/crc.h>
#include <asm/arch/hardware.h>

#include "am62x_eeprom.h"

int var_eeprom_is_valid(struct var_eeprom *ep)
{
	if (htons(ep->magic) != VAR_EEPROM_MAGIC) {
		debug("Invalid EEPROM magic 0x%04x, expected 0x%04x\n",
			htons(ep->magic), VAR_EEPROM_MAGIC);
		return 0;
	}

	return 1;
}

int var_eeprom_get_dram_size(struct var_eeprom *ep, uint64_t *size)
{
	/* No data in EEPROM - return default DRAM size */
	if (!var_eeprom_is_valid(ep)) {
		*size = DEFAULT_SDRAM_SIZE;
		return 0;
	}

	*size = ((uint64_t)ep->dramsize * 128UL) * (1UL << 20);

	return 0;
}

#if defined(CONFIG_DM_I2C)
static int var_eeprom_get_dev(struct udevice **devp)
{
	int ret;
	struct udevice *bus;

	ret = uclass_get_device_by_seq(UCLASS_I2C, VAR_EEPROM_I2C_BUS, &bus);
	if (ret) {
		printf("%s: No EEPROM I2C bus %d\n", __func__, VAR_EEPROM_I2C_BUS);
		return ret;
	}

	ret = dm_i2c_probe(bus, VAR_EEPROM_I2C_ADDR, 0, devp);
	if (ret) {
		printf("%s: I2C EEPROM probe failed\n", __func__);
		return ret;
	}

	i2c_set_chip_offset_len(*devp, 1);
	i2c_set_chip_addr_offset_mask(*devp, 1);

	return 0;
}

int var_eeprom_read_header(struct var_eeprom *e)
{
	int ret;
	struct udevice *dev;

	ret = var_eeprom_get_dev(&dev);
	if (ret) {
		printf("%s: Failed to detect I2C EEPROM\n", __func__);
		return ret;
	}

	/* Read EEPROM header to memory */
	ret = dm_i2c_read(dev, VAR_EEPROM_HEADER_ADDR, (void *)e, sizeof(*e));
	if (ret) {
		printf("%s: EEPROM read failed, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#else
int var_eeprom_read_header(struct var_eeprom *e)
{
	int ret;

	/* Probe EEPROM */
	i2c_set_bus_num(VAR_EEPROM_I2C_BUS);
	ret = i2c_probe(VAR_EEPROM_I2C_ADDR);
	if (ret) {
		printf("%s: I2C EEPROM probe failed\n", __func__);
		return ret;
	}

	/* Read EEPROM header to memory */
	ret = i2c_read(VAR_EEPROM_I2C_ADDR, VAR_EEPROM_HEADER_ADDR, 1, (uint8_t *)e, sizeof(*e));
	if (ret) {
		printf("%s: EEPROM read failed ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif /* CONFIG_DM_I2C */

#ifndef CONFIG_SPL_BUILD
/* Returns carrier board revision string via 'rev' argument.
 * For legacy carrier board revisions the "legacy" string is returned.
 * For new carrier board revisions the actual carrier revision is returned.
 * Symphony-Board 1.4 and below are legacy, 1.4a and above are new.
 * DT8MCustomBoard 1.4 and below are legacy, 2.0 and above are new.
 */
void var_carrier_eeprom_get_revision(struct var_carrier_eeprom *ep, char *rev, size_t size)
{
	if (var_carrier_eeprom_is_valid(ep))
		strncpy(rev, (const char *)ep->carrier_rev, size);
	else
		strncpy(rev, "legacy", size);
}


int var_eeprom_get_mac(struct var_eeprom *ep, u8 *mac)
{
	flush_dcache_all();
	if (!var_eeprom_is_valid(ep))
		return -1;

	memcpy(mac, ep->mac, sizeof(ep->mac));

	return 0;
}

void var_eeprom_print_prod_info(struct var_eeprom *ep)
{
	u8 partnum[8] = {0};

	flush_dcache_all();

	if (!var_eeprom_is_valid(ep)) {
		printf("%s Error: EEPROM is not valid\n", __func__);
		return;
	}

	/* Read first part of P/N  */
	memcpy(partnum, ep->partnum, sizeof(ep->partnum));

	/* Read second part of P/N  */
	if (ep->version >= 3)
		memcpy(partnum + sizeof(ep->partnum), ep->partnum2, sizeof(ep->partnum2));

	printf("\nPart number: VSM-AM62-%.*s\n", (int)sizeof(partnum), partnum);
	printf("Assembly: AS%.*s\n", (int)sizeof(ep->assembly), (char *)ep->assembly);

	printf("Production date: %.*s %.*s %.*s\n",
			4, /* YYYY */
			(char *)ep->date,
			3, /* MMM */
			((char *)ep->date) + 4,
			2, /* DD */
			((char *)ep->date) + 4 + 3);

	printf("Serial Number: %02x:%02x:%02x:%02x:%02x:%02x\n",
		ep->mac[0], ep->mac[1], ep->mac[2], ep->mac[3], ep->mac[4], ep->mac[5]);
	debug("EEPROM version: 0x%x\n", ep->version);
	debug("SOM features: 0x%x\n", ep->features);
	printf("SOM revision: 0x%x\n", ep->somrev);
	if (ep->dramsize < 8)
		printf("DRAM size: %d MiB\n", ep->dramsize * 128);
	else
		printf("DRAM size: %d GiB\n", (ep->dramsize * 128) / 1024);
	printf("DRAM PN: VIC%04d\n\n", ep->ddr_partnum);
}
#endif

#if defined(CONFIG_K3_AM64_DDRSS)
static int var_eeprom_crc32(struct var_eeprom *ep, const uint32_t offset,
							const uint32_t len, uint32_t * crc32_val) {
	uint32_t i;
	struct udevice *dev;
	int ret;

	/* No data in EEPROM - return -1 */
	if (!var_eeprom_is_valid(ep)) {
		return -1;
	}

	ret = var_eeprom_get_dev(&dev);
	if (ret) {
		debug("%s: Failed 2 to detect I2C EEPROM\n", __func__);
		return ret;
	}

	*crc32_val = crc32(0, NULL, 0);
	for (i = 0; i < len; i++) {
		uint8_t data;
		dm_i2c_read(dev, offset + i, &data, 1);
		*crc32_val = crc32(*crc32_val, &data, 1);
		// debug("%s: offset=0x%02x data=0x%02x crc=0x%08x\n", __func__, offset + i, data, *crc32_val);
	}

	debug("%s: crc32=0x%08x (offset=%d len=%d)\n", __func__, *crc32_val, offset, len);

	return 0;
}

int var_eeprom_ddr_table_is_valid(struct var_eeprom *ep)
{
	uint32_t i, ddr_crc32, ddr_adjust_bytes;
	static int ddr_table_is_valid = -1;

	if (ddr_table_is_valid >= 0) {
		debug("%s: %d\n", __func__, ddr_table_is_valid);
		return ddr_table_is_valid;
	}

	ddr_table_is_valid = 0;

	/* No data in EEPROM - return -1 */
	if (!var_eeprom_is_valid(ep))
		return ddr_table_is_valid;

	/* Calculate Size of DDR Adjust Table */
	for (i = 0; i < DRAM_TABLE_NUM; i++)
		ddr_adjust_bytes += ep->off[i + 1] - ep->off[i];

	/* Calculate DDR Adjust table CRC32 */
	if (var_eeprom_crc32(ep, ep->off[0], ddr_adjust_bytes, &ddr_crc32)) {
		printf("%s: Error: DDR adjust table crc calculation failed\n", __func__);
		return ddr_table_is_valid;
	}

	/* Verify DDR Adjust table CRC32 */
	if (ddr_crc32 != ep->ddr_crc32) {
		printf("%s: Error: DDR adjust table invalid CRC "
			"eeprom=0x%08x, calculated=0x%08x, len=%d\n",
			__func__, ep->ddr_crc32, ddr_crc32, ddr_adjust_bytes);
		return ddr_table_is_valid;
	}

	debug("crc32: eeprom=0x%08x, calculated=0x%08x, len=%d\n", ep->ddr_crc32, ddr_crc32, ddr_adjust_bytes);
	ddr_table_is_valid = 1;

	return ddr_table_is_valid;
}

static void var_eeprom_adjust_ddr_u32(const char * name, const u32 * ep_val, u32 * dt_val) {
	if (*ep_val != *dt_val)
	{
		printf("%s: adjusting %s from %d to %d\n", __func__, name, *dt_val, *ep_val);
		*dt_val = *ep_val;
	} else {
		printf("%s: %s no change (%d)\n", __func__, name, *dt_val);
	}
}

void board_k3_adjust_ddr_freq(u32 * ddr_freq1, u32 * ddr_freq2,
				 u32 * ddr_fhs_cnt)
{
	struct var_eeprom *ep = VAR_EEPROM_DATA;

	if (var_eeprom_read_header(ep))
	{
		printf("%s: Error - Could not read EEPROM header\n", __func__);
		return;
	}

	if (!var_eeprom_is_valid(ep))
	{
		printf("%s: Error - EEPROM header is not valid\n", __func__);
		return;
	}

	debug("%s: EEPROM version is %d\n", __func__, ep->version);

	if (!var_eeprom_ddr_table_is_valid(ep)) {
		printf("%s: Error: ddr table is not valid\n", __func__);
		return;
	}

	var_eeprom_adjust_ddr_u32("ddr_freq1", ddr_freq1, &ep->ddr_freq[0]);
	var_eeprom_adjust_ddr_u32("ddr_freq2", ddr_freq2, &ep->ddr_freq[1]);
	var_eeprom_adjust_ddr_u32("ddr_fhs_cnt", ddr_fhs_cnt, &ep->ddr_fhs_cnt);
}

void var_eeprom_adjust_ddr_regs(const char * name, u32 * regvalues, u16 * regnum,
				 const u16 regcount, const u8 ep_offset)
{
	int i, ret;
	uint off;
	u8 adj_table_size[DRAM_TABLE_NUM];
	struct var_eeprom *ep = VAR_EEPROM_DATA;
	struct udevice *dev;

	printf("%s: Adjusting %s table\n", __func__, name);

	if (!var_eeprom_ddr_table_is_valid(ep)) {
		printf("%s: Error: ddr table is not valid\n", __func__);
		return;
	}

	if (ep_offset >= DRAM_TABLE_NUM)
	{
		printf("%s: Error - EEPROM ddr table offset is not valid\n", __func__);
	}

	/* EEPROM header should already be read in board_k3_adjust_ddr_freq */
	if (!var_eeprom_is_valid(ep))
	{
		printf("%s: Error - EEPROM header is not valid\n", __func__);
		return;
	}

	debug("%s: EEPROM offset table\n", __func__);
	for (i = 0; i < DRAM_TABLE_NUM + 1; i++)
		debug("%s: off[%d]=%d\n", __func__, i, ep->off[i]);

	/* Calculate DRAM adjustment table sizes */
	debug("%s: EEPROM size table\n", __func__);
	for (i = 0; i < DRAM_TABLE_NUM; i++) {
		adj_table_size[i] = (ep->off[i + 1] - ep->off[i]) /
				(sizeof(dram_cfg_param_t));
		debug("%s: sizes[%d]=%d\n", __func__, i, adj_table_size[i]);
	}

	/* Get EEPROM device */
	ret = var_eeprom_get_dev(&dev);
	if (ret) {
		printf("%s: Failed to detect I2C EEPROM\n", __func__);
		return;
	}

	off = VAR_EEPROM_HEADER_ADDR + ep->off[ep_offset];

	/* Iterate over EEPROM DDR table and adjust regvalues */
	for (i = 0; i < adj_table_size[ep_offset]; i++) {
		dram_cfg_param_t adjust_param;

		/* Read next entry from adjustment table */
		dm_i2c_read(dev, off, (uint8_t *) &adjust_param, sizeof(dram_cfg_param_t));

		/* Make sure adjust register is in range */
		if (adjust_param.reg > regcount)
		{
			printf("%s: Error - Register out of range (%d > %d)\n", __func__,
				adjust_param.reg, regcount);
			return;
		}

		/* Adjust the value if it's different */
		if (adjust_param.val != regvalues[adjust_param.reg]) {
			debug("%s: Adjusting eeprom[%d] adjust[%d] reg=0x%04x val=0x%08xU, // %s_%d_VAL\n",
				__func__, off, i, adjust_param.reg, adjust_param.val, name, adjust_param.reg);
			regvalues[adjust_param.reg] = adjust_param.val;
		}

		off += sizeof(dram_cfg_param_t);
	}
}

void board_k3_adjust_ddr_ctl_regs(u32 * regvalues, u16 * regnum,
				 const u16 regcount )
{
	var_eeprom_adjust_ddr_regs("DDRSS_CTL", regvalues, regnum, regcount, DDR_OFF_CTL_DATA);
}

void board_k3_adjust_ddr_pi_regs(u32 * regvalues, u16 * regnum,
				 const u16 regcount )
{
	var_eeprom_adjust_ddr_regs("DDRSS_PI", regvalues, regnum, regcount, DDR_OFF_PI_DATA);
}

void board_k3_adjust_ddr_phy_regs(u32 * regvalues, u16 * regnum,
				 const u16 regcount )
{
	var_eeprom_adjust_ddr_regs("DDRSS_PHY", regvalues, regnum, regcount, DDR_OFF_PHY_DATA);
}
#endif

#ifndef CONFIG_SPL_BUILD
#if defined(CONFIG_DM_I2C)
int var_carrier_eeprom_read(int bus_no, int addr, struct var_carrier_eeprom *ep)
{
	int ret;
	struct udevice *bus;
	struct udevice *dev;

	ret = uclass_get_device_by_seq(UCLASS_I2C, bus_no, &bus);
	if (ret) {
		debug("%s: No bus %d\n", __func__, bus_no);
		return ret;
	}

	ret = dm_i2c_probe(bus, addr, 0, &dev);
	if (ret) {
		debug("%s: Carrier EEPROM I2C probe failed\n", __func__);
		return ret;
	}

	/* Read EEPROM to memory */
	ret = dm_i2c_read(dev, 0, (void *)ep, sizeof(*ep));
	if (ret) {
		debug("%s: Carrier EEPROM read failed, ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#else
int var_carrier_eeprom_read(int bus_no, int addr, struct var_carrier_eeprom *ep)
{
	int ret;

	/* Probe EEPROM */
	i2c_set_bus_num(bus_no);
	ret = i2c_probe(addr);
	if (ret) {
		debug("%s: Carrier EEPROM probe failed\n", __func__);
		return ret;
	}

	/* Read EEPROM contents */
	ret = i2c_read(addr, 0, 1, (uint8_t *)ep, sizeof(*ep));
	if (ret) {
		debug("%s: Carrier EEPROM read failed ret=%d\n", __func__, ret);
		return ret;
	}

	return 0;
}
#endif

int var_carrier_eeprom_is_valid(struct var_carrier_eeprom *ep)
{
	u32 crc, crc_offset = offsetof(struct var_carrier_eeprom, crc);

	if (htons(ep->magic) != VAR_CARRIER_EEPROM_MAGIC) {
		debug("Invalid carrier EEPROM magic 0x%hx, expected 0x%hx\n",
			htons(ep->magic), VAR_CARRIER_EEPROM_MAGIC);
		return 0;
	}

	if (ep->struct_ver < 1) {
		printf("Invalid carrier EEPROM version 0x%hx\n", ep->struct_ver);
		return 0;
	}

	if (ep->struct_ver == 1)
		return 1;

	/* Only EEPROM structure above version 1 has CRC field */
	crc = crc32(0, (void *)ep, crc_offset);

	if (crc != ep->crc) {
		printf("Carrier EEPROM CRC mismatch (%08x != %08x)\n",
			crc, be32_to_cpu(ep->crc));
		return 0;
	}

	return 1;
}
#endif