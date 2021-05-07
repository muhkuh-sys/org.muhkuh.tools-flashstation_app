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



void sha384_update(void *pvData, unsigned int sizChunk)
{
	HOSTDEF(ptCryptArea);
	union PTR_UNION
	{
		const unsigned char *puc;
		const unsigned long *pul;
		unsigned long ul;
	} tPtr;
	unsigned long ulEnd;


	ulEnd = ((unsigned long)pvData) + sizChunk;

	tPtr.puc = pvData;

	/* Add bytes to the hash until the address is aligned to 4. */
	while( (tPtr.ul & 3U)!=0 )
	{
		sha384_update_uc(*(tPtr.puc++));
	}
	/* Add DWORDs to the hash as long as enough data is available. */
	while( (tPtr.ul+4)<=ulEnd )
	{
		sha384_update_ul(*(tPtr.pul++));
	}
	/* Add the rest of the data as bytes to the hash. */
	while( tPtr.ul<ulEnd )
	{
		sha384_update_uc(*(tPtr.puc++));
	}
}



void sha384_finalize(SHA384_T *ptHash, unsigned long ulDataSizeDw)
{
	HOSTDEF(ptCryptArea);
	unsigned long long ullBits;
	unsigned long ulValue;
	unsigned long ulValueRev;
	unsigned long ulPadDw;
	unsigned int sizHash;


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
	if( ptHash!=NULL )
	{
		/* Get the size of the hash in DWORDS. */
		sizHash = 48 / sizeof(unsigned long);
		while(sizHash!=0)
		{
			--sizHash;
			ptHash->aul[sizHash] = ptCryptArea->aulCrypt_sha_hash[sizHash];
		}
	}
}



void sha384_finalize_byte(SHA384_T *ptHash, unsigned long ulDataSizeByte)
{
	HOSTDEF(ptCryptArea);
	unsigned long long ullBits;
	unsigned long ulValue;
	unsigned long ulValueRev;
	unsigned long ulPadByte;
	unsigned int sizHash;


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
	if( ptHash!=NULL )
	{
		/* Get the size of the hash in DWORDS. */
		sizHash = 48 / sizeof(unsigned long);
		while(sizHash!=0)
		{
			--sizHash;
			ptHash->aul[sizHash] = ptCryptArea->aulCrypt_sha_hash[sizHash];
		}
	}
}
