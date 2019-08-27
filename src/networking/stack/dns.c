/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <string.h>

#include "networking/stack/dns.h"

#include "networking/stack/eth.h"
#include "networking/stack/ipv4.h"
#include "networking/stack/udp.h"

#include "options.h"
#include "rng.h"
#include "systime.h"


#if CFG_DEBUGMSG==1
#       include "uprintf_debug.h"

#       define DEBUGZONE(n)  (g_t_romloader_options.t_debug_settings.ul_networking_dns&(0x00000001<<(n)))

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

#       define DEBUGMSG(cond,...) ((void)((cond)?(uprintf_debug(__VA_ARGS__)),1:0))
#else
#       define DEBUGMSG(cond,...) ((void)0)
#endif


#define DNS_SRC_PORT 32768
#define DNS_DST_PORT 53
#define MDNS_PORT 5353

#define MSK_DNS_OPTION_RESPONSE                 0x8000U
#define SRT_DNS_OPTION_RESPONSE                 15
#define MSK_DNS_OPTION_OPCODE                   0x7800U
#define SRT_DNS_OPTION_OPCODE                   11
#define DNS_OPTION_OPCODE_STANDARD_QUERY        0x0000U
#define DNS_OPTION_AUTHORATIVE                  0x0400U
#define MSK_DNS_OPTION_TRUNCATED                0x0200U
#define DNS_OPTION_RECURSION_DESIRED            0x0100U
#define DNS_OPTION_RECURSION_AVAILABLE          0x0080U
#define DNS_OPTION_ANSWER_AUTHENTICATED         0x0020U
#define DNS_OPTION_ACCEPT_NON_AUTH_DATA         0x0010U
#define MSK_DNS_OPTION_REPLY_CODE               0x000fU


typedef enum
{
	DNS_QUERY_TYPE_A                        = 0x01,  /* a host address */
	DNS_QUERY_TYPE_NS                       = 0x02,  /* an authoritative name server */
	DNS_QUERY_TYPE_MD                       = 0x03,  /* a mail destination (Obsolete - use MX) */
	DNS_QUERY_TYPE_MF                       = 0x04,  /* a mail forwarder (Obsolete - use MX) */
	DNS_QUERY_TYPE_CNAME                    = 0x05,  /* the canonical name for an alias */
	DNS_QUERY_TYPE_SOA                      = 0x06,  /* marks the start of a zone of authority */
	DNS_QUERY_TYPE_MB                       = 0x07,  /* a mailbox domain name (EXPERIMENTAL) */
	DNS_QUERY_TYPE_MG                       = 0x08,  /* a mail group member (EXPERIMENTAL) */
	DNS_QUERY_TYPE_MR                       = 0x09,  /* a mail rename domain name (EXPERIMENTAL) */
	DNS_QUERY_TYPE_NULL                     = 0x0a,  /* a null RR (EXPERIMENTAL) */
	DNS_QUERY_TYPE_WKS                      = 0x0b,  /* a well known service description */
	DNS_QUERY_TYPE_PTR                      = 0x0c,  /* a domain name pointer */
	DNS_QUERY_TYPE_HINFO                    = 0x0d,  /* host information */
	DNS_QUERY_TYPE_MINFO                    = 0x0e,  /* mailbox or mail list information */
	DNS_QUERY_TYPE_MX                       = 0x0f,  /* mail exchange */
	DNS_QUERY_TYPE_TXT                      = 0x10,  /* text strings */
	DNS_QUERY_TYPE_SRV                      = 0x21,  /* Service Location. */
	DNS_QUERY_TYPE_AXFR                     = 0xfc,  /* A request for a transfer of an entire zone */
	DNS_QUERY_TYPE_MAILB                    = 0xfd,  /* A request for mailbox-related records (MB, MG or MR) */
	DNS_QUERY_TYPE_MAILA                    = 0xfe,  /* A request for mail agent RRs (Obsolete - see MX) */
	DNS_QUERY_TYPE_ALL                      = 0xff   /* A request for all records */
} DNS_QUERY_TYPE_E;


