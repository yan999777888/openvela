/****************************************************************************
 * drivers/net/stmmac.c
 *
 * Synopsys DWC Ethernet MAC (stmmac/dwmac) driver for NuttX.
 * Supports Rockchip RK3566 GMAC.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#if defined(CONFIG_NET) && defined(CONFIG_NET_STMMAC)

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <debug.h>
#include <unistd.h>

#include <arpa/inet.h>

#include <nuttx/arch.h>
#include <nuttx/irq.h>
#include <nuttx/kmalloc.h>
#include <nuttx/wqueue.h>
#include <nuttx/net/ip.h>
#include <nuttx/net/netdev.h>
#include <nuttx/net/mii.h>
#include <nuttx/clock.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Register access macros */

#define putreg32(v, x) (*(FAR volatile uint32_t *)(x) = (v))
#define getreg32(x)    (*(FAR volatile uint32_t *)(x))

/* DMA descriptor count and buffer size */

#define STMMAC_DMA_DESC_COUNT   64
#define STMMAC_RX_BUFSIZE       1536

/* MDIO clock range (CSR clock divider) */

#ifndef CONFIG_STMMAC_MDIO_CR
#  define CONFIG_STMMAC_MDIO_CR 6
#endif

#ifndef CONFIG_STMMAC_PHY_ADDR
#  define CONFIG_STMMAC_PHY_ADDR 0
#endif

/****************************************************************************
 * MAC Register Offsets
 ****************************************************************************/

#define MAC_CFG          0x0000  /* MAC Configuration */
#define MAC_PKT_FLT      0x0008  /* MAC Packet Filter */
#define MAC_ADDR_HI(n)   (0x0040 + (n) * 0x08)
#define MAC_ADDR_LO(n)   (0x0044 + (n) * 0x08)
#define MAC_MDIO_ADDR    0x0200  /* MDIO Address */
#define MAC_MDIO_DATA    0x0204  /* MDIO Data */

/* MAC Configuration bits */

#define MAC_CFG_RE       (1 << 0)   /* Receiver Enable */
#define MAC_CFG_TE       (1 << 1)   /* Transmitter Enable */
#define MAC_CFG_DM       (1 << 13)  /* Duplex Mode (1=full) */
#define MAC_CFG_FES      (1 << 14)  /* Speed (1=100Mbps) */
#define MAC_CFG_PS       (1 << 15)  /* Port Select (1=MII) */
#define MAC_CFG_JD       (1 << 17)  /* Jabber Disable */
#define MAC_CFG_WD       (1 << 19)  /* Watchdog Disable */
#define MAC_CFG_TC       (1 << 24)  /* TX config in RGMII */
#define MAC_CFG_ACS      (1 << 12)  /* Auto CRC Strip */
#define MAC_CFG_CST      (1 << 25)  /* CRC Strip Type */

/* MDIO Address register
 * GMAC v4.20a has a different layout than standard stmmac:
 * Bits [25:21] = GMII Device Address (PA/PDA)
 * Bits [20:16] = GMII Register Address (GR)
 * Bit  [15]    = Clause 45 Enable (C45E)
 * Bits [13:11] = CSR Clock Range (CR)
 * Bit  [2:1]   = GMII Operation Command (GOC): 01=write, 10=read (or 10/11)
 * Bit  [0]     = GMII Busy (GB)
 */

#define MDIO_ADDR_GB       (1 << 0)     /* GMII Busy */
#define MDIO_ADDR_GOC_WR   (2 << 1)     /* Write command */
#define MDIO_ADDR_GOC_RD   (3 << 1)     /* Read command */
#define MDIO_ADDR_CR_SHIFT 11
#define MDIO_ADDR_GR_SHIFT 16
#define MDIO_ADDR_GA_SHIFT 21

/* MAC Extended Configuration and RGMII registers */

#define MAC_EXT_CFG           0x0004  /* Extended Conf */
#define MAC_EXT_CFG_SPDSEL    (3 << 29) /* Speed Selection for RGMII */

/****************************************************************************
 * DMA Register Offsets
 ****************************************************************************/

#define DMA_BUS_MODE     0x0000
#define DMA_TX_POLL      0x0004
#define DMA_RX_POLL      0x0008
#define DMA_RX_DESC      0x000c
#define DMA_TX_DESC      0x0010
#define DMA_STATUS       0x0014
#define DMA_OP_MODE      0x0018
#define DMA_INT_EN       0x001c

/* DMA Bus Mode bits */

#define DMA_BUS_MODE_SWR (1 << 0)   /* Software Reset */
#define DMA_BUS_MODE_PBL_SHIFT 8
#define DMA_BUS_MODE_FB  (1 << 16)  /* Fixed Burst */
#define DMA_BUS_MODE_AAL (1 << 25)  /* Address Aligned Beats */

/* DMA Status bits */

#define DMA_STATUS_TI    (1 << 0)   /* TX Interrupt */
#define DMA_STATUS_RI    (1 << 6)   /* RX Interrupt */
#define DMA_STATUS_NIS   (1 << 16)  /* Normal Summary */
#define DMA_STATUS_AIS   (1 << 15)  /* Abnormal Summary */

/* DMA Operation Mode bits */

