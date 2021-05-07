#ifndef __SHA384_H__
#define __SHA384_H__


typedef union SHA384_UNION
{
    unsigned char auc[48];
    unsigned long aul[48/sizeof(unsigned long)];
} SHA384_T;

void sha384_initialize(void);
void sha384_update(void *pvData, unsigned int sizChunk);
#define sha384_update_ul(ulData) {ptCryptArea->ulCrypt_sha_din = ulData;}
#define sha384_update_uc(ucData) {*((volatile unsigned char*)(&(ptCryptArea->ulCrypt_sha_din))) = ucData;}
void sha384_finalize(SHA384_T *ptHash, unsigned long ulDataSizeDw);
void sha384_finalize_byte(SHA384_T *ptHash, unsigned long ulDataSizeByte);


#endif  /* __SHA384_H__ */

