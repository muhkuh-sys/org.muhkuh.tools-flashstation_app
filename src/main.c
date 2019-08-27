
#include <string.h>

#include "blinki_codes.h"
#include "netx_io_areas.h"
#include "options.h"
#include "rdy_run.h"
#include "snuprintf.h"
#include "systime.h"
#include "uart_standalone.h"
#include "uprintf.h"
#include "version.h"

#include "networking/driver/drv_eth_xc.h"
#include "networking/stack/arp.h"
#include "networking/stack/buckets.h"
#include "networking/stack/dhcp.h"
#include "networking/stack/dns.h"
#include "networking/stack/icmp.h"
#include "networking/stack/ipv4.h"
#include "networking/stack/tftp.h"
#include "networking/stack/udp.h"

#include "flasher_interface.h"
#include "flasher_sdio.h"
#include "flasher_version.h"

/*-------------------------------------------------------------------------*/


static TIMER_HANDLE_T tEthernetTimer;

typedef int (*PFN_ROM_TFTP_SEND_ACK_T) (void);

/* Send an ACK for the last TFTP packet. */
static int ackLastRomcodePacket(void)
{
	volatile unsigned int *pulCurrentBlockNumber = (volatile unsigned int*)0x000225c0U;
	PFN_ROM_TFTP_SEND_ACK_T pfnRomTftpSendAck = (PFN_ROM_TFTP_SEND_ACK_T)0x04108bd5U;
	unsigned int uiCurrentBlockNumber;
	int iResult;


	/* Increase the current TFTP block number. */
	uiCurrentBlockNumber = *pulCurrentBlockNumber;
	uprintf("block number: 0x%08x\n", uiCurrentBlockNumber);
	++uiCurrentBlockNumber;
	uprintf("sending ACK for block %d.\n", uiCurrentBlockNumber);
	*pulCurrentBlockNumber = uiCurrentBlockNumber;
	iResult = pfnRomTftpSendAck();
	uprintf("ROM ACK returned %d.\n", iResult);

	return iResult;
}



static int initializeNetwork(void)
{
	int iResult;
	const NETWORK_IF_T *ptNetworkIf;
	const ETHERNET_CONFIGURATION_T *ptRomEthernetConfiguration = (const ETHERNET_CONFIGURATION_T*)0x00024a88U;


	iResult = -1;

	ptNetworkIf = drv_eth_xc_initialize(0);
	if( ptNetworkIf!=NULL )
	{
		buckets_init();

		eth_init(ptNetworkIf);
		arp_init();
		ipv4_init();
		icmp_init();
		udp_init();
		tftp_init();
		dhcp_init();
		dns_init();

		systime_handle_start_ms(&tEthernetTimer, 1000);

		/* Copy the ROM Ethernet settings. */
		memcpy(&(g_t_romloader_options.t_ethernet), ptRomEthernetConfiguration, sizeof(ETHERNET_CONFIGURATION_T));

		iResult = 0;
	}

	return iResult;
}



static void ethernet_cyclic_process(void)
{
	eth_process_packet();

	if( systime_handle_is_elapsed(&tEthernetTimer)!=0 )
	{
		/* process cyclic events here */
		arp_timer();
		tftp_timer();
		dhcp_timer();
		dns_timer();

		systime_handle_start_ms(&tEthernetTimer, 1000);
	}
}



