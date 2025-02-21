/**
 * @file emac.c
 *
 */
/* Copyright (C) 2018-2021 by Arjan van Vught mailto:info@orangepi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
//#ifndef NDEBUG
# include <stdio.h>
//#endif
#include <assert.h>

#include "h3.h"
#include "h3_sid.h"

#include "phy.h"
#include "mii.h"

#include "debug.h"

#define BUS_SOFT_RESET2_EPHY_RST 	(1 << 2)
#define BUS_CLK_GATING4_EPHY_GATING	(1 << 0)

#define CTL0_DUPLEX_FULL_DUPLEX		(1 << 0)
#define CTL0_SPEED_10M				(2 << 2)
#define CTL0_SPEED_100M				(3 << 2)
	#define CTL0_SPEED_SHIFT	2
	#define CTL0_SPEED_MASK		3

#define TX_CTL0_TX_EN				(1U << 31)
#define TX_CTL1_TX_DMA_EN			(1 << 30)

#define RX_CTL0_RX_EN				(1U << 31)
#define RX_CTL1_RX_DMA_EN			(1 << 30)

#define RX_FRM_FLT_RX_ALL_MULTICAST	(1 << 16)

#define	ARM_DMA_ALIGN	64

#define CONFIG_TX_DESCR_NUM	48
#define CONFIG_RX_DESCR_NUM 48
#define CONFIG_ETH_BUFSIZE	2048 /* Note must be DMA aligned */
/*
 * The datasheet says that each descriptor can transfers up to 4096 bytes
 * But later, the register documentation reduces that value to 2048,
 * using 2048 cause strange behaviors and even BSP driver use 2047
 */
#define CONFIG_ETH_RXSIZE	2044 /* Note must fit in ETH_BUFSIZE */

#define TX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_TX_DESCR_NUM)
#define RX_TOTAL_BUFSIZE	(CONFIG_ETH_BUFSIZE * CONFIG_RX_DESCR_NUM)

#define __aligned(x)            __attribute__((aligned(x)))

struct emac_dma_desc {
	uint32_t status;
	uint32_t st;
	uint32_t buf_addr;
	uint32_t next;
} __aligned(ARM_DMA_ALIGN);

struct coherent_region {
	struct emac_dma_desc rx_chain[CONFIG_TX_DESCR_NUM];
	struct emac_dma_desc tx_chain[CONFIG_RX_DESCR_NUM];
	char rxbuffer[RX_TOTAL_BUFSIZE] __aligned(ARM_DMA_ALIGN);
	char txbuffer[TX_TOTAL_BUFSIZE] __aligned(ARM_DMA_ALIGN);
	uint32_t rx_currdescnum;
	uint32_t tx_currdescnum;
};

static struct coherent_region *p_coherent_region = 0;

#define H3_EPHY_DEFAULT_VALUE	0x00058000
#define H3_EPHY_DEFAULT_MASK	0xFFFF8000
#define H3_EPHY_ADDR_SHIFT		20
#define H3_EPHY_LED_POL			(1U << 17) 	// 1: active low, 0: active high
#define H3_EPHY_SHUTDOWN		(1U << 16) 	// 1: shutdown, 0: power up
#define H3_EPHY_SELECT			(1U << 15) 	// 1: internal PHY, 0: external PHY

#define PHY_ADDR		1

__attribute__((cold)) void emac_shutdown(void) {
	uint32_t value;

	value = H3_EMAC->RX_CTL0;
	value &= ~RX_CTL0_RX_EN;
	H3_EMAC->RX_CTL0 = value;

	value = H3_EMAC->TX_CTL0;
	value &= ~TX_CTL0_TX_EN;
	H3_EMAC->TX_CTL0 = value;

	value = H3_EMAC->TX_CTL1;
	value &= (uint32_t)~TX_CTL1_TX_DMA_EN;
	H3_EMAC->TX_CTL1 = value;

	value = H3_EMAC->RX_CTL1;
	value &= (uint32_t)~RX_CTL1_RX_DMA_EN;
	H3_EMAC->RX_CTL1 = value;

	H3_CCU->BUS_CLK_GATING4 &= (uint32_t)~BUS_CLK_GATING4_EPHY_GATING;

	H3_SYSTEM->EMAC_CLK |= H3_EPHY_SHUTDOWN;

#if 0
	//-> The below gives continues leds on
	H3_CCU->BUS_SOFT_RESET2 &= ~BUS_SOFT_RESET2_EPHY_RST;

	phy_shutdown(PHY_ADDR);
	//<-
#endif
}

