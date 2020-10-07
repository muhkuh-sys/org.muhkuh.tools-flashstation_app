
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
#include "flasher_spi.h"
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



static int flashDataFile(BUS_T tBus, DEVICE_DESCRIPTION_T *ptDeviceDesc, unsigned long ulOffsetInBytes, unsigned char *pucData, unsigned long ulDataSizeInBytes)
{
	int iResult;
	unsigned long ulAlignedStartAdr;
	unsigned long ulAlignedEndAdr;
	union RETURN_MESSAGE_UNION
	{
		void *pv;
		unsigned long ul;
	} tReturnMessage;
	NETX_CONSOLEAPP_RESULT_T tResult;
	CMD_PARAMETER_FLASH_T tFlashParams;


	uprintf("Flashing data file.\n");

	iResult = -1;

	switch(tBus)
	{
	case BUS_ParFlash:
		uprintf("Parallel flash is not supported yet.\n");
		break;

	case BUS_SPI:
		tResult = spi_getEraseArea(&(ptDeviceDesc->uInfo.tSpiInfo), ulOffsetInBytes, ulOffsetInBytes+ulDataSizeInBytes, &ulAlignedStartAdr, &ulAlignedEndAdr);
		if( tResult==NETX_CONSOLEAPP_RESULT_OK )
		{
			uprintf("Using erase area 0x%08x - 0x%08x.\n", ulAlignedStartAdr, ulAlignedEndAdr);

			tResult = spi_isErased(&(ptDeviceDesc->uInfo.tSpiInfo), ulAlignedStartAdr, ulAlignedEndAdr, &(tReturnMessage.pv));
			if( tResult==NETX_CONSOLEAPP_RESULT_OK )
			{
				if( tReturnMessage.ul==0xffU )
				{
					uprintf("The area is already erased.\n");
					iResult = 0;
				}
				else
				{
					uprintf("Erasing the area 0x%08x - 0x%08x.\n", ulAlignedStartAdr, ulAlignedEndAdr);
					tResult = spi_erase(&(ptDeviceDesc->uInfo.tSpiInfo), ulAlignedStartAdr, ulAlignedEndAdr);
					if( tResult==NETX_CONSOLEAPP_RESULT_OK )
					{
						tResult = spi_isErased(&(ptDeviceDesc->uInfo.tSpiInfo), ulAlignedStartAdr, ulAlignedEndAdr, &(tReturnMessage.pv));
						if( tReturnMessage.ul==0xffU )
						{
							uprintf("The area is now erased.\n");
							iResult = 0;
						}
						else
						{
							uprintf("The area is not clean after the erase command. Is the flash broken?\n");
						}
					}
					else
					{
						uprintf("Failed to erase the area 0x%08x - 0x%08x.\n", ulAlignedStartAdr, ulAlignedEndAdr);
					}
				}
			}
			else
			{
				uprintf("Failed to check if the area is erased.\n");
			}

			if( iResult==0 )
			{
				tResult = spi_flash(&(ptDeviceDesc->uInfo.tSpiInfo), ulOffsetInBytes, ulDataSizeInBytes, pucData);
				if( tResult==NETX_CONSOLEAPP_RESULT_OK )
				{
					uprintf("Flashing OK.\n");
				}
				else
				{
					uprintf("Failed to flash the area.\n");
					iResult = -1;
				}
			}
		}
		else
		{
			uprintf("Failed to get the erase area for 0x%08x-0x%08x.\n", ulOffsetInBytes, ulOffsetInBytes+ulDataSizeInBytes);
		}

		break;

	case BUS_IFlash:
		uprintf("Intflash is not supported yet.\n");
		break;

	case BUS_SDIO:
		tFlashParams.ptDeviceDescription = ptDeviceDesc;
		tFlashParams.ulStartAdr = ulOffsetInBytes;
		tFlashParams.ulDataByteSize = ulDataSizeInBytes;
		tFlashParams.pucData = pucData;

		tResult = sdio_write(&tFlashParams);
		if( tResult==NETX_CONSOLEAPP_RESULT_OK )
		{
			uprintf("Flashing OK.\n");
			iResult = 0;
		}
		else
		{
			uprintf("Failed to flash.\n");
		}
		break;
	}

	return iResult;
}



static char aucBuffer[8192];