static int tftp_load_file(const char *pcFileName, unsigned char *pucDataBuffer, unsigned char **ppucDataBufferEnd, unsigned char *pucHash)
{
	int iResult;
	tTftpState tState;
	unsigned char *pucDataStart;
	unsigned char *pucDataEnd;
	unsigned int uiCnt;
	int iIsElapsed;
	TIMER_HANDLE_T tProgress;
	const unsigned char *pucTftpHash;


	/* Process any waiting Ethernet packets to make some room. */
	for(uiCnt=0; uiCnt<32; ++uiCnt)
	{
		ethernet_cyclic_process();
	}

	/* Open the TFTP connection. */
	pucDataStart = pucDataBuffer;
	iResult = tftp_open(pcFileName, pucDataStart, 1024);
	if( iResult!=0 )
	{
		uprintf("Failed to open TFTP file.\n");
	}
	else
	{
		systime_handle_start_ms(&tProgress, 2000U);

		do
		{
			ethernet_cyclic_process();

			tState = tftp_getState();
			switch( tState )
			{
			case TFTPSTATE_Idle:
				/* The TFTP layer should not be idle, there is an open connection. */
				iResult = -1;
				break;

			case TFTPSTATE_Init:
				/* Sending the "open" packet and acknowledge the options. */
				break;

			case TFTPSTATE_Transfer:
				/* Transfering data... */
				break;

			case TFTPSTATE_Finished:
				/* Finished to transfer the data. */
				iResult = 1;
				break;

			case TFTPSTATE_Error:
				/* Transfer failed. */
				iResult = -1;
				break;
			}

			iIsElapsed = systime_handle_is_elapsed(&tProgress);
			if( iIsElapsed!=0 )
			{
				uprintf("Loaded 0x%08x bytes...\n", tftp_getLoadedBytes());

				systime_handle_start_ms(&tProgress, 2000U);
			}
		} while( iResult==0 );

		uprintf("State: %d, result: %d\n", tState, iResult);

		if( iResult!=1 )
		{
			uprintf("Failed to load the TFTP data.\n");
		}
		else
		{
			uprintf("TFTP data loaded successfully.\n");
			pucDataEnd = tftp_getDataPointer();
			uprintf("Data: 0x%08x - 0x%08x\n", pucDataStart, pucDataEnd);
			pucTftpHash = (const unsigned char*)tftp_getHash();
//			hexdump(pucTftpHash, 48);

			if( ppucDataBufferEnd!=NULL )
			{
				*ppucDataBufferEnd = pucDataEnd;
			}
			if( pucHash!=NULL )
			{
				memcpy(pucHash, pucTftpHash, 48);
			}

			iResult = 0;
		}
	}

	return iResult;
}



static char acFileNameData[1024];
static unsigned char aucDataHash[48];
static unsigned long ulDataFileSize;

