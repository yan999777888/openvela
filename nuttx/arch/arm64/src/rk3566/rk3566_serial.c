/***************************************************************************
 * arch/arm64/src/rk3566/rk3566_serial.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ***************************************************************************/

/* This driver implements UART support for the Rockchip RK3566 SoC.
 * The RK3566 uses the DesignWare APB UART (8250-compatible), which is
 * the same IP block used in RK3399 and many other SoCs.
 *
 * UART clocks are assumed to be configured by the bootloader (U-Boot).
 * The input clock is 24MHz (xin24m).
 */

/***************************************************************************
 * Included Files
 ***************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#ifdef CONFIG_SERIAL_TERMIOS
#  include <termios.h>
#endif

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/spinlock.h>
#include <nuttx/init.h>
#include <nuttx/fs/ioctl.h>
#include <nuttx/semaphore.h>
#include <nuttx/serial/serial.h>

#include "arm64_arch.h"
#include "arm64_internal.h"
#include "rk3566_serial.h"
#include "arm64_arch_timer.h"
#include "rk3566_boot.h"
#include "arm64_gic.h"

#ifdef USE_SERIALDRIVER

/***************************************************************************
 * Pre-processor Definitions
 ***************************************************************************/

/* Default UART settings */

#ifndef CONFIG_UART0_BAUD
#  define CONFIG_UART0_BAUD 115200
#endif

#ifndef CONFIG_UART0_BITS
#  define CONFIG_UART0_BITS 8
#endif

#ifndef CONFIG_UART0_PARITY
#  define CONFIG_UART0_PARITY 0
#endif

#ifndef CONFIG_UART0_2STOP
#  define CONFIG_UART0_2STOP 0
#endif

#ifndef CONFIG_UART0_RXBUFSIZE
#  define CONFIG_UART0_RXBUFSIZE 256
#endif

#ifndef CONFIG_UART0_TXBUFSIZE
#  define CONFIG_UART0_TXBUFSIZE 256
#endif

/* UART2 is console and ttys0 for KICKPI K11C */

#define CONSOLE_DEV     g_uart2port         /* UART2 is console */
#define TTYS0_DEV       g_uart2port         /* UART2 is ttyS0 */
#define UART2_ASSIGNED  1

/* RK3566 UART Input Clock: 24MHz from xin24m */

#define UART_SCLK 24000000

/* Timeout for UART Busy Wait, in milliseconds */

#define UART_TIMEOUT_MS 100

/* DW APB UART Registers (8250-compatible) */

#define UART_THR(uart_addr) (uart_addr + 0x00)  /* Tx Holding */
#define UART_RBR(uart_addr) (uart_addr + 0x00)  /* Rx Buffer */
#define UART_DLL(uart_addr) (uart_addr + 0x00)  /* Divisor Latch Low */
#define UART_DLH(uart_addr) (uart_addr + 0x04)  /* Divisor Latch High */
#define UART_IER(uart_addr) (uart_addr + 0x04)  /* Interrupt Enable */
#define UART_IIR(uart_addr) (uart_addr + 0x08)  /* Interrupt Identity */
#define UART_FCR(uart_addr) (uart_addr + 0x08)  /* FIFO Control */
#define UART_LCR(uart_addr) (uart_addr + 0x0c)  /* Line Control */
#define UART_MCR(uart_addr) (uart_addr + 0x10)  /* Modem Control */
#define UART_LSR(uart_addr) (uart_addr + 0x14)  /* Line Status */
#define UART_MSR(uart_addr) (uart_addr + 0x18)  /* Modem Status */
#define UART_USR(uart_addr) (uart_addr + 0x7c)  /* UART Status (DW) */

/* UART Register Bit Definitions */

#define UART_IER_ERBFI (1 << 0)  /* Enable Rx Data Interrupt */
#define UART_IER_ETBEI (1 << 1)  /* Enable Tx Empty Interrupt */
#define UART_LSR_DR    (1 << 0)  /* Rx Data Ready */
#define UART_LSR_THRE  (1 << 5)  /* Tx Empty */
#define UART_USR_BUSY  (1 << 0)  /* UART Busy */

