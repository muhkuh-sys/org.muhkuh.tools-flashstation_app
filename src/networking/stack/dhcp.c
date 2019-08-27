/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <string.h>

#include "dhcp.h"

#include "systime.h"
#include "networking/stack/ipv4.h"
#include "networking/stack/udp.h"
#include "options.h"



#define DHCP_OP_BOOTREQUEST 1
#define DHCP_OP_BOOTREPLY 2

#define DHCP_DISCOVER_SRC_PORT 68
#define DHCP_DISCOVER_DST_PORT 67
#define DHCP_DISCOVER_DST_IP IP_ADR(255,255,255,255)


typedef enum
{
	DHCP_MSGTYP_DHCPDISCOVER	= 1,
	DHCP_MSGTYP_DHCPOFFER		= 2,
	DHCP_MSGTYP_DHCPREQUEST		= 3,
	DHCP_MSGTYP_DHCPDECLINE		= 4,
	DHCP_MSGTYP_DHCPACK		= 5,
	DHCP_MSGTYP_DHCPNAK		= 6,
	DHCP_MSGTYP_DHCPRELEASE		= 7,
	DHCP_MSGTYP_DHCPINFORM		= 8
} DHCP_MSG_TYP_T;


typedef enum
{
	DHCP_OPT_Pad				= 0,
	DHCP_OPT_SubnetMask			= 1,
	DHCP_OPT_TimeOffset			= 2,
	DHCP_OPT_Router				= 3,
	DHCP_OPT_TimeServer			= 4,
	DHCP_OPT_NameServer			= 5,
	DHCP_OPT_DomainNameServer		= 6,
	DHCP_OPT_LogServer			= 7,
	DHCP_OPT_CookieServer			= 8,
	DHCP_OPT_LPRServer			= 9,
	DHCP_OPT_ImpressServer			= 10,
	DHCP_OPT_RessourceLocationServer	= 11,
	DHCP_OPT_HostName			= 12,
	DHCP_OPT_BootFileSize			= 13,
	DHCP_OPT_MeritDumpFile			= 14,
	DHCP_OPT_DomainName			= 15,
	DHCP_OPT_SwapServer			= 16,
	DHCP_OPT_RootPath			= 17,
	DHCP_OPT_ExtensionsPath			= 18,
	DHCP_OPT_IpForwarding			= 19,
	DHCP_OPT_NonLocalSourceRouting		= 20,
	DHCP_OPT_PolicyFilter			= 21,
	DHCP_OPT_MaximumDatagramReassemblySize	= 22,
	DHCP_OPT_DefaultIpTTL			= 23,
	DHCP_OPT_PathMtuAgingTimeout		= 24,
	DHCP_OPT_PathMtuPlateauTable		= 25,
	DHCP_OPT_InterfaceMtu			= 26,
	DHCP_OPT_AllSubnetsAreLocal		= 27,
	DHCP_OPT_BroadcastAddress		= 28,
	DHCP_OPT_PerformMaskDiscovery		= 29,
	DHCP_OPT_MaskSupplier			= 30,
	DHCP_OPT_PerformRouterDiscovery		= 31,
	DHCP_OPT_RouterSolicitationAddress	= 32,
	DHCP_OPT_StaticRoute			= 33,
	DHCP_OPT_TrailerEncapsulation		= 34,
	DHCP_OPT_ARPCacheTimeout		= 35,
	DHCP_OPT_EthernetEncapsulation		= 36,
	DHCP_OPT_TCPDefaultTTL			= 37,
	DHCP_OPT_TCPKeepaliveInterval		= 38,
	DHCP_OPT_TCPKeepaliveGarbage		= 39,
	DHCP_OPT_NISDomain			= 40,
	DHCP_OPT_NISServers			= 41,
	DHCP_OPT_NTPServers			= 42,

	DHCP_OPT_RequestedIPAddress		= 50,
	DHCP_OPT_IPAddressLeaseTime		= 51,
	DHCP_OPT_DHCPMessageType		= 53,
	DHCP_OPT_ServerIdentifier		= 54,
	DHCP_OPT_ParameterRequestList		= 55,
	DHCP_OPT_Message			= 56,
	DHCP_OPT_MaximumDHCPMessageSize		= 57,

	DHCP_OPT_TFTPServerName			= 66,
	DHCP_OPT_BootfileName			= 67,

	DHCP_OPT_End				= 255
} DHCP_OPTION_T;


