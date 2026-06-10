/****************************************************************************
 * drivers/net/stmmac.h
 *
 * Synopsys DWC Ethernet MAC (stmmac/dwmac) register definitions.
 * Used by Rockchip RK3566 GMAC and similar SoCs.
 *
 ****************************************************************************/

#ifndef __DRIVERS_NET_STMMAC_H
#define __DRIVERS_NET_STMMAC_H

/****************************************************************************
 * MAC Registers (offset from MAC base)
 ****************************************************************************/

#define STMMAC_MAC_CFG          0x0000  /* MAC Configuration */
#define STMMAC_MAC_EXT_CFG      0x0004  /* MAC Extended Configuration */
#define STMMAC_MAC_PKT_FLT      0x0008  /* MAC Packet Filter */
#define STMMAC_MAC_WDOG_TO      0x000c  /* MAC Watchdog Timeout */
#define STMMAC_MAC_HASH_TAB_0   0x0010  /* MAC Hash Table 0 */
#define STMMAC_MAC_HASH_TAB_1   0x0014  /* MAC Hash Table 1 */

#define STMMAC_MAC_ADDR_HI(n)   (0x0040 + (n) * 0x08)  /* MAC Address High */
#define STMMAC_MAC_ADDR_LO(n)   (0x0044 + (n) * 0x08)  /* MAC Address Low */

#define STMMAC_MAC_MDIO_ADDR    0x0200  /* MDIO Address */
#define STMMAC_MAC_MDIO_DATA    0x0204  /* MDIO Data */

/* MAC Configuration register bits */

#define MAC_CFG_RE              (1 << 0)   /* Receiver Enable */
#define MAC_CFG_TE              (1 << 1)   /* Transmitter Enable */
#define MAC_CFG_PRELEN_SHIFT    2
#define MAC_CFG_PRELEN_MASK     (3 << 2)
#define MAC_CFG_DC              (1 << 4)   /* Deferral Check */
#define MAC_CFG_BL_SHIFT        5
#define MAC_CFG_BL_MASK         (3 << 5)
#define MAC_CFG_DR              (1 << 8)   /* Disable Retry */
#define MAC_CFG_DCRS            (1 << 9)   /* Disable Carrier Sense */
#define MAC_CFG_ACS             (1 << 12)  /* Automatic CRC Stripping */
#define MAC_CFG_LM              (1 << 14)  /* Loopback Mode */
#define MAC_CFG_DM              (1 << 13)  /* Duplex Mode (1=full) */
#define MAC_CFG_FES             (1 << 14)  /* Speed (1=100Mbps, 0=10Mbps) */
#define MAC_CFG_PS              (1 << 15)  /* Port Select (1=MII, 0=GMII) */
#define MAC_CFG_JE              (1 << 16)  /* Jumbo Enable */
#define MAC_CFG_JD              (1 << 17)  /* Jabber Disable */
#define MAC_CFG_WD              (1 << 19)  /* Watchdog Disable */
#define MAC_CFG_TC              (1 << 24)  /* Transmit Configuration in RGMII */
#define MAC_CFG_CST             (1 << 25)  /* CRC Stripping for Type Frames */
#define MAC_CFG_SARC_SHIFT      28
#define MAC_CFG_SARC_MASK       (7 << 28)

/* MAC Packet Filter register bits */

#define MAC_PKT_FLT_PR          (1 << 0)   /* Promiscuous Mode */
#define MAC_PKT_FLT_HUC         (1 << 1)   /* Hash Unicast */
#define MAC_PKT_FLT_HMC         (1 << 2)   /* Hash Multicast */
#define MAC_PKT_FLT_DAIF        (1 << 3)   /* DA Inverse Filtering */
#define MAC_PKT_FLT_PM          (1 << 4)   /* Pass All Multicast */
#define MAC_PKT_FLT_DBF         (1 << 5)   /* Disable Broadcast Frames */
#define MAC_PKT_FLT_PCF_SHIFT   6
#define MAC_PKT_FLT_PCF_MASK    (3 << 6)
#define MAC_PKT_FLT_SAIF        (1 << 7)   /* SA Inverse Filtering */
#define MAC_PKT_FLT_SAF         (1 << 8)   /* Source Address Filter */
#define MAC_PKT_FLT_HPF         (1 << 10)  /* Hash or Perfect Filter */
#define MAC_PKT_FLT_VTFE        (1 << 16)  /* VLAN Tag Filter Enable */

/* MDIO Address register bits */