/* UART Interrupt Identity Register */

#define UART_IIR_IID_SHIFT        (0)
#define UART_IIR_IID_MASK         (15 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_MODEM      (0 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_NONE       (1 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_TXEMPTY    (2 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_RECV       (4 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_LINESTATUS (6 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_BUSY       (7 << UART_IIR_IID_SHIFT)
#  define UART_IIR_IID_TIMEOUT    (12 << UART_IIR_IID_SHIFT)

#define UART_IIR_FEFLAG_SHIFT     (6)
#define UART_IIR_FEFLAG_MASK      (3 << UART_IIR_FEFLAG_SHIFT)
#  define UART_IIR_FEFLAG_DISABLE (0 << UART_IIR_FEFLAG_SHIFT)
#  define UART_IIR_FEFLAG_ENABLE  (3 << UART_IIR_FEFLAG_SHIFT)

/* UART FIFO Control Register */

#define UART_FCR_FIFOE            (1 << 0)
#define UART_FCR_RFIFOR           (1 << 1)
#define UART_FCR_XFIFOR           (1 << 2)
#define UART_FCR_DMAM             (1 << 3)
#define UART_FCR_TFT_SHIFT        (4)
#define UART_FCR_TFT_MASK         (3 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_EMPTY      (0 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_TWO        (1 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_QUARTER    (2 << UART_FCR_TFT_SHIFT)
#  define UART_FCR_TFT_HALF       (3 << UART_FCR_TFT_SHIFT)

#define UART_FCR_RT_SHIFT         (6)
#define UART_FCR_RT_MASK          (3 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_ONE         (0 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_QUARTER     (1 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_HALF        (2 << UART_FCR_RT_SHIFT)
#  define UART_FCR_RT_MINUS2      (3 << UART_FCR_RT_SHIFT)

/* UART Line Control Register */

#define UART_LCR_DLS_SHIFT        (0)
#define UART_LCR_DLS_MASK         (3 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_5BITS      (0 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_6BITS      (1 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_7BITS      (2 << UART_LCR_DLS_SHIFT)
#  define UART_LCR_DLS_8BITS      (3 << UART_LCR_DLS_SHIFT)

#define UART_LCR_STOP             (1 << 2)
#define UART_LCR_PEN              (1 << 3)
#define UART_LCR_EPS              (1 << 4)
#define UART_LCR_BC               (1 << 6)
#define UART_LCR_DLAB             (1 << 7)

/***************************************************************************
 * Private Types
 ***************************************************************************/

/* RK3566 UART Configuration */

struct rk3566_uart_config
{
  unsigned long uart;  /* UART Base Address */
};

/* RK3566 UART Device Data */

struct rk3566_uart_data
{
  uint32_t baud_rate;  /* UART Baud Rate */
  uint32_t ier;        /* Saved IER value */
  uint8_t  parity;     /* 0=none, 1=odd, 2=even */
  uint8_t  bits;       /* Number of bits (7 or 8) */
  bool     stopbits2;  /* true: Configure with 2 stop bits */
};

/* RK3566 UART Port */

struct rk3566_uart_port_s
{
  struct rk3566_uart_data data;     /* UART Device Data */
  struct rk3566_uart_config config; /* UART Configuration */
  unsigned int irq_num;             /* UART IRQ Number */
  bool is_console;                  /* true if this UART is console */
};

/***************************************************************************
 * Private Function Prototypes
 ***************************************************************************/

static void rk3566_uart_rxint(struct uart_dev_s *dev, bool enable);
static void rk3566_uart_txint(struct uart_dev_s *dev, bool enable);

/***************************************************************************
 * Private Functions
 ***************************************************************************/