static DHCP_STATE_T tState;
static UDP_ASSOCIATION_T *ptAssoc;
static unsigned long ulLastGoodPacket;
static unsigned int uiRetryCnt;
static unsigned long ulXId;
static unsigned long ulRequestIp;
static unsigned long aucServerIdentifier[4];
static char ac_tftp_server_name[64];
static char ac_tftp_bootfile_name[128];



static const unsigned char aucDhcpMagic[4] =
{
	0x63, 0x82, 0x53, 0x63
};


static const unsigned char aucDhcpOptionDiscover[3] =
{
	DHCP_OPT_DHCPMessageType,  1, DHCP_MSGTYP_DHCPDISCOVER
};


static const unsigned char aucDhcpOptionRequest[3] =
{
	DHCP_OPT_DHCPMessageType,  1, DHCP_MSGTYP_DHCPREQUEST
};


static const unsigned char aucDhcpOptionParamReqList[7] =
{
	DHCP_OPT_ParameterRequestList,
	5,
	DHCP_OPT_SubnetMask,
	DHCP_OPT_Router,
	DHCP_OPT_DomainNameServer,
	DHCP_OPT_TFTPServerName,
	DHCP_OPT_BootfileName
};


static void dhcp_cleanup(void)
{
	if( ptAssoc!=NULL )
	{
		udp_unregisterPort(ptAssoc);
		ptAssoc = NULL;
	}
}


static int dhcp_send_discover_packet(void)
{
	int iResult;
	ETH2_PACKET_T *ptSendPacket;
	unsigned char *pucOpts;
	unsigned long ulOptsSize;


	/* get a free frame for sending */
	ptSendPacket = eth_get_empty_packet();
	if( ptSendPacket==NULL )
	{
		iResult = -1;
	}
	else
	{
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucOp = DHCP_OP_BOOTREQUEST;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHType = 1;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHLen = 6;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHops = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulXId = ulXId;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.usSecs = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.usFlags = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulCiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulYiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulSiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulGiAddr = 0;

		memcpy(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.aucChAddr, g_t_romloader_options.t_ethernet.aucMac, 6);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.aucChAddr+6, 0, 10);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.acSName, 0, 64);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.acFile, 0, 128);

		pucOpts = ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt);
		memcpy(pucOpts, aucDhcpMagic, sizeof(aucDhcpMagic));
		pucOpts += sizeof(aucDhcpMagic);
		memcpy(pucOpts, aucDhcpOptionDiscover, sizeof(aucDhcpOptionDiscover));
		pucOpts += sizeof(aucDhcpOptionDiscover);
		/* add parameter request list */
		memcpy(pucOpts, aucDhcpOptionParamReqList, sizeof(aucDhcpOptionParamReqList));
		pucOpts += sizeof(aucDhcpOptionParamReqList);
		/* end of list */
		*(pucOpts++) = DHCP_OPT_End;

		/* get the size of the options */
		ulOptsSize = (unsigned long)(pucOpts - ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt));
		/* fillup to a minimum of 342 bytes */
		if( ulOptsSize<342 )
		{
			memset(pucOpts, 0, 342-ulOptsSize);
			ulOptsSize = 342;
		}

		udp_send_packet(ptSendPacket, sizeof(DHCP_PACKET_T)+ulOptsSize, ptAssoc);

		iResult = 0;
	}

	return iResult;
}


