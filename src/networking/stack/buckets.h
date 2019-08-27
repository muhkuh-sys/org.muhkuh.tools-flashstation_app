/***************************************************************************
 *   Copyright (C) 2005, 2006, 2007, 2008, 2009 by Hilscher GmbH           *
 *                                                                         *
 *   Author: Christoph Thelen (cthelen@hilscher.com)                       *
 *                                                                         *
 *   Redistribution or unauthorized use without expressed written          *
 *   agreement from the Hilscher GmbH is forbidden.                        *
 ***************************************************************************/


#include <stddef.h>


#ifndef __BUCKETS_H__
#define __BUCKETS_H__


#define BUCKET_SIZE 1560

void buckets_init(void);

size_t buckets_getFreeBytes(void);
size_t buckets_getValidBytes(void);

int buckets_write(unsigned char *pucData, size_t sizLen);
unsigned char *buckets_getPtr(void);
int buckets_bytesProcessed(void);


#endif	/* __BUCKETS_H__ */
