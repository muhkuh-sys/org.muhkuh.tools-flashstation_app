/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include "udp.h"

#include "networking/stack/checksum.h"
#include "networking/stack/ipv4.h"
#include "options.h"


#define CFG_DEBUGMSG 0
static const unsigned long ulDebugMessages = 0xffffffffU;

#if CFG_DEBUGMSG==1
#       include "uprintf.h"

#       define DEBUGZONE(n)  (ulDebugMessages&(0x00000001<<(n)))

        /*
         * These defines must match the ZONE_* defines
         */
#       define DBG_ZONE_ERROR           0
#       define DBG_ZONE_WARNING         1
#       define DBG_ZONE_FUNCTION        2
#       define DBG_ZONE_INIT            3
#       define DBG_ZONE_VERBOSE         7

#       define ZONE_ERROR               DEBUGZONE(DBG_ZONE_ERROR)
#       define ZONE_WARNING             DEBUGZONE(DBG_ZONE_WARNING)
#       define ZONE_FUNCTION            DEBUGZONE(DBG_ZONE_FUNCTION)
#       define ZONE_INIT                DEBUGZONE(DBG_ZONE_INIT)
#       define ZONE_VERBOSE             DEBUGZONE(DBG_ZONE_VERBOSE)

#       define DEBUGMSG(cond,...) ((void)((cond)?(uprintf(__VA_ARGS__)),1:0))
#else
#       define DEBUGMSG(cond,...) ((void)0)
#endif


#define UDP_PORTLIST_MAX 8

static UDP_ASSOCIATION_T atUdpPortAssoc[UDP_PORTLIST_MAX];



static unsigned short udp_buildChecksum(ETH2_PACKET_T *ptPkt, size_t sizUdpPacketSize)
{
	unsigned int uiIpChecksum;
	unsigned int uiUdpChecksum;
	unsigned int uiMyUdpChecksum;


	/* Save the IP checksum. */
	uiIpChecksum = ptPkt->uEth2Data.tIpPkt.tIpHdr.usChecksum;
	/* Use IP checksum field for UDP length. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usChecksum = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usLength;

	/* Save the UDP checksum. */
	uiUdpChecksum = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usChecksum;
	/* Clear the UDP checksum. */
	ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usChecksum = 0;

	/* Generate the checksum. */
	uiMyUdpChecksum = checksum_add_complement(&ptPkt->uEth2Data.tIpPkt.tIpHdr.ucProtocol, sizUdpPacketSize+11);

	/* Restore the IP checksum and the UDP checksum. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usChecksum = (unsigned short)uiIpChecksum;
	ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usChecksum = (unsigned short)uiUdpChecksum;

	return (unsigned short)uiMyUdpChecksum;
}



void udp_init(void)
{
	UDP_ASSOCIATION_T *ptAssocCnt;
	UDP_ASSOCIATION_T *ptAssocEnd;


	ptAssocCnt = atUdpPortAssoc;
	ptAssocEnd = ptAssocCnt + UDP_PORTLIST_MAX;
	while( ptAssocCnt<ptAssocEnd )
	{
		ptAssocCnt->uiLocalPort = 0;
		++ptAssocCnt;
	}
}



void udp_process_packet(ETH2_PACKET_T *ptPkt, size_t sizPacket)
{
	size_t sizUdpPacketSize;
	size_t sizTransferedSize;
	unsigned int uiDstPort;
	unsigned int uiMyUdpChecksum;
	unsigned int uiUdpChecksum;
	UDP_ASSOCIATION_T *ptAssocCnt;
	UDP_ASSOCIATION_T *ptAssocEnd;
	UDP_ASSOCIATION_T *ptAssocHit;


	/* check size */
	if( sizPacket>=(sizeof(ETH2_HEADER_T)+sizeof(IPV4_HEADER_T)+sizeof(UDP_HEADER_T)) )
	{
		/* The size field in UDP packet must not exceed the transfered data. */
		sizUdpPacketSize = NUS2MUS(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usLength);
		sizTransferedSize = sizPacket - sizeof(ETH2_HEADER_T) - sizeof(IPV4_HEADER_T);
		if( sizUdpPacketSize<=sizTransferedSize )
		{
			/* test UDP checksum */
			uiMyUdpChecksum = udp_buildChecksum(ptPkt, sizUdpPacketSize);
			uiUdpChecksum = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usChecksum;
			if( uiMyUdpChecksum==uiUdpChecksum )
			{
				/* check destination port */
				uiDstPort = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usDstPort;

				/* loop over all port associations */
				ptAssocCnt = atUdpPortAssoc;
				ptAssocEnd = ptAssocCnt + UDP_PORTLIST_MAX;
				ptAssocHit = NULL;
				while( ptAssocCnt<ptAssocEnd )
				{
					/* does the local port match? */
					if( ptAssocCnt->uiLocalPort==uiDstPort )
					{
						ptAssocHit = ptAssocCnt;
						break;
					}
					++ptAssocCnt;
				}
				if( ptAssocHit!=NULL )
				{
					/* yes -> pass packet data to the callback */
					ptAssocHit->pfn_recHandler(ptPkt, sizUdpPacketSize-sizeof(UDP_HEADER_T), ptAssocHit->pvUser);
				}
				else
				{
					DEBUGMSG(ZONE_VERBOSE, "[UDP] Port %d is not open.\n", uiDstPort);
				}
			}
			else
			{
				DEBUGMSG(ZONE_VERBOSE, "[UDP] Checksum mismatch: 0x%08x != 0x%08x\n", uiMyUdpChecksum, uiUdpChecksum);
			}
		}
		else
		{
		DEBUGMSG(ZONE_VERBOSE, "[UDP] Size exceeds packet size: %d %d\n", sizUdpPacketSize, sizTransferedSize);
		}
	}
	else
	{
		DEBUGMSG(ZONE_VERBOSE, "[UDP] Invalid size: %d\n", sizPacket);
	}
}



