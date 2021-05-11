#include <string.h>

#include "network_lwip.h"
#include "options.h"
#include "systime.h"
#include "uprintf.h"


#define ETHERNET_MAXIMUM_FRAMELENGTH                    1518

#include "lwip/etharp.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"

#include "lwip/apps/http_client.h"

#include "networking/network_interface.h"
#include "networking/driver/drv_eth_xc.h"


typedef struct HTTP_DOWNLOAD_STATE_STRUCT
{
	unsigned char *pucStart;  /* Start of the buffer for the downloaded data. */
	unsigned char *pucCnt;    /* The current end of the downloaded data. */
	unsigned char *pucEnd;    /* End of the available buffer. */

	SHA384_T tHash;           /* The SHA384 sum of the downloaded data. */

	httpc_result_fn pfnResult;
	unsigned int fIsFinished;
	httpc_result_t tHttpcResult;
	u32_t ulServerResponse;
	err_t tResult;
} HTTP_DOWNLOAD_STATE_T;



typedef struct NETWORK_DEVICE_STATE_STRUCT
{
	ip4_addr_t tIpAddr;
	ip4_addr_t tNetmask;
	ip4_addr_t tGateway;
	unsigned char aucMAC[6];

	const NETWORK_IF_T *ptNetworkIf;

	struct netif tNetIf;
} NETWORK_DEVICE_STATE_T;


static NETWORK_DEVICE_STATE_T tNetworkDeviceState;



void network_init(void)
{
	memset(&tNetworkDeviceState, 0, sizeof(tNetworkDeviceState));
}



static err_t netif_output(struct netif *ptNetIf, struct pbuf *ptPBuf)
{
	NETWORK_DEVICE_STATE_T *ptState;
	err_t tResult;
	void *pvFrame;


	/* Get the pointer to the interface. */
	tResult = ERR_VAL;
	ptState = (NETWORK_DEVICE_STATE_T*)(ptNetIf->state);
	if( ptState!=NULL )
	{
		pvFrame = ptState->ptNetworkIf->pfnGetEmptyPacket(NULL);
		if( pvFrame==NULL )
		{
			tResult = ERR_BUF;
		}
		else
		{
			pbuf_copy_partial(ptPBuf, pvFrame, ptPBuf->tot_len, 0);
			ptState->ptNetworkIf->pfnSendPacket(pvFrame, ptPBuf->tot_len, NULL);
			tResult = ERR_OK;
		}
	}

	return tResult;
}



static err_t initialize_interface(struct netif *ptNetIf)
{
	NETWORK_DEVICE_STATE_T *ptState;


	ptState = (NETWORK_DEVICE_STATE_T*)(ptNetIf->state);

	ptNetIf->linkoutput = netif_output;
	ptNetIf->output     = etharp_output;
	ptNetIf->mtu        = ETHERNET_MAXIMUM_FRAMELENGTH;
	ptNetIf->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET | NETIF_FLAG_IGMP | NETIF_FLAG_MLD6;

	SMEMCPY(ptNetIf->hwaddr, ptState->aucMAC, sizeof(ptNetIf->hwaddr));
	ptNetIf->hwaddr_len = sizeof(ptNetIf->hwaddr);

	return ERR_OK;
}



