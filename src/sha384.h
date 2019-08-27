#ifndef __SHA384_H__
#define __SHA384_H__


void sha384_initialize(void);
#define sha384_update_ul(ulData) {ptCryptArea->ulCrypt_sha_din = ulData;}
#define sha384_update_uc(ucData) {*((volatile unsigned char*)(&(ptCryptArea->ulCrypt_sha_din))) = ucData;}
void sha384_finalize(unsigned long *pulHash, unsigned int sizHash, unsigned long ulDataSizeDw);
void sha384_finalize_byte(unsigned long *pulHash, unsigned int sizHash, unsigned long ulDataSizeByte);


#endif  /* __SHA384_H__ */