/***************************************************************************
 * Name: rk3566_uart_divisor
 *
 * Description:
 *   Select a divisor to produce the BAUD from the UART SCLK.
 *
 *     BAUD = SCLK / (16 * DL), or
 *     DL   = SCLK / BAUD / 16
 *
 ***************************************************************************/

static uint32_t rk3566_uart_divisor(uint32_t baud)
{
  DEBUGASSERT(baud != 0);
  return UART_SCLK / (baud << 4);
}

/***************************************************************************
 * Name: rk3566_uart_irq_handler
 *
 * Description:
 *   Common UART interrupt handler.
 *
 ***************************************************************************/

static int rk3566_uart_irq_handler(int irq, void *context, void *arg)
{
  struct uart_dev_s *dev = (struct uart_dev_s *)arg;
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;
  uint32_t status;
  int passes;

  DEBUGASSERT(dev != NULL && dev->priv != NULL);

  for (passes = 0; passes < 256; passes++)
    {
      status = getreg32(UART_IIR(config->uart));

      switch (status & UART_IIR_IID_MASK)
        {
          case UART_IIR_IID_RECV:
          case UART_IIR_IID_TIMEOUT:
            {
              uart_recvchars(dev);
              break;
            }

          case UART_IIR_IID_TXEMPTY:
            {
              uart_xmitchars(dev);
              break;
            }

          case UART_IIR_IID_MODEM:
            {
              status = getreg32(UART_MSR(config->uart));
              break;
            }

          case UART_IIR_IID_LINESTATUS:
            {
              status = getreg32(UART_LSR(config->uart));
              break;
            }

          case UART_IIR_IID_BUSY:
            {
              status = getreg32(UART_USR(config->uart));
              break;
            }

          case UART_IIR_IID_NONE:
            {
              return OK;
            }

          default:
            {
              _err("ERROR: Unexpected IIR: %02" PRIx32 "\n", status);
              break;
            }
        }
    }

  return OK;
}

/***************************************************************************
 * Name: rk3566_uart_wait
 *
 * Description:
 *   Wait for UART to be non-busy.
 *
 ***************************************************************************/

static int rk3566_uart_wait(struct uart_dev_s *dev)
{
  struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;
  int i;

  for (i = 0; i < UART_TIMEOUT_MS; i++)
    {
      uint32_t status = getreg32(UART_USR(config->uart));

      if ((status & UART_USR_BUSY) == 0)
        {
          return OK;
        }

      up_mdelay(1);
    }

  _err("UART timeout\n");
  return ERROR;
}

/***************************************************************************
 * Name: rk3566_uart_setup
 *
 * Description:
 *   Configure the UART baud, bits, parity, fifos, etc.
 *
 ***************************************************************************/