int setupNetwork(void)
{
	int iResult;
	const ETHERNET_CONFIGURATION_T *ptRomEthernetConfiguration = (const ETHERNET_CONFIGURATION_T*)0x00024a88U;
	unsigned long ulGw;
	unsigned long ulIp;
	unsigned long ulNm;
	void *pvUser;
	struct netif *ptNetIf;
	const NETWORK_IF_T *ptNetworkIf;


	iResult = -1;

	ptNetworkIf = drv_eth_xc_initialize(0);
	if( ptNetworkIf!=NULL )
	{
		ulGw = ptRomEthernetConfiguration->ulGatewayIp;
		ulIp = ptRomEthernetConfiguration->ulIp;
		ulNm = ptRomEthernetConfiguration->ulNetmask;

		uprintf("IP: %d.%d.%d.%d\n",
			ulIp&0xff,
			(ulIp>> 8U) & 0xffU,
			(ulIp>>16U) & 0xffU,
			(ulIp>>24U) & 0xffU
		);
		uprintf("NM: %d.%d.%d.%d\n",
			ulNm&0xff,
			(ulNm>> 8U) & 0xffU,
			(ulNm>>16U) & 0xffU,
			(ulNm>>24U) & 0xffU
		);
		uprintf("GW: %d.%d.%d.%d\n",
			ulGw&0xff,
			(ulGw>> 8U) & 0xffU,
			(ulGw>>16U) & 0xffU,
			(ulGw>>24U) & 0xffU
		);
		hexdump(ptRomEthernetConfiguration->aucMac, 6);

		IP4_ADDR(&(tNetworkDeviceState.tIpAddr),  ulIp&0xff, (ulIp>> 8U) & 0xffU, (ulIp>>16U) & 0xffU, (ulIp>>24U) & 0xffU);
		IP4_ADDR(&(tNetworkDeviceState.tGateway), ulGw&0xff, (ulGw>> 8U) & 0xffU, (ulGw>>16U) & 0xffU, (ulGw>>24U) & 0xffU);
		IP4_ADDR(&(tNetworkDeviceState.tNetmask), ulNm&0xff, (ulNm>> 8U) & 0xffU, (ulNm>>16U) & 0xffU, (ulNm>>24U) & 0xffU);
		memcpy(tNetworkDeviceState.aucMAC, ptRomEthernetConfiguration->aucMac, 6);

		tNetworkDeviceState.ptNetworkIf = ptNetworkIf;

		lwip_init();

		ptNetIf = &(tNetworkDeviceState.tNetIf);
		pvUser = (void*)&tNetworkDeviceState;
		netif_add(ptNetIf, &(tNetworkDeviceState.tIpAddr), &(tNetworkDeviceState.tNetmask), &(tNetworkDeviceState.tGateway), pvUser, initialize_interface, netif_input);
		ptNetIf->name[0] = 'e';
		ptNetIf->name[1] = '0';

		netif_set_default(ptNetIf);
		netif_set_up(ptNetIf);

		/* The link is already up at this point. */
		netif_set_link_up(ptNetIf);

		iResult = 0;
	}

	return iResult;
}



void network_cyclic_process(void)
{
	struct netif *ptNetIf;
	const NETWORK_IF_T *ptNetworkIf;
	unsigned int uiLinkState;
	void *pvFrame;
	unsigned int sizFrame;
	unsigned short usPacketSize;
	struct pbuf *ptPBuf;
	err_t tResult;


	ptNetIf = &(tNetworkDeviceState.tNetIf);
	ptNetworkIf = tNetworkDeviceState.ptNetworkIf;

	if( ptNetworkIf!=NULL )
	{
		uiLinkState = ptNetworkIf->pfnGetLinkStatus(NULL);
		if( uiLinkState==0 )
		{
			/* TODO: Move to an error state. */
			uprintf("The link is down.\n");
			netif_set_link_down(ptNetIf);
		}
		else
		{
			/* Check for received frames, feed them to lwIP */
			pvFrame = ptNetworkIf->pfnGetReceivedPacket(&sizFrame, NULL);
			if( pvFrame!=NULL )
			{
				/* Allocate a buffer from the pool. */
				usPacketSize = (unsigned short)(sizFrame);
				ptPBuf = pbuf_alloc(PBUF_RAW, usPacketSize, PBUF_POOL);
				if( ptPBuf==NULL )
				{
					ptNetworkIf->pfnReleasePacket(pvFrame, NULL);
				}
				else
				{
					/* Copy the Ethernet frame into the buffer. */
					pbuf_take(ptPBuf, pvFrame, usPacketSize);

					ptNetworkIf->pfnReleasePacket(pvFrame, NULL);

					tResult = ptNetIf->input(ptPBuf, ptNetIf);
					if( tResult!=ERR_OK)
					{
						pbuf_free(ptPBuf);
					}
				}
			}
		}

		/* Cyclic lwIP timers check */
		sys_check_timeouts();
	}
}