void _write_hwaddr(uint8_t *mac_id) {
	const uint32_t macid_lo = mac_id[0] + ((uint32_t)mac_id[1] << 8) + ((uint32_t)mac_id[2] << 16) + ((uint32_t)mac_id[3] << 24);
	const uint32_t macid_hi = mac_id[4] + ((uint32_t)mac_id[5] << 8);

	H3_EMAC->ADDR[0].HIGH = macid_hi;
	H3_EMAC->ADDR[0].LOW = macid_lo;
}

void _set_syscon_ephy(void) {
	/* H3 based SoC's that has an Internal 100MBit PHY
	 * needs to be configured and powered up before use
	 */
	uint32_t value = H3_SYSTEM->EMAC_CLK;

	value &= ~H3_EPHY_DEFAULT_MASK;
	value |= H3_EPHY_DEFAULT_VALUE;
	value |= PHY_ADDR << H3_EPHY_ADDR_SHIFT;
	value &= ~H3_EPHY_SHUTDOWN;
	value |= H3_EPHY_SELECT;

	H3_SYSTEM->EMAC_CLK = value;
}

void _adjust_link(bool duplex, uint32_t speed) {
	uint32_t value = H3_EMAC->CTL0;

	if (duplex) {
		value |= CTL0_DUPLEX_FULL_DUPLEX;
	} else {
		value &= (uint32_t)~CTL0_DUPLEX_FULL_DUPLEX;
	}

	value &= (uint32_t)~(CTL0_SPEED_MASK << CTL0_SPEED_SHIFT);

	switch (speed) {
	case 1000:
		break;
	case 100:
		value |= CTL0_SPEED_100M;
		break;
	case 10:
		value |= CTL0_SPEED_10M;
		break;
	default:
		break;
	}

	H3_EMAC->CTL0 = value;
}

__attribute__((cold)) void emac_init(void) {
	DEBUG_PRINTF("PHY{%d} ID = %08x", PHY_ADDR, phy_get_id(PHY_ADDR));

	uint8_t mac_address[6];
	uint8_t rootkey[16];

	h3_sid_get_rootkey(rootkey);

	mac_address[0] = 0x2;
	mac_address[1] = rootkey[3];
	mac_address[2] = rootkey[12];
	mac_address[3] = rootkey[13];
	mac_address[4] = rootkey[14];
	mac_address[5] = rootkey[15];

	_write_hwaddr(mac_address);

	DEBUG_PRINTF("H3_EMAC->ADDR[0].LOW=%08x, H3_EMAC->ADDR[0].HIGH=%08x", H3_EMAC->ADDR[0].LOW, H3_EMAC->ADDR[0].HIGH);
}

static void _rx_descs_init(void) {
	struct emac_dma_desc *desc_table_p = &p_coherent_region->rx_chain[0];
	char *rxbuffs = &p_coherent_region->rxbuffer[0];
	struct emac_dma_desc *desc_p;
	uint32_t idx;

	for (idx = 0; idx < CONFIG_RX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->buf_addr = (uintptr_t) &rxbuffs[idx * CONFIG_ETH_BUFSIZE];
		desc_p->next = (uintptr_t) &desc_table_p[idx + 1];
		desc_p->st = CONFIG_ETH_RXSIZE;
		desc_p->status = (1U << 31);
	}

	/* Correcting the last pointer of the chain */
	desc_p->next = (uintptr_t) &desc_table_p[0];

	H3_EMAC->RX_DMA_DESC = (uintptr_t)&desc_table_p[0];
	p_coherent_region->rx_currdescnum = 0;
}

static void _tx_descs_init(void) {
	struct emac_dma_desc *desc_table_p = &p_coherent_region->tx_chain[0];
	char *txbuffs = &p_coherent_region->txbuffer[0];
	struct emac_dma_desc *desc_p;
	uint32_t idx;

	for (idx = 0; idx < CONFIG_TX_DESCR_NUM; idx++) {
		desc_p = &desc_table_p[idx];
		desc_p->buf_addr = (uintptr_t) &txbuffs[idx * CONFIG_ETH_BUFSIZE];
		desc_p->next = (uintptr_t) &desc_table_p[idx + 1];
		desc_p->status = (1U << 31);
		desc_p->st = 0;
	}

	/* Correcting the last pointer of the chain */
	desc_p->next = (uintptr_t) &desc_table_p[0];

	H3_EMAC->TX_DMA_DESC = (uintptr_t)&desc_table_p[0];
	p_coherent_region->tx_currdescnum = 0;
}

