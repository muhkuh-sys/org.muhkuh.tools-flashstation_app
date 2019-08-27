/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <stddef.h>

#include "networking/network_interface.h"


#ifndef __ETH_H__
#define __ETH_H__

/* machine unsigned short to network unsigned short */
#define MUS2NUS(a) ((unsigned short)((((a)&0xffU)<<8U)|(((a)>>8U)&0xffU)))
/* network unsigned short to machine unsigned short */
#define NUS2MUS(a) ((unsigned short)((((a)&0xffU)<<8U)|(((a)>>8U)&0xffU)))

#define MUS2NUSARR(a) ((unsigned char)(((a)>>8U)&0xffU)), ((unsigned char)((a)&0xffU))
#define MUL2NULARR(a) ((unsigned char)(((a)>>24U)&0xffU)), ((unsigned char)(((a)>>16U)&0xffU)), ((unsigned char)(((a)>>8U)&0xffU)), ((unsigned char)((a)&0xffU))

#define ETH2HEADER_TYP_IP	MUS2NUS(0x0800)
#define ETH2HEADER_TYP_ARP	MUS2NUS(0x0806)

#define ETH_USER_DATA_ADR(a) (((unsigned char*)&a)+sizeof(a))


typedef struct STRUCT_MAC_ADR
{
	unsigned char aucMac[6];
} __attribute__((packed)) tMacAdr;


typedef struct STRUCT_ETH2_HEADER
{
	tMacAdr tDstMac;
	tMacAdr tSrcMac;
	unsigned short usTyp;
} __attribute__((packed)) ETH2_HEADER_T;

typedef struct STRUCT_ARP_PACKET
{
	unsigned short usHardwareTyp;
	unsigned short usProtocolTyp;
	unsigned char ucHardwareSize;
	unsigned char ucProtocolSize;
	unsigned short usOpcode;
	tMacAdr tSrcMacAdr;
	unsigned long ulSrcIpAdr;
	tMacAdr tDstMacAdr;
	unsigned long ulDstIpAdr;
} __attribute__((packed)) ARP_PACKET_T;

typedef struct STRUCT_IPV4_HEADER
{
	unsigned char ucVersion;
	unsigned char ucDSF;
	unsigned short usLength;
	unsigned short usId;
	unsigned short usFlags;
	unsigned char ucTTL;
	unsigned char ucProtocol;
	unsigned short usChecksum;
	unsigned long ulSrcIp;
	unsigned long ulDstIp;
} __attribute__((packed)) IPV4_HEADER_T;

typedef struct STRUCT_ICMP_PACKET
{
	unsigned char ucType;
	unsigned char ucCode;
	unsigned short usChecksum;
	unsigned short usIdentifier;
	unsigned short usSequenceNumber;
//	unsigned char aucData[0];
} __attribute__((packed)) ICMP_PACKET_T;

typedef struct STRUCT_UDP_HEADER
{
	unsigned short usSrcPort;
	unsigned short usDstPort;
	unsigned short usLength;
	unsigned short usChecksum;
} __attribute__((packed)) UDP_HEADER_T;

typedef struct STRUCT_TFTP_PACKET
{
	unsigned short usOpcode;
	union
	{
		struct
		{
			unsigned short usBlockNr;
//			unsigned char aucData[0];
		} __attribute__((packed)) sData;
		struct
		{
			unsigned short usErrorCode;
//			unsigned char aucMessage[0];
		} __attribute__((packed)) sError;
		struct
		{
			unsigned short usBlockNr;
		} __attribute__((packed)) sAck;
	} __attribute__((packed)) uPacket;
} __attribute__((packed)) TFTP_PACKET_T;

typedef struct STRUCT_DHCP_PACKET
{
	unsigned char ucOp;
	unsigned char ucHType;
	unsigned char ucHLen;
	unsigned char ucHops;
	unsigned long ulXId;
	unsigned short usSecs;
	unsigned short usFlags;
	unsigned long ulCiAddr;
	unsigned long ulYiAddr;
	unsigned long ulSiAddr;
	unsigned long ulGiAddr;
	unsigned char aucChAddr[16];
	char acSName[64];
	char acFile[128];
//	unsigned char aucOptions[0];
} __attribute__((packed)) DHCP_PACKET_T;

typedef struct STRUCT_DNS_PACKET
{
	unsigned short usTransactionId;
	unsigned short usFlags;
	unsigned short usQuestions;
	unsigned short usAnswerRRs;
	unsigned short usAuthorityRRs;
	unsigned short usAdditionalRRs;
//	unsigned char aucQueries[0];
} __attribute__((packed)) DNS_PACKET_T;

typedef struct STRUCT_ETH2_PACKET
{
	ETH2_HEADER_T tEth2Hdr;
	union
	{
		struct
		{
			IPV4_HEADER_T tIpHdr;
			union
			{
				struct
				{
					UDP_HEADER_T tUdpHdr;
					union
					{
//						unsigned char aucUdpData[0];
						TFTP_PACKET_T tTftpPkt;
						DHCP_PACKET_T tDhcpPkt;
						DNS_PACKET_T tDnsPkt;
					} __attribute__((packed)) uUdpData;
				} __attribute__((packed)) tUdpPkt;
				ICMP_PACKET_T tIcmpPkt;
			} __attribute__((packed)) uIpData;
		} __attribute__((packed)) tIpPkt;
		ARP_PACKET_T tArpPkt;
	} __attribute__((packed)) uEth2Data;
} __attribute__((packed)) ETH2_PACKET_T;



extern const tMacAdr g_tBroadcastMac;
extern const tMacAdr g_tEmptyMac;


void eth_init(const NETWORK_IF_T *ptNetworkIf);

void eth_process_packet(void);

void eth_send_packet(ETH2_PACKET_T *ptPacket, size_t sizEthUserData, const tMacAdr *ptDstMac, unsigned int uiTyp);

ETH2_PACKET_T *eth_get_empty_packet(void);

void eth_release_packet(ETH2_PACKET_T *ptPacket);

//ETH2_PACKET_T *eth_receive_packet(size_t *psizPacket);

unsigned int eth_wait_for_link_up(void);
unsigned int eth_get_link_status(void);

void eth_deactivate(void);


#endif  /* __ETH_H__ */
