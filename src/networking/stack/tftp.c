/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <stddef.h>
#include <string.h>

#include "tftp.h"

#include "networking/stack/buckets.h"
#include "networking/stack/ipv4.h"
#include "networking/stack/udp.h"
#include "options.h"
#include "sha384.h"
#include "systime.h"
#include "uprintf.h"



#define CFG_DEBUGMSG 1
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



static unsigned int uiPortCounter;

static UDP_ASSOCIATION_T *ptAssoc;
static const char *pcTftpFileName;
static unsigned int uiExpectedBlockNr;
static tTftpState tState;
static unsigned long ulLastGoodPacket;
static unsigned int uiRetryCnt;
static unsigned int uiBlockSize;
static char acBlockSize[6];
static size_t sizBlockSizeLen;
static unsigned long ulLoadedBytes;
static unsigned char *pucDstBuffer;
static unsigned char aucHash[48];


static const char acOctet[6] =
{
	/* "octet" in netascii */
	0x6f, 0x63, 0x74, 0x65, 0x74, 0x00
};

static const char acBlksize[8] =
{
	/* "blksize" in netascii */
	0x62, 0x6c, 0x6b, 0x73, 0x69, 0x7a, 0x65, 0x00
};


static void tftp_cleanup(void)
{
	if( ptAssoc!=NULL )
	{
		udp_unregisterPort(ptAssoc);
		ptAssoc = NULL;
	}
}



static int tftp_send_open_packet(void)
{
	size_t sizFileName;
	int iResult;
	ETH2_PACKET_T *ptSendPacket;


	/* Get a free frame for sending. */
	ptSendPacket = eth_get_empty_packet();
	if( ptSendPacket==NULL )
	{
		DEBUGMSG(ZONE_VERBOSE, "[TFTP] No free packet available.\n");
		iResult = -1;
	}
	else
	{
		sizFileName = strlen(pcTftpFileName) + 1U;

		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.usOpcode = TFTP_OPCODE_ReadReq;
		memcpy(((unsigned char*)&(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)), pcTftpFileName, sizFileName);
		memcpy(((unsigned char*)&(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)) + sizFileName, acOctet, sizeof(acOctet));
		memcpy(((unsigned char*)&(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)) + sizFileName + sizeof(acOctet), acBlksize, sizeof(acBlksize));

		/* Copy the block size. */
		memcpy(((unsigned char*)&(ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)) + sizFileName + sizeof(acOctet) + sizeof(acBlksize), acBlockSize, sizBlockSizeLen);

		udp_send_packet(ptSendPacket, sizeof(unsigned short)+sizFileName+sizeof(acOctet)+sizeof(acBlksize)+sizBlockSizeLen, ptAssoc);

		iResult = 0;
	}

	return iResult;
}



static int tftp_send_ack_packet(void)
{
	int iResult;
	ETH2_PACKET_T *ptSendPacket;


	ptSendPacket = eth_get_empty_packet();
	if( ptSendPacket==NULL )
	{
		DEBUGMSG(ZONE_VERBOSE, "[TFTP] No free packet available.\n");
		iResult = -1;
	}
	else
	{
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.usOpcode = TFTP_OPCODE_Ack;
		ptSendPacket->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket.sAck.usBlockNr = MUS2NUS(uiExpectedBlockNr);

		udp_send_packet(ptSendPacket, sizeof(unsigned short)+sizeof(unsigned short), ptAssoc);

		iResult = 0;
	}

	return iResult;
}



static void copy_data(unsigned char *pucDst, const unsigned char *pucSrc, unsigned int sizSrc)
{
	HOSTDEF(ptCryptArea);
	const unsigned char *pucSrcCnt;
	const unsigned char *pucSrcEnd;
	unsigned char ucData;


	pucSrcCnt = pucSrc;
	pucSrcEnd = pucSrc + sizSrc;
	while( pucSrcCnt<pucSrcEnd )
	{
		ucData = *(pucSrcCnt++);
		sha384_update_uc(ucData);
		*(pucDst++) = ucData;
	}
}