__attribute__((hot)) int emac_eth_recv(uint8_t **packetp) {
	uint32_t status, desc_num = p_coherent_region->rx_currdescnum;
	struct emac_dma_desc *desc_p = &p_coherent_region->rx_chain[desc_num];
	int length;

	status = desc_p->status;

	/* Check for DMA own bit */
	if (!(status & (1U << 31))) {
		length = (desc_p->status >> 16) & 0x3FFF;

		if (length < 0x40) {
			DEBUG_PUTS("Bad Packet (length < 0x40)");
			return -1;
		} else {
			if (length > CONFIG_ETH_RXSIZE) {
				DEBUG_PRINTF("Received packet is too big (length=%d)\n", length);
				return -1;
			}

			*packetp = (uint8_t*) (uint32_t) desc_p->buf_addr;
#ifdef DEBUG_DUMP
			debug_dump((void*) *packetp, (uint16_t) length);
#endif
			return length;
		}
	}

	return -1;
}

void emac_eth_send(void *packet, int len) {
	uint32_t value;
	uint32_t desc_num = p_coherent_region->tx_currdescnum;
	struct emac_dma_desc *desc_p = &p_coherent_region->tx_chain[desc_num];
	uintptr_t data_start = (uintptr_t) desc_p->buf_addr;

	desc_p->st = (uint32_t)len;
	/* Mandatory undocumented bit */
	desc_p->st |= (1U << 24);

	h3_memcpy((void *) data_start, packet, (size_t)len);
#ifdef DEBUG_DUMP
	debug_dump((void *) data_start, (uint16_t) len);
#endif
	/* frame end */
	desc_p->st |= (1 << 30);
	desc_p->st |= (1U << 31);

	/*frame begin */
	desc_p->st |= (1 << 29);
	desc_p->status = (1U << 31);

	/* Move to next Descriptor and wrap around */
	if (++desc_num >= CONFIG_TX_DESCR_NUM) {
		desc_num = 0;
	}

	p_coherent_region->tx_currdescnum = desc_num;

	/* Start the DMA */
	value = H3_EMAC->TX_CTL1;
	value |= (1U << 31);/* mandatory */
	value |= (1 << 30);/* mandatory */
	H3_EMAC->TX_CTL1 = value;
}

void emac_free_pkt(void) {
	uint32_t desc_num = p_coherent_region->rx_currdescnum;
	struct emac_dma_desc *desc_p = &p_coherent_region->rx_chain[desc_num];

	/* Make the current descriptor valid again */
	desc_p->status |= (1U << 31);

	/* Move to next desc and wrap-around condition. */
	if (++desc_num >= CONFIG_RX_DESCR_NUM) {
		desc_num = 0;
	}

	p_coherent_region->rx_currdescnum = desc_num;
}

void _autonegotiation(void) {
	uint32_t value;

	DEBUG_PRINTF( "PHY status BMCR: %04X, BMSR: %04X, ADVERTISE: %04X", phy_read(PHY_ADDR, MII_BMCR), phy_read(PHY_ADDR, MII_BMSR), phy_read(PHY_ADDR, MII_ADVERTISE));

	phy_write(PHY_ADDR, MII_ADVERTISE, ADVERTISE_FULL | ADVERTISE_RFAULT);

	value = (uint32_t)phy_read (PHY_ADDR, MII_BMCR);
	value |= BMCR_ANRESTART;
	phy_write(PHY_ADDR, MII_BMCR, (uint16_t) value);

	const uint32_t micros_timeout = H3_TIMER->AVS_CNT1 + (5 * 1000 * 1000);

	while (H3_TIMER->AVS_CNT1 < micros_timeout) {
		if (phy_read (PHY_ADDR, MII_BMSR) & BMSR_ANEGCOMPLETE) {
			break;
		}
	}

	DEBUG_PRINTF("%d", micros_timeout - H3_TIMER->AVS_CNT1);
	DEBUG_PRINTF( "PHY status BMCR: %04x, BMSR: %04x", phy_read (PHY_ADDR, MII_BMCR), phy_read (PHY_ADDR, MII_BMSR) );

}

extern void mac_address_get(uint8_t paddr[]);

