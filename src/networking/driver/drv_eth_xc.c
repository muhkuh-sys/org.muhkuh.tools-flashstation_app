#include "networking/driver/drv_eth_xc.h"

#include "netx_io_areas.h"
#include "networking/driver/eth_std_mac_xpec_regdef.h"
#include "networking/driver/nec_cb12.h"
#include "systime.h"



#define ETHERNET_MINIMUM_FRAMELENGTH                    60
#define ETHERNET_MAXIMUM_FRAMELENGTH                    1518

#define NUM_FIFO_CHANNELS_PER_UNIT                      8      /**< Number of FIFO units per XC channel */
#define FIFO_ENTRIES                                    100    /**< FIFO depth for each of the 8 FIFOs  */
#define ETH_FRAME_BUF_SIZE                              1560   /**< size of a frame buffer     */
#define INTRAM_SEGMENT_SIZE                             0x10000 /**< size of the internal ram segments */

#define ETHERNET_FIFO_EMPTY                             0      /**< Empty pointer FIFO               */
#define ETHERNET_FIFO_IND_HI                            1      /**< High priority indication FIFO    */
#define ETHERNET_FIFO_IND_LO                            2      /**< Low priority indication FIFO     */
#define ETHERNET_FIFO_REQ_HI                            3      /**< High priority request FIFO       */
#define ETHERNET_FIFO_REQ_LO                            4      /**< Low priority request FIFO        */
#define ETHERNET_FIFO_CON_HI                            5      /**< High priority confirmation FIFO  */
#define ETHERNET_FIFO_CON_LO                            6      /**< Low priority confirmation FIFO   */



typedef enum PHYCTRL_LED_MODE_Etag
{
	PHYCTRL_LED_MODE_MANUAL    = 0,
	PHYCTRL_LED_MODE_STATIC    = 1,
	PHYCTRL_LED_MODE_FLASHING  = 2,
	PHYCTRL_LED_MODE_COMBINED  = 3
} PHYCTRL_LED_MODE_E;



typedef struct STRUCTURE_DRV_ETH_XC_HANDLE
{
	unsigned int uiEthPortNr;    /* The ethernet port 0-3. */
	unsigned int uiXcUnit;       /* Use XC 0 or XC 1. This is derived from the ethernet port. */
	unsigned int uiXcPort;       /* Use port 0 or 1 of the selected XC unit. This is derived from the ethernet port. */
} DRV_ETH_XC_HANDLE_T;


static DRV_ETH_XC_HANDLE_T tHandle;



static NX4000_PHY_CTRL_AREA_T * const aptPhyCtrl[4] =
{
	(NX4000_PHY_CTRL_AREA_T*) Addr_NX4000_xc0_phy_ctrl0,
	(NX4000_PHY_CTRL_AREA_T*) Addr_NX4000_xc0_phy_ctrl1,
	(NX4000_PHY_CTRL_AREA_T*) Addr_NX4000_xc1_phy_ctrl0,
	(NX4000_PHY_CTRL_AREA_T*) Addr_NX4000_xc1_phy_ctrl1
};



static NX4000_POINTER_FIFO_AREA_T * const aptPFifo[2] =
{
	(NX4000_POINTER_FIFO_AREA_T*) Addr_NX4000_xc0_pointer_fifo,
	(NX4000_POINTER_FIFO_AREA_T*) Addr_NX4000_xc1_pointer_fifo
};



static NX4000_SYSTIME_AREA_T * const aptSystime[2] =
{
    (NX4000_SYSTIME_AREA_T*) Addr_NX4000_systime0,
    (NX4000_SYSTIME_AREA_T*) Addr_NX4000_systime1
};



static const unsigned long aulIntRamStart[4] =
{
	Addr_NX4000_intram0,
	Addr_NX4000_intram1,
	Addr_NX4000_intram2,
	Addr_NX4000_intram3
};



static ETHMAC_XPEC_DPM * const aptXpecDramArea[4] =
{
	(ETHMAC_XPEC_DPM*) Adr_NX4000_xc0_tpec0_dram_ram_start,
	(ETHMAC_XPEC_DPM*) Adr_NX4000_xc0_tpec1_dram_ram_start,
	(ETHMAC_XPEC_DPM*) Adr_NX4000_xc1_tpec0_dram_ram_start,
	(ETHMAC_XPEC_DPM*) Adr_NX4000_xc1_tpec1_dram_ram_start
};



static NX4000_XMAC_AREA_T * const aptXmacArea[4] =
{
	(NX4000_XMAC_AREA_T*) Addr_NX4000_xc0_xmac0_regs,
	(NX4000_XMAC_AREA_T*) Addr_NX4000_xc0_xmac1_regs,
	(NX4000_XMAC_AREA_T*) Addr_NX4000_xc1_xmac0_regs,
	(NX4000_XMAC_AREA_T*) Addr_NX4000_xc1_xmac1_regs
};