static void tftp_recHandler(void *pvData, size_t sizTftpLength, void *pvUser __attribute__((unused)))
{
	ETH2_PACKET_T *ptPkt;
	unsigned int uiOpcode;
	unsigned int uiBlockNr;
	unsigned int uiWantedBlockNr;
	size_t sizData;


	/* Cast the data to a TFTP packet. */
	ptPkt = (ETH2_PACKET_T*)pvData;
	/* Get the opcode. */
	uiOpcode = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.usOpcode;

	switch(tState)
	{
	case TFTPSTATE_Idle:
	case TFTPSTATE_Finished:
	case TFTPSTATE_Error:
		DEBUGMSG(ZONE_VERBOSE, "[TFTP] Ignoring incoming packet as the connection is not open.\n");
		/* The connection is not open, ignore the packet. */
		break;

	case TFTPSTATE_Init:
		/* The only valid responses are error and data. */
		if( uiOpcode==TFTP_OPCODE_Error )
		{
			DEBUGMSG(ZONE_VERBOSE, "[TFTP] Server sent error in init phase.\n");
			tState = TFTPSTATE_Error;
			tftp_cleanup();
			break;
		}
		else if( uiOpcode==TFTP_OPCODE_OptionAck )
		{
			/* Check the options. */
			if( sizTftpLength!=(sizeof(unsigned short)+sizeof(acBlksize)+sizBlockSizeLen) )
			{
				DEBUGMSG(ZONE_VERBOSE, "[TFTP] Received invalid length %d. Expected %d.\n", sizTftpLength, sizeof(unsigned short)+sizeof(acBlksize)+sizBlockSizeLen);
				tState = TFTPSTATE_Error;
				tftp_cleanup();
			}
			else if( memcmp(((unsigned char*)&(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)), acBlksize, sizeof(acBlksize))==0 && memcmp(((unsigned char*)&(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket)) + sizeof(acBlksize), acBlockSize, sizBlockSizeLen)==0 )
			{
				/* OK, the values match -> acknowledge this packet. */

				/* Get the new port. */
				ptAssoc->uiRemotePort = ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.tUdpHdr.usSrcPort;

				/* Send ACK with sequence number 0. */
				uiExpectedBlockNr = 0;
				tftp_send_ack_packet();

				/* Received a good packet. */
				ulLastGoodPacket = systime_get_ms();
				uiRetryCnt = g_t_romloader_options.t_ethernet.ucTftpRetries;

				tState = TFTPSTATE_Transfer;
			}
			else
			{
				DEBUGMSG(ZONE_VERBOSE, "[TFTP] Received invalid packet in init state.\n");
				tState = TFTPSTATE_Error;
				tftp_cleanup();
			}
			break;
		}
		else if( uiOpcode==TFTP_OPCODE_Data )
		{
			/* The server did not accept the options! */
			DEBUGMSG(ZONE_VERBOSE, "[TFTP] Server refused the options.\n");
			tState = TFTPSTATE_Error;
			tftp_cleanup();
			break;
		}
		else
		{
			break;
		}

	case TFTPSTATE_Transfer:
		/* The only valid packets are data and error. */
		if( uiOpcode==TFTP_OPCODE_Error )
		{
			DEBUGMSG(ZONE_VERBOSE, "[TFTP] Server sent error in transfer phase.\n");
			tState = TFTPSTATE_Error;
			tftp_cleanup();
		}
		else if( uiOpcode==TFTP_OPCODE_Data )
		{
			/* Check the sequence number. */
			uiBlockNr = NUS2MUS(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket.sData.usBlockNr);
			uiWantedBlockNr = (uiExpectedBlockNr + 1U) & 0xffffU;
			if( uiBlockNr==uiWantedBlockNr )
			{
				uiExpectedBlockNr = uiWantedBlockNr;

				/* Get the size of the data part. */
				sizData = sizTftpLength - sizeof(unsigned short) - sizeof(unsigned short);
				if( sizData!=0 )
				{
					/* Process data. */
					copy_data(pucDstBuffer, ETH_USER_DATA_ADR(ptPkt->uEth2Data.tIpPkt.uIpData.tUdpPkt.uUdpData.tTftpPkt.uPacket.sData), sizData);
					pucDstBuffer += sizData;
					ulLoadedBytes += sizData;
				}

				/* Send acknowledge. */
				tftp_send_ack_packet();

				/* Received a good packet. */
				ulLastGoodPacket = systime_get_ms();
				uiRetryCnt = g_t_romloader_options.t_ethernet.ucTftpRetries;

				if( sizData<uiBlockSize )
				{
					/* End of transfer. */
					DEBUGMSG(ZONE_VERBOSE, "[TFTP] End of data found.\n");
					tState = TFTPSTATE_Finished;
					tftp_cleanup();

					/* Finalize the SHA384 sum. */
					sha384_finalize_byte(aucHash, 12U, ulLoadedBytes);
				}
			}
			else
			{
				DEBUGMSG(ZONE_VERBOSE, "[TFTP] Ignoring unexpected block %d. Wanted %d.\n", uiBlockNr, uiExpectedBlockNr+1);
			}
		}
		break;
	}
}



tTftpState tftp_getState(void)
{
	return tState;
}



void tftp_init(void)
{
	/* Start using ports from 1024 on. */
	uiPortCounter = 1024;

	/* No TFTP connection open. */
	ptAssoc = NULL;

	/* Mode is "idle". */
	tState = TFTPSTATE_Idle;
}



