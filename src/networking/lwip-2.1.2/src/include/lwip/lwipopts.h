#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/* No OS stuff. */
#define NO_SYS 1
/* No lightweight inter-task protection. */
#define SYS_LIGHTWEIGHT_PROT 0

/* Set the memory alignment to 4. */
#define MEM_ALIGNMENT 4U

/* TODO: is this needed for the HTTP client? */
#define LWIP_ALTCP 1

/* DHCP is not needed. The IP address was already retrieved by the ROM code. */
#define LWIP_DHCP 0
/* AUTOIP is also not needed as the IP address is already there. */
#define LWIP_AUTOIP 0
/* The station uses only IP addresses and no host names. */
#define LWIP_DNS 0

/* No multicast necessary. */
#define LWIP_IGMP 0

// #define LWIP_NETIF_STATUS_CALLBACK 0
// #define LWIP_NUM_NETIF_CLIENT_DATA 1

#define LWIP_NETCONN 0
#define LWIP_SOCKET 0

/* Do not collect statistics. */
#define LWIP_STATS 0


#endif  /* __LWIPOPTS_H__ */
