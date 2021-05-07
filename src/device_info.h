#include "sha384.h"

#ifndef __DEVICE_INFO_H__
#define __DEVICE_INFO_H__


typedef struct DEVICE_INFO_STRUCT
{
	/* These fields are filled with data from the FDL.
	 * They are necessary to retrieve the correct files from the server.
	 */
	unsigned long ulDeviceNr;
	unsigned long ulHwRev;
	unsigned long ulSerial;

	char acDataUri[1024];
	SHA384_T tHash;
} DEVICE_INFO_T;


#endif  /* __DEVICE_INFO_H__ */
