#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "sha384.h"

#ifndef __NETWORK_LWIP_H__
#define __NETWORK_LWIP_H__

void network_init(void);
int setupNetwork(void);
void network_cyclic_process(void);
err_t httpDownload(ip4_addr_t *ptServerIpAddr, const char *pcUri, unsigned char *pucBuffer, unsigned int sizBuffer, unsigned int *psizDownloaded, SHA384_T *ptHash);


#endif  /* __NETWORK_LWIP_H__ */