#define DMA_OP_MODE_SR   (1 << 1)   /* Start Receive */
#define DMA_OP_MODE_ST   (1 << 13)  /* Start Transmit */
#define DMA_OP_MODE_TSF  (1 << 21)  /* TX Store & Forward */
#define DMA_OP_MODE_RSF  (1 << 25)  /* RX Store & Forward */
#define DMA_OP_MODE_FTF  (1 << 20)  /* Flush TX FIFO */

/* DMA Interrupt Enable bits */

#define DMA_INT_EN_TIE   (1 << 0)
#define DMA_INT_EN_RIE   (1 << 6)
#define DMA_INT_EN_NIE   (1 << 16)
#define DMA_INT_EN_AIE   (1 << 15)

/****************************************************************************
 * DMA Descriptor
 ****************************************************************************/

struct stmmac_dma_desc_s
{
  volatile uint32_t des0;
  volatile uint32_t des1;
  volatile uint32_t des2;
  volatile uint32_t des3;
};

/* TX descriptor DES0 bits */

#define TX_DESC_OWN      (1u << 31)
#define TX_DESC_LS       (1 << 29)
#define TX_DESC_FS       (1 << 28)
#define TX_DESC_TER      (1 << 25)
#define TX_DESC_TCH      (1 << 24)

/* RX descriptor DES0 bits */

#define RX_DESC_OWN      (1u << 31)
#define RX_DESC_FL_SHIFT 16
#define RX_DESC_FL_MASK  0x3fff0000
#define RX_DESC_ES       (1 << 15)
#define RX_DESC_FS       (1 << 9)
#define RX_DESC_LS       (1 << 8)

/* RX descriptor DES1 bits */

#define RX_DESC_RCH      (1 << 24)
#define RX_DESC_RER      (1 << 25)
#define RX_DESC_RBS1_MASK 0x1fff

/****************************************************************************
 * Private Types
 ****************************************************************************/

struct stmmac_priv_s
{
  struct net_driver_s dev;        /* NuttX network device */
  struct work_s       irqwork;    /* Interrupt work queue */

  uintptr_t mac_base;             /* MAC register base */
  uintptr_t dma_base;             /* DMA register base */

  /* DMA descriptors (must be aligned) */

  struct stmmac_dma_desc_s *tx_desc;
  struct stmmac_dma_desc_s *rx_desc;
  uint8_t *tx_buf[STMMAC_DMA_DESC_COUNT];
  uint8_t *rx_buf[STMMAC_DMA_DESC_COUNT];

  /* Descriptor indices */

  uint32_t tx_head;
  uint32_t tx_tail;
  uint32_t rx_idx;

  /* PHY state */

  int  phy_addr;
  bool link_up;
  int  speed;
  bool duplex;
};

/****************************************************************************
 * Private Function Prototypes
 ****************************************************************************/

static int  stmmac_mdio_read(struct stmmac_priv_s *priv, int addr, int reg);
static void stmmac_mdio_write(struct stmmac_priv_s *priv, int addr,
                              int reg, int val);
static int  stmmac_phy_init(struct stmmac_priv_s *priv);
static int  stmmac_phy_update_link(struct stmmac_priv_s *priv);
static int  stmmac_dma_init(struct stmmac_priv_s *priv);
static void stmmac_tx(struct stmmac_priv_s *priv, FAR const uint8_t *buf,
                      uint32_t len);
static int  stmmac_txpoll(FAR struct net_driver_s *dev);
static void stmmac_rx(struct stmmac_priv_s *priv);
static void stmmac_irq_work(void *arg);
static int  stmmac_interrupt(int irq, void *context, void *arg);
static int  stmmac_ifup(FAR struct net_driver_s *dev);
static int  stmmac_ifdown(FAR struct net_driver_s *dev);
static int  stmmac_txavail(FAR struct net_driver_s *dev);
#ifdef CONFIG_NET_MCASTGROUP
static int  stmmac_addmac(FAR struct net_driver_s *dev,
                          FAR const uint8_t *mac);
static int  stmmac_rmmac(FAR struct net_driver_s *dev,
                          FAR const uint8_t *mac);
#endif
#ifdef CONFIG_NETDEV_IOCTL
static int  stmmac_ioctl(FAR struct net_driver_s *dev, int cmd,
                         unsigned long arg);
#endif

/****************************************************************************
 * Private Functions: MDIO / PHY
 ****************************************************************************/

static int stmmac_mdio_read(struct stmmac_priv_s *priv, int addr, int reg)
{
  uint32_t v;
  int timeout = 1000;

  v = (addr << MDIO_ADDR_GA_SHIFT) | (reg << MDIO_ADDR_GR_SHIFT) |
      (CONFIG_STMMAC_MDIO_CR << MDIO_ADDR_CR_SHIFT) |
      MDIO_ADDR_GOC_RD | MDIO_ADDR_GB;
  putreg32(v, priv->mac_base + MAC_MDIO_ADDR);

  while (timeout-- > 0)
    {
      if (!(getreg32(priv->mac_base + MAC_MDIO_ADDR) & MDIO_ADDR_GB))
        {
          return getreg32(priv->mac_base + MAC_MDIO_DATA) & 0xffff;
        }

      up_udelay(10);
    }

  return -ETIMEDOUT;
}

