/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include "eth.h"

#ifndef __ICMP_H__
#define __ICMP_H__


#define ICMP_ECHO_REPLY		0x00
#define ICMP_ECHO_REQUEST	0x08

void icmp_process_packet(ETH2_PACKET_T *ptEthPkt, size_t sizPacket);
void icmp_init(void);

#endif	// __ICMP_H__
