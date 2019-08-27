#include "sha384.h"

#include <string.h>

#include "netx_io_areas.h"


/* Why SHA384 instead of SHA512: https://en.wikipedia.org/wiki/Length_extension_attack */

void sha384_initialize(void)
{
	HOSTDEF(ptCryptArea);
	unsigned long ulValue;


	/* Reset the unit and select SHA384.
	 * NOTE: the reset bit will be cleared by the hardware.
	 */
	ulValue  = HOSTMSK(crypt_sha_cfg_reset);
	ulValue |= 1 << HOSTSRT(crypt_sha_cfg_mode);
	ptCryptArea->ulCrypt_sha_cfg = ulValue;
}



void sha384_finalize(unsigned long *pulHash, unsigned int sizHash, unsigned long ulDataSizeDw)
{
	HOSTDEF(ptCryptArea);
	unsigned long long ullBits;
	unsigned long ulValue;
	unsigned long ulValueRev;
	unsigned long ulPadDw;


	/* Pad the data. */
	ulPadDw = ulDataSizeDw & 0x1fU;
	if( ulPadDw<28 )
	{
		ulPadDw = 29U - ulPadDw;
	}
	else
	{
		ulPadDw = 61U - ulPadDw;
	}

	/* Start the padding data with a '1' bit. */
	ptCryptArea->ulCrypt_sha_din = 0x00000080U;
	/* Continue with a lot of '0' bits. */
	while( ulPadDw!=0 )
	{
		ptCryptArea->ulCrypt_sha_din = 0x00000000U;
		--ulPadDw;
	}

	/* Convert the number of DWORDs to bits. */
	ullBits = ((unsigned long long)ulDataSizeDw) * 32U;
	ulValue = ((unsigned long)((ullBits >> 32U) & 0xffffffffU));
	ulValueRev = ((ulValue & 0x000000ffU) << 24U) |
	             ((ulValue & 0x0000ff00U) <<  8U) |
	             ((ulValue & 0x00ff0000U) >>  8U) |
	             ((ulValue & 0xff000000U) >> 24U);
	ptCryptArea->ulCrypt_sha_din = ulValueRev;

	/* Clear the IRQ before writing the last data element. */
	ptCryptArea->ulCrypt_irq_raw = HOSTMSK(crypt_irq_raw_hash_ready);

	ulValue = ((unsigned long)( ullBits         & 0xffffffffU ));
	ulValueRev = ((ulValue & 0x000000ffU) << 24U) |
	             ((ulValue & 0x0000ff00U) <<  8U) |
	             ((ulValue & 0x00ff0000U) >>  8U) |
	             ((ulValue & 0xff000000U) >> 24U);
	ptCryptArea->ulCrypt_sha_din = ulValueRev;

	/* Wait for the hash calculation. */
	do
	{
		ulValue  = ptCryptArea->ulCrypt_irq_raw;
		ulValue &= HOSTMSK(crypt_irq_raw_hash_ready);
	} while( ulValue==0 );

	/* Copy the hash to the buffer. */
	if( pulHash!=NULL )
	{
		while(sizHash!=0)
		{
			--sizHash;
			pulHash[sizHash] = ptCryptArea->aulCrypt_sha_hash[sizHash];
		}
	}
}



void sha384_finalize_byte(unsigned long *pulHash, unsigned int sizHash, unsigned long ulDataSizeByte)
{
	HOSTDEF(ptCryptArea);
	unsigned long long ullBits;
	unsigned long ulValue;
	unsigned long ulValueRev;
	unsigned long ulPadByte;


	/* Pad the data. */
	ulPadByte = ulDataSizeByte & 0x7fU;
	if( ulPadByte<112 )
	{
		ulPadByte = 119U - ulPadByte;
	}
	else
	{
		ulPadByte = 247U - ulPadByte;
	}

	/* Start the padding data with a '1' bit. */
	*((volatile unsigned char*)(&(ptCryptArea->ulCrypt_sha_din))) = 0x80U;
	/* Continue with a lot of '0' bits. */
	while( ulPadByte!=0 )
	{
		*((volatile unsigned char*)(&(ptCryptArea->ulCrypt_sha_din))) = 0x00U;
		--ulPadByte;
	}

	/* Convert the number of DWORDs to bits. */
	ullBits = ((unsigned long long)ulDataSizeByte) * 8U;
	ulValue = ((unsigned long)((ullBits >> 32U) & 0xffffffffU));
	ulValueRev = ((ulValue & 0x000000ffU) << 24U) |
	             ((ulValue & 0x0000ff00U) <<  8U) |
	             ((ulValue & 0x00ff0000U) >>  8U) |
	             ((ulValue & 0xff000000U) >> 24U);
	ptCryptArea->ulCrypt_sha_din = ulValueRev;

	/* Clear the IRQ before writing the last data element. */
	ptCryptArea->ulCrypt_irq_raw = HOSTMSK(crypt_irq_raw_hash_ready);

	ulValue = ((unsigned long)( ullBits         & 0xffffffffU ));
	ulValueRev = ((ulValue & 0x000000ffU) << 24U) |
	             ((ulValue & 0x0000ff00U) <<  8U) |
	             ((ulValue & 0x00ff0000U) >>  8U) |
	             ((ulValue & 0xff000000U) >> 24U);
	ptCryptArea->ulCrypt_sha_din = ulValueRev;

	/* Wait for the hash calculation. */
	do
	{
		ulValue  = ptCryptArea->ulCrypt_irq_raw;
		ulValue &= HOSTMSK(crypt_irq_raw_hash_ready);
	} while( ulValue==0 );

	/* Copy the hash to the buffer. */
	if( pulHash!=NULL )
	{
		while(sizHash!=0)
		{
			--sizHash;
			pulHash[sizHash] = ptCryptArea->aulCrypt_sha_hash[sizHash];
		}
	}
}
