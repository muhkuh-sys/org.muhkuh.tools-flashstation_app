/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#ifndef __DHCP_H__
#define __DHCP_H__


typedef enum
{
	DHCP_STATE_Idle			= 0,
	DHCP_STATE_Discover		= 1,
	DHCP_STATE_Request		= 2,
	DHCP_STATE_Error		= 3,
	DHCP_STATE_Ok			= 4
} DHCP_STATE_T;


void dhcp_init(void);

DHCP_STATE_T dhcp_getState(void);

void dhcp_request(void);

void dhcp_timer(void);


#endif	/* __DHCP_H__ */