static int rk3566_uart_setup(struct uart_dev_s *dev)
{
#ifndef CONFIG_SUPPRESS_UART_CONFIG
  struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;
  struct rk3566_uart_data *data = &port->data;
  uint16_t dl;
  uint32_t lcr;
  int ret;

  DEBUGASSERT(data != NULL);

  /* Clear fifos */

  putreg32(UART_FCR_RFIFOR | UART_FCR_XFIFOR, UART_FCR(config->uart));

  /* Set trigger */

  putreg32(UART_FCR_FIFOE | UART_FCR_RT_HALF, UART_FCR(config->uart));

  /* Set up the IER */

  data->ier = getreg32(UART_IER(config->uart));

  /* Set up the LCR */

  lcr = 0;

  switch (data->bits)
    {
    case 5:
      lcr |= UART_LCR_DLS_5BITS;
      break;

    case 6:
      lcr |= UART_LCR_DLS_6BITS;
      break;

    case 7:
      lcr |= UART_LCR_DLS_7BITS;
      break;

    case 8:
    default:
      lcr |= UART_LCR_DLS_8BITS;
      break;
    }

  if (data->stopbits2)
    {
      lcr |= UART_LCR_STOP;
    }

  if (data->parity == 1)
    {
      lcr |= UART_LCR_PEN;
    }
  else if (data->parity == 2)
    {
      lcr |= (UART_LCR_PEN | UART_LCR_EPS);
    }

  /* Set DLAB when UART is not busy */

  ret = rk3566_uart_wait(dev);

  if (ret < 0)
    {
      _err("UART wait failed, ret=%d\n", ret);
      return ret;
    }

  putreg32(lcr | UART_LCR_DLAB, UART_LCR(config->uart));

  ret = rk3566_uart_wait(dev);

  if (ret < 0)
    {
      _err("UART wait failed, ret=%d\n", ret);
      return ret;
    }

  /* Set the BAUD divisor */

  dl = rk3566_uart_divisor(data->baud_rate);
  putreg8(dl >> 8,   UART_DLH(config->uart));
  putreg8(dl & 0xff, UART_DLL(config->uart));

  /* Check the BAUD divisor */

  if (getreg8(UART_DLH(config->uart)) != (dl >> 8) ||
      getreg8(UART_DLL(config->uart)) != (dl & 0xff))
    {
      _err("UART BAUD divisor failed\n");
      return ERROR;
    }

  /* Clear DLAB */

  putreg32(lcr, UART_LCR(config->uart));

  /* Configure the FIFOs */

  putreg32(UART_FCR_RT_HALF | UART_FCR_XFIFOR | UART_FCR_RFIFOR |
           UART_FCR_FIFOE, UART_FCR(config->uart));

#endif /* CONFIG_SUPPRESS_UART_CONFIG */
  return OK;
}

/***************************************************************************
 * Name: rk3566_uart_shutdown
 *
 * Description:
 *   Disable the UART Port.
 *
 ***************************************************************************/

static void rk3566_uart_shutdown(struct uart_dev_s *dev)
{
  rk3566_uart_rxint(dev, false);
  rk3566_uart_txint(dev, false);
}

/***************************************************************************
 * Name: rk3566_uart_attach
 *
 * Description:
 *   Configure the UART to operation in interrupt driven mode.
 *
 ***************************************************************************/

static int rk3566_uart_attach(struct uart_dev_s *dev)
{
  int ret;
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;

  DEBUGASSERT(port != NULL);

  ret = irq_attach(port->irq_num, rk3566_uart_irq_handler, dev);

  /* Set Interrupt Priority */

  up_prioritize_irq(port->irq_num, 0);
  up_set_irq_type(port->irq_num, IRQ_RISING_EDGE);

  if (ret == OK)
    {
      up_enable_irq(port->irq_num);
    }
  else
    {
      _err("IRQ attach failed, ret=%d\n", ret);
    }

  return ret;
}

/***************************************************************************
 * Name: rk3566_uart_detach
 *
 * Description:
 *   Detach UART interrupts.
 *
 ***************************************************************************/

static void rk3566_uart_detach(struct uart_dev_s *dev)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;

  DEBUGASSERT(port != NULL);

  up_disable_irq(port->irq_num);
  irq_detach(port->irq_num);
}

/***************************************************************************
 * Name: rk3566_uart_ioctl
 *
 * Description:
 *   All ioctl calls will be routed through this method.
 *
 ***************************************************************************/

static int rk3566_uart_ioctl(struct file *filep, int cmd, unsigned long arg)
{
  int ret = OK;

  UNUSED(filep);
  UNUSED(arg);

  switch (cmd)
    {
      case TIOCSBRK:
      case TIOCCBRK:
      default:
        {
          ret = -ENOTTY;
          break;
        }
    }

  return ret;
}

/***************************************************************************
 * Name: rk3566_uart_receive
 *
 * Description:
 *   Called (usually) from the interrupt level to receive one character.
 *
 ***************************************************************************/

static int rk3566_uart_receive(struct uart_dev_s *dev, unsigned int *status)
{
  struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;
  uint32_t rbr;

  *status = getreg8(UART_LSR(config->uart));
  rbr     = getreg8(UART_RBR(config->uart));
  return rbr;
}