static err_t httpDownloadDataCallback(void *pvUser, struct altcp_pcb *conn, struct pbuf *p, err_t err)
{
	HTTP_DOWNLOAD_STATE_T *ptHttpState;
	uint16_t sizChunk;
	struct pbuf *ptBuf;
	unsigned char *pucRec;
	unsigned int sizBufLeft;
	void *pvChunk;
	err_t tResult;


	tResult = ERR_OK;

	ptHttpState = (HTTP_DOWNLOAD_STATE_T*)pvUser;
	if( ptHttpState!=NULL && p!=NULL && err==ERR_OK )
	{
		/* Get the space left in the buffer in bytes. */
		sizBufLeft = (unsigned int)(ptHttpState->pucEnd - ptHttpState->pucCnt);

		/* Loop over all chunks in the received data. */
		pucRec = ptHttpState->pucCnt;
		ptBuf = p;
		while( ptBuf!=NULL )
		{
			/* Get the next chunk in the linked list. */
			pvChunk = ptBuf->payload;
			sizChunk = ptBuf->len;

			/* Add the chunk data to the hash sum. */
			sha384_update(pvChunk, sizChunk);

			/* Copy the data to the download buffer - if there is one. */
			if( pucRec!=NULL )
			{
				/* Does the chunk fit into the buffer? */
				if( sizBufLeft>sizChunk )
				{
					/* Yes -> copy. */
					memcpy(pucRec, pvChunk, sizChunk);
					pucRec += sizChunk;
					sizBufLeft -= sizChunk;
				}
				else
				{
					/* No -> exit with error. */
					tResult = ERR_MEM;
					break;
				}
			}

			/* Move to the next chunk. */
			ptBuf = ptBuf->next;
		}
		ptHttpState->pucCnt = pucRec;

		altcp_recved(conn, p->tot_len);
		pbuf_free(p);
	}

	return tResult;
}



static void httpDownloadResultCallback(void *pvUser, httpc_result_t httpc_result, u32_t rx_content_len, u32_t srv_res, err_t err)
{
	HTTP_DOWNLOAD_STATE_T *ptHttpState;
	httpc_result_fn pfnResult;


	ptHttpState = (HTTP_DOWNLOAD_STATE_T*)pvUser;
	if( ptHttpState!=NULL )
	{
		pfnResult = ptHttpState->pfnResult;
		if( pfnResult!=NULL )
		{
			pfnResult(pvUser, httpc_result, rx_content_len, srv_res, err);
		}

		/* Only finish the hash if the download was successful. */
		if( httpc_result==HTTPC_RESULT_OK && srv_res==200 && err==ERR_OK )
		{
			sha384_finalize_byte(&(ptHttpState->tHash), rx_content_len);
		}

		ptHttpState->fIsFinished = 1U;
		ptHttpState->tHttpcResult = httpc_result;
		ptHttpState->ulServerResponse = srv_res;
		ptHttpState->tResult = err;
	}
}