static int getDeviceInfo(void)
{
	unsigned long ulDeviceNumber;
	unsigned long ulHardwareRevision;
	unsigned long ulDeviceNumberHi;
	unsigned long ulDeviceNumberLo;
	int iResult;
	unsigned char *pucBuffer;
	char *pcCnt;
	unsigned char *pucInfoEnd;
	char *pcInfoEnd;
	unsigned int uiLineSize;
	unsigned int uiShift;
	unsigned int uiInc;
	char cDigit;
	unsigned char ucData;
	unsigned char *pucHash;
	char acFileNameInfo[32];


	/* TODO: Get this from the FDL maybe? */
	ulDeviceNumber = 1320102;
	ulHardwareRevision = 3;

	ulDeviceNumberHi = ulDeviceNumber / 1000U;
	ulDeviceNumberLo = ulDeviceNumber - (1000U * ulDeviceNumberHi);

	snuprintf(acFileNameInfo, sizeof(acFileNameInfo), "%d.%dR%d.txt", ulDeviceNumberHi, ulDeviceNumberLo, ulHardwareRevision);
	uprintf("Found hardware %d.%d R%d, loading info file '%s'.\n", ulDeviceNumberHi, ulDeviceNumberLo, ulHardwareRevision, acFileNameInfo);

	/* Load the info file with TFTP. Use the DDR as a buffer. */
	pucBuffer = (unsigned char*)0x40000000U;
	iResult = tftp_load_file(acFileNameInfo, pucBuffer, &pucInfoEnd, NULL);
	if( iResult==0 )
	{
		/* Try to parse the Info file. */
		pcInfoEnd = (char*)pucInfoEnd;

		/* Look for the first newline. */
		pcCnt = (char*)pucBuffer;
		uiLineSize = 0;
		while( pcCnt<pcInfoEnd )
		{
			if( *pcCnt=='\n' )
			{
				break;
			}
			else
			{
				++pcCnt;
				++uiLineSize;
			}
		}

		if( pcCnt<pcInfoEnd )
		{
			if( uiLineSize<sizeof(acFileNameData)-1 )
			{
				/* Copy the filename. */
				memcpy(acFileNameData, pucBuffer, uiLineSize);
				/* Terminate the filename. */
				acFileNameData[uiLineSize] = 0x00;

				/* Skip the newline. */
				++pcCnt;

				/* Expect 96 hex digits. */
				pucHash = aucDataHash;
				memset(aucDataHash, 0, sizeof(aucDataHash));
				if( (pcCnt+96)<pcInfoEnd )
				{
					uiLineSize = 0;
					do
					{
						/* The shift value should alternate between 4 and 0, so that...
						 *  pos   012345678...
						 *  inc   010101010...
						 *  shift 404040404...
						 */
						uiInc = uiLineSize & 1U;
						uiShift = (uiInc ^ 1U) << 2U;
						cDigit = pcCnt[uiLineSize];
						if( cDigit>='0' && cDigit<='9' )
						{
							ucData = (unsigned char)((cDigit - '0') << uiShift);
							*pucHash |= ucData;
							pucHash += uiInc;
							++uiLineSize;
						}
						else if( cDigit>='a' && cDigit<='f' )
						{
							ucData = (unsigned char)(((cDigit - 'a') + 10) << uiShift);
							*pucHash |= ucData;
							pucHash += uiInc;
							++uiLineSize;
						}
						else if( cDigit>='A' && cDigit<='F' )
						{
							ucData = (unsigned char)(((cDigit - 'A') + 10) << uiShift);
							*pucHash |= ucData;
							pucHash += uiInc;
							++uiLineSize;
						}
						else
						{
							break;
						}
					} while( uiLineSize<96 );
					if( uiLineSize==96 )
					{
						/* The hash is OK. */
						iResult = 0;
					}
					else
					{
						/* Failed to parse the hash. */
						uprintf("Failed to parse the hash.\n");
						iResult = -1;
					}
				}
				else
				{
					uprintf("The info file is too small to hold a SHA384 after the file name.\n");
					iResult = -1;
				}
			}
			else
			{
				uprintf("The filename must not exceed %d bytes. Here it has %d bytes.\n", sizeof(acFileNameData)-1, uiLineSize);
				iResult = -1;
			}
		}
		else
		{
			uprintf("No newline found in info file!\n");
			iResult = -1;
		}
	}
	else
	{
		uprintf("Failed to load the info file.\n");
	}

	return iResult;
}



static int getDataFile(void)
{
	int iResult;
	unsigned char *pucBuffer;
	unsigned char aucMyHash[48];


	iResult = -1;

	uprintf("Reading data file '%s' with the expected hash:\n", acFileNameData);
	hexdump(aucDataHash, sizeof(aucDataHash));

	/* Load the info file with TFTP. Use the DDR as a buffer. */
	pucBuffer = (unsigned char*)0x40000000U;
	iResult = tftp_load_file(acFileNameData, pucBuffer, NULL, aucMyHash);
	if( iResult==0 )
	{
		/* Compare the hash. */
		if( memcmp(aucMyHash, aucDataHash, sizeof(aucDataHash))==0 )
		{
			uprintf("The data file is OK.\n");

			ulDataFileSize = tftp_getLoadedBytes();

			iResult = 0;
		}
		else
		{
			uprintf("The hash of the received file does not match the value in the info file.\n");
		}
	}
	else
	{
		uprintf("Failed to load the data file.\n");
	}

	return iResult;
}



