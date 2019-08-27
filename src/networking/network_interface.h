#include <stddef.h>

#ifndef __NETWORK_INTERFACE_H__
#define __NETWORK_INTERFACE_H__


typedef unsigned int (*PFN_NETWORK_IF_GET_LINK_STATUS)(void *pvUser);
typedef void *(*PFN_NETWORK_IF_GET_EMPTY_PACKET)(void *pvUser);
typedef void (*PFN_NETWORK_IF_RELEASE_PACKET)(void *pvPacket, void *pvUser);
typedef void (*PFN_NETWORK_IF_SEND_PACKET)(void *pvPacket, size_t sizPacket, void *pvUser);
typedef void *(*PFN_NETWORK_IF_GET_RECEIVED_PACKET)(size_t *psizPacket, void *pvUser);
typedef void (*PFN_NETWORK_IF_DEACTIVATE)(void *pvUser);
#if CFG_DEBUGMSG==1
typedef void (*PFN_NETWORK_IF_STATISTICS)(void *pvUser);
#endif


typedef struct STRUCT_NETWORK_IF
{
	PFN_NETWORK_IF_GET_LINK_STATUS pfnGetLinkStatus;
	PFN_NETWORK_IF_GET_EMPTY_PACKET pfnGetEmptyPacket;
	PFN_NETWORK_IF_RELEASE_PACKET pfnReleasePacket;
	PFN_NETWORK_IF_SEND_PACKET pfnSendPacket;
	PFN_NETWORK_IF_GET_RECEIVED_PACKET pfnGetReceivedPacket;
	PFN_NETWORK_IF_DEACTIVATE pfnDeactivate;
#if CFG_DEBUGMSG==1
	PFN_NETWORK_IF_STATISTICS pfnStatistics;
#endif
} NETWORK_IF_T;


#endif  /* __NETWORK_INTERFACE_H__ */


