#include <stdint.h>

#include "device_info.h"


#ifndef __FDL_H__
#define __FDL_H__

typedef struct FDL_HEADER_STRUCT
{
	char acStartToken[12];
	uint16_t usLabelSize;
	uint16_t usContentSize; 
} FDL_HEADER_T;

typedef struct FDL_BASIC_DEVICE_DATA_STRUCT
{
	uint16_t usManufacturerID;
	uint16_t usDeviceClassificationNumber;
	uint32_t ulDeviceNumber;
	uint32_t ulSerialNumber;
	uint8_t  ucHardwareCompatibilityNumber;
	uint8_t  ucHardwareRevisionNumber;
	uint16_t usProductionDate;
	uint8_t  ucReserved1;
	uint8_t  ucReserved2;
	uint8_t aucReservedFields[14];
} FDL_BASIC_DEVICE_DATA_T;

typedef struct FDL_MAC_STRUCT
{
	uint8_t  aucMAC[6];
	uint8_t  aucReserved[2];
} FDL_MAC_T;

typedef struct FDL_PRODUCT_IDENTIFICATION_STRUCT
{
	uint16_t usUSBVendorID;
	uint16_t usUSBProductID;
	uint8_t  aucUSBVendorName[16];
	uint8_t  aucUSBProductName[16];
	uint8_t  aucReservedFields[76];
} FDL_PRODUCT_IDENTIFICATION_T;

typedef struct FDL_OEM_IDENTIFICATION_STRUCT
{
	uint32_t ulOEMDataOptionFlags;
	uint8_t  aucOEMSerialNumber[28];
	uint8_t  aucOEMOrderNumber[32];
	uint8_t  aucOEMHardwareRevision[16];
	uint8_t  aucOEMProductionDateTime[32];
	uint8_t  aucOEMReservedFields[12];
	uint8_t  aucOEMSpecificData[112];
} FDL_OEM_IDENTIFICATION_T;

typedef struct FDL_FLASH_LAYOUT_AREA_STRUCT
{
	uint32_t ulAreaContentType;
	uint32_t ulAreaStartAddress;
	uint32_t ulAreaSize;
	uint32_t ulChipNumber;
	uint8_t  aucAreaName[16];
	uint8_t  ucAreaAccessType;
	uint8_t  aucReserved[3];
} FDL_FLASH_LAYOUT_AREA_T;

typedef struct FDL_FLASH_LAYOUT_CHIP_STRUCT
{
	uint32_t ulChipNumber;
	uint8_t  aucFlashDriverName[16];
	uint32_t ulBlockSize;
	uint32_t ulFlashSize;
	uint32_t ulMaxEraseWriteCycles;
} FDL_FLASH_LAYOUT_CHIP_T;

typedef struct FDL_FLASH_LAYOUT_STRUCT
{
	FDL_FLASH_LAYOUT_AREA_T atArea[10];
	FDL_FLASH_LAYOUT_CHIP_T atChip[4];
} FDL_FLASH_LAYOUT_T;

typedef struct FDL_DATA_STUCT
{
	FDL_BASIC_DEVICE_DATA_T tBasicDeviceData;
	FDL_MAC_T atMacCOM[8];
	FDL_MAC_T atMacAPP[4];
	FDL_PRODUCT_IDENTIFICATION_T tProductIdentification;
	FDL_OEM_IDENTIFICATION_T tOEMIdentification;
	FDL_FLASH_LAYOUT_T tFlashLayout;
} FDL_DATA_T;

typedef union FDL_DATA_BUFFER_UNION
{
	FDL_DATA_T t;
	uint8_t  auc[sizeof(FDL_DATA_T)];
	uint32_t aul[sizeof(FDL_DATA_T)/sizeof(uint32_t)];
} FDL_DATA_BUFFER_T;

typedef struct FDL_FOOTER_STRUCT
{
	uint32_t ulChecksum;
	char acEndLabel[12];
} FDL_FOOTER_T;

typedef struct FDL_STRUCT
{
	FDL_HEADER_T tHeader;
	FDL_DATA_BUFFER_T tData;
	FDL_FOOTER_T tFooter;
} FDL_T;

typedef union FDL_BUFFER_UNION
{
	FDL_T t;
	unsigned char auc[sizeof(FDL_T)];
} FDL_BUFFER_T;


int readFDL(DEVICE_INFO_T *ptDeviceInfo);


#endif  /* __FDL_H__ */