static int flashDataFile(DEVICE_DESCRIPTION_T *ptDeviceDesc)
{
	int iResult;
	NETX_CONSOLEAPP_RESULT_T tFlasherResult;
	CMD_PARAMETER_FLASH_T tParams;


	uprintf("Flashing data file.\n");

	iResult = -1;

	tParams.ptDeviceDescription = ptDeviceDesc;
	tParams.ulStartAdr = 0U;
	tParams.ulDataByteSize = ulDataFileSize;
	tParams.pucData = (unsigned char*)0x40000000U;

	tFlasherResult = sdio_write(&tParams);
	if( tFlasherResult==NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("Flashing OK.\n");
		iResult = 0;
	}
	else
	{
		uprintf("Failed to flash.\n");
	}

	return iResult;
}



/*-------------------------------------------------------------------------*/


UART_STANDALONE_DEFINE_GLOBALS


void flashapp_main(void);
void flashapp_main(void)
{
	BLINKI_HANDLE_T tBlinkiHandle;
	NETX_CONSOLEAPP_RESULT_T tResult;
	DEVICE_DESCRIPTION_T tDeviceDesc;
	int iResult;


	systime_init();
	uart_standalone_initialize();

	uprintf("\f. *** Flasher APP by doc_bacardi@users.sourceforge.net ***\n");
	uprintf("V" VERSION_ALL "\n\n");
	uprintf("Using flasher V" FLASHER_VERSION_ALL " " FLASHER_VERSION_VCS "\n");

	/* Switch all LEDs off. */
	rdy_run_setLEDs(RDYRUN_OFF);

	/* Send the last ACK packet for the ROM transfer. */
	iResult = ackLastRomcodePacket();
	if( iResult!=0 )
	{
		uprintf("Failed to acknowledge the last ROM packet.\n");
	}
	else
	{
		uprintf("Initializing network...\n");
		iResult = initializeNetwork();
		if( iResult!=0 )
		{
			uprintf("Failed to initialize the network.\n");
			tResult = NETX_CONSOLEAPP_RESULT_ERROR;
		}
		else
		{
			/* Detect the eMMC. */
			tResult = sdio_detect_wrap(&(tDeviceDesc.uInfo.tSdioHandle));
			if( tResult==NETX_CONSOLEAPP_RESULT_OK )
			{
				uprintf("Found eMMC.\n");

				tDeviceDesc.fIsValid = 1;
				tDeviceDesc.sizThis = sizeof(DEVICE_DESCRIPTION_T);
				tDeviceDesc.ulVersion = FLASHER_INTERFACE_VERSION;
				tDeviceDesc.tSourceTyp = BUS_SDIO;

				uprintf("Reading the device info file.\n");
				iResult = getDeviceInfo();
				if( iResult==0 )
				{
					uprintf("Device info OK.\n");

					/* Now load the data file. */
					iResult = getDataFile();
					if( iResult==0 )
					{
						uprintf("Data file OK.\n");

						iResult = flashDataFile(&tDeviceDesc);
						if( iResult==0 )
						{
							uprintf("Flashing OK.\n");
							tResult = NETX_CONSOLEAPP_RESULT_OK;
						}
						else
						{
							uprintf("Failed to flash.\n");
							tResult = NETX_CONSOLEAPP_RESULT_ERROR;
						}
					}
					else
					{
						uprintf("Failed to read the data file.\n");
						tResult = NETX_CONSOLEAPP_RESULT_ERROR;
					}
				}
				else
				{
					uprintf("Failed to read the device info file.\n");
					tResult = NETX_CONSOLEAPP_RESULT_ERROR;
				}
			}
			else
			{
				uprintf("Failed to detect the eMMC.\n");
			}
		}
	}

	if( tResult==NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("OK\n");

		rdy_run_blinki_init(&tBlinkiHandle, BLINKI_M_FLASHING_OK, BLINKI_S_FLASHING_OK);
		while(1)
		{
			rdy_run_blinki(&tBlinkiHandle);
		}
	}
	else
	{
		uprintf("ERROR\n");
		rdy_run_setLEDs(RDYRUN_YELLOW);
	}

	while(1) {};
}
