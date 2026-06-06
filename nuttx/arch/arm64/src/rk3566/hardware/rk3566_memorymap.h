/****************************************************************************
 * arch/arm64/src/rk3566/hardware/rk3566_memorymap.h
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
 ****************************************************************************/

#ifndef __ARCH_ARM64_SRC_RK3566_HARDWARE_RK3566_MEMORYMAP_H
#define __ARCH_ARM64_SRC_RK3566_HARDWARE_RK3566_MEMORYMAP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Peripheral Base Addresses - extracted from RK3566 device tree */

/* GPIO Controllers */

#define RK3566_GPIO0_ADDR        0xfdd60000
#define RK3566_GPIO1_ADDR        0xfe740000
#define RK3566_GPIO2_ADDR        0xfe750000
#define RK3566_GPIO3_ADDR        0xfe760000
#define RK3566_GPIO4_ADDR        0xfe770000

/* UART Controllers (DW APB UART) */

#define RK3566_UART0_ADDR        0xfdd50000
#define RK3566_UART1_ADDR        0xfe650000
#define RK3566_UART2_ADDR        0xfe660000
#define RK3566_UART3_ADDR        0xfe670000
#define RK3566_UART4_ADDR        0xfe680000
#define RK3566_UART5_ADDR        0xfe690000
#define RK3566_UART6_ADDR        0xfe6a0000
#define RK3566_UART7_ADDR        0xfe6b0000
#define RK3566_UART8_ADDR        0xfe6c0000
#define RK3566_UART9_ADDR        0xfe6d0000

/* I2C Controllers */

#define RK3566_I2C0_ADDR         0xfdd40000
#define RK3566_I2C1_ADDR         0xfe5a0000
#define RK3566_I2C2_ADDR         0xfe5b0000
#define RK3566_I2C3_ADDR         0xfe5c0000
#define RK3566_I2C4_ADDR         0xfe5d0000
#define RK3566_I2C5_ADDR         0xfe5e0000

/* SPI Controllers */

#define RK3566_SPI0_ADDR         0xfe610000
#define RK3566_SPI1_ADDR         0xfe620000
#define RK3566_SPI2_ADDR         0xfe630000
#define RK3566_SPI3_ADDR         0xfe640000

/* PWM Controllers */

#define RK3566_PWM0_ADDR         0xfdd70000
#define RK3566_PWM1_ADDR         0xfdd70010
#define RK3566_PWM2_ADDR         0xfdd70020
#define RK3566_PWM3_ADDR         0xfdd70030

/* Timer */

#define RK3566_TIMER_ADDR        0xfe5f0000

/* Watchdog */

#define RK3566_WDT_ADDR          0xfe600000

/* Ethernet GMAC */

#define RK3566_GMAC1_ADDR        0xfe010000

/* USB */

#define RK3566_USBOTG_ADDR       0xfcc00000
#define RK3566_USBHOST_ADDR      0xfd000000

/* eMMC / SDHCI */

#define RK3566_SDHCI_ADDR        0xfe310000

/* Display */

#define RK3566_VOP_ADDR          0xfe040000
#define RK3566_HDMI_ADDR         0xfe0a0000

/* I2S Audio */

#define RK3566_I2S0_ADDR         0xfe400000
#define RK3566_I2S1_ADDR         0xfe410000
#define RK3566_I2S2_ADDR         0xfe420000
#define RK3566_I2S3_ADDR         0xfe430000

/* CSI Camera */

#define RK3566_CIF_ADDR          0xfdfe0000

/* SARADC */

#define RK3566_SARADC_ADDR       0xfe720000

/* TSADC (Temperature Sensor) */

#define RK3566_TSADC_ADDR        0xfe710000

/* CRU (Clock Reset Unit) */

#define RK3566_PMUCRU_ADDR       0xfdd00000
#define RK3566_CRU_ADDR          0xfdd20000

/* GRF (General Register Files) */

#define RK3566_PMUGRF_ADDR       0xfdc20000
#define RK3566_GRF_ADDR          0xfdc60000

/* PMU */

#define RK3566_PMU_ADDR          0xfdd90000

/* GIC v3 */

#define RK3566_GICD_ADDR         0xfd400000
#define RK3566_GICR_ADDR         0xfd460000

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/****************************************************************************
 * Public Functions Prototypes
 ****************************************************************************/

#endif /* __ARCH_ARM64_SRC_RK3566_HARDWARE_RK3566_MEMORYMAP_H */