int httpDownload(ip4_addr_t *ptServerIpAddr, const char *pcUri, unsigned char *pucBuffer, unsigned int sizBuffer, unsigned int *psizDownloaded, SHA384_T *ptHash)
{
	int iResult;
	err_t tResult;
	httpc_state_t *ptConnection;
	unsigned long ulTimeStart;
	unsigned long ulTimeEnd;
	httpc_connection_t tHttpConnection;
	HTTP_DOWNLOAD_STATE_T tHttpDownload;


	iResult = -1;

	tHttpDownload.pucStart = pucBuffer;
	tHttpDownload.pucCnt = pucBuffer;
	tHttpDownload.pucEnd = pucBuffer + sizBuffer;
	memset(&tHttpDownload.tHash, 0, sizeof(tHttpDownload.tHash));
	tHttpDownload.pfnResult = NULL;
	tHttpDownload.fIsFinished = 0U;
	tHttpDownload.tHttpcResult = 0;
	tHttpDownload.ulServerResponse = 0;
	tHttpDownload.tResult = 0;

	sha384_initialize();

	/* Get the system time in ms. */
	ulTimeStart = systime_get_ms();

	memset(&tHttpConnection, 0, sizeof(httpc_connection_t));
	tResult = httpc_get_file(ptServerIpAddr, HTTP_DEFAULT_PORT, pcUri, &tHttpConnection, httpDownloadDataCallback, &tHttpDownload, &ptConnection);
	if( tResult==ERR_OK )
	{
		/* Save the HTTP client result callback. */
		tHttpDownload.pfnResult = tHttpConnection.result_fn;
		/* Set the local result callback. */
		tHttpConnection.result_fn = httpDownloadResultCallback;

		/* Wait until the download is finished. */
		while( tHttpDownload.fIsFinished==0U )
		{
			network_cyclic_process();
		}

		/* Get the system time in ms. */
		ulTimeEnd = systime_get_ms();
		uprintf("Download finished after %dms: %d %d %d\n", ulTimeEnd-ulTimeStart, tHttpDownload.tHttpcResult, tHttpDownload.ulServerResponse, tHttpDownload.tResult);
		if( tHttpDownload.tHttpcResult==HTTPC_RESULT_OK && tHttpDownload.ulServerResponse==200 && tHttpDownload.tResult==ERR_OK )
		{
			if( psizDownloaded!=NULL )
			{
				*psizDownloaded = (unsigned int)(tHttpDownload.pucCnt-tHttpDownload.pucStart);
			}
			if( ptHash!=NULL )
			{
				memcpy(ptHash, &(tHttpDownload.tHash), sizeof(SHA384_T));
			}

			iResult = -1;
		}
	}

	return iResult;
}



typedef struct SEND_MESSAGE_CALLBACK_STRUCT
{
	unsigned int uiIsConnected;
	unsigned long ulSentData;
	err_t tResult;
	unsigned long ulChunkTransmitTimeoutInMs;
	TIMER_HANDLE_T tTimer;
} SEND_MESSAGE_CALLBACK_T;



static err_t sendMessageConnectCallback(void *pvUser, struct tcp_pcb *ptPcb __attribute__((unused)), err_t tErr)
{
	SEND_MESSAGE_CALLBACK_T *ptUser;


//	uprintf("Connect Callback %d\n", tErr);
	if( tErr==ERR_OK )
	{
		ptUser = (SEND_MESSAGE_CALLBACK_T*)pvUser;
		if( ptUser!=NULL )
		{
			ptUser->uiIsConnected = 1U;
		}
		else
		{
			tErr = ERR_VAL;
		}
	}

	return tErr;
}



static err_t sendMessageSentCallback(void *pvUser, struct tcp_pcb *ptPcb __attribute__((unused)), u16_t usLen)
{
	err_t tResult;
	SEND_MESSAGE_CALLBACK_T *ptUser;


//	uprintf("Sent Callback %d\n", usLen);
	ptUser = (SEND_MESSAGE_CALLBACK_T*)pvUser;
	if( ptUser!=NULL )
	{
		ptUser->ulSentData += usLen;

		/* Sent something. */
		systime_handle_start_ms(&(ptUser->tTimer), ptUser->ulChunkTransmitTimeoutInMs);

		tResult = ERR_OK;
	}
	else
	{
		tResult = ERR_VAL;
	}

	return tResult;
}



