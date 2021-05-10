#include "wfp.h"

#include "flasher_interface.h"
#include "flasher_spi.h"
#include "flasher_sdio.h"
#include "uprintf.h"

/*-------------------------------------------------------------------------*/

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



int processWfp(DEVICE_INFO_T *ptDeviceInfo)
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
	static char aucBuffer[8192];


	iResult = 0;

	uiLastBus = 0xffffffffU;
	uiLastUnit = 0xffffffffU;
	uiLastCs = 0xffffffffU;

	/* Loop over the complete image. */
	pucDataCnt = ptDeviceInfo->pucWfpImage;
	pucDataEnd = ptDeviceInfo->pucWfpImage + ptDeviceInfo->sizWfpImage;

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
