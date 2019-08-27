/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <string.h>

#include "icmp.h"

#include "networking/stack/checksum.h"
#include "networking/stack/ipv4.h"
#include "options.h"



void icmp_process_packet(ETH2_PACKET_T *ptEthPkt, size_t sizPacket)
{
	ETH2_PACKET_T *ptSendPacket;
	unsigned char ucType;
	size_t sizIcmpPacketSize;


	if( sizPacket>=(sizeof(ETH2_HEADER_T)+sizeof(IPV4_HEADER_T)+sizeof(ICMP_PACKET_T)) )
	{
		ucType = ptEthPkt->uEth2Data.tIpPkt.uIpData.tIcmpPkt.ucType;
		if( ucType==ICMP_ECHO_REQUEST )
		{
			/* Get a free frame for sending. */
			ptSendPacket = eth_get_empty_packet();
			if( ptSendPacket!=NULL )
			{
				/* Get the ICMP data size. */
				sizIcmpPacketSize = NUS2MUS(ptEthPkt->uEth2Data.tIpPkt.tIpHdr.usLength) - sizeof(IPV4_HEADER_T);

				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.ucType = ICMP_ECHO_REPLY;
				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.ucCode = ptEthPkt->uEth2Data.tIpPkt.uIpData.tIcmpPkt.ucCode;
				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usChecksum = 0;
				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usIdentifier = ptEthPkt->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usIdentifier;
				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usSequenceNumber = ptEthPkt->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usSequenceNumber;
				memcpy(ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt), ETH_USER_DATA_ADR(ptEthPkt->uEth2Data.tIpPkt.uIpData.tIcmpPkt), sizIcmpPacketSize - sizeof(ICMP_PACKET_T));

				/* Generate the checksum. */
				ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt.usChecksum = MUS2NUS(checksum_add_complement(&ptSendPacket->uEth2Data.tIpPkt.uIpData.tIcmpPkt, sizIcmpPacketSize));

				ipv4_send_packet(ptSendPacket, ptEthPkt->uEth2Data.tIpPkt.tIpHdr.ulSrcIp, IP_PROTOCOL_ICMP, sizIcmpPacketSize);
			}
		}
	}
}


void icmp_init(void)
{
}