static void __attribute__((unused)) stmmac_mdio_write(struct stmmac_priv_s *priv,
                              int addr, int reg, int val)
{
  uint32_t v;
  int timeout = 1000;

  putreg32(val & 0xffff, priv->mac_base + MAC_MDIO_DATA);
  v = (addr << MDIO_ADDR_GA_SHIFT) | (reg << MDIO_ADDR_GR_SHIFT) |
      (CONFIG_STMMAC_MDIO_CR << MDIO_ADDR_CR_SHIFT) |
      MDIO_ADDR_GOC_WR | MDIO_ADDR_GB;
  putreg32(v, priv->mac_base + MAC_MDIO_ADDR);

  while (timeout-- > 0)
    {
      if (!(getreg32(priv->mac_base + MAC_MDIO_ADDR) & MDIO_ADDR_GB))
        {
          return;
        }

      up_udelay(10);
    }
}

static int stmmac_phy_init(struct stmmac_priv_s *priv)
{
  int id;
  int timeout;

  /* Try MDIO scan first */

  for (priv->phy_addr = 0; priv->phy_addr < 32; priv->phy_addr++)
    {
      id = stmmac_mdio_read(priv, priv->phy_addr, MII_PHYID1);
      if (id > 0 && id != 0xffff)
        {
          _err("PHY found at addr %d (ID: 0x%04x)\n", priv->phy_addr, id);
          break;
        }
    }

  if (priv->phy_addr >= 32)
    {
      _err("PHY: no scan response, using addr 0\n");
      priv->phy_addr = 0;
    }

  /* Software reset PHY */

  stmmac_mdio_write(priv, priv->phy_addr, MII_MCR, MII_MCR_RESET);
  timeout = 100;
  while (timeout-- > 0)
    {
      up_mdelay(10);
      if (!(stmmac_mdio_read(priv, priv->phy_addr, MII_MCR) &
            MII_MCR_RESET))
        {
          break;
        }
    }

  _err("PHY: reset done, auto-neg...\n");

  /* Start auto-negotiation */

  stmmac_mdio_write(priv, priv->phy_addr, MII_MCR,
                    MII_MCR_ANENABLE | MII_MCR_ANRESTART);

  /* Wait for auto-negotiation */

  timeout = 500;
  while (timeout-- > 0)
    {
      int bmsr = stmmac_mdio_read(priv, priv->phy_addr, MII_MSR);
      if (bmsr & MII_MSR_ANEGCOMPLETE)
        {
          break;
        }

      up_mdelay(10);
    }

  int bmcr = stmmac_mdio_read(priv, priv->phy_addr, MII_MCR);
  int lpa = stmmac_mdio_read(priv, priv->phy_addr, MII_ADVERTISE);

  _err("PHY: BMCR=0x%04x LPA=0x%04x BMSR=0x%04x\n",
       bmcr, lpa, stmmac_mdio_read(priv, priv->phy_addr, MII_MSR));

  if (lpa & MII_ADVERTISE_100BASETXFULL)
    {
      priv->speed = 100;
      priv->duplex = true;
    }
  else if (lpa & MII_ADVERTISE_100BASETXHALF)
    {
      priv->speed = 100;
      priv->duplex = false;
    }
  else if (lpa & MII_ADVERTISE_10BASETXFULL)
    {
      priv->speed = 10;
      priv->duplex = true;
    }
  else
    {
      priv->speed = 10;
      priv->duplex = false;
    }

  priv->link_up = true;

  _err("PHY: %dMbps %s-duplex\n",
       priv->speed, priv->duplex ? "full" : "half");

  return OK;

#if 0  /* Dead code: CRU not accessible at EL2 */
  /* Enable GMAC1 clocks in CRU.
   * From Linux: CON3=0xf9f, CON26=0xe00
   * Use volatile pointer writes to bypass MMU caching issues.
   * CRU register format: bits[15:8] = write-enable mask, bits[7:0] = value
   */

  volatile uint32_t *crui = (volatile uint32_t *)(cru_base + 0x300);

  _err("CRU CON3=0x%08x CON26=0x%08x\n", crui[0x0c], crui[0x9a]);

  /* Enable GMAC1 clock gates in CON3 (offset 0x30c = 0x300 + 0x0c) */

  crui[0x0c] = 0x009f;

  /* Enable clk_mac_speed in CON26 (offset 0x368 = 0x300 + 0x9a) */

  crui[0x9a] = 0x0e00;

  up_udelay(100);

  _err("After enable: CON3=0x%08x CON26=0x%08x\n", crui[0x0c], crui[0x9a]);

  _err("After clock enable: MAC_CFG=0x%08x\n",
       getreg32(priv->mac_base + MAC_CFG));

  /* Software reset MAC */

  putreg32(MAC_CFG_RE, priv->mac_base + MAC_CFG);
  up_mdelay(10);
  _err("After MAC reset: MAC_CFG=0x%08x\n",
       getreg32(priv->mac_base + MAC_CFG));

  /* Test MDIO register accessibility */

  _err("MDIO_ADDR before: 0x%08x\n",
       getreg32(priv->mac_base + MAC_MDIO_ADDR));
  putreg32(0xdeadbeef, priv->mac_base + MAC_MDIO_ADDR);
  _err("MDIO_ADDR after write 0xdeadbeef: 0x%08x\n",
       getreg32(priv->mac_base + MAC_MDIO_ADDR));
  putreg32(0, priv->mac_base + MAC_MDIO_ADDR);

  /* Also dump some key register addresses */

  _err("DMA_BUS_MODE (0x%08lx)=0x%08x\n",
       priv->dma_base + 0x00, getreg32(priv->dma_base + 0x00));
  _err("DMA_STATUS   (0x%08lx)=0x%08x\n",
       priv->dma_base + 0x14, getreg32(priv->dma_base + 0x14));
  _err("MAC_ADDR_HI0 (0x%08lx)=0x%08x\n",
       priv->mac_base + 0x40, getreg32(priv->mac_base + 0x40));
  _err("MAC_ADDR_LO0 (0x%08lx)=0x%08x\n",
       priv->mac_base + 0x44, getreg32(priv->mac_base + 0x44));

  /* Test MDIO read for PHY addr 0, reg 2 (PHYID1) with debug */

  {
    uint32_t addr_val;
    uint32_t data_val;

    addr_val = (0 << MDIO_ADDR_GA_SHIFT) |   /* PHY addr 0 */
               (2 << MDIO_ADDR_GR_SHIFT) |   /* Reg 2 (PHYID1) */
               (CONFIG_STMMAC_MDIO_CR << MDIO_ADDR_CR_SHIFT) |
               MDIO_ADDR_GOC_RD | MDIO_ADDR_GB;

    _err("MDIO test: writing 0x%08x to MDIO_ADDR\n", addr_val);
    putreg32(addr_val, priv->mac_base + MAC_MDIO_ADDR);

    up_udelay(100);

    _err("MDIO test: MDIO_ADDR=0x%08x, MDIO_DATA=0x%08x\n",
         getreg32(priv->mac_base + MAC_MDIO_ADDR),
         getreg32(priv->mac_base + MAC_MDIO_DATA));

    /* Wait for busy to clear */

    int t = 1000;
    while (t-- > 0)
      {
        if (!(getreg32(priv->mac_base + MAC_MDIO_ADDR) & MDIO_ADDR_GB))
          break;
        up_udelay(10);
      }

    data_val = getreg32(priv->mac_base + MAC_MDIO_DATA);
    _err("MDIO test result: addr=0x%08x data=0x%08x (timeout=%d)\n",
         getreg32(priv->mac_base + MAC_MDIO_ADDR), data_val, t);
  }
  /* Configure MDIO pins: GPIO3 pin 14 (MDC) and pin 15 (MDIO)
   * DTS: rockchip,pins = <0x03 0x14 0x03 0x111 0x03 0x15 0x03 0x111>;
   * GPIO3 IOMUX_H register at GRF (0xfdc60000) + 0x104
   * Pin 14: bits [5:4]=func, [7:6]=pull, [9:8]=drv
   * Pin 15: bits [13:12]=func, [15:14]=pull, [18:16]=drv
   * Function 3 (GMAC1 MDIO), pull-up, normal drive
   * Write-enable: set bit [16+N] for each bit N being written
   */

  uintptr_t grf_base = 0xfdc60000;

  /* GPIO3 pin 14: func=3, pull-up, drv=normal */

  putreg32(0x00030330, grf_base + 0x104);

  /* GPIO3 pin 15: func=3, pull-up, drv=normal */

  putreg32(0x00c30c30, grf_base + 0x104);

  up_udelay(100);

  /* Scan PHY */

  /* Known PHY from Linux: address 0, Realtek gigabit PHY
   * MDIO returns 0xffff because MAC is not fully configured yet.
   * Configure MAC first, then verify MDIO.
   */

  priv->phy_addr = 0;
  priv->speed = 1000;
  priv->duplex = true;

  _err("PHY: using known addr 0 (Realtek GbE)\n");

  /* Software reset PHY */

  stmmac_mdio_write(priv, priv->phy_addr, MII_MCR, MII_MCR_RESET);

  while (timeout-- > 0)
    {
      up_mdelay(10);
      if (!(stmmac_mdio_read(priv, priv->phy_addr, MII_MCR) &
            MII_MCR_RESET))
        {
          break;
        }
    }

  /* Auto-negotiate */

  stmmac_mdio_write(priv, priv->phy_addr, MII_MCR,
                    MII_MCR_ANENABLE | MII_MCR_ANRESTART);

  priv->link_up = false;
  priv->speed   = 100;
  priv->duplex  = true;
  return OK;
#endif /* Dead code: CRU not accessible at EL2 */
}

