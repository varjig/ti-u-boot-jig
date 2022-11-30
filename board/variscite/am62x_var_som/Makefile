#
# Copyright (C) 2020-2022 Texas Instruments Incorporated - https://www.ti.com/
#	Suman Anna <s-anna@ti.com>
#
# SPDX-License-Identifier:     GPL-2.0+
#

obj-y	+= am62x_var_som.o
obj-y += ../common/am62x_eeprom.o
obj-y += ../common/am62x_dram.o
ifndef CONFIG_CPU_V7R
obj-y += ../common/am62x_mmc.o
endif
obj-$(CONFIG_TI_AM65_CPSW_NUSS) += ../common/am62x_eth.o