typedef enum
{
	DNS_QUERY_CLASS_IN			= 0x01,		/* the Internet */
	DNS_QUERY_CLASS_CS			= 0x02,		/* the CSNET class (Obsolete) */
	DNS_QUERY_CLASS_CH			= 0x03,		/* the CHAOS class */
	DNS_QUERY_CLASS_HS			= 0x04,		/* Hesiod [Dyer 87] */
	DNS_QUERY_CLASS_ALL			= 0xff		/* any class */
} DNS_QUERY_CLASS_T;

typedef enum
{
	DNS_REPLYCODE_Ok			= 0,		/* No error condition */
	DNS_REPLYCODE_FormatErr			= 1,		/* Format error */
	DNS_REPLYCODE_Failed			= 2,		/* Server failure */
	DNS_REPLYCODE_NameErr			= 3,		/* Name Error */
	DNS_REPLYCODE_NotImpl			= 4,		/* Not Implemented */
	DNS_REPLYCODE_Refused			= 5		/* Refused */
} DNS_REPLYCODE_T;


typedef struct
{
	unsigned short usType;
	unsigned short usClass;
	unsigned long ulTimeToLive;
	unsigned short usAddressLength;
} __attribute__((packed)) DNS_ANSWER_T;


static DNS_STATE_T tState;
static unsigned short us_dns_transaction_id_cnt;
static UDP_ASSOCIATION_T *ptAssoc;
static unsigned long ulLastGoodPacket;
static unsigned int uiRetryCnt;
static const char *pcHostName;
static unsigned long ulHostIp;
static unsigned char aucHostNameBuffer[64];
static unsigned char *pucBuffer;


static unsigned char *convert_hostname_to_dns(unsigned char *pucQuery, const char *pcName)
{
	unsigned char ucByte;
	unsigned char *pucChunkLength;


	/* reserve first  */
	pucChunkLength = pucQuery++;
	do
	{
		/* get the next byte */
		ucByte = (char)(*(pcName++));
		if( ucByte=='.' )
		{
			/*
			 * end of chunk
			 */

			/* set the length byte */
			*pucChunkLength = (unsigned char)(pucQuery-pucChunkLength-1);

			/* reserve new length position */
			pucChunkLength = pucQuery++;
		}
		else if( ucByte!=0 )
		{
			/*
			 * copy normal chars
			 */
			*(pucQuery++) = ucByte;
		}
	} while( ucByte!=0 );

	/* set the length byte */
	*pucChunkLength = (unsigned char)(pucQuery-pucChunkLength-1);
	/* terminate the string */
	*(pucQuery++) = 0U;

	/* return the new output pointer */
	return pucQuery;
}


static const unsigned char *skip_query(const unsigned char *pucQuery, const unsigned char *pucQueryEnd)
{
	unsigned char ucLength;


	/* skip the query */
	do
	{
		/* get the length of the next chunk */
		ucLength = *(pucQuery++);
		/* skip the chunk */
		pucQuery += ucLength;
	} while( pucQuery<pucQueryEnd && ucLength!=0U );

	/* skip the type and class information */
	pucQuery += sizeof(unsigned short) + sizeof(unsigned short);

	/* is the pointer already outside the packet? */
	if( pucQuery>=pucQueryEnd )
	{
		/* yes -> invalidate it */
		pucQuery = NULL;
	}

	/* return the new position */
	return pucQuery;
}