static int stmmac_phy_update_link(struct stmmac_priv_s *priv)
{
  int bmsr;
  int bmcr;
  int adv;
  int lpa;
  int common;

  bmsr = stmmac_mdio_read(priv, priv->phy_addr, MII_MSR);
  if (bmsr < 0)
    {
      return bmsr;
    }

  if (!(bmsr & MII_MSR_LINKSTATUS))
    {
      if (priv->link_up)
        {
          ninfo("Link down\n");
          priv->link_up = false;
        }

      return OK;
    }

  if (!priv->link_up)
    {
      bmcr = stmmac_mdio_read(priv, priv->phy_addr, MII_MCR);

      if (bmcr & MII_MCR_ANENABLE)
        {
          adv = stmmac_mdio_read(priv, priv->phy_addr, MII_ADVERTISE);
          lpa = stmmac_mdio_read(priv, priv->phy_addr, MII_LPA);
          common = adv & lpa;

          if (common & MII_ADVERTISE_100BASETXFULL)
            {
              priv->speed = 100; priv->duplex = true;
            }
          else if (common & MII_ADVERTISE_100BASETXHALF)
            {
              priv->speed = 100; priv->duplex = false;
            }
          else if (common & MII_ADVERTISE_10BASETXFULL)
            {
              priv->speed = 10; priv->duplex = true;
            }
          else
            {
              priv->speed = 10; priv->duplex = false;
            }
        }
      else
        {
          priv->speed  = (bmcr & MII_MCR_SPEED100) ? 100 : 10;
          priv->duplex = (bmcr & MII_MCR_FULLDPLX) ? true : false;
        }

      priv->link_up = true;
      ninfo("Link up: %dMbps %s-duplex\n",
            priv->speed, priv->duplex ? "full" : "half");
    }

  return OK;
}

