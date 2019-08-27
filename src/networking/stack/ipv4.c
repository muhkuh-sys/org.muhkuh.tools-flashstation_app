/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include "ipv4.h"

#include "networking/stack/arp.h"
#include "networking/stack/checksum.h"
#include "networking/stack/icmp.h"
#include "networking/stack/udp.h"
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



static unsigned int uiIpOutputId;


static unsigned long ipv4_get_destination_ip(unsigned long ulDstIp)
{
	unsigned long ulMyNetwork;
	unsigned long ulDstNetwork;


	/* Route the packet to the gateway if:
	 *  - the netmask is set (i.e. it is not 0)
	 *  - the destination is not the broadcast address
	 *  - the destination is not a multicast address
	 */
	if( g_t_romloader_options.t_ethernet.ulNetmask!=0 &&
	    ulDstIp!=IP_ADR(0xff, 0xff, 0xff, 0xff) &&
	    (ulDstIp&IP_ADR(0xf0, 0x00, 0x00, 0x00))!=IP_ADR(0xe0, 0x00, 0x00, 0x00)
	  )
	{
		/* get my network */
		ulMyNetwork = g_t_romloader_options.t_ethernet.ulIp & g_t_romloader_options.t_ethernet.ulNetmask;
		/* get the destination network */
		ulDstNetwork = ulDstIp & g_t_romloader_options.t_ethernet.ulNetmask;

		/* is the destination ip in my network? */
		if( ulMyNetwork!=ulDstNetwork )
		{
			/* no -> send this packet to the gateway */
			ulDstIp = g_t_romloader_options.t_ethernet.ulGatewayIp;
		}
	}

	/* return the destination ip */
	return ulDstIp;
}


void ipv4_init(void)
{
	uiIpOutputId = 0;
}


void ipv4_process_packet(ETH2_PACKET_T *ptEthPkt, size_t sizPacket)
{
	unsigned char ucValue;


	if( sizPacket>=(sizeof(ETH2_HEADER_T)+sizeof(IPV4_HEADER_T)) )
	{
		ucValue = ptEthPkt->uEth2Data.tIpPkt.tIpHdr.ucVersion;
		if( ucValue==IP_VERSION )
		{
			ucValue = ptEthPkt->uEth2Data.tIpPkt.tIpHdr.ucProtocol;

			switch( ucValue )
			{
			case IP_PROTOCOL_ICMP:
				icmp_process_packet(ptEthPkt, sizPacket);
				break;

			case IP_PROTOCOL_UDP:
				udp_process_packet(ptEthPkt, sizPacket);
				break;

			default:
				DEBUGMSG(ZONE_VERBOSE, "[IPV4] Ignoring unknown protocol type: %d\n", ucValue);
				break;
			}
		}
		else
		{
			DEBUGMSG(ZONE_VERBOSE, "[IPV4] Invalid IP version: %d\n", ucValue);
		}
	}
}



void ipv4_send_packet(ETH2_PACKET_T *ptPkt, unsigned long ulDstIp, unsigned int uiProtocol, size_t sizIpUserData)
{
	size_t sizIpPacket;
	unsigned long ulFirstDstIp;

	/* Get the size of the complete IP packet. */
	sizIpPacket = sizeof(IPV4_HEADER_T) + sizIpUserData;

	/* Fill the IP header. */

	/* Set the version and length. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ucVersion = IP_VERSION;
	/* Set the differentiated service field to default. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ucDSF = 0;
	/* Set the IP packet size. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usLength = MUS2NUS(sizIpPacket);
	/* Set the output ID- */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usId = MUS2NUS(uiIpOutputId);
	++uiIpOutputId;
	/* Set the 'do not fragment' flag. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usFlags = MUS2NUS(0x4000);
	/* Set the default time to live. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ucTTL = 64;
	/* Set the requested protocol. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ucProtocol = (unsigned char)uiProtocol;
	/* Clear the checksum for the calculation below. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usChecksum = 0;
	/* Source IP is my IP. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ulSrcIp = g_t_romloader_options.t_ethernet.ulIp;
	/* Set the requested destination IP. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.ulDstIp = ulDstIp;

	/* Calculate the checksum. */
	ptPkt->uEth2Data.tIpPkt.tIpHdr.usChecksum = MUS2NUS(checksum_add_complement(&ptPkt->uEth2Data.tIpPkt.tIpHdr, sizeof(IPV4_HEADER_T)));

	/* Get the destination IP. */
	ulFirstDstIp = ipv4_get_destination_ip(ulDstIp);

	/* Send the packet. */
	arp_send_ipv4_packet(ptPkt, sizIpPacket, ulFirstDstIp);
}