/***************************************************************************
 * Name: rk3566_uart_rxint
 *
 * Description:
 *   Call to enable or disable RX interrupts
 *
 ***************************************************************************/

static void rk3566_uart_rxint(struct uart_dev_s *dev, bool enable)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;

  if (enable)
    {
      modreg8(UART_IER_ERBFI, UART_IER_ERBFI, UART_IER(config->uart));
    }
  else
    {
      modreg8(0, UART_IER_ERBFI, UART_IER(config->uart));
    }
}

/***************************************************************************
 * Name: rk3566_uart_rxavailable
 *
 * Description:
 *   Return true if the Receive FIFO is not empty
 *
 ***************************************************************************/

static bool rk3566_uart_rxavailable(struct uart_dev_s *dev)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;

  return getreg8(UART_LSR(config->uart)) & UART_LSR_DR;
}

/***************************************************************************
 * Name: rk3566_uart_send
 *
 * Description:
 *   Send one byte on the UART
 *
 ***************************************************************************/

static void rk3566_uart_send(struct uart_dev_s *dev, int ch)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;

  putreg8(ch, UART_THR(config->uart));
}

/***************************************************************************
 * Name: rk3566_uart_txint
 *
 * Description:
 *   Call to enable or disable TX interrupts
 *
 ***************************************************************************/

static void rk3566_uart_txint(struct uart_dev_s *dev, bool enable)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;

  if (enable)
    {
      modreg8(UART_IER_ETBEI, UART_IER_ETBEI, UART_IER(config->uart));
    }
  else
    {
      modreg8(0, UART_IER_ETBEI, UART_IER(config->uart));
    }
}

/***************************************************************************
 * Name: rk3566_uart_txready
 *
 * Description:
 *   Return true if the Transmit FIFO is not full
 *
 ***************************************************************************/

static bool rk3566_uart_txready(struct uart_dev_s *dev)
{
  const struct rk3566_uart_port_s *port =
    (struct rk3566_uart_port_s *)dev->priv;
  const struct rk3566_uart_config *config = &port->config;

  return (getreg8(UART_LSR(config->uart)) & UART_LSR_THRE) != 0;
}

/***************************************************************************
 * Name: rk3566_uart_txempty
 *
 * Description:
 *   Return true if the Transmit FIFO is empty
 *
 ***************************************************************************/

static bool rk3566_uart_txempty(struct uart_dev_s *dev)
{
  return rk3566_uart_txready(dev);
}

/***************************************************************************
 * Name: rk3566_uart_wait_send
 *
 * Description:
 *   Wait for Transmit FIFO until it is not full, then transmit.
 *
 ***************************************************************************/

static void rk3566_uart_wait_send(struct uart_dev_s *dev, int ch)
{
  DEBUGASSERT(dev != NULL);
  while (!rk3566_uart_txready(dev));
  rk3566_uart_send(dev, ch);
}

/***************************************************************************
 * Private Data
 ***************************************************************************/

/* UART Operations */

static const struct uart_ops_s g_uart_ops =
{
  .setup    = rk3566_uart_setup,
  .shutdown = rk3566_uart_shutdown,
  .attach   = rk3566_uart_attach,
  .detach   = rk3566_uart_detach,
  .ioctl    = rk3566_uart_ioctl,
  .receive  = rk3566_uart_receive,
  .rxint    = rk3566_uart_rxint,
  .rxavailable = rk3566_uart_rxavailable,
#ifdef CONFIG_SERIAL_IFLOWCONTROL
  .rxflowcontrol    = NULL,
#endif
  .send     = rk3566_uart_send,
  .txint    = rk3566_uart_txint,
  .txready  = rk3566_uart_txready,
  .txempty  = rk3566_uart_txempty,
};

/* UART2 - Console (KICKPI K11C debug UART) */

