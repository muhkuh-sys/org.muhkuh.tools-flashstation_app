/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <stddef.h>


#include "eth.h"


#ifndef __ARP_H__
#define __ARP_H__


#define ARP_OPCODE_REQUEST	MUS2NUS(0x0001)
#define ARP_OPCODE_REPLY	MUS2NUS(0x0002)


void arp_init(void);

void arp_process_packet(ETH2_PACKET_T *ptEthPkt, size_t sizPacket);

void arp_send_ipv4_packet(ETH2_PACKET_T *ptPkt, size_t sizPacket, unsigned long ulDstIp);

void arp_timer(void);

#endif	/* __ARP_H__ */