/****************************************************************************
 * Private Functions: DMA
 ****************************************************************************/

static int stmmac_dma_init(struct stmmac_priv_s *priv)
{
  int i;
  uint32_t busmode;

  /* Read current DMA bus mode - if clocks are off, this reads 0 */

  busmode = getreg32(priv->dma_base + DMA_BUS_MODE);
  _err("DMA: busmode=0x%08x (before reset)\n", busmode);

  /* Only reset if DMA is already accessible (clocks enabled).
   * If busmode==0, clocks are not enabled and reset will hang.
   */

  if (busmode != 0)
    {
      /* DMA registers accessible. Skip software reset when coming
       * from Linux warm reboot - hardware is already configured.
       * Just clear any pending state.
       */

      _err("DMA: skipping reset (busmode=0x%08x)\n", busmode);
      putreg32(0, priv->dma_base + DMA_BUS_MODE);
      up_udelay(100);
    }
  else
    {
      _err("DMA: clocks not enabled (busmode=0)\n");
      return -ETIMEDOUT;
    }

  /* Allocate descriptors */

  priv->tx_desc = kmm_zalloc(STMMAC_DMA_DESC_COUNT *
                              sizeof(struct stmmac_dma_desc_s));
  _err("DMA: tx_desc=%p size=%d\n", priv->tx_desc,
       (int)(STMMAC_DMA_DESC_COUNT * sizeof(struct stmmac_dma_desc_s)));
  priv->rx_desc = kmm_zalloc(STMMAC_DMA_DESC_COUNT *
                              sizeof(struct stmmac_dma_desc_s));
  _err("DMA: rx_desc=%p\n", priv->rx_desc);
  if (!priv->tx_desc || !priv->rx_desc)
    {
      _err("DMA: DESC ALLOC FAILED\n");
      return -ENOMEM;
    }

  /* Allocate buffers */

  for (i = 0; i < STMMAC_DMA_DESC_COUNT; i++)
    {
      priv->tx_buf[i] = kmm_zalloc(STMMAC_RX_BUFSIZE);
      priv->rx_buf[i] = kmm_zalloc(STMMAC_RX_BUFSIZE);
      if (!priv->tx_buf[i] || !priv->rx_buf[i])
        {
          _err("DMA: BUF ALLOC FAILED at i=%d tx=%p rx=%p\n",
               i, priv->tx_buf[i], priv->rx_buf[i]);
          return -ENOMEM;
        }
    }
  _err("DMA: all allocations OK\n");

  /* Init TX descriptors */

  for (i = 0; i < STMMAC_DMA_DESC_COUNT; i++)
    {
      priv->tx_desc[i].des0 = 0;
      priv->tx_desc[i].des1 = 0;
      priv->tx_desc[i].des2 = (uint32_t)(uintptr_t)priv->tx_buf[i];
      priv->tx_desc[i].des3 = 0;
    }

  priv->tx_desc[STMMAC_DMA_DESC_COUNT - 1].des1 |= TX_DESC_TER;

  /* Init RX descriptors */

  for (i = 0; i < STMMAC_DMA_DESC_COUNT; i++)
    {
      priv->rx_desc[i].des0 = RX_DESC_OWN;
      priv->rx_desc[i].des1 = RX_DESC_RCH |
                               (STMMAC_RX_BUFSIZE & RX_DESC_RBS1_MASK);
      priv->rx_desc[i].des2 = (uint32_t)(uintptr_t)priv->rx_buf[i];
      priv->rx_desc[i].des3 = (uint32_t)(uintptr_t)&priv->rx_desc[i + 1];
    }

  priv->rx_desc[STMMAC_DMA_DESC_COUNT - 1].des1 |= RX_DESC_RER;
  priv->rx_desc[STMMAC_DMA_DESC_COUNT - 1].des3 =
    (uint32_t)(uintptr_t)&priv->rx_desc[0];

  priv->tx_head = 0;
  priv->tx_tail = 0;
  priv->rx_idx  = 0;

  /* Set descriptor addresses */

  putreg32((uint32_t)(uintptr_t)priv->tx_desc,
           priv->dma_base + DMA_TX_DESC);
  putreg32((uint32_t)(uintptr_t)priv->rx_desc,
           priv->dma_base + DMA_RX_DESC);

  /* Configure DMA */

  putreg32((32 << DMA_BUS_MODE_PBL_SHIFT) | DMA_BUS_MODE_FB |
           DMA_BUS_MODE_AAL,
           priv->dma_base + DMA_BUS_MODE);

  putreg32(DMA_OP_MODE_ST | DMA_OP_MODE_SR |
           DMA_OP_MODE_TSF | DMA_OP_MODE_RSF,
           priv->dma_base + DMA_OP_MODE);

  /* Enable interrupts */

  putreg32(DMA_INT_EN_NIE | DMA_INT_EN_RIE |
           DMA_INT_EN_TIE | DMA_INT_EN_AIE,
           priv->dma_base + DMA_INT_EN);

  return OK;
}

