
#include <string.h>

#include "blinki_codes.h"
#include "device_info.h"
#include "fdl.h"
#include "flasher_version.h"
#include "network_lwip.h"
#include "netx_io_areas.h"
#include "rdy_run.h"
#include "sha384.h"
#include "systime.h"
#include "uart_standalone.h"
#include "uprintf.h"
#include "version.h"
#include "wfp.h"

/*-------------------------------------------------------------------------*/


#if 0
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
#endif



static void setupHardware(DEVICE_INFO_T *ptDeviceInfo)
{
	HOSTDEF(ptAsicCtrlArea);
	HOSTDEF(ptMmioCtrlArea);
	unsigned long ulDeviceNr;
//	unsigned long ulHwRev;


	ulDeviceNr = ptDeviceInfo->ulDeviceNr;
//	ulHwRev = ptDeviceInfo->ulHwRev;
	if( (ulDeviceNr==9387000) || (ulDeviceNr==9387001) )
	{
		/* Set the Ethernet PHY LEDs.
		 * ACT is connected to MMIO01 and
		 * LINK is connected to MMIO02.
		 */
		ptAsicCtrlArea->ulAsic_ctrl_access_key = ptAsicCtrlArea->ulAsic_ctrl_access_key;
		ptMmioCtrlArea->aulMmio_cfg[1] = NX4000_MMIO_CFG_PHY0_LED_PHY_CTRL_ACT;

		ptAsicCtrlArea->ulAsic_ctrl_access_key = ptAsicCtrlArea->ulAsic_ctrl_access_key;
		ptMmioCtrlArea->aulMmio_cfg[2] = NX4000_MMIO_CFG_PHY0_LED_PHY_CTRL_LNK;
	}
}



static int parseInfoFile(const char *pcBuffer, unsigned int sizBuffer, DEVICE_INFO_T *ptDeviceInfo)
{
	int iResult;
	const char *pcCnt;
	const char *pcInfoEnd;
	unsigned int uiLineSize;
	unsigned int uiShift;
	unsigned int uiInc;
	char cDigit;
	unsigned char ucData;
	unsigned char *pucHash;


	/* Try to parse the Info file. */
	pcInfoEnd = pcBuffer + sizBuffer;

	/* Look for the first newline. */
	pcCnt = pcBuffer;
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
		if( uiLineSize<sizeof(ptDeviceInfo->acDataUri)-1 )
		{
			/* Start the URI with a slash. */
			ptDeviceInfo->acDataUri[0] = '/';
			/* Append the filename. */
			memcpy(ptDeviceInfo->acDataUri + 1U, pcBuffer, uiLineSize);
			/* Terminate the filename. */
			ptDeviceInfo->acDataUri[uiLineSize + 1U] = 0x00;

			/* Skip the newline. */
			++pcCnt;

			/* Expect 96 hex digits. */
			pucHash = ptDeviceInfo->tHash.auc;
			memset(&(ptDeviceInfo->tHash), 0, sizeof(SHA384_T));
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
			uprintf("The filename must not exceed %d bytes. Here it has %d bytes.\n", sizeof(ptDeviceInfo->acDataUri)-1, uiLineSize);
			iResult = -1;
		}
	}
	else
	{
		uprintf("No newline found in info file!\n");
		iResult = -1;
	}

	return iResult;
}



static int getDeviceInfo(ip4_addr_t *ptServerIpAddr, DEVICE_INFO_T *ptDeviceInfo)
{
	unsigned long ulDeviceNumber;
	unsigned long ulHardwareRevision;
	unsigned long ulDeviceNumberHi;
	unsigned long ulDeviceNumberLo;
	err_t tResult;
	unsigned int uiDownloadSize;
	int iResult;
	char acUri[32];
	union INFO_BUFFER_UNION
	{
		char ac[2048];
		unsigned char auc[2048];
	} uInfoBuffer;


	/* Get the device number and the hardware revision from the device info block. */
	ulDeviceNumber = ptDeviceInfo->ulDeviceNr;
	ulHardwareRevision = ptDeviceInfo->ulHwRev;

	/* Split the device number to insert a thousands separator. */
	ulDeviceNumberHi = ulDeviceNumber / 1000U;
	ulDeviceNumberLo = ulDeviceNumber - (1000U * ulDeviceNumberHi);

	/* Construct the URI for the file info. */
	usnprintf(acUri, sizeof(acUri), "/%04d.%03dR%d.txt", ulDeviceNumberHi, ulDeviceNumberLo, ulHardwareRevision);
	uprintf("Loading info file '%s'.\n", acUri);

	/* FIXME: debug, remove this. */
	memset(uInfoBuffer.auc, 0, sizeof(uInfoBuffer));

	/* Download the file. */
	tResult = httpDownload(ptServerIpAddr, acUri, uInfoBuffer.auc, sizeof(uInfoBuffer), &uiDownloadSize, NULL);
	if( tResult==ERR_OK )
	{
		iResult = parseInfoFile(uInfoBuffer.ac, uiDownloadSize, ptDeviceInfo);
	}
	else
	{
		uprintf("Failed to load the info file.\n");
		iResult = -1;
	}

	return iResult;
}



