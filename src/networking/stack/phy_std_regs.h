
#ifndef __PHY_STD_REGS_H__
#define __PHY_STD_REGS_H__


/* phy registers */
#define PHY_REG_CONTROL				0		/* Control */
#define PHY_REG_STATUS				1		/* Status #1 */
#define PHY_REG_ID_1				2		/* PHY Identification 1 */
#define PHY_REG_ID_2				3		/* PHY Identification 2 */
#define PHY_REG_AUTO_NEG_ADVERTISEMENT		4		/* Auto-Negotiation Advertisement */


/* Register 0 - Control Register Bit Definition */
#define DFLT_PHY_CTRL				0x00003100U
#define MSK_PHY_CTRL_RESET			0x00008000U	/* PHY reset */
#define MSK_PHY_CTRL_LOOPBACK			0x00004000U	/* Enable loopback */
#define MSK_PHY_CTRL_SPEED_SELECT_1		0x00002000U	/* Speed selection */
#define MSK_PHY_CTRL_AUTO_NEG_ENABLE		0x00001000U	/* Auto-Negotiation Enable */
#define MSK_PHY_CTRL_POWER_DOWN			0x00000800U	/* Power-down */
#define MSK_PHY_CTRL_ISOLATE			0x00000400U	/* Isolate PHY from MII */
#define MSK_PHY_CTRL_AUTO_NEG_RESTART		0x00000200U	/* Restart Auto-Negotiation */
#define MSK_PHY_CTRL_FULL_DUPLEX		0x00000100U	/* Duplex Mode */
#define MSK_PHY_CTRL_COL_TEST			0x00000080U	/* Enable COL signal test */
#define MSK_PHY_CTRL_SPEED_SELECT_2		0x00000040U	/* Speed selection */

/* Register 1 - Status Register Bit Definition */
#define MSK_PHY_STATUS_100_BASE_T4		0x00008000U	/* 100BASE-T4 support */
#define MSK_PHY_STATUS_100_BASE_X_FDX		0x00004000U	/* 100BASE-X full duplex support */
#define MSK_PHY_STATUS_100_BASE_X_HDX		0x00002000U	/* 100BASE-X half duplex support */
#define MSK_PHY_STATUS_10_MBPS_FDX		0x00001000U	/* 10 Mbps full duplex support */
#define MSK_PHY_STATUS_10_MBPS_HDX		0x00000800U	/* 10 Mbps half duplex support */
#define MSK_PHY_STATUS_100_BASE_T2_FDX		0x00000400U	/* 100BASE-T2 full duplex support */
#define MSK_PHY_STATUS_100_BASE_T2_HDX		0x00000200U	/* 100BASE-T2 half duplex support */
#define MSK_PHY_STATUS_EXTENDED_STATUS		0x00000100U	/* Extended status availability */
#define MSK_PHY_STATUS_MF_PREAMBLE_SUPPRESS	0x00000040U	/* MF preamble suppress acceptance */
#define MSK_PHY_STATUS_AUTO_NEG_COMPLETE	0x00000020U	/* Auto-Negotiation complete */
#define MSK_PHY_STATUS_REMOTE_FAULT		0x00000010U	/* Remote fault detected */
#define MSK_PHY_STATUS_AUTO_NEG_ABILITY		0x00000008U	/* Auto-Negotiation ability */
#define MSK_PHY_STATUS_LINK_UP			0x00000004U	/* Link status */
#define MSK_PHY_STATUS_JABBER_DETECT		0x00000002U	/* Jabber detected */
#define MSK_PHY_STATUS_EXTENDED_CAPABILITY	0x00000001U	/* Basic/extended register capability */

/* Register 4 - Auto Negotiation Advertisement Register Bit Definition */
#define DFLT_PHY_ANADV				0x000001e1U
#define MSK_PHY_ANADV_NEXT_PAGE			0x00008000U	/* Ability to send multiple pages */
#define MSK_PHY_ANADV_REMOTE_FAULT		0x00002000U	/* Remote fault */
#define MSK_PHY_ANADV_ASYM_PAUSE		0x00000800U	/* Asymmetric pause operation */
#define MSK_PHY_ANADV_PAUSE			0x00000400U	/* Pause operation (for full-duplex) */
#define MSK_PHY_ANADV_100_BASE_T4		0x00000200U	/* 100BASE-T4 capability (not supp.) */
#define MSK_PHY_ANADV_100_BASE_TX_FDX		0x00000100U	/* 100BASE-TX full-duplex capability */
#define MSK_PHY_ANADV_100_BASE_TX		0x00000080U	/* 100BASE-TX capability */
#define MSK_PHY_ANADV_10_BASE_T_FDX		0x00000040U	/* 10BASE-T full-duplex capability */
#define MSK_PHY_ANADV_10_BASE_T			0x00000020U	/* 10BASE-T capability */


#endif	/* __PHY_STD_REGS_H__ */

