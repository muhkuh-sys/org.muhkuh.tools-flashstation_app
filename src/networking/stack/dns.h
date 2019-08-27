/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/

#ifndef __DNS_H__
#define __DNS_H__


typedef enum
{
	DNS_STATE_Idle			= 0,
	DNS_STATE_Request		= 1,
	DNS_STATE_Ok			= 2,
	DNS_STATE_NotFound		= 3,
	DNS_STATE_Error			= 4
} DNS_STATE_T;


void dns_init(void);

DNS_STATE_T dns_getState(void);

int dns_request(const char *pcName);

unsigned long dns_get_result_ip(void);

void dns_timer(void);


#endif	/* __DNS_H__ */