static int dhcp_send_request_packet(void)
{
	int iResult;
	ETH2_PACKET_T *ptSendPacket;
	unsigned char *pucOpts;
	unsigned long ulOptsSize;


	/* get a free frame for sending */
	ptSendPacket = eth_get_empty_packet();
	if( ptSendPacket==NULL )
	{
		iResult = -1;
	}
	else
	{
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucOp = DHCP_OP_BOOTREQUEST;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHType = 1;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHLen = 6;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ucHops = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulXId = ulXId;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.usSecs = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.usFlags = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulCiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulYiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulSiAddr = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.ulGiAddr = 0;

		memcpy(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.aucChAddr, g_t_romloader_options.t_ethernet.aucMac, 6);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.aucChAddr+6, 0, 10);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.acSName, 0, 64);
		memset(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt.acFile, 0, 128);

		/* dhcp options start here */
		pucOpts = ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt);
		/* magic must be first */
		memcpy(pucOpts, aucDhcpMagic, sizeof(aucDhcpMagic));
		pucOpts += sizeof(aucDhcpMagic);
		/* add the packet typ */
		memcpy(pucOpts, aucDhcpOptionRequest, sizeof(aucDhcpOptionRequest));
		pucOpts += sizeof(aucDhcpOptionRequest);
		/* add server identifier */
		*(pucOpts++) = DHCP_OPT_ServerIdentifier;
		*(pucOpts++) = 4;
		memcpy(pucOpts, aucServerIdentifier, 4);
		pucOpts += 4;
		/* add requested ip */
		*(pucOpts++) = 50;
		*(pucOpts++) = 4;
		memcpy(pucOpts, &ulRequestIp, 4);
		pucOpts += 4;
		/* add parameter request list */
		memcpy(pucOpts, aucDhcpOptionParamReqList, sizeof(aucDhcpOptionParamReqList));
		pucOpts += sizeof(aucDhcpOptionParamReqList);
		/* end of list */
		*(pucOpts++) = DHCP_OPT_End;

		/* get the size of the options */
		ulOptsSize = (unsigned long)(pucOpts - ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt));
		/* fillup to a minimum of 342 bytes */
		if( ulOptsSize<342 )
		{
			memset(pucOpts, 0, 342-ulOptsSize);
			ulOptsSize = 342;
		}

		udp_send_packet(ptSendPacket, sizeof(DHCP_PACKET_T)+ulOptsSize, ptAssoc);

		iResult = 0;
	}

	return iResult;
}


static const unsigned char *dhcp_getOption(DHCP_PACKET_T *ptPacket, size_t sizPacket, unsigned int uiOption)
{
	const unsigned char *pucCnt;
	const unsigned char *pucOpt;
	const unsigned char *pucEnd;


	pucCnt = ((unsigned char*)ptPacket) + sizeof(DHCP_PACKET_T);
	pucOpt = NULL;
	pucEnd = pucCnt + sizPacket;

	if( pucCnt+4<=pucEnd )
	{
		/* check magic */
		if( memcmp(pucCnt, aucDhcpMagic, sizeof(aucDhcpMagic))==0 )
		{
			pucCnt += 4;

			/* loop over all options */
			while( pucCnt+2<=pucEnd )
			{
				if( *pucCnt==uiOption && (pucCnt+2+pucCnt[1])<=pucEnd )
				{
					pucOpt = pucCnt;
					break;
				}
				pucCnt += 2 + pucCnt[1];
			}
		}
	}

	return pucOpt;
}


