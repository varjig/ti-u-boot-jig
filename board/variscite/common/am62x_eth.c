#include <common.h>
#include <net.h>
#include <miiphy.h>
#include <env.h>
#include "am62x_eth.h"


#define CHAR_BIT 8

#if defined(CONFIG_ARCH_K3)
static uint64_t mac2int(const uint8_t hwaddr[])
{
	int8_t i;
	uint64_t ret = 0;
	const uint8_t *p = hwaddr;

	for (i = 5; i >= 0; i--) {
		ret |= (uint64_t)*p++ << (CHAR_BIT * i);
	}

	return ret;
}

static void int2mac(const uint64_t mac, uint8_t *hwaddr)
{
	int8_t i;
	uint8_t *p = hwaddr;

	for (i = 5; i >= 0; i--) {
		*p++ = mac >> (CHAR_BIT * i);
	}
}
#endif

/**
 * Set the MAC address in the environment and print a warning if
 * it already exists
 *
 * @param interface	Ethernet interface, 0 (=>eth0),1 (=>eth1) ...
 * @param enetaddr	MAC address value to assign to ethernet interface
 * @return 		0 if ok, 1 on error
 */
static int var_eth_env_set_enetaddr(const uint8_t interface,
                                    const uint8_t *enetaddr)
{
	int ret;
	char name[12] = "enetaddr";
	uint8_t enetaddr_env[ARP_HLEN];

	/* Initialize the environment variable name based on the interface */
	if (interface == 0)
		snprintf(name, sizeof(name), "ethaddr");
	else
		snprintf(name, sizeof(name), "eth%daddr", interface);

	/* Try to update the environment variable */
	ret = eth_env_set_enetaddr(name, enetaddr);

	/* If the variable already exists, read it and print a warning */
	if (ret == -EEXIST)
	{
		eth_env_get_enetaddr(name, enetaddr_env);

		if (memcmp(enetaddr, enetaddr_env, ARP_HLEN) != 0)
		{
			printf("Warning: eth%d MAC addresses don't match:\n",
			       interface);
			printf("Address in EEPROM is\t\t%pM\n", enetaddr);
			printf("Address in environment is \t%pM\n",
			       enetaddr_env);
		}
	}

	return ret;
}

/**
 * Read MAC address(es) from Variscite EEPROM and set the MAC address(es)
 * in the U-Boot environment.
 *
 * @param eeprom pointer to EEPROM Structure
 * @return 0 if ok, 1 on error
 */
int var_setup_mac(struct var_eeprom *eeprom)
{
	int ret;
	uint8_t enetaddr[ARP_HLEN];

#if defined(CONFIG_ARCH_K3)
	uint64_t addr;
	uint8_t enet1addr[ARP_HLEN];
#endif

	/* Read MAC address from EEPROM */
	ret = var_eeprom_get_mac(eeprom, enetaddr);
	if (ret)
		return ret;

	/* Make sure EEPROM address is valid */
	if (!is_valid_ethaddr(enetaddr))
		return -1;

	/* Set eth0 MAC address */
	var_eth_env_set_enetaddr(0, enetaddr);

#if defined(CONFIG_ARCH_K3)
	/* Set eth1 MAC address to eth0 + 1 */
	addr = mac2int(enetaddr);
	int2mac(addr + 1, enet1addr);
	var_eth_env_set_enetaddr(1, enet1addr);
#endif

	return 0;
}

int var_eth_get_rgmii_id_quirk(struct var_eeprom *ep)
{
	flush_dcache_all();

	if (!var_eeprom_is_valid(ep))
		return -1;

	env_set("var_rgmii_id_quirk", (ep->features & VAR_EEPROM_F_ETH_HW_DLY) ?
		 NULL : "phy-gmii-sel.var_rgmii_id_quirk\\=1");

	return 0;
}
