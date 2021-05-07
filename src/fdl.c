#include "fdl.h"

#include "flasher_interface.h"
#include "flasher_spi.h"
#include "flasher_sdio.h"

#include "crc32.h"
#include "uprintf.h"

/*-------------------------------------------------------------------------*/

int readFDL(DEVICE_INFO_T *ptDeviceInfo)
{
	int iResult;
	unsigned long ulOffset;
	unsigned long ulCrc;
	unsigned long ulDeviceNr;
	unsigned char ucHwRev;
	unsigned long ulSerial;
	NETX_CONSOLEAPP_RESULT_T tResult;
	DEVICE_DESCRIPTION_T tDeviceDesc;
	FLASHER_SPI_CONFIGURATION_T tSpiConfig;
	FDL_BUFFER_T tBuf;
    char acFlashBuffer[8192];


	iResult = -1;

	ulOffset = 0xff0000;

	/* Detect a flash connected to SPI unit 0, chip select 0. */
	tSpiConfig.uiUnit = 0;
	tSpiConfig.uiChipSelect = 0;
	tSpiConfig.ulInitialSpeedKhz = 1000;
	tSpiConfig.ulMaximumSpeedKhz = 25000;
	tSpiConfig.uiIdleCfg = MSK_SQI_CFG_IDLE_IO1_OE | MSK_SQI_CFG_IDLE_IO1_OUT | MSK_SQI_CFG_IDLE_IO2_OE | MSK_SQI_CFG_IDLE_IO2_OUT | MSK_SQI_CFG_IDLE_IO3_OE | MSK_SQI_CFG_IDLE_IO3_OUT;
	tSpiConfig.uiMode = 3;
	tSpiConfig.aucMmio[0] = 0xffU;
	tSpiConfig.aucMmio[1] = 0xffU;
	tSpiConfig.aucMmio[2] = 0xffU;
	tSpiConfig.aucMmio[3] = 0xffU;
	tResult = spi_detect(&tSpiConfig, &(tDeviceDesc.uInfo.tSpiInfo), acFlashBuffer+sizeof(acFlashBuffer));
	if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
	{
		uprintf("Failed to detect the SPI flash.\n");
	}
	else
	{
		/* Read the FDL. */
		tResult = spi_read(&(tDeviceDesc.uInfo.tSpiInfo), ulOffset, ulOffset+sizeof(FDL_T), tBuf.auc);
		if( tResult!=NETX_CONSOLEAPP_RESULT_OK )
		{
			uprintf("Failed to read the FDL.\n");
		}
		else
		{
			/* Is this a valid FDL? */
			if( memcmp("ProductData>", tBuf.t.tHeader.acStartToken, 12)!=0 )
			{
				uprintf("FDL: Missing start token.\n");
			}
			else if( memcmp("<ProductData", tBuf.t.tFooter.acEndLabel, 12)!=0 )
			{
				uprintf("FDL: Missing end token.\n");
			}
			else if( tBuf.t.tHeader.usLabelSize!=(tBuf.t.tHeader.usContentSize+sizeof(FDL_HEADER_T)+sizeof(FDL_FOOTER_T)) )
			{
				uprintf("FDL: the complete size is not the header+data+footer.\n");
			}
			else if( sizeof(FDL_T)<tBuf.t.tHeader.usLabelSize )
			{
				uprintf("FDL: the size in the header exceeds the available data.\n");
			}
			else
			{
				ulCrc = crc_gen_crc32b(tBuf.t.tData.auc, tBuf.t.tHeader.usContentSize);
				if( ulCrc!=tBuf.t.tFooter.ulChecksum )
				{
					uprintf("FDL: the checksum does not match.\n");
				}
				else
				{
					ulDeviceNr = tBuf.t.tData.t.tBasicDeviceData.ulDeviceNumber;
					ucHwRev = tBuf.t.tData.t.tBasicDeviceData.ucHardwareRevisionNumber;
					ulSerial = tBuf.t.tData.t.tBasicDeviceData.ulSerialNumber;

					ptDeviceInfo->ulDeviceNr = ulDeviceNr;
					ptDeviceInfo->ulHwRev = ucHwRev;
					ptDeviceInfo->ulSerial = ulSerial;

					iResult = 0;
				}
			}
		}
	}

	return iResult;
}

/*-------------------------------------------------------------------------*/