#define MDIO_ADDR_GD_ADDR_SHIFT 16
#define MDIO_ADDR_GD_ADDR_MASK  (0x1f << 16)
#define MDIO_ADDR_GR_SHIFT      11
#define MDIO_ADDR_GR_MASK       (0x1f << 11)
#define MDIO_ADDR_CR_SHIFT      8
#define MDIO_ADDR_CR_MASK       (0x7 << 8)  /* CSR Clock Range */
#define MDIO_ADDR_SKAP          (1 << 4)    /* Skip Address Packet */
#define MDIO_ADDR_GOC_SHIFT     2
#define MDIO_ADDR_GOC_MASK      (3 << 2)
#define MDIO_ADDR_GOC_WRITE     (1 << 2)    /* GMII Operation Command Write */
#define MDIO_ADDR_GOC_READ      (3 << 2)    /* GMII Operation Command Read */
#define MDIO_ADDR_C45E          (1 << 1)    /* Clause 45 Enable */
#define MDIO_ADDR_GB            (1 << 0)    /* GMII Busy */

/****************************************************************************
 * DMA Registers (offset from DMA base)
 ****************************************************************************/

#define STMMAC_DMA_BUS_MODE     0x0000  /* DMA Bus Mode */
#define STMMAC_DMA_TX_POLL      0x0004  /* DMA Transmit Poll Demand */
#define STMMAC_DMA_RX_POLL      0x0008  /* DMA Receive Poll Demand */
#define STMMAC_DMA_RX_DESC      0x000c  /* DMA Receive Descriptor List */
#define STMMAC_DMA_TX_DESC      0x0010  /* DMA Transmit Descriptor List */
#define STMMAC_DMA_STATUS       0x0014  /* DMA Status */
#define STMMAC_DMA_OP_MODE      0x0018  /* DMA Operation Mode */
#define STMMAC_DMA_INT_EN       0x001c  /* DMA Interrupt Enable */
#define STMMAC_DMA_MISS_FRM     0x0020  /* DMA Missed Frame */

/* DMA Bus Mode register bits */

#define DMA_BUS_MODE_SWR        (1 << 0)   /* Software Reset */
#define DMA_BUS_MODE_DA         (1 << 1)   /* DMA Arbitration Scheme */
#define DMA_BUS_MODE_DSL_SHIFT  2
#define DMA_BUS_MODE_DSL_MASK   (0x1f << 2)
#define DMA_BUS_MODE_ATDS       (1 << 7)   /* Alternate Descriptor Size */
#define DMA_BUS_MODE_PBL_SHIFT  8
#define DMA_BUS_MODE_PBL_MASK   (0x3f << 8) /* Programmable Burst Length */
#define DMA_BUS_MODE_PR_SHIFT   14
#define DMA_BUS_MODE_PR_MASK    (3 << 14)  /* Priority Ratio */
#define DMA_BUS_MODE_FB         (1 << 16)  /* Fixed Burst */
#define DMA_BUS_MODE_RPBL_SHIFT 17
#define DMA_BUS_MODE_RPBL_MASK  (0x3f << 17)
#define DMA_BUS_MODE_USP        (1 << 23)  /* Use Separate PBL */
#define DMA_BUS_MODE_8xPBL      (1 << 24)  /* 8x PBL Mode */
#define DMA_BUS_MODE_AAL        (1 << 25)  /* Address Aligned Beats */

/* DMA Status register bits */

#define DMA_STATUS_TI           (1 << 0)   /* Transmit Interrupt */
#define DMA_STATUS_TPS           (1 << 1)   /* Transmit Process Stopped */
#define DMA_STATUS_TU           (1 << 2)   /* Transmit Buffer Unavailable */
#define DMA_STATUS_TJT          (1 << 3)   /* Transmit Jabber Timeout */
#define DMA_STATUS_OVF          (1 << 4)   /* Receive Overflow */
#define DMA_STATUS_UNF          (1 << 5)   /* Transmit Underflow */
#define DMA_STATUS_RI           (1 << 6)   /* Receive Interrupt */
#define DMA_STATUS_RU           (1 << 7)   /* Receive Buffer Unavailable */
#define DMA_STATUS_RPS          (1 << 8)   /* Receive Process Stopped */
#define DMA_STATUS_RWT          (1 << 9)   /* Receive Watchdog Timeout */
#define DMA_STATUS_ETI          (1 << 10)  /* Early Transmit Interrupt */
#define DMA_STATUS_FBI          (1 << 13)  /* Fatal Bus Error Interrupt */
#define DMA_STATUS_ERI          (1 << 14)  /* Early Receive Interrupt */
#define DMA_STATUS_AIS          (1 << 15)  /* Abnormal Interrupt Summary */
#define DMA_STATUS_NIS          (1 << 16)  /* Normal Interrupt Summary */
#define DMA_STATUS_RS_SHIFT     17
#define DMA_STATUS_RS_MASK      (7 << 17)  /* Receive Process State */
#define DMA_STATUS_TS_SHIFT     20
#define DMA_STATUS_TS_MASK      (7 << 20)  /* Transmit Process State */
#define DMA_STATUS_EB_SHIFT     23
#define DMA_STATUS_EB_MASK      (7 << 23)  /* Error Bits */