/****************************************************************************
 * Private Functions: TX / RX
 ****************************************************************************/

static void stmmac_tx(struct stmmac_priv_s *priv, FAR const uint8_t *buf,
                      uint32_t len)
{
  struct stmmac_dma_desc_s *desc;
  uint32_t idx;

  if (!priv->link_up || len == 0)
    {
      return;
    }

  idx = priv->tx_head;
  desc = &priv->tx_desc[idx];

  if (desc->des0 & TX_DESC_OWN)
    {
      return;
    }

  memcpy(priv->tx_buf[idx], buf, len);

  desc->des2 = (uint32_t)(uintptr_t)priv->tx_buf[idx];
  desc->des1 = TX_DESC_FS | TX_DESC_LS | (len & 0x1fff);
  desc->des3 = 0;
  desc->des0 = TX_DESC_OWN;

  priv->tx_head = (idx + 1) % STMMAC_DMA_DESC_COUNT;

  putreg32(1, priv->dma_base + DMA_TX_POLL);
}

/****************************************************************************
 * Name: stmmac_txpoll
 *
 * Description:
 *   Poll callback from devif_poll(). Called by the network stack when
 *   there is a packet ready to send. d_buf and d_len are already set.
 ****************************************************************************/

static int stmmac_txpoll(FAR struct net_driver_s *dev)
{
  struct stmmac_priv_s *priv = (struct stmmac_priv_s *)dev->d_private;

  if (dev->d_sndlen > 0)
    {
      stmmac_tx(priv, dev->d_buf, dev->d_sndlen);
    }
  else if (dev->d_len > 0)
    {
      stmmac_tx(priv, dev->d_buf, dev->d_len);
    }

  dev->d_sndlen = 0;
  dev->d_len = 0;
  dev->d_buf = NULL;

  return OK;
}

static void stmmac_rx(struct stmmac_priv_s *priv)
{
  struct net_driver_s *dev = &priv->dev;
  struct stmmac_dma_desc_s *desc;
  uint32_t idx;
  uint32_t status;
  uint32_t len;

  idx = priv->rx_idx;

  for (;;)
    {
      desc = &priv->rx_desc[idx];
      status = desc->des0;

      if (status & RX_DESC_OWN)
        {
          break;
        }

      if (!(status & RX_DESC_ES) &&
          (status & RX_DESC_FS) && (status & RX_DESC_LS))
        {
          len = (status & RX_DESC_FL_MASK) >> RX_DESC_FL_SHIFT;

          if (len > 0 && len <= STMMAC_RX_BUFSIZE)
            {
              dev->d_buf = priv->rx_buf[idx];
              dev->d_len = len;
              NETDEV_RXPACKETS(dev);
              arp_input(dev);
              ipv4_input(dev);
            }
        }

      desc->des0 = RX_DESC_OWN;
      desc->des2 = (uint32_t)(uintptr_t)priv->rx_buf[idx];

      idx = (idx + 1) % STMMAC_DMA_DESC_COUNT;
    }

  priv->rx_idx = idx;
  putreg32(1, priv->dma_base + DMA_RX_POLL);
}

/****************************************************************************
 * Private Functions: Interrupt
 ****************************************************************************/

static void stmmac_irq_work(void *arg)
{
  struct stmmac_priv_s *priv = (struct stmmac_priv_s *)arg;
  uint32_t status;

  status = getreg32(priv->dma_base + DMA_STATUS);
  putreg32(status, priv->dma_base + DMA_STATUS);

  if (status & DMA_STATUS_TI)
    {
      while (priv->tx_tail != priv->tx_head)
        {
          if (priv->tx_desc[priv->tx_tail].des0 & TX_DESC_OWN)
            {
              break;
            }

          priv->tx_tail = (priv->tx_tail + 1) % STMMAC_DMA_DESC_COUNT;
        }

      NETDEV_TXDONE(&priv->dev);
    }

  if (status & DMA_STATUS_RI)
    {
      stmmac_rx(priv);
    }

  stmmac_phy_update_link(priv);
  up_enable_irq(CONFIG_STMMAC_IRQ);
}

static int stmmac_interrupt(int irq, void *context, void *arg)
{
  struct stmmac_priv_s *priv = (struct stmmac_priv_s *)arg;

  up_disable_irq(CONFIG_STMMAC_IRQ);
  work_queue(HPWORK, &priv->irqwork, stmmac_irq_work, priv, 0);
  return OK;
}

/****************************************************************************
 * Private Functions: Network Interface
 ****************************************************************************/