static const unsigned char *unpack_entry_recursive(const unsigned char *pucQuery, const unsigned char *pucPacketStart, const unsigned char *pucQueryEnd)
{
	unsigned char ucLength;
	const unsigned char *pucPtrEntry;


	do
	{
		/* get the length character */
		ucLength = *(pucQuery++);
		if( ucLength==0U )
		{
			break;
		}
		else if( ucLength<0x40U )
		{
			/* copy plaintext */
			memcpy(pucBuffer, pucQuery, ucLength);
			/* add length to pointers */
			pucBuffer += ucLength;
			pucQuery += ucLength;
			/* append dot to buffer */
			*(pucBuffer++) = '.';
		}
		else if( ucLength<0xc0U )
		{
			/* the range from 0x40..0xc0 is invalid */
			pucQuery = NULL;
		}
		else if( ucLength>=0xc0U )
		{
			/* copy pointer */
			pucPtrEntry = pucPacketStart + *(pucQuery++);
			pucPtrEntry = unpack_entry_recursive(pucPtrEntry, pucPacketStart, pucQueryEnd);
			if( pucPtrEntry==NULL )
			{
				pucQuery = NULL;
			}
			/* stop after a pointer */
			break;
		}
	} while( pucQuery!=NULL && pucQuery<pucQueryEnd );

	/* reached over the packet's end? */
	if( pucQuery>=pucQueryEnd )
	{
		pucQuery = NULL;
	}

	return pucQuery;
}


static const unsigned char *parse_answer(const unsigned char *pucQuery, const unsigned char *pucPacketStart, const unsigned char *pucQueryEnd, unsigned long *pulResultIp)
{
	unsigned int uiLength;
	size_t sizName;
	const DNS_ANSWER_T *ptDnsAnswer;
	const unsigned char *pucAddress;


	/* init the buffer pointer */
	pucBuffer = aucHostNameBuffer;

	/* depack the entry */
	pucQuery = unpack_entry_recursive(pucQuery, pucPacketStart, pucQueryEnd);
	/* was the entry valid? */
	if( pucQuery!=NULL )
	{
		/* the name must be at least one char */
		sizName = (size_t)(pucBuffer-aucHostNameBuffer);
		if( sizName==0 )
		{
			/* this is an error */
			pucQuery = NULL;
		}
		else
		{
			/* replace the last dot with a terminating 0 */
			aucHostNameBuffer[sizName-1] = 0;

			/* compare the names */
			if( memcmp(aucHostNameBuffer, pcHostName, sizName)==0 )
			{
				/* get a pointer to the answer structure */
				ptDnsAnswer = (const DNS_ANSWER_T *)pucQuery;

				/* get the length */
				uiLength = MUS2NUS(ptDnsAnswer->usAddressLength);

				/* the type must be "A" (host address) */
				if(
					ptDnsAnswer->usType==MUS2NUS(DNS_QUERY_TYPE_A) &&
					ptDnsAnswer->usClass==MUS2NUS(DNS_QUERY_CLASS_IN) &&
					uiLength==4
				)
				{
					/* valid response type, get the ip */
					pucAddress = ((const unsigned char*)ptDnsAnswer) + sizeof(DNS_ANSWER_T);
					*pulResultIp = IP_ADR(pucAddress[0], pucAddress[1], pucAddress[2], pucAddress[3]);
				}

				/* set pointer behind the address */
				pucQuery += sizeof(DNS_ANSWER_T) + uiLength;
			}
		}
	}

	return pucQuery;
}


static int dns_send_query_packet(void)
{
	int iResult;
	ETH2_PACKET_T *ptSendPacket;
	unsigned char *pucQuery;
	unsigned long ulQuerySize;


	/* get a free frame for sending */
	ptSendPacket = eth_get_empty_packet();
	if( ptSendPacket==NULL )
	{
		DEBUGMSG(ZONE_ERROR, "[dns] failed to get frame!\n");
		iResult = -1;
	}
	else
	{
		/* set the current transaction id */
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usTransactionId = MUS2NUS(us_dns_transaction_id_cnt);
		/* allow recursion */
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usFlags = MUS2NUS(DNS_OPTION_RECURSION_DESIRED);
		/* always send only 1 question */
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usQuestions = MUS2NUS(1);
		/* no answers yet */
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usAnswerRRs = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usAuthorityRRs = 0;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt.usAdditionalRRs = 0;

		/* start writing the options */
		pucQuery = ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt);

		/* convert string to dns name */
		pucQuery = convert_hostname_to_dns(pucQuery, pcHostName);
		/* set the type to "A" (hostname) */
		*(pucQuery++) = DNS_QUERY_TYPE_A>>8U;
		*(pucQuery++) = DNS_QUERY_TYPE_A&0xffU;
		/* set the class to "IN" */
		*(pucQuery++) = DNS_QUERY_CLASS_IN>>8U;
		*(pucQuery++) = DNS_QUERY_CLASS_IN&0xffU;

		/* get the size of the options */
		ulQuerySize = (unsigned long)(pucQuery - ETH_USER_DATA_ADR(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt));

		/* send the packet */
		udp_send_packet(ptSendPacket, sizeof(DNS_PACKET_T)+ulQuerySize, ptAssoc);

		iResult = 0;
	}

	return iResult;
}


