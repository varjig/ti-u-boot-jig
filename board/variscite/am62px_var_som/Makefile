#
# Copyright (C) 2023 Texas Instruments Incorporated - https://www.ti.com/
# Copyright 2025 Variscite Ltd. - https://www.variscite.com/
#
# SPDX-License-Identifier:     GPL-2.0+
#

obj-y	+= am62px_var_som.o
obj-y += ../common/am62x_eeprom.o
obj-y += ../common/am62x_dram.o
ifndef CONFIG_CPU_V7R
obj-y += ../common/am62x_mmc.o
endif
obj-$(CONFIG_TI_AM65_CPSW_NUSS) += ../common/am62x_eth.o
obj-${CONFIG_ARCH_K3} += ../common/k3-ddr-init.o