void __attribute__((cold)) emac_start(uint8_t paddr[]) {
	mac_address_get(paddr);

	uint32_t value;

	H3_CCU->BUS_SOFT_RESET2 |= BUS_SOFT_RESET2_EPHY_RST;
	udelay(1000); // 1ms
	H3_CCU->BUS_CLK_GATING4 &= (uint32_t)~BUS_CLK_GATING4_EPHY_GATING;
	udelay(1000); // 1ms
	H3_CCU->BUS_CLK_GATING4 |= BUS_CLK_GATING4_EPHY_GATING;

	_set_syscon_ephy();
	_autonegotiation();
	_adjust_link(true, 100);

#ifndef NDEBUG
	printf("sizeof(struct coherent_region)=%u\n", sizeof(struct coherent_region));
	printf("H3_SYSTEM->EMAC_CLK=%p ", H3_SYSTEM->EMAC_CLK);
	debug_print_bits(H3_SYSTEM->EMAC_CLK);
	printf("H3_EMAC->CTL0=%p ", H3_EMAC->CTL0);
	debug_print_bits(H3_EMAC->CTL0);
	printf( "PHY status BMCR: %04x, BMSR: %04x\n", phy_read (PHY_ADDR, MII_BMCR), phy_read (PHY_ADDR, MII_BMSR) );
	debug_print_bits((uint32_t) phy_read (PHY_ADDR, MII_BMCR));
	debug_print_bits((uint32_t) phy_read (PHY_ADDR, MII_BMSR));
#endif

	assert(p_coherent_region == 0);
	assert(sizeof(struct coherent_region) < MEGABYTE/2);

	p_coherent_region = (struct coherent_region *)H3_MEM_COHERENT_REGION;

	value = H3_EMAC->RX_CTL0;
	value &= ~RX_CTL0_RX_EN;
	H3_EMAC->RX_CTL0 = value;

	value = H3_EMAC->TX_CTL0;
	value &= ~TX_CTL0_TX_EN;
	H3_EMAC->TX_CTL0 = value;

	value = H3_EMAC->TX_CTL1;
	value &= (uint32_t) ~TX_CTL1_TX_DMA_EN;
	H3_EMAC->TX_CTL1 = value;

	value = H3_EMAC->RX_CTL1;
	value &= (uint32_t) ~RX_CTL1_RX_DMA_EN;
	H3_EMAC->RX_CTL1 = value;

	_rx_descs_init();
	_tx_descs_init();

	H3_EMAC->RX_FRM_FLT = RX_FRM_FLT_RX_ALL_MULTICAST;

	value = H3_EMAC->RX_CTL1;
	value |= RX_CTL1_RX_DMA_EN;
	H3_EMAC->RX_CTL1 = value;

	value = H3_EMAC->TX_CTL1;
	value |= TX_CTL1_TX_DMA_EN;
	H3_EMAC->TX_CTL1 = value;

	value = H3_EMAC->RX_CTL0;
	value |= RX_CTL0_RX_EN;
	H3_EMAC->RX_CTL0 = value;

	value = H3_EMAC->TX_CTL0;
	value |= TX_CTL0_TX_EN;
	H3_EMAC->TX_CTL0 = value;

//	H3_EMAC->INT_EN = (uint32_t)(~0);

#ifndef NDEBUG
	printf("================\n");
	printf("H3_EMAC->RX_CTL0=%p ", H3_EMAC->RX_CTL0);
	debug_print_bits(H3_EMAC->RX_CTL0);
	printf("H3_EMAC->RX_CTL1=%p ", H3_EMAC->RX_CTL1);
	debug_print_bits(H3_EMAC->RX_CTL1);
	printf("H3_EMAC->RX_FRM_FLT=%p ", H3_EMAC->RX_FRM_FLT);
	debug_print_bits(H3_EMAC->RX_FRM_FLT);
	puts("");
	printf("H3_EMAC->TX_CTL0=%p ", H3_EMAC->TX_CTL0);
	debug_print_bits(H3_EMAC->TX_CTL0);
	printf("H3_EMAC->TX_CTL1=%p ", H3_EMAC->TX_CTL1);
	debug_print_bits(H3_EMAC->TX_CTL1);
	puts("");
	printf("H3_EMAC->ADDR[0].LOW=%08x, H3_EMAC->ADDR[0].HIGH=%08x\n", H3_EMAC->ADDR[0].LOW, H3_EMAC->ADDR[0].HIGH);
	puts("");
	printf("H3_EMAC->TX_DMA_STA=%p\n", H3_EMAC->TX_DMA_STA);
	printf("H3_EMAC->TX_CUR_DESC=%p\n", H3_EMAC->TX_CUR_DESC);
	printf("H3_EMAC->TX_CUR_BUF=%p\n", H3_EMAC->TX_CUR_BUF);
	puts("");
	printf("H3_EMAC->RX_DMA_STA=%p\n", H3_EMAC->RX_DMA_STA);
	printf("H3_EMAC->RX_CUR_DESC=%p\n", H3_EMAC->RX_CUR_DESC);
	printf("H3_EMAC->RX_CUR_BUF=%p\n", H3_EMAC->RX_CUR_BUF);
	printf("================\n");
#endif
}