static const unsigned int auiNumToAscii[4] =
{
	10,
	100,
	1000,
	10000
};



static void printBlocksize(char *pcPrint, unsigned int uiNum)
{
	int iIdx;
	int iCnt;
	size_t sizLen;
	int iPrintZeros;


	sizLen = 0;
	iIdx = 3;
	iPrintZeros = 0;
	while(iIdx>=0)
	{
		iCnt = 0;
		while( uiNum>=auiNumToAscii[iIdx] )
		{
			++iCnt;
			uiNum -= auiNumToAscii[iIdx];
		}
		if( iPrintZeros!=0 || iCnt>0 )
		{
			/* print the digit */
			pcPrint[sizLen] = (char)('0'|iCnt);
			++sizLen;
			/* From now on print all zeros. */
			iPrintZeros = 1;
		}
		--iIdx;
	}

	/* print the last digit */
	pcPrint[sizLen] = (char)('0'|uiNum);
	++sizLen;
	/* terminate the string */
	pcPrint[sizLen] = 0;
	++sizLen;

	sizBlockSizeLen = sizLen;
}



int tftp_open(const char *pcFileName, unsigned char *pucDst, unsigned int uiBlkSize)
{
	unsigned int uiTftpPort;
	int iResult;


	DEBUGMSG(ZONE_VERBOSE, "[TFTP] Loading file '%s' from server %08x:%d to 0x%08x.\n", pcFileName, g_t_romloader_options.t_ethernet.ulTftpIp,g_t_romloader_options.t_ethernet.usTftpPort, (unsigned long)pucDst);

	/* expect failure */
	iResult = -1;

	/* set the block size */
	uiBlockSize = uiBlkSize;

	/* Set the destination buffer. */
	pucDstBuffer = pucDst;

	/* Convert the requested block size to ASCII. */
	printBlocksize(acBlockSize, uiBlkSize);

	/* Allocate a new port. */
	uiTftpPort = uiPortCounter++;
	if( uiPortCounter>65535 )
	{
		uiPortCounter = 1024;
	}

	/* Initialize the SHA384 sum. */
	sha384_initialize();

	/* No bytes loaded yet. */
	ulLoadedBytes = 0;

	/* Open UDP port and register callback. */
	ptAssoc = udp_registerPort(MUS2NUS(uiTftpPort), g_t_romloader_options.t_ethernet.ulTftpIp, MUS2NUS(g_t_romloader_options.t_ethernet.usTftpPort), tftp_recHandler, NULL);
	if( ptAssoc!=NULL )
	{
		/* save the name */
		pcTftpFileName = pcFileName;

		/* open TFTP connection */
		iResult = tftp_send_open_packet();
		if( iResult==0 )
		{
			/* state is now "init" */
			tState = TFTPSTATE_Init;

			/* reset timeout */
			ulLastGoodPacket = systime_get_ms();
			uiRetryCnt = g_t_romloader_options.t_ethernet.ucTftpRetries;
		}
		else
		{
			/* cleanup */
			tftp_cleanup();
		}
	}
	else
	{
		DEBUGMSG(ZONE_VERBOSE, "[TFTP] Failed to register UDP association.\n");
	}

	return iResult;
}



void tftp_timer(void)
{
	TIMER_HANDLE_T tHandle;
	int iRes;


	/* is a transfer in progress? */
	if( (tState==TFTPSTATE_Init) || (tState==TFTPSTATE_Transfer) )
	{
		/* yes -> check the time since the last good packet */
		tHandle.ulStart = ulLastGoodPacket;
		tHandle.ulDuration = g_t_romloader_options.t_ethernet.usTftpTimeout;
		iRes = systime_handle_is_elapsed(&tHandle);
		if( iRes!=0 )
		{
			/* timeout -> are retries left? */
			if( uiRetryCnt>0 )
			{
				/* re-send the last packet */
				if( tState==TFTPSTATE_Init )
				{
					/* re-send the open packet */
					tftp_send_open_packet();
				}
				else if( tState==TFTPSTATE_Transfer )
				{
					/* re-send the ACK */
					tftp_send_ack_packet();
				}

				ulLastGoodPacket = systime_get_ms();
				--uiRetryCnt;
			}
			else
			{
				/* close the connection */
				DEBUGMSG(ZONE_VERBOSE, "[TFTP] No more retries left in state %d.\n", tState);
				tState = TFTPSTATE_Error;
				tftp_cleanup();
			}
		}
	}
}



unsigned char *tftp_getDataPointer(void)
{
	return pucDstBuffer;
}



unsigned long tftp_getLoadedBytes(void)
{
	return ulLoadedBytes;
}



unsigned long *tftp_getHash(void)
{
	return aucHash;
}
