/* SPDX-License-Identifier: GPL-2.0+
 *
 * Copyright (C) 2025 Variscite Ltd. - https://www.variscite.com/.
 *
 */

#ifndef _AM62X_ETH_H_
#define _AM62X_ETH_H_

#include "am62x_eeprom.h"

int var_setup_mac(struct var_eeprom *eeprom);
int var_eth_get_rgmii_id_quirk(struct var_eeprom *ep);

#endif