static int stmmac_ifup(FAR struct net_driver_s *dev)
{
  struct stmmac_priv_s *priv = (struct stmmac_priv_s *)dev->d_private;

  /* Set MAC address */

  putreg32(dev->d_mac.ether.ether_addr_octet[0] |
           (dev->d_mac.ether.ether_addr_octet[1] << 8) |
           (dev->d_mac.ether.ether_addr_octet[2] << 16) |
           (dev->d_mac.ether.ether_addr_octet[3] << 24),
           priv->mac_base + MAC_ADDR_LO(0));

  putreg32(dev->d_mac.ether.ether_addr_octet[4] |
           (dev->d_mac.ether.ether_addr_octet[5] << 8),
           priv->mac_base + MAC_ADDR_HI(0));

  stmmac_phy_init(priv);

  /* Enable GMAC1 clocks via CRU (Main CRU at 0xFDD20000)
   * CRU gate registers are write-only: bits[31:16]=write-enable, bits[15:0]=value
   * Try enabling ALL gates in CON0-CON7 as a broad approach
   */

  {
    volatile uint32_t *cru = (volatile uint32_t *)0xFDD20000;

    /* Enable all clock gates in CON0-CON7 (offsets 0x300-0x31C)
     * Write-enable mask=0xFFFF in upper 16 bits, value=0 in lower 16 bits
     * This clears all gate bits = enables all clocks
     */

    int i;
    for (i = 0; i < 8; i++)
      {
        cru[0xC0 + i] = 0xFFFF0000;  /* 0x300/4 + i = 0xC0 + i */
      }

    /* Configure MDIO/MDC pin mux via GRF (0xFDC60000)
     * GPIO3 pin 20 = MDC (func 3), pin 21 = MDIO (func 3)
     * GRF GPIO3_IOMUX for pins 20-23 at GRF+0x0034
     * Pin 20: bits [3:0], Pin 21: bits [7:4]
     * Write-enable: bits [16:23], value: function 3
     */

    volatile uint32_t *grf = (volatile uint32_t *)0xFDC60000;
    grf[0x0D] = 0x00FF0033;  /* GPIO3 pins 20,21 func=3 */
    grf[0x0E] = 0x00FF0033;  /* GPIO3 pins 20,21 func=3 (backup) */

    /* Also try the dead-code offset from original driver */
    grf[0x41] = 0x00030330;
    grf[0x41] = 0x00c30c30;

    _err("IFUP: CRU+GRF configured\n");
  }

  _err("IFUP: calling stmmac_dma_init...\n");
  int dma_ret = stmmac_dma_init(priv);
  _err("IFUP: dma_init returned %d tx_desc=%p rx_desc=%p tx_buf[0]=%p\n",
       dma_ret, priv->tx_desc, priv->rx_desc,
       priv->tx_buf[0] ? priv->tx_buf[0] : NULL);

  if (dma_ret < 0)
    {
      _err("IFUP: DMA INIT FAILED: %d\n", dma_ret);
      return dma_ret;
    }

  /* Copy exact MAC configuration from working Linux system */

  putreg32(0x08072203, priv->mac_base + 0x000);  /* MAC_CFG */
  putreg32(0x00000404, priv->mac_base + 0x008);  /* PKT_FLT */
  putreg32(0x00000001, priv->mac_base + 0x014);  /* Hash Low */
  putreg32(0x00000001, priv->mac_base + 0x0b0);  /* Flow Ctrl */
  putreg32(0x00000030, priv->mac_base + 0x0b4);  /* Flow Ctrl */
  putreg32(0x000d0000, priv->mac_base + 0x0f8);  /* LPI Timer */

  /* Copy DMA configuration from Linux */

  putreg32(0x0000000e, priv->dma_base + 0x004);  /* Bus Mode */
  putreg32(0x00006300, priv->dma_base + 0x00c);  /* Desc Poll */
  putreg32(0x00010000, priv->dma_base + 0x100);  /* DMA Ch0 Control */
  putreg32(0x00081011, priv->dma_base + 0x104);  /* DMA Ch0 TX Control */
  putreg32(0x00080c01, priv->dma_base + 0x108);  /* DMA Ch0 RX Control */

  /* MAC is already configured with RE+TE in the Linux config value above */

  /* Enable DMA */

  putreg32(DMA_OP_MODE_ST | DMA_OP_MODE_SR | DMA_OP_MODE_TSF |
           DMA_OP_MODE_RSF,
           priv->dma_base + DMA_OP_MODE);

  /* Attach interrupt */

  irq_attach(CONFIG_STMMAC_IRQ, stmmac_interrupt, priv);
  up_enable_irq(CONFIG_STMMAC_IRQ);

  /* Mark link as up so ARP can resolve */

  netdev_carrier_on(dev);

  /* === HARDWARE TEST: send a raw ARP request directly via DMA === */

  {
    uint8_t *pkt = priv->tx_buf[0];
    struct stmmac_dma_desc_s *desc = &priv->tx_desc[0];
    uint32_t len;

    /* Build a minimal ARP request: who has 10.0.0.1? */

    memset(pkt, 0xff, 6);                    /* dst: broadcast */
    pkt[6] = 0x02; pkt[7] = 0x42;            /* src MAC */
    pkt[8] = 0xac; pkt[9] = 0x12;
    pkt[10] = 0x34; pkt[11] = 0x56;
    pkt[12] = 0x08; pkt[13] = 0x06;          /* EtherType: ARP */
    pkt[14] = 0x00; pkt[15] = 0x01;          /* HW type: Ethernet */
    pkt[16] = 0x08; pkt[17] = 0x00;          /* Proto: IPv4 */
    pkt[18] = 0x06;                           /* HW size */
    pkt[19] = 0x04;                           /* Proto size */
    pkt[20] = 0x00; pkt[21] = 0x01;          /* Op: request */
    /* Sender MAC */
    pkt[22] = 0x02; pkt[23] = 0x42; pkt[24] = 0xac;
    pkt[25] = 0x12; pkt[26] = 0x34; pkt[27] = 0x56;
    /* Sender IP: 10.0.0.2 */
    pkt[28] = 10; pkt[29] = 0; pkt[30] = 0; pkt[31] = 2;
    /* Target MAC: 00:00:00:00:00:00 */
    memset(pkt + 32, 0, 6);
    /* Target IP: 10.0.0.1 */
    pkt[38] = 10; pkt[39] = 0; pkt[40] = 0; pkt[41] = 1;

    len = 42;  /* Ethernet header (14) + ARP (28) */

    /* Set up DMA descriptor */

    desc->des2 = (uint32_t)(uintptr_t)pkt;
    desc->des1 = TX_DESC_FS | TX_DESC_LS | (len & 0x1fff);
    desc->des3 = 0;
    desc->des0 = TX_DESC_OWN;

    /* Dump key registers for debug */

    _err("HW_TEST: MAC_CFG=0x%08x DMA_STATUS=0x%08x DMA_OP=0x%08x\n",
         getreg32(priv->mac_base + MAC_CFG),
         getreg32(priv->dma_base + DMA_STATUS),
         getreg32(priv->dma_base + DMA_OP_MODE));
    _err("HW_TEST: TX_DESC=0x%08x pkt=%p len=%d\n",
         (uint32_t)(uintptr_t)desc, pkt, len);
    _err("HW_TEST: DMA_TX_DESC_REG=0x%08x DMA_RX_DESC_REG=0x%08x\n",
         getreg32(priv->dma_base + DMA_TX_DESC),
         getreg32(priv->dma_base + DMA_RX_DESC));
    _err("HW_TEST: MAC_ADDR_LO=0x%08x MAC_ADDR_HI=0x%08x\n",
         getreg32(priv->mac_base + MAC_ADDR_LO(0)),
         getreg32(priv->mac_base + MAC_ADDR_HI(0)));

    /* Kick DMA */

    putreg32(1, priv->dma_base + DMA_TX_POLL);

    up_mdelay(10);

    _err("HW_TEST: AFTER DMA_STATUS=0x%08x desc0=0x%08x\n",
         getreg32(priv->dma_base + DMA_STATUS), desc->des0);
  }

  return OK;
}

