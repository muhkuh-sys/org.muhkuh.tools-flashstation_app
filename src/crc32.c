#include "crc32.h"
#include "netx_io_areas.h"


unsigned long crc_gen_crc32b(const unsigned char *pucData, unsigned int uiSizData)
{
	HOSTDEF(ptCrcArea);
	unsigned long ulValue;
	const unsigned char *pucEnd;


	/* Set the configuration. */
	ulValue  = 31U << HOSTSRT(crc_config_crc_len);
	ulValue |= HOSTMSK(crc_config_crc_shift_right);
	ulValue |= 3U << HOSTSRT(crc_config_crc_nof_bits);
	ulValue |= HOSTMSK(crc_config_crc_in_msb_low);
	ptCrcArea->ulCrc_config = ulValue;

	/* Set the polynomial for CRC32b. */
	ptCrcArea->ulCrc_polynomial = 0x04c11db7;

	/* Set the initial value for CRC32b. */
	ptCrcArea->ulCrc_crc = 0xffffffff;

	/* Get pointer to the end of the data. */
	pucEnd = pucData + uiSizData;
	while( pucData<pucEnd )
	{
		ptCrcArea->ulCrc_data_in = *(pucData++);
	}

	ulValue  = ptCrcArea->ulCrc_crc;
	ulValue ^= 0xffffffffU;

	return ulValue;
}