/* DMA Operation Mode register bits */

#define DMA_OP_MODE_SR          (1 << 1)   /* Start/Stop Receive */
#define DMA_OP_MODE_OSF         (1 << 2)   /* Operate on Second Frame */
#define DMA_OP_MODE_RTC_SHIFT   3
#define DMA_OP_MODE_RTC_MASK    (3 << 3)   /* Receive Threshold Control */
#define DMA_OP_MODE_FUF         (1 << 6)   /* Forward Undersized Good Frames */
#define DMA_OP_MODE_FEF         (1 << 7)   /* Forward Error Frames */
#define DMA_OP_MODE_ST          (1 << 13)  /* Start/Stop Transmit */
#define DMA_OP_MODE_TTC_SHIFT   14
#define DMA_OP_MODE_TTC_MASK    (7 << 14)  /* Transmit Threshold Control */
#define DMA_OP_MODE_FTF         (1 << 20)  /* Flush Transmit FIFO */
#define DMA_OP_MODE_TSF         (1 << 21)  /* Transmit Store and Forward */
#define DMA_OP_MODE_RSF         (1 << 25)  /* Receive Store and Forward */
#define DMA_OP_MODE_DT          (1 << 26)  /* Disable Dropping TCP/IP Checksum */

/* DMA Interrupt Enable register bits */

#define DMA_INT_EN_TIE          (1 << 0)   /* Transmit Interrupt Enable */
#define DMA_INT_EN_TSE          (1 << 1)   /* Transmit Stopped Enable */
#define DMA_INT_EN_TUE          (1 << 2)   /* Transmit Buffer Unavailable */
#define DMA_INT_EN_TJE          (1 << 3)   /* Transmit Jabber Timeout */
#define DMA_INT_EN_OVE          (1 << 4)   /* Overflow Interrupt Enable */
#define DMA_INT_EN_UNE          (1 << 5)   /* Underflow Interrupt Enable */
#define DMA_INT_EN_RIE          (1 << 6)   /* Receive Interrupt Enable */
#define DMA_INT_EN_RUE          (1 << 7)   /* Receive Buffer Unavailable */
#define DMA_INT_EN_RSE          (1 << 8)   /* Receive Stopped Enable */
#define DMA_INT_EN_RWE          (1 << 9)   /* Receive Watchdog Timeout */
#define DMA_INT_EN_ETE          (1 << 10)  /* Early Transmit Interrupt */
#define DMA_INT_EN_FBE          (1 << 13)  /* Fatal Bus Error Enable */
#define DMA_INT_EN_ERE          (1 << 14)  /* Early Receive Interrupt */
#define DMA_INT_EN_AIE          (1 << 15)  /* Abnormal Interrupt Summary */
#define DMA_INT_EN_NIE          (1 << 16)  /* Normal Interrupt Summary */

/****************************************************************************
 * DMA Descriptor Structure
 ****************************************************************************/

/* Normal descriptor (used when ATDS=0) */

struct stmmac_dma_desc_s
{
  uint32_t des0;  /* Status / Control */
  uint32_t des1;  /* Size / Control */
  uint32_t des2;  /* Buffer Address 1 */
  uint32_t des3;  /* Buffer Address 2 / Next Descriptor */
};

/* TX Descriptor bits (DES0) */