static int stmmac_ifdown(FAR struct net_driver_s *dev)
{
  struct stmmac_priv_s *priv = (struct stmmac_priv_s *)dev->d_private;

  up_disable_irq(CONFIG_STMMAC_IRQ);
  irq_detach(CONFIG_STMMAC_IRQ);
  work_cancel(HPWORK, &priv->irqwork);

  putreg32(0, priv->mac_base + MAC_CFG);
  return OK;
}

static int stmmac_txavail(FAR struct net_driver_s *dev)
{
  /* Trigger the network stack to poll for pending packets.
   * devif_poll() will call stmmac_txpoll() with d_buf/d_len filled.
   */

  net_lock();
  devif_poll(dev, stmmac_txpoll);
  net_unlock();

  return OK;
}

#ifdef CONFIG_NET_MCASTGROUP
static int stmmac_addmac(FAR struct net_driver_s *dev,
                         FAR const uint8_t *mac)
{
  return OK;
}

static int stmmac_rmmac(FAR struct net_driver_s *dev,
                        FAR const uint8_t *mac)
{
  return OK;
}
#endif

#ifdef CONFIG_NETDEV_IOCTL
static int stmmac_ioctl(FAR struct net_driver_s *dev, int cmd,
                        unsigned long arg)
{
  return -ENOTTY;
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stmmac_initialize
 ****************************************************************************/

int stmmac_initialize(uintptr_t mac_base, uintptr_t dma_base)
{
  struct stmmac_priv_s *priv;
  struct net_driver_s *dev;

  priv = kmm_zalloc(sizeof(struct stmmac_priv_s));
  if (!priv)
    {
      return -ENOMEM;
    }

  priv->mac_base = mac_base;
  priv->dma_base = dma_base;
  priv->phy_addr = CONFIG_STMMAC_PHY_ADDR;

  dev = &priv->dev;
  dev->d_buf    = NULL;
  dev->d_ifup   = stmmac_ifup;
  dev->d_ifdown = stmmac_ifdown;
  dev->d_txavail = stmmac_txavail;
#ifdef CONFIG_NET_MCASTGROUP
  dev->d_addmac  = stmmac_addmac;
  dev->d_rmmac   = stmmac_rmmac;
#endif
#ifdef CONFIG_NETDEV_IOCTL
  dev->d_ioctl   = stmmac_ioctl;
#endif
  dev->d_private = priv;

  /* Default MAC address */

  dev->d_mac.ether.ether_addr_octet[0] = 0x02;
  dev->d_mac.ether.ether_addr_octet[1] = 0x42;
  dev->d_mac.ether.ether_addr_octet[2] = 0xac;
  dev->d_mac.ether.ether_addr_octet[3] = 0x12;
  dev->d_mac.ether.ether_addr_octet[4] = 0x34;
  dev->d_mac.ether.ether_addr_octet[5] = 0x56;

  netdev_register(dev, NET_LL_ETHERNET);

  /* Override d_txavail: netdev_register sets it to netdev_upper_txpoll
   * which uses IOB-based packet API incompatible with our legacy driver.
   * Use our own txavail that calls devif_poll() directly.
   */

  dev->d_txavail = stmmac_txavail;

  ninfo("stmmac: MAC=0x%08lx DMA=0x%08lx\n",
        (long)mac_base, (long)dma_base);

  return OK;
}

#endif /* CONFIG_NET && CONFIG_NET_STMMAC */