int sendMessage(ip4_addr_t *ptServerIpAddr, unsigned short usServerPort, const void *pvMessage, unsigned int sizMessage)
{
	int iResult;
	err_t tResult;
	struct tcp_pcb *ptPcb;
	const unsigned char *pucCnt;
	const unsigned char *pucEnd;
	unsigned int sizLeft;
	u16_t usChunk;
	u8_t ucFlags;
	SEND_MESSAGE_CALLBACK_T tCallbackData;
	const unsigned long ulConnectionTimeoutInMs = 5000U;
	const unsigned long ulChunkTransmitTimeoutInMs = 10000U;


	iResult = -1;

	tCallbackData.uiIsConnected = 0;
	tCallbackData.ulSentData = 0;
	tCallbackData.tResult = ERR_OK;

	/* Create a new PCB for the connection. */
	ptPcb = tcp_new();
	if( ptPcb==NULL )
	{
		uprintf("Failed to allocate a new pcb.\n");
	}
	else
	{
		/* Add the callback data to the PCB. */
		tcp_arg(ptPcb, &tCallbackData);

		/* Set a callback for transmitted data. */
		tcp_sent(ptPcb, sendMessageSentCallback);

		/* Connect. */
		tResult = tcp_connect(ptPcb, ptServerIpAddr, usServerPort, sendMessageConnectCallback);
		if( tResult!=ERR_OK )
		{
			uprintf("Failed to connect.\n");
		}
		else
		{
			/* Wait for the connection. */
			systime_handle_start_ms(&(tCallbackData.tTimer), ulConnectionTimeoutInMs);
			while( tCallbackData.tResult==ERR_OK && tCallbackData.uiIsConnected==0U )
			{
				network_cyclic_process();
				if( systime_handle_is_elapsed(&(tCallbackData.tTimer))!=0 )
				{
					tResult = ERR_TIMEOUT;
					break;
				}
			}
//			uprintf("%d: Connected: %d %d %d\n", systime_get_ms(), tResult, tCallbackData.tResult, tCallbackData.uiIsConnected);

			if( tResult==ERR_OK && tCallbackData.tResult==ERR_OK )
			{
				systime_handle_start_ms(&(tCallbackData.tTimer), ulChunkTransmitTimeoutInMs);
				tCallbackData.ulChunkTransmitTimeoutInMs = ulChunkTransmitTimeoutInMs;
				pucCnt = (const unsigned char*)pvMessage;
				pucEnd = pucCnt + sizMessage;
				while( pucCnt<pucEnd )
				{
					sizLeft = (unsigned int)(pucEnd-pucCnt);

					/* How many bytes can be send now?
					 * This is limited by the available space in the stack, which can be queried with tcp_sndbuf, and
					 * the amount of unsent bytes left in the buffer.
					 *
					 * This part also sets the flags for the write operation.
					 * If the remaining data in the buffer does not fit into the next send chunk,
					 * set "TCP_WRITE_FLAG_MORE" to indicate that there will be more data.
					 */
					ucFlags = TCP_WRITE_FLAG_MORE;
					usChunk = tcp_sndbuf(ptPcb);
					if( usChunk>0 )
					{
						if( usChunk>=sizLeft )
						{
							usChunk = (u16_t)sizLeft;
							ucFlags = 0U;
						}

						/* Queue the data. */
						tResult = tcp_write(ptPcb, pucCnt, usChunk, ucFlags);
						if( tResult!=ERR_OK )
						{
							uprintf("Failed to queue data: %d\n", tResult);
							break;
						}

						/* Move the buffer pointer to the next unsent data. */
						pucCnt += usChunk;
					}

					/* Check for a send timeout. */
					if( systime_handle_is_elapsed(&(tCallbackData.tTimer))!=0 )
					{
						uprintf("Send timeout!\n");
						tResult = ERR_TIMEOUT;
						break;
					}

					network_cyclic_process();
				}
				if( tResult==ERR_OK )
				{
					tResult = tcp_output(ptPcb);
					if( tResult!=ERR_OK )
					{
						uprintf("Pusing the data with tcp_output failed: %d\n", tResult);
					}
					else
					{
						/* Wait until all data is sent. */
						while( tCallbackData.ulSentData<sizMessage )
						{
							/* Check for a send timeout. */
							if( systime_handle_is_elapsed(&(tCallbackData.tTimer))!=0 )
							{
								uprintf("Push timeout!\n");
								tResult = ERR_TIMEOUT;
								break;
							}

							network_cyclic_process();
						}
					}
				}
			}

			if( tResult==ERR_OK )
			{
				tResult = tcp_close(ptPcb);
				if( tResult!=ERR_OK )
				{
					uprintf("Close failed: %d\n", tResult);
				}
			}

			if( tResult==ERR_OK )
			{
				/* All ok. */
				iResult = 0;
			}
			else
			{
				tcp_abort(ptPcb);
			}
		}
	}

	return iResult;
}