static int processWfp(unsigned char *pucWfpImage, unsigned long ulWfpDataSizeInBytes)
{
	int iResult;
	unsigned char *pucDataCnt;
	unsigned char *pucDataEnd;
	unsigned int uiLastBus;
	unsigned int uiLastUnit;
	unsigned int uiLastCs;
	unsigned int uiBus;
	unsigned int uiUnit;
	unsigned int uiCs;
	BUS_T tBus;
	unsigned long ulOffset;
	unsigned long ulDataSizeInBytes;
	NETX_CONSOLEAPP_RESULT_T tResult;
	DEVICE_DESCRIPTION_T tDeviceDesc;
	FLASHER_SPI_CONFIGURATION_T tSpiConfig;


	iResult = 0;

	uiLastBus = 0xffffffffU;
	uiLastUnit = 0xffffffffU;
	uiLastCs = 0xffffffffU;

	/* Loop over the complete image. */
	pucDataCnt = pucWfpImage;
	pucDataEnd = pucWfpImage + ulWfpDataSizeInBytes;

	if( (pucDataCnt+4)>pucDataEnd )
	{
		uprintf("The image is too small for a magic.\n");
		iResult = -1;
	}
	else
	{
		/* Does the image start with the SWFP magic? */
		if( pucDataCnt[0]==0x53 && pucDataCnt[1]==0x57 && pucDataCnt[2]==0x46 && pucDataCnt[3]==0x50 )
		{
			/* Found the magic. */
			pucDataCnt += 4U;

			while( pucDataCnt<pucDataEnd )
			{
				/* A chunk has at least 12 bytes:
				 *  1 byte: bus
				 *  1 byte: unit
				 *  1 byte: cs
				 *  4 bytes: offset
				 *  4 bytes: data size in bytes (n)
				 *  n bytes: data
				 */
				if( (pucDataCnt+12U)>pucDataEnd )
				{
					uprintf("Not enough data left for a chunk header.\n");
					iResult = -1;
					break;
				}
				else
				{
					/* Get the data. */
					uiBus = pucDataCnt[0];
					uiUnit = pucDataCnt[1];
					uiCs = pucDataCnt[2];
					ulOffset  =  (unsigned long) pucDataCnt[3];
					ulOffset |= ((unsigned long)(pucDataCnt[4])) <<  8U;
					ulOffset |= ((unsigned long)(pucDataCnt[5])) << 16U;
					ulOffset |= ((unsigned long)(pucDataCnt[6])) << 24U;
					ulDataSizeInBytes  =  (unsigned long) pucDataCnt[7];
					ulDataSizeInBytes |= ((unsigned long)(pucDataCnt[8])) <<  8U;
					ulDataSizeInBytes |= ((unsigned long)(pucDataCnt[9])) << 16U;
					ulDataSizeInBytes |= ((unsigned long)(pucDataCnt[10]))<< 24U;
					pucDataCnt += 11U;

					uprintf("Found new chunk for bus %d, unit %d, cs %d .\n", uiBus, uiUnit, uiCs);
					uprintf("Offset 0x%08x, size 0x%08x .\n", ulOffset, ulDataSizeInBytes);

					/* Does the data fit into the image? */
					if( (pucDataCnt+ulDataSizeInBytes)>pucDataEnd )
					{
						uprintf("Not enough data left for the chunk data.\n");
						iResult = -1;
						break;
					}
					else
					{
						if( uiLastBus==uiBus && uiLastUnit==uiUnit && uiLastCs==uiCs )
						{
							uprintf("Reusing last device.\n");
							tBus = (BUS_T)uiBus;
						}
						else
						{
							/* Clear the old device. */
							memset(&tDeviceDesc, 0, sizeof(DEVICE_DESCRIPTION_T));

							iResult = -1;
							tBus = (BUS_T)uiBus;
							switch(tBus)
							{
							case BUS_ParFlash:
								uprintf("Bus: parallel flash\n");
								uprintf("Parallel flash is not supported yet.\n");
								break;

							case BUS_SPI:
								uprintf("Bus: SPI\n");

								tSpiConfig.uiUnit = uiUnit;
								tSpiConfig.uiChipSelect = uiCs;
								tSpiConfig.ulInitialSpeedKhz = 1000;
								tSpiConfig.ulMaximumSpeedKhz = 25000;
								tSpiConfig.uiIdleCfg = MSK_SQI_CFG_IDLE_IO1_OE | MSK_SQI_CFG_IDLE_IO1_OUT | MSK_SQI_CFG_IDLE_IO2_OE | MSK_SQI_CFG_IDLE_IO2_OUT | MSK_SQI_CFG_IDLE_IO3_OE | MSK_SQI_CFG_IDLE_IO3_OUT;
								tSpiConfig.uiMode = 3;
								tSpiConfig.aucMmio[0] = 0xffU;
								tSpiConfig.aucMmio[1] = 0xffU;
								tSpiConfig.aucMmio[2] = 0xffU;
								tSpiConfig.aucMmio[3] = 0xffU;
								tResult = spi_detect(&tSpiConfig, &(tDeviceDesc.uInfo.tSpiInfo), aucBuffer+sizeof(aucBuffer));
								if( tResult==NETX_CONSOLEAPP_RESULT_OK )
								{
									iResult = 0;
								}
								else
								{
									uprintf("Failed to detect the SPI flash.\n");
								}
								break;

							case BUS_IFlash:
								uprintf("Bus: IFlash\n");
								uprintf("Intflash is not supported yet.\n");
								break;

							case BUS_SDIO:
								uprintf("Bus: SDIO\n");
								tResult = sdio_detect_wrap(&(tDeviceDesc.uInfo.tSdioHandle));
								if( tResult==NETX_CONSOLEAPP_RESULT_OK )
								{
									iResult = 0;
								}
								else
								{
									uprintf("Failed to detect the SD/MMC card.\n");
								}
								break;
							}
							if( iResult!=0 )
							{
								uprintf("Invalid bus: %d\n", uiBus);
								break;
							}
							else
							{
								tDeviceDesc.fIsValid = 1;
								tDeviceDesc.sizThis = sizeof(DEVICE_DESCRIPTION_T);
								tDeviceDesc.ulVersion = FLASHER_INTERFACE_VERSION;
								tDeviceDesc.tSourceTyp = tBus;

								/* Remember this as the last used device. */
								uiLastBus = uiBus;
								uiLastUnit = uiUnit;
								uiLastCs = uiCs;
							}
						}

						/* Flash the data. */
						iResult = flashDataFile(tBus, &tDeviceDesc, ulOffset, pucDataCnt, ulDataSizeInBytes);
						if( iResult==0 )
						{
							pucDataCnt += ulDataSizeInBytes;
							uprintf("\n");
						}
						else
						{
							break;
						}
					}
				}
			}
		}
		else
		{
			uprintf("Invalid magic found.\n");
			iResult = -1;
		}
	}

	return iResult;
}


