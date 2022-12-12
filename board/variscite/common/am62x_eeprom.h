/*
 * Copyright (C) 2023 Variscite Ltd.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _AM62X_VAR_EEPROM_H_
#define _AM62X_VAR_EEPROM_H_

#define VAR_EEPROM_MAGIC	0x414D /* == HEX("AM") */

#define VAR_EEPROM_I2C_BUS	3
#define VAR_EEPROM_I2C_ADDR	0x50

/* Use EEPROM on TI AM63X EVK */
#define VAR_EEPROM_HEADER_ADDR	0 /* Start address of header in EEPROM */

/* Optional SOM features */
#define VAR_EEPROM_F_WIFI		(1 << 0)
#define VAR_EEPROM_F_ETH		(1 << 1)
#define VAR_EEPROM_F_AUDIO		(1 << 2)

enum {
	DDR_OFF_CTL_DATA =	0,
	DDR_OFF_PI_DATA,
	DDR_OFF_PHY_DATA,
	DRAM_TABLE_NUM, /* Number of DRAM adjustment tables */
};

struct __attribute__((packed)) var_eeprom
{
	u16 magic;                /* 00-0x00 - magic number       */
	u8 partnum[3];            /* 02-0x02 - part number        */
	u8 assembly[10];          /* 05-0x05 - assembly number    */
	u8 date[9];               /* 15-0x0f - build date         */
	u8 mac[6];                /* 24-0x18 - MAC address        */
	u8 somrev;                /* 30-0x1e - SOM revision       */
	u8 version;               /* 31-0x1f - EEPROM version     */
	u8 features;              /* 32-0x20 - SOM features       */
	u8 dramsize;              /* 33-0x21 - DRAM size          */
	u16 off[DRAM_TABLE_NUM+1]; /* 34-0x22 - DRAM table offsets */
	u8 partnum2[5];           /* 42-0x2a - part number        */
	u8 reserved[5];           /* 47-0x2f - reserved, align to 32bit */
	u32 ddr_fhs_cnt;          /* 52-0x34 - Number of times to communicate to DDR for frequency handshake. */
	u32 ddr_freq[2];          /* 56-0x38 - Frequency set points */
	u16 ddr_partnum;          /* 64-0x40 - DDR VIC Part Number */
	u32 ddr_crc32;            /* 66-0x42 - DDR adjust table crc32 */

	/*
	Three dram_cfg_param_t adjustment tables located at off[i]:
	 - ti,ctl-data:		An array containing the controller settings.
	 - ti,pi-data:		An array containing the phy independent block settings
	 - ti,phy-data:		An array containing the ddr phy settings.
	*/
};

typedef struct __attribute__((packed)) {
	uint16_t reg;
	uint32_t val;
} dram_cfg_param_t;

#ifdef CONFIG_CPU_V7R
/* Use HSM SRAM scratch pad for R5 SPL */
#define VAR_EEPROM_SCRATCH_START	0x43c30000
#else
/* Use OCRAM as scratch pad for A53 SPL/U-Boot */
#define VAR_SCRATCH_BOOT_DEVICE	0x70000000
#define VAR_EEPROM_SCRATCH_START	(VAR_SCRATCH_BOOT_DEVICE + sizeof(uint32_t))
#endif

#define VAR_EEPROM_DATA ((struct var_eeprom *) (VAR_EEPROM_SCRATCH_START))

#define VAR_CARRIER_EEPROM_MAGIC	0x5643 /* == HEX("VC") */

#define CARRIER_REV_LEN 16
struct __attribute__((packed)) var_carrier_eeprom
{
	u16 magic;                          /* 00-0x00 - magic number		*/
	u8 struct_ver;                      /* 01-0x01 - EEPROM structure version	*/
	u8 carrier_rev[CARRIER_REV_LEN];    /* 02-0x02 - carrier board revision	*/
	u32 crc;                            /* 10-0x0a - checksum			*/
};

int var_eeprom_read_header(struct var_eeprom *e);
int var_eeprom_get_dram_size(struct var_eeprom *e, uint64_t *size);
int var_eeprom_get_mac(struct var_eeprom *e, u8 *mac);
int var_eeprom_is_valid(struct var_eeprom *ep);
void var_eeprom_print_prod_info(struct var_eeprom *e);
int var_carrier_eeprom_read(int bus, int addr, struct var_carrier_eeprom *ep);
int var_carrier_eeprom_is_valid(struct var_carrier_eeprom *ep);
void var_carrier_eeprom_get_revision(struct var_carrier_eeprom *ep, char *rev, size_t size);

#endif /* _MX8M_VAR_EEPROM_H_ */