#ifdef CONFIG_RK3566_UART2
static struct rk3566_uart_port_s g_uart2priv =
{
  .data   =
    {
      .baud_rate  = CONFIG_UART2_BAUD,
      .parity     = CONFIG_UART2_PARITY,
      .bits       = CONFIG_UART2_BITS,
      .stopbits2  = CONFIG_UART2_2STOP
    },

  .config =
    {
      .uart       = RK3566_UART2_ADDR
    },

    .irq_num      = RK3566_UART2_IRQ,
    .is_console   = 1
};

static char g_uart2rxbuffer[CONFIG_UART2_RXBUFSIZE];
static char g_uart2txbuffer[CONFIG_UART2_TXBUFSIZE];

static struct uart_dev_s g_uart2port =
{
  .recv  =
    {
      .size   = CONFIG_UART2_RXBUFSIZE,
      .buffer = g_uart2rxbuffer,
    },

  .xmit  =
    {
      .size   = CONFIG_UART2_TXBUFSIZE,
      .buffer = g_uart2txbuffer,
    },

  .ops   = &g_uart_ops,
  .priv  = &g_uart2priv,
};
#endif /* CONFIG_RK3566_UART2 */

/* UART0 */

#ifdef CONFIG_RK3566_UART0
static struct rk3566_uart_port_s g_uart0priv =
{
  .data   =
    {
      .baud_rate  = CONFIG_UART0_BAUD,
      .parity     = CONFIG_UART0_PARITY,
      .bits       = CONFIG_UART0_BITS,
      .stopbits2  = CONFIG_UART0_2STOP
    },

  .config =
    {
      .uart       = RK3566_UART0_ADDR
    },

    .irq_num      = RK3566_UART0_IRQ,
    .is_console   = 0
};

static char g_uart0rxbuffer[CONFIG_UART0_RXBUFSIZE];
static char g_uart0txbuffer[CONFIG_UART0_TXBUFSIZE];

static struct uart_dev_s g_uart0port =
{
  .recv  =
    {
      .size   = CONFIG_UART0_RXBUFSIZE,
      .buffer = g_uart0rxbuffer,
    },

  .xmit  =
    {
      .size   = CONFIG_UART0_TXBUFSIZE,
      .buffer = g_uart0txbuffer,
    },

  .ops   = &g_uart_ops,
  .priv  = &g_uart0priv,
};
#endif /* CONFIG_RK3566_UART0 */

/* UART1 */

#ifdef CONFIG_RK3566_UART1
static struct rk3566_uart_port_s g_uart1priv =
{
  .data   =
    {
      .baud_rate  = CONFIG_UART1_BAUD,
      .parity     = CONFIG_UART1_PARITY,
      .bits       = CONFIG_UART1_BITS,
      .stopbits2  = CONFIG_UART1_2STOP
    },

  .config =
    {
      .uart       = RK3566_UART1_ADDR
    },

    .irq_num      = RK3566_UART1_IRQ,
    .is_console   = 0
};

static char g_uart1rxbuffer[CONFIG_UART1_RXBUFSIZE];
static char g_uart1txbuffer[CONFIG_UART1_TXBUFSIZE];

static struct uart_dev_s g_uart1port =
{
  .recv  =
    {
      .size   = CONFIG_UART1_RXBUFSIZE,
      .buffer = g_uart1rxbuffer,
    },

  .xmit  =
    {
      .size   = CONFIG_UART1_TXBUFSIZE,
      .buffer = g_uart1txbuffer,
    },

  .ops   = &g_uart_ops,
  .priv  = &g_uart1priv,
};
#endif /* CONFIG_RK3566_UART1 */

/* UART3 */

#ifdef CONFIG_RK3566_UART3
static struct rk3566_uart_port_s g_uart3priv =
{
  .data   =
    {
      .baud_rate  = CONFIG_UART3_BAUD,
      .parity     = CONFIG_UART3_PARITY,
      .bits       = CONFIG_UART3_BITS,
      .stopbits2  = CONFIG_UART3_2STOP
    },