static void dns_recHandler(void *pvData, size_t sizDnsLength, void *pvUser __attribute__((unused)))
{
	ETH2_PACKET_T *ptPkt;
	DNS_PACKET_T *ptDnsPacket;
	unsigned int uiFlags;
	const unsigned char *pucQuery;
	const unsigned char *pucQueryEnd;


	/* cast the data to a eth2 packet */
	ptPkt = (ETH2_PACKET_T*)pvData;
	/* get the dns packet */
	ptDnsPacket = &(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt);

	/* check for the minimum packet size */
	if( sizDnsLength<sizeof(DNS_PACKET_T) )
	{
		DEBUGMSG(ZONE_VERBOSE, "[dns] The packet is too small for a valid DNS packet.\n");
	}
	else
	{
		switch(tState)
		{
		case DNS_STATE_Idle:
		case DNS_STATE_Ok:
		case DNS_STATE_Error:
		case DNS_STATE_NotFound:
			/* the connection is not open, ignore the packet */
			DEBUGMSG(ZONE_VERBOSE, "[dns] received unwanted packet\n");
			break;

		case DNS_STATE_Request:
			uiFlags = NUS2MUS(ptDnsPacket->usFlags);
			/* compare the transaction id (is this a packet connected to our request?) */
			if( ptDnsPacket->usTransactionId!=MUS2NUS(us_dns_transaction_id_cnt) )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Received unknown transaction id: 0x%04x\n", ptDnsPacket->usTransactionId);
			}
			/* is this packet a response? */
			else if( (uiFlags&MSK_DNS_OPTION_RESPONSE)==0 )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Received a request, ignoring!\n");
			}
			/* the opcode must be 'standard query' */
			else if( (uiFlags&MSK_DNS_OPTION_OPCODE)!=0 )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Response is no standard query!\n");
			}
			/* the message must not be truncated */
			else if( (uiFlags&MSK_DNS_OPTION_TRUNCATED)!=0 )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Response is truncated!\n");
			}
			/* the reply code must be 'ok' */
			else if( (uiFlags&MSK_DNS_OPTION_REPLY_CODE)!=DNS_REPLYCODE_Ok )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Response status is not ok!\n");
				tState = DNS_STATE_Error;
			}
			/* number of questions must be 1 */
			else if( ptDnsPacket->usQuestions!=MUS2NUS(1U) )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] Strage number of questions: %d\n", ptDnsPacket->usQuestions);
				tState = DNS_STATE_NotFound;
			}
			/* we need at least one answer */
			else if( (ptDnsPacket->usAnswerRRs|ptDnsPacket->usAuthorityRRs|ptDnsPacket->usAdditionalRRs)==0U )
			{
				DEBUGMSG(ZONE_VERBOSE, "[dns] No answer to our request!\n");
				tState = DNS_STATE_NotFound;
			}
			else
			{
				/* get the pointer to the query start */
				pucQuery = ETH_USER_DATA_ADR(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tDnsPkt);
				/* get the pointer to the end of the packet */
				pucQueryEnd = ((unsigned char*)(&(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData))) + sizDnsLength;

				/* skip the query and compare the answers with my query. */
				pucQuery = skip_query(pucQuery, pucQueryEnd);
				if( pucQuery!=NULL )
				{
					/* invalidate ip */
					ulHostIp = 0U;

					/* parse all answers until one fits */
					do
					{
						pucQuery = parse_answer(pucQuery, ((unsigned char*)&(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData)), pucQueryEnd, &ulHostIp);
					} while( ulHostIp==0U && pucQuery!=NULL );

					/* received an IP? */
					if( ulHostIp==0U )
					{
						tState = DNS_STATE_NotFound;
					}
					else
					{
						DEBUGMSG(ZONE_VERBOSE, "[dns] received IP: %d.%d.%d.%d\n", ulHostIp&0xff, (ulHostIp>>8)&0xff, (ulHostIp>>16)&0xff, (ulHostIp>>24)&0xff);
						tState = DNS_STATE_Ok;
					}
				}
			}
			break;
		}
	}
}