#define TX_DESC_OWN             (1 << 31)  /* Own Bit (1=DMA, 0=CPU) */
#define TX_DESC_IC              (1 << 30)  /* Interrupt on Completion */
#define TX_DESC_LS              (1 << 29)  /* Last Segment */
#define TX_DESC_FS              (1 << 28)  /* First Segment */
#define TX_DESC_CIC_SHIFT       27
#define TX_DESC_CIC_MASK        (1 << 27)  /* Checksum Insertion Control */
#define TX_DESC_DC              (1 << 26)  /* Disable CRC */
#define TX_DESC_TER             (1 << 25)  /* Transmit End of Ring */
#define TX_DESC_TCH             (1 << 24)  /* Second Address Chained */
#define TX_DESC_DP              (1 << 23)  /* Disable Padding */
#define TX_DESC_TTSE            (1 << 24)  /* Transmit Timestamp Status */
#define TX_DESC_PPM             (1 << 26)  /* PTP Mode */
#define TX_DESC_FT              (1 << 27)  /* Frame Type */
#define TX_DESC_IHE             (1 << 16)  /* IP Header Error */
#define TX_DESC_ES              (1 << 15)  /* Error Summary */
#define TX_DESC_JT              (1 << 14)  /* Jabber Timeout */
#define TX_DESC_FF              (1 << 13)  /* Frame Flushed */
#define TX_DESC_PCE             (1 << 12)  /* Payload Checksum Error */
#define TX_DESC_LOC             (1 << 11)  /* Loss of Carrier */
#define TT_DESC_NC              (1 << 10)  /* No Carrier */
#define TX_DESC_LC              (1 << 9)   /* Late Collision */
#define TX_DESC_EC              (1 << 8)   /* Excessive Collision */
#define TX_DESC_VF              (1 << 7)   /* VLAN Frame */
#define TX_DESC_CC_SHIFT        3
#define TX_DESC_CC_MASK         (0xf << 3) /* Collision Count */
#define TX_DESC_ED              (1 << 2)   /* Excessive Deferral */
#define TX_DESC_UF              (1 << 1)   /* Underflow Error */
#define TX_DESC_DB              (1 << 0)   /* Deferred Bit */

/* TX Descriptor bits (DES1) */

#define TX_DESC_TBS1_SHIFT      0
#define TX_DESC_TBS1_MASK       0x1fff     /* Buffer 1 Size */
#define TX_DESC_TBS2_SHIFT      16
#define TX_DESC_TBS2_MASK       0x1fff0000 /* Buffer 2 Size */

/* RX Descriptor bits (DES0) */

#define RX_DESC_OWN             (1 << 31)  /* Own Bit */
#define RX_DESC_AFM             (30 << 1)  /* Destination Address Filter */
#define RX_DESC_FL_SHIFT        16
#define RX_DESC_FL_MASK         0x3fff0000 /* Frame Length */
#define RX_DESC_ES              (1 << 15)  /* Error Summary */
#define RX_DESC_DE              (1 << 14)  /* Descriptor Error */
#define RX_DESC_SAF             (1 << 13)  /* Source Address Filter Fail */
#define RX_DESC_LE              (1 << 12)  /* Length Error */
#define RX_DESC_OE              (1 << 11)  /* Overflow Error */
#define RX_DESC_VLAN            (1 << 10)  /* VLAN Tag */
#define RX_DESC_FS              (1 << 9)   /* First Descriptor */
#define RX_DESC_LS              (1 << 8)   /* Last Descriptor */
#define RX_DESC_TSA             (1 << 7)   /* Timestamp Available */
#define RX_DESC_LC              (1 << 6)   /* Late Collision */
#define RX_DESC_FT              (1 << 5)   /* Frame Type */
#define RX_DESC_RWT             (1 << 4)   /* Receive Watchdog Timeout */
#define RX_DESC_RE              (1 << 3)   /* Receive Error */
#define RX_DESC_DBE             (1 << 2)   /* Dribble Bit Error */
#define RX_DESC_CE              (1 << 1)   /* CRC Error */
#define RX_DESC_MAMP             (1 << 0)   /* MAC Address Match */

/* RX Descriptor bits (DES1) */

#define RX_DESC_RBS1_SHIFT      0
#define RX_DESC_RBS1_MASK       0x1fff     /* Buffer 1 Size */
#define RX_DESC_RER             (1 << 25)  /* Receive End of Ring */
#define RX_DESC_RCH             (1 << 24)  /* Second Address Chained */
#define RX_DESC_RBS2_SHIFT      16
#define RX_DESC_RBS2_MASK       0x1fff0000 /* Buffer 2 Size */

/****************************************************************************
 * Configuration
 ****************************************************************************/

#define STMMAC_DMA_DESC_COUNT   64         /* Number of DMA descriptors */
#define STMMAC_RX_BUFSIZE       1536       /* RX buffer size (MTU + headers) */

#endif /* __DRIVERS_NET_STMMAC_H */
