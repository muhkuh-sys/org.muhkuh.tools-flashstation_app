/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


//#include "boot_common.h"

#ifndef __BOOT_DRV_ETH_H__
#define __BOOT_DRV_ETH_H__


typedef struct STRUCT_ETHERNET_CONFIGURATION
{
	char ac_tftp_server_name[64];
	char ac_tftp_bootfile_name[128];
	unsigned char aucMac[6];
	unsigned short usTftpPort;
	unsigned long ulIp;
	unsigned long ulGatewayIp;
	unsigned long ulNetmask;
	unsigned long ulTftpIp;
	unsigned long ulDnsIp;
	unsigned long ulPhyControl;
	unsigned short ausLvdsPortControl[2];
	unsigned short usLinkUpDelay;
	unsigned short usArpTimeout;
	unsigned short usDhcpTimeout;
	unsigned short usDnsTimeout;
	unsigned short usTftpTimeout;
	unsigned char ucArpRetries;
	unsigned char ucDhcpRetries;
	unsigned char ucDnsRetries;
	unsigned char ucTftpRetries;
	unsigned char aucMmioCfg[2];
} ETHERNET_CONFIGURATION_T;


typedef enum ETHERNET_PORT_ENUM
{
	ETHERNET_PORT_None    = 0,
	ETHERNET_PORT_INTPHY0 = 1,
	ETHERNET_PORT_INTPHY1 = 2,
	ETHERNET_PORT_LVDS0   = 3,
	ETHERNET_PORT_LVDS1   = 4
} ETHERNET_PORT_T;

//BOOTING_T boot_eth(ETHERNET_PORT_T tPort);


#endif  /* __BOOT_DRV_ETH_H__ */