static NX4000_XPEC_AREA_T * const aptRpecRegArea[4] =
{
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc0_rpec0_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc0_rpec1_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc1_rpec0_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc1_rpec1_regs
};



static NX4000_XPEC_AREA_T * const aptTpecRegArea[4] =
{
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc0_tpec0_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc0_tpec1_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc1_tpec0_regs,
	(NX4000_XPEC_AREA_T*) Addr_NX4000_xc1_tpec1_regs
};


static uint32_t * const apulRpecPramArea[4] =
{
	(uint32_t*) Adr_NX4000_xc0_rpec0_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc0_rpec1_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc1_rpec0_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc1_rpec1_pram_ram_start
};



static uint32_t * const apulTpecPramArea[4] =
{
	(uint32_t*) Adr_NX4000_xc0_tpec0_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc0_tpec1_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc1_tpec0_pram_ram_start,
	(uint32_t*) Adr_NX4000_xc1_tpec1_pram_ram_start
};



static void *convert_fifo_value_to_packet_pointer(unsigned long ulFifoValue)
{
	unsigned long ulFrameNr;
	unsigned long ulRamSegment;
	unsigned long ulValue;
	void *pvPacket;


	/* extract ram bank and frame number */ 
	ulFrameNr   = ulFifoValue;
	ulFrameNr  &= MSK_ETHMAC_FIFO_ELEMENT_FRAME_BUF_NUM;
	ulFrameNr >>= SRT_ETHMAC_FIFO_ELEMENT_FRAME_BUF_NUM;

	ulRamSegment   = ulFifoValue;
	ulRamSegment  &= MSK_ETHMAC_FIFO_ELEMENT_INT_RAM_SEGMENT_NUM;
	ulRamSegment >>= SRT_ETHMAC_FIFO_ELEMENT_INT_RAM_SEGMENT_NUM;

	ulValue  = aulIntRamStart[ulRamSegment];
	ulValue += ETH_FRAME_BUF_SIZE * ulFrameNr;

	pvPacket = (void*)ulValue;
	return pvPacket;
}



/* Convert a frame address to a FIFO pointer. */
static unsigned long convert_packet_pointer_to_fifo_value(void *pvPacket)
{       
        unsigned long ulFrameNr;
        unsigned long ulRamSegment;
        unsigned long ulValue;

        
        /* NOTE: This routine assumes that all RAM areas have a size of 0x00020000 bytes
         *       and that they are sequential one after the other.
         *       In this case the segment number is in bit 17 and 18.
         */
        ulRamSegment   = (unsigned long)pvPacket;
        ulRamSegment >>= 17U;
        ulRamSegment  &= 3U;

        /* Extract the frame buffer number. */
        ulFrameNr  = (unsigned long)pvPacket;
        ulFrameNr &= 0x1ffffU;
        ulFrameNr /= ETH_FRAME_BUF_SIZE;
        
        /* Combine the ram segment and frame number. */
        ulValue  = ulRamSegment << SRT_ETHMAC_FIFO_ELEMENT_INT_RAM_SEGMENT_NUM;
        ulValue |= ulFrameNr << SRT_ETHMAC_FIFO_ELEMENT_FRAME_BUF_NUM;

        return ulValue;
}



/*-----------------------------------------------------------------------------------------------------------*/


static unsigned int drv_eth_xc_get_link_status(void *pvUser __attribute__ ((unused)))
{
	NX4000_PHY_CTRL_AREA_T *ptPhyCtrl;
	unsigned long ulValue;
	unsigned int uiLinkStatus;


	uiLinkStatus = 0;

	/* retrieve the link status from the Ethernet port */
	ptPhyCtrl = aptPhyCtrl[tHandle.uiEthPortNr];
	ulValue  = ptPhyCtrl->ulInt_phy_ctrl_led;
	ulValue &= HOSTMSK(int_phy_ctrl_led_link_ro);
	if( ulValue!=0 )
	{
		uiLinkStatus = 1;
	}
	
	return uiLinkStatus;
}



static void *drv_eth_xc_get_empty_packet(void *pvUser __attribute__ ((unused)))
{
	NX4000_POINTER_FIFO_AREA_T *ptPfifoArea;
	unsigned long ulFifoValue;
	unsigned int uiFifoNr; 
	unsigned long ulValue;
	void *pvPacket;


	/* Expect no free packet. */
	pvPacket = NULL;

	/* get FIFO fill level and check if there is at least one element */
	uiFifoNr = (tHandle.uiXcPort * NUM_FIFO_CHANNELS_PER_UNIT) + ETHERNET_FIFO_EMPTY;

	/* keep at least one pointer for the XC level (two parties share this empty pointer FIFO) */
	ptPfifoArea = aptPFifo[tHandle.uiXcUnit];
	ulValue = ptPfifoArea->aulPfifo_fill_level[uiFifoNr];
	if( ulValue>1U )
	{
		/* retrieve the FIFO element */
		ulFifoValue = ptPfifoArea->aulPfifo[uiFifoNr];
		pvPacket = convert_fifo_value_to_packet_pointer(ulFifoValue);
	}

	return pvPacket;
}



