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


#ifndef __UDP_H__
#define __UDP_H__

typedef void (*pfn_udp_receive_handler)(void *pvData, size_t sizLength, void *pvUser);

typedef struct
{
	unsigned int uiLocalPort;
	unsigned long ulRemoteIp;
	unsigned int uiRemotePort;
	pfn_udp_receive_handler pfn_recHandler;
	void *pvUser;
} UDP_ASSOCIATION_T;


void udp_init(void);

void udp_process_packet(ETH2_PACKET_T *ptEthPkt, size_t sizPacket);

void udp_send_packet(ETH2_PACKET_T *ptPkt, size_t sizUdpUserData, UDP_ASSOCIATION_T *ptAssoc);

UDP_ASSOCIATION_T *udp_registerPort(unsigned int uiLocalPort, unsigned long ulRemoteIp, unsigned int uiRemotePort, pfn_udp_receive_handler pfn_recHandler, void *pvUser);
void udp_unregisterPort(UDP_ASSOCIATION_T *ptAssoc);

#endif	/* __UDP_H__ */