static void dhcp_recHandler(void *pvData, size_t sizDhcpLength, void *pvUser __attribute__((unused)))
{
	ETH2_PACKET_T *ptPkt;
	DHCP_PACKET_T *ptDhcpPacket;
	int iResult;
	const unsigned char *pucOpt;
	size_t sizOption;
	unsigned long ulNewIp;
	unsigned long ulNewNetmask;
	unsigned long ulNewGatewayIp;
	unsigned long ulNewDnsIp;


	/* cast the data to a eth2 packet */
	ptPkt = (ETH2_PACKET_T*)pvData;
	ptDhcpPacket = &(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDhcpPkt);

	switch(tState)
	{

	case DHCP_STATE_Idle:
	case DHCP_STATE_Error:
	case DHCP_STATE_Ok:
		/* the connection is not open, ignore the packet */
		break;

	case DHCP_STATE_Discover:
		/* the packet must be a bootreply */
		if( ptDhcpPacket->ucOp==DHCP_OP_BOOTREPLY &&
		    ptDhcpPacket->ucHType==1 &&
		    ptDhcpPacket->ucHLen==6 &&
		    ptDhcpPacket->ulXId==ulXId &&
		    memcmp(ptDhcpPacket->aucChAddr, g_t_romloader_options.t_ethernet.aucMac, 6) == 0 &&
		    ptDhcpPacket->ulYiAddr!=0 )
		{
			/* is this a dhcp offer? */
			pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_DHCPMessageType);
			if( pucOpt!=NULL && pucOpt[1]==1 && pucOpt[2]==DHCP_MSGTYP_DHCPOFFER )
			{
				/* get the server identifier */
				pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_ServerIdentifier);
				if( pucOpt!=NULL && pucOpt[1]==4 )
				{
					memcpy(aucServerIdentifier, pucOpt+2, 4);
				}
				else
				{
					memset(aucServerIdentifier, 0, 4);
				}

				/* Request the IP. */
				ulRequestIp = ptDhcpPacket->ulYiAddr;

				iResult = dhcp_send_request_packet();
				if( iResult==0 )
				{
					/* state is now "request" */
					tState = DHCP_STATE_Request;

					/* reset timeout */
					ulLastGoodPacket = systime_get_ms();
					uiRetryCnt = g_t_romloader_options.t_ethernet.ucDhcpRetries;
				}
				else
				{
					/* cleanup */
					dhcp_cleanup();
				}
			}
		}
		break;

	case DHCP_STATE_Request:
		/* the packet must be a bootreply */
		if( ptDhcpPacket->ucOp==DHCP_OP_BOOTREPLY &&
		    ptDhcpPacket->ucHType==1 &&
		    ptDhcpPacket->ucHLen==6 &&
		    ptDhcpPacket->ulXId==ulXId &&
		    memcmp(ptDhcpPacket->aucChAddr, g_t_romloader_options.t_ethernet.aucMac, 6) == 0 &&
		    ptDhcpPacket->ulYiAddr!=0 )
		{
			/* invalidate the tftp server name and bootfile name */
			memset(ac_tftp_server_name, 0, sizeof(ac_tftp_server_name));
			memset(ac_tftp_bootfile_name, 0, sizeof(ac_tftp_bootfile_name));

			/* store the new ip address */
			ulNewIp = ptDhcpPacket->ulYiAddr;

			/* is this a dhcp offer? */
			pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_DHCPMessageType);
			if( pucOpt!=NULL && pucOpt[1]==1 )
			{
				if( pucOpt[2]==DHCP_MSGTYP_DHCPACK )
				{
					/* copy the server host name */
					/* NOTE: Do not copy the last byte of the buffer, this is already set to 0 to force termination of the string. */
					memcpy(ac_tftp_server_name, ptDhcpPacket->acSName, sizeof(ac_tftp_server_name)-1);
					/* copy the boot file name */
					/* NOTE: Do not copy the last byte of the buffer, this is already set to 0 to force termination of the string. */
					memcpy(ac_tftp_bootfile_name, ptDhcpPacket->acFile, sizeof(ac_tftp_bootfile_name)-1);

					/* check for the subnetmask option */
					pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_SubnetMask);
					if( pucOpt!=NULL && pucOpt[1]==4 )
					{
						/* store the new netmask */
						ulNewNetmask = IP_ADR(pucOpt[2], pucOpt[3], pucOpt[4], pucOpt[5]);

						/* check for the gateway option (router) */
						pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_Router);
						if( pucOpt!=NULL && pucOpt[1]==4 )
						{
							/* get the new gateway */
							ulNewGatewayIp = IP_ADR(pucOpt[2], pucOpt[3], pucOpt[4], pucOpt[5]);

							/* now the settings are complete, accept the stored values */
							g_t_romloader_options.t_ethernet.ulIp = ulNewIp;
							g_t_romloader_options.t_ethernet.ulNetmask = ulNewNetmask;
							g_t_romloader_options.t_ethernet.ulGatewayIp = ulNewGatewayIp;

							/* check for the domain name server option (NOTE: this is optional, the server ip can be preset or the server name is an ip) */
							pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_DomainNameServer);
							if( pucOpt!=NULL && pucOpt[1]>=4 )
							{
								ulNewDnsIp = IP_ADR(pucOpt[2], pucOpt[3], pucOpt[4], pucOpt[5]);
								g_t_romloader_options.t_ethernet.ulDnsIp = ulNewDnsIp;
							}

							/* check for the tftp server name option (NOTE: this is optional and overrides the sname field) */
							pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_TFTPServerName);
							if( pucOpt!=NULL )
							{
								sizOption = pucOpt[1];
								if( sizOption>0U && sizOption<sizeof(ac_tftp_server_name) )
								{
									/* option is present, override the server name */
									memcpy(ac_tftp_server_name, pucOpt+2, sizOption);
									/* terminate the server name */
									ac_tftp_server_name[sizOption] = 0;
								}
							}
							/* check for the boot file name option (NOTE: this is optional and overrides the bootfile field) */
							pucOpt = dhcp_getOption(ptDhcpPacket, sizDhcpLength, DHCP_OPT_BootfileName);
							if( pucOpt!=NULL )
							{
								sizOption = pucOpt[1];
								if( sizOption>0U && sizOption<sizeof(ac_tftp_bootfile_name) )
								{
									/* option is present, override the boot file name */
									memcpy(ac_tftp_bootfile_name, pucOpt+2, sizOption);
									/* terminate the boot file name */
									ac_tftp_bootfile_name[sizOption] = 0;
								}
							}

							/* if the tftp server name and the bootfile name are valid, copy them to the global options and clear the tftp servers ip */
							if( ac_tftp_server_name[0]!=0 && ac_tftp_bootfile_name[0]!=0 )
							{
								/* copy the server name */
								memcpy(g_t_romloader_options.t_ethernet.ac_tftp_server_name, ac_tftp_server_name, sizeof(ac_tftp_server_name));

								/* copy the bootfile name */
								memcpy(g_t_romloader_options.t_ethernet.ac_tftp_bootfile_name, ac_tftp_bootfile_name, sizeof(ac_tftp_bootfile_name));

								/* clear the server ip */
								g_t_romloader_options.t_ethernet.ulTftpIp = 0U;
							}

							/* cleanup */
							dhcp_cleanup();

							tState = DHCP_STATE_Ok;
						}
						else
						{
							/* cleanup */
							dhcp_cleanup();

							tState = DHCP_STATE_Error;
						}
					}
					else
					{
						/* cleanup */
						dhcp_cleanup();

						tState = DHCP_STATE_Error;
					}
				}
				else if( pucOpt[2]==DHCP_MSGTYP_DHCPNAK )
				{
					/* cleanup */
					dhcp_cleanup();

					tState = DHCP_STATE_Error;
				}
			}
		}
		break;
	}
}