static void drv_eth_xc_release_packet(void *pvPacket, void *pvUser __attribute__ ((unused)))
{
	NX4000_POINTER_FIFO_AREA_T *ptPfifoArea;
	unsigned int uiFifoNr; 
	unsigned long ulFifoValue;


	/* Convert the pointer to a FIFO value. */
	ulFifoValue = convert_packet_pointer_to_fifo_value(pvPacket);

	uiFifoNr = (tHandle.uiXcPort * NUM_FIFO_CHANNELS_PER_UNIT) + ETHERNET_FIFO_EMPTY;
	ptPfifoArea = aptPFifo[tHandle.uiXcUnit];
	ptPfifoArea->aulPfifo[uiFifoNr] = ulFifoValue;
}



static void drv_eth_xc_send_packet(void *pvPacket, size_t sizPacket, void *pvUser __attribute__ ((unused)))
{
	NX4000_POINTER_FIFO_AREA_T *ptPfifoArea;
	unsigned long ulFifoValue;
	unsigned int uiFifoNr;


	/* Pad too short packets. */
	if( sizPacket<ETHERNET_MINIMUM_FRAMELENGTH )
	{
		sizPacket = ETHERNET_MINIMUM_FRAMELENGTH;
	}
	/* Crop oversized packets. */
	else if( sizPacket>ETHERNET_MAXIMUM_FRAMELENGTH )
	{
		sizPacket = ETHERNET_MAXIMUM_FRAMELENGTH;
	}
	
	/* Convert the pointer to a FIFO value. */
	ulFifoValue  = convert_packet_pointer_to_fifo_value(pvPacket);
	/* Do not send a confirmation. */
	ulFifoValue |= MSK_ETHMAC_FIFO_ELEMENT_SUPPRESS_CON;
	/* Add the size information. */
	ulFifoValue |= sizPacket << SRT_ETHMAC_FIFO_ELEMENT_FRAME_LEN;

	uiFifoNr = (tHandle.uiXcPort * NUM_FIFO_CHANNELS_PER_UNIT) + ETHERNET_FIFO_REQ_LO;
	ptPfifoArea = aptPFifo[tHandle.uiXcUnit];

	ptPfifoArea->aulPfifo[uiFifoNr] = ulFifoValue;
}



static void *drv_eth_xc_get_received_packet(size_t *psizPacket, void *pvUser __attribute__ ((unused)))
{
	NX4000_POINTER_FIFO_AREA_T *ptPfifoArea;
	unsigned long ulFillLevel;
	unsigned int uiFifoNr;
	void *pvPacket;
	unsigned long ulFifoValue;
	size_t sizPacket;


	/* Expect no packet. */
	pvPacket = NULL;

	uiFifoNr = (tHandle.uiXcPort * NUM_FIFO_CHANNELS_PER_UNIT) + ETHERNET_FIFO_IND_LO;
	ptPfifoArea = aptPFifo[tHandle.uiXcUnit];

	ulFillLevel = ptPfifoArea->aulPfifo_fill_level[uiFifoNr];
	if( ulFillLevel!=0 )
	{
		ulFifoValue = ptPfifoArea->aulPfifo[uiFifoNr];
		pvPacket = convert_fifo_value_to_packet_pointer(ulFifoValue);

		ulFifoValue  &= MSK_ETHMAC_FIFO_ELEMENT_FRAME_LEN;
		ulFifoValue >>= SRT_ETHMAC_FIFO_ELEMENT_FRAME_LEN;
		sizPacket = (size_t)ulFifoValue;
		*psizPacket = sizPacket;
	}

	return pvPacket;
}



static void drv_eth_xc_deactivate(void *pvUser __attribute__ ((unused)))
{
}



static const NETWORK_IF_T tNetworkIfXc =
{
	.pfnGetLinkStatus = drv_eth_xc_get_link_status,
	.pfnGetEmptyPacket = drv_eth_xc_get_empty_packet,
	.pfnReleasePacket = drv_eth_xc_release_packet,
	.pfnSendPacket = drv_eth_xc_send_packet,
	.pfnGetReceivedPacket = drv_eth_xc_get_received_packet,
	.pfnDeactivate = drv_eth_xc_deactivate,
};



const NETWORK_IF_T *drv_eth_xc_initialize(unsigned int uiPort)
{
	const NETWORK_IF_T *ptIf;


	ptIf = NULL;

	/* Check the port number. */
	if( uiPort<=3U )
	{
		/* Initialize the internal handle. */
		tHandle.uiEthPortNr = uiPort;
		tHandle.uiXcUnit = (uiPort>>1)&1;
		tHandle.uiXcPort = uiPort & 1;

		ptIf = &tNetworkIfXc;
	}

	return ptIf;
}