static void dns_cleanup(void)
{
	if( ptAssoc!=NULL )
	{
		udp_unregisterPort(ptAssoc);
		ptAssoc = NULL;
	}
}


void dns_init(void)
{
	/* Initialize the DNS client state */
	tState = DNS_STATE_Idle;

	/* Generate a random transaction number. */
	us_dns_transaction_id_cnt = (unsigned short)rng_get_value();

	/* No UDP association established. */
	ptAssoc = NULL;

	/* No packet seen. */
	ulLastGoodPacket = 0;

	/* No retries. */
	uiRetryCnt = 0;

	/* No host name. */
	pcHostName = NULL;

	/* No result. */
	ulHostIp = 0U;
}


DNS_STATE_T dns_getState(void)
{
	return tState;
}


int dns_request(const char *pcName)
{
	int iResult;


	/* copy a refernece to the host name */
	pcHostName = pcName;

	/* open udp port and register callback */
	/* NOTE: do not register the port permanently in the init function. This is the client. */
	ptAssoc = udp_registerPort(MUS2NUS(DNS_SRC_PORT), g_t_romloader_options.t_ethernet.ulDnsIp, MUS2NUS(DNS_DST_PORT), dns_recHandler, NULL);
	if( ptAssoc==NULL )
	{
		DEBUGMSG(ZONE_ERROR, "[dns] failed to register port\n");
		iResult = -1;
	}
	else
	{
		/* generate a new transaction id */
		++us_dns_transaction_id_cnt;

		iResult = dns_send_query_packet();
		if( iResult==0 )
		{
			/* state is now "discover" */
			tState = DNS_STATE_Request;

			/* reset timeout */
			ulLastGoodPacket = systime_get_ms();
			uiRetryCnt = g_t_romloader_options.t_ethernet.ucDnsRetries;
		}
		else
		{
			/* cleanup */
			dns_cleanup();

			tState = DNS_STATE_Error;
		}
	}

	return iResult;
}


unsigned long dns_get_result_ip(void)
{
	return ulHostIp;
}


void dns_timer(void)
{
	TIMER_HANDLE_T tHandle;
	int iRes;


	switch(tState)
	{
	case DNS_STATE_Idle:
	case DNS_STATE_Ok:
	case DNS_STATE_Error:
	case DNS_STATE_NotFound:
		break;

	case DNS_STATE_Request:
		tHandle.ulStart = ulLastGoodPacket;
		tHandle.ulDuration = g_t_romloader_options.t_ethernet.usDnsTimeout;
		iRes = systime_handle_is_elapsed(&tHandle);
		if( iRes!=0 )
		{
			/* Timeout -> are retries left? */
			if( uiRetryCnt>0 )
			{
				/* Re-send the last packet. */
				DEBUGMSG(ZONE_VERBOSE, "[DNS] re-send query packet\n");
				dns_send_query_packet();

				ulLastGoodPacket = systime_get_ms();
				--uiRetryCnt;
			}
			else
			{
				DEBUGMSG(ZONE_VERBOSE, "[DNS] giving up!\n");

				/* Close the connection. */
				dns_cleanup();

				tState = DNS_STATE_Error;
			}
		}
		break;
	}
}