void dhcp_init(void)
{
	tState = DHCP_STATE_Idle;

	/* No DHCP connection open. */
	ptAssoc = NULL;

	/* Initialize XID. */
	ulXId = ((unsigned long)(g_t_romloader_options.t_ethernet.aucMac[0])) |
	        (((unsigned long)(g_t_romloader_options.t_ethernet.aucMac[1])) <<  8U) |
		(((unsigned long)(g_t_romloader_options.t_ethernet.aucMac[2])) << 16U) |
		(((unsigned long)(g_t_romloader_options.t_ethernet.aucMac[3])) << 24U);
}


DHCP_STATE_T dhcp_getState(void)
{
	return tState;
}


void dhcp_request(void)
{
	int iResult;


	/* open UDP port and register callback */
	ptAssoc = udp_registerPort(MUS2NUS(DHCP_DISCOVER_SRC_PORT), DHCP_DISCOVER_DST_IP, MUS2NUS(DHCP_DISCOVER_DST_PORT), dhcp_recHandler, NULL);
	if( ptAssoc!=NULL )
	{
		++ulXId;
		iResult = dhcp_send_discover_packet();
		if( iResult==0 )
		{
			/* state is now "discover" */
			tState = DHCP_STATE_Discover;

			/* reset timeout */
			ulLastGoodPacket = systime_get_ms();
			uiRetryCnt = g_t_romloader_options.t_ethernet.ucDhcpRetries;

		}
		else
		{
			/* cleanup */
			dhcp_cleanup();

			tState = DHCP_STATE_Error;
		}
	}
}


void dhcp_timer(void)
{
	TIMER_HANDLE_T tHandle;
	int iRes;


	tHandle.ulStart = ulLastGoodPacket;
	tHandle.ulDuration = g_t_romloader_options.t_ethernet.usDhcpTimeout;

	switch(tState)
	{
	case DHCP_STATE_Idle:
	case DHCP_STATE_Error:
	case DHCP_STATE_Ok:
		break;

	case DHCP_STATE_Discover:
		iRes = systime_handle_is_elapsed(&tHandle);
		if( iRes!=0 )
		{
			/* Timeout -> are retries left? */
			if( uiRetryCnt>0 )
			{
				/* Re-send the last packet. */
				dhcp_send_discover_packet();

				ulLastGoodPacket = systime_get_ms();
				--uiRetryCnt;
			}
			else
			{
				/* Close the connection. */
				dhcp_cleanup();

				tState = DHCP_STATE_Error;
			}
		}
		break;

	case DHCP_STATE_Request:
		iRes = systime_handle_is_elapsed(&tHandle);
		if( iRes!=0 )
		{
			/* Timeout -> are retries left? */
			if( uiRetryCnt>0 )
			{
				/* Re-send the last packet. */
				dhcp_send_request_packet();

				ulLastGoodPacket = systime_get_ms();
				--uiRetryCnt;
			}
			else
			{
				/* Close the connection. */
				dhcp_cleanup();

				tState = DHCP_STATE_Error;
			}
		}
		break;
	}
}