  .config =
    {
      .uart       = RK3566_UART3_ADDR
    },

    .irq_num      = RK3566_UART3_IRQ,
    .is_console   = 0
};

static char g_uart3rxbuffer[CONFIG_UART3_RXBUFSIZE];
static char g_uart3txbuffer[CONFIG_UART3_TXBUFSIZE];

static struct uart_dev_s g_uart3port =
{
  .recv  =
    {
      .size   = CONFIG_UART3_RXBUFSIZE,
      .buffer = g_uart3rxbuffer,
    },

  .xmit  =
    {
      .size   = CONFIG_UART3_TXBUFSIZE,
      .buffer = g_uart3txbuffer,
    },

  .ops   = &g_uart_ops,
  .priv  = &g_uart3priv,
};
#endif /* CONFIG_RK3566_UART3 */

/* Pick ttys1.  This could be any of UART0-9 (excluding console UART2). */

#if defined(CONFIG_RK3566_UART0) && !defined(UART0_ASSIGNED)
#  define TTYS1_DEV           g_uart0port
#  define UART0_ASSIGNED      1
#elif defined(CONFIG_RK3566_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS1_DEV           g_uart1port
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_RK3566_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS1_DEV           g_uart3port
#  define UART3_ASSIGNED      1
#endif

/* Pick ttys2 */

#if defined(CONFIG_RK3566_UART1) && !defined(UART1_ASSIGNED)
#  define TTYS2_DEV           g_uart1port
#  define UART1_ASSIGNED      1
#elif defined(CONFIG_RK3566_UART3) && !defined(UART3_ASSIGNED)
#  define TTYS2_DEV           g_uart3port
#  define UART3_ASSIGNED      1
#endif

/***************************************************************************
 * Public Functions
 ***************************************************************************/

/***************************************************************************
 * Name: arm64_earlyserialinit
 *
 * Description:
 *   Performs the low level UART initialization early in debug so that
 *   the serial console will be available during bootup.
 *
 ***************************************************************************/

void arm64_earlyserialinit(void)
{
  /* NOTE: UART clock and pin configuration is assumed to be done by
   * the bootloader (U-Boot). We only need to set up the serial driver.
   */

#ifdef CONSOLE_DEV
  CONSOLE_DEV.isconsole = true;
  rk3566_uart_setup(&CONSOLE_DEV);
#endif
}

/***************************************************************************
 * Name: up_putc
 *
 * Description:
 *   Provide priority, low-level access to support OS debug writes
 *
 ***************************************************************************/

void up_putc(int ch)
{
#ifdef CONSOLE_DEV
  struct uart_dev_s *dev = &CONSOLE_DEV;

  rk3566_uart_wait_send(dev, ch);
#endif
}

/***************************************************************************
 * Name: arm64_serialinit
 *
 * Description:
 *   Register serial console and serial ports.
 *
 ***************************************************************************/

void arm64_serialinit(void)
{
#ifdef CONSOLE_DEV
  int ret;

  ret = uart_register("/dev/console", &CONSOLE_DEV);
  if (ret < 0)
    {
      _err("Register /dev/console failed, ret=%d\n", ret);
    }

  ret = uart_register("/dev/ttyS0", &TTYS0_DEV);
  if (ret < 0)
    {
      _err("Register /dev/ttyS0 failed, ret=%d\n", ret);
    }

#ifdef TTYS1_DEV
  ret = uart_register("/dev/ttyS1", &TTYS1_DEV);
  if (ret < 0)
    {
      _err("Register /dev/ttyS1 failed, ret=%d\n", ret);
    }
#endif

#ifdef TTYS2_DEV
  ret = uart_register("/dev/ttyS2", &TTYS2_DEV);
  if (ret < 0)
    {
      _err("Register /dev/ttyS2 failed, ret=%d\n", ret);
    }
#endif

#endif
}

#endif /* USE_SERIALDRIVER */