static int getDataFile(ip4_addr_t *ptServerIpAddr, DEVICE_INFO_T *ptDeviceInfo)
{
	int iResult;
	unsigned char *pucBuffer;
	unsigned int sizBuffer;
	unsigned int uiDownloadSize;
	err_t tResult;
	SHA384_T tMyHash;


	iResult = -1;

	uprintf("Reading data file '%s' with the expected hash:\n", ptDeviceInfo->acDataUri);
	hexdump(ptDeviceInfo->tHash.auc, sizeof(SHA384_T));

	/* Load the info file with TFTP. Use the DDR as a buffer. */
	pucBuffer = (unsigned char*)0x40000000U;
	sizBuffer = 0x40000000U;
	tResult = httpDownload(ptServerIpAddr, ptDeviceInfo->acDataUri, pucBuffer, sizBuffer, &uiDownloadSize, &tMyHash);
	if( tResult==ERR_OK )
	{
		/* Compare the hash. */
		if( memcmp(&tMyHash, &(ptDeviceInfo->tHash), sizeof(SHA384_T))==0 )
		{
			/* Set the pointer to the downloaded image. */
			ptDeviceInfo->pucWfpImage = pucBuffer;
			ptDeviceInfo->sizWfpImage = uiDownloadSize;

			uprintf("The data file is OK.\n");

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


/*-------------------------------------------------------------------------*/

UART_STANDALONE_DEFINE_GLOBALS

void flashapp_main(void);
void flashapp_main(void)
{
	BLINKI_HANDLE_T tBlinkiHandle;
	int iResult;
	unsigned long ulBlinkMask;
	unsigned long ulBlinkState;
	DEVICE_INFO_T tDeviceInfo;
	ip4_addr_t tServerIpAddr;


	systime_init();
	uart_standalone_initialize();
	network_init();

	uprintf("\f. *** Flasher APP by doc_bacardi@users.sourceforge.net ***\n");
	uprintf("V" VERSION_ALL "\n\n");
	uprintf("Using flasher V" FLASHER_VERSION_ALL " " FLASHER_VERSION_VCS "\n");

	/* Switch all LEDs off. */
	rdy_run_setLEDs(RDYRUN_OFF);

	/* The final application will be downloaded and started by the netX4000
	 * ROM code. The ROM has the limitation that it does not know when a boot
	 * image is finished. It is possible that the image continues after a code
	 * is started. This is a common use case in HWC and MWC files.
	 * That's why the TFTP connection for the application download is not
	 * closed when this code is executed. As the flasher app does not return
	 * to the ROM code, we should close the connection here, or the TFTP
	 * server will keep retrying for a while.
	 *
	 * The ACK should be masked out if the application is started by a JTAG
	 * debugger.
	 */

#if 0
	/* Send the last ACK packet for the ROM transfer. */
	iResult = ackLastRomcodePacket();
	if( iResult!=0 )
	{
		uprintf("Failed to acknowledge the last ROM packet.\n");

		ulBlinkMask = BLINKI_M_ROM_ACK_FAILED;
		ulBlinkState = BLINKI_S_ROM_ACK_FAILED;
	}
	else
#else
	/* This should be used with a JTAG debugger. */
	iResult = 0;
#endif
	{
		/* Read the FDL structure and extract the device number, the hardware
		* revision and the serial. This will be used for all log messages and
		* to determine the image to flash.
		*/
		iResult = readFDL(&tDeviceInfo);
		if( iResult!=0 )
		{
			ulBlinkMask = BLINKI_M_FDL_READ_FAILED;
			ulBlinkState = BLINKI_S_FDL_READ_FAILED;
		}
		else
		{
			uprintf("Found a valid FDL for %dR%dSN%d.\n", tDeviceInfo.ulDeviceNr, tDeviceInfo.ulHwRev, tDeviceInfo.ulSerial);

			setupHardware(&tDeviceInfo);

			uprintf("Initializing network...\n");
			setupNetwork();

			/* FIXME: get the server address from... somewhere. */
			IP4_ADDR(&tServerIpAddr, 192,168,2,118);

			uprintf("Reading the device info file.\n");
			iResult = getDeviceInfo(&tServerIpAddr, &tDeviceInfo);
			if( iResult!=0 )
			{
				ulBlinkMask = BLINKI_M_DOWNLOAD_DEVICEINFO_FAILED;
				ulBlinkState = BLINKI_S_DOWNLOAD_DEVICEINFO_FAILED;
			}
			else
			{
				uprintf("Device info OK.\n");

				iResult = getDataFile(&tServerIpAddr, &tDeviceInfo);
				if( iResult!=0 )
				{
					ulBlinkMask = BLINKI_M_DOWNLOAD_SWFP_FAILED;
					ulBlinkState = BLINKI_S_DOWNLOAD_SWFP_FAILED;
				}
				else
				{
					iResult = processWfp(&tDeviceInfo);
					if( iResult==0 )
					{
						uprintf("Flashed the complete WFP.\n");

						ulBlinkMask = BLINKI_M_FLASHING_OK;
						ulBlinkState = BLINKI_S_FLASHING_OK;
					}
					else
					{
						uprintf("Failed to flash the WFP.\n");

						ulBlinkMask = BLINKI_M_FLASHING_FAILED;
						ulBlinkState = BLINKI_S_FLASHING_FAILED;
					}
				}
			}

			rdy_run_blinki_init(&tBlinkiHandle, ulBlinkMask, ulBlinkState);
			while(1)
			{
				rdy_run_blinki(&tBlinkiHandle);
				network_cyclic_process();
			}
		}
	}

	rdy_run_blinki_init(&tBlinkiHandle, ulBlinkMask, ulBlinkState);
	while(1)
	{
		rdy_run_blinki(&tBlinkiHandle);
	}
}