/*-------------------------------------------------------------------------*/


UART_STANDALONE_DEFINE_GLOBALS


void flashapp_main(void);
void flashapp_main(void)
{
	BLINKI_HANDLE_T tBlinkiHandle;
	int iResult;
	unsigned char *pucWfpImage;


	systime_init();
	uart_standalone_initialize();

	uprintf("\f. *** Flasher APP by doc_bacardi@users.sourceforge.net ***\n");
	uprintf("V" VERSION_ALL "\n\n");
	uprintf("Using flasher V" FLASHER_VERSION_ALL " " FLASHER_VERSION_VCS "\n");

	/* Switch all LEDs off. */
	rdy_run_setLEDs(RDYRUN_OFF);
#if 1
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
		}
		else
		{
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

					pucWfpImage = (unsigned char*)0x40000000U;
					iResult = processWfp(pucWfpImage, ulDataFileSize);
					if( iResult==0 )
					{
						uprintf("Flashed the complete WFP.\n");
					}
					else
					{
						uprintf("Failed to flash the WFP.\n");
					}
				}
				else
				{
					uprintf("Failed to read the data file.\n");
				}
			}
			else
			{
				uprintf("Failed to read the device info file.\n");
			}
		}
	}
#else
	pucWfpImage = (unsigned char*)0x04020000U;
	ulDataFileSize = 587517;
	iResult = processWfp(pucWfpImage, ulDataFileSize);
	if( iResult==0 )
	{
		uprintf("Flashed the complete WFP.\n");
	}
	else
	{
		uprintf("Failed to flash the WFP.\n");
	}
#endif

	if( iResult==0 )
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