void udp_send_packet(ETH2_PACKET_T *ptPkt, size_t sizUdpUserData, UDP_ASSOCIATION_T *ptAssoc)
{
	size_t sizPacketSize;


	/* Is the pointer inside the array? */
	if( ptAssoc>=atUdpPortAssoc && ptAssoc<(atUdpPortAssoc+UDP_PORTLIST_MAX) )
	{
		/* Get the size of the complete packet. */
		sizPacketSize = sizeof(UDP_HEADER_T) + sizUdpUserData;

		/* Fill the part of the IP header which will be used for the checksum. */

		/* Set the requested protocol. */
		ptPkt->uEth2Data.tIpPkt.tIpHdr.ucProtocol = IP_PROTOCOL_UDP;
		/* Source IP is my IP. */
		ptPkt->uEth2Data.tIpPkt.tIpHdr.ulSrcIp = g_t_romloader_options.t_ethernet.ulIp;
		/* Set the requested destination IP. */
		ptPkt->uEth2Data.tIpPkt.tIpHdr.ulDstIp = ptAssoc->ulRemoteIp;


		ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usSrcPort = (unsigned short)ptAssoc->uiLocalPort;
		ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usDstPort = (unsigned short)ptAssoc->uiRemotePort;
		ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usLength = MUS2NUS(sizPacketSize);
		ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usChecksum = udp_buildChecksum(ptPkt, sizPacketSize);

		ipv4_send_packet(ptPkt, ptAssoc->ulRemoteIp, IP_PROTOCOL_UDP, sizPacketSize);
	}
}



UDP_ASSOCIATION_T *udp_registerPort(unsigned int uiLocalPort, unsigned long ulRemoteIp, unsigned int uiRemotePort, pfn_udp_receive_handler pfn_recHandler, void *pvUser)
{
	UDP_ASSOCIATION_T *ptAssocCnt;
	UDP_ASSOCIATION_T *ptAssocEnd;
	UDP_ASSOCIATION_T *ptAssocHit;


	/* Find a free port table entry. */
	ptAssocCnt = atUdpPortAssoc;
	ptAssocEnd = ptAssocCnt + UDP_PORTLIST_MAX;
	ptAssocHit = NULL;
	while( ptAssocCnt<ptAssocEnd )
	{
		if( ptAssocCnt->uiLocalPort==0 )
		{
			/* Found free port association. */
			ptAssocHit = ptAssocCnt;
			ptAssocHit->uiLocalPort = uiLocalPort;
			ptAssocHit->ulRemoteIp = ulRemoteIp;
			ptAssocHit->uiRemotePort = uiRemotePort;
			ptAssocHit->pfn_recHandler = pfn_recHandler;
			ptAssocHit->pvUser = pvUser;
			break;
		}
		++ptAssocCnt;
	}

	return ptAssocHit;
}



void udp_unregisterPort(UDP_ASSOCIATION_T *ptAssoc)
{
	if( ptAssoc>=atUdpPortAssoc && ptAssoc<(atUdpPortAssoc+UDP_PORTLIST_MAX) )
	{
		ptAssoc->uiLocalPort = 0;
	}
}

