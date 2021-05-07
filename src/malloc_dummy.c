#include <string.h>

#include "uprintf.h"


void *_sbrk(int incr);

void *_sbrk(int incr __attribute__((unused)))
{
	uprintf("ERROR: out of memory\n");
	return NULL;
}
