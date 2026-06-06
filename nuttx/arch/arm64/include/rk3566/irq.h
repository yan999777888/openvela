/****************************************************************************
 * arch/arm64/include/rk3566/irq.h
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

/* This file should never be included directly but, rather,
 * only indirectly through nuttx/irq.h
 */

#ifndef __ARCH_ARM64_INCLUDE_RK3566_IRQ_H
#define __ARCH_ARM64_INCLUDE_RK3566_IRQ_H

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Rockchip RK3566 Interrupts
 *
 * GICv3 interrupt numbering:
 *   0-15:  SGI (Software Generated Interrupts)
 *   16-31: PPI (Private Peripheral Interrupts)
 *   32+:   SPI (Shared Peripheral Interrupts)
 *
 * In the device tree, SPI interrupts are specified as:
 *   interrupts = <0x00 SPI_NUM 0x04>
 * The GIC IRQ number = SPI_NUM + 32
 */

#define NR_IRQS                 256   /* Total number of interrupts */

/* SGI Interrupts (0-15) */

#define RK3566_IRQ_SGI0         (0)
#define RK3566_IRQ_SGI1         (1)
#define RK3566_IRQ_SGI2         (2)
#define RK3566_IRQ_SGI3         (3)
#define RK3566_IRQ_SGI4         (4)
#define RK3566_IRQ_SGI5         (5)
#define RK3566_IRQ_SGI6         (6)
#define RK3566_IRQ_SGI7         (7)
#define RK3566_IRQ_SGI8         (8)
#define RK3566_IRQ_SGI9         (9)
#define RK3566_IRQ_SGI10        (10)
#define RK3566_IRQ_SGI11        (11)
#define RK3566_IRQ_SGI12        (12)
#define RK3566_IRQ_SGI13        (13)
#define RK3566_IRQ_SGI14        (14)
#define RK3566_IRQ_SGI15        (15)

/* PPI Interrupts (16-31) */

#define RK3566_IRQ_PPI0         (16)
#define RK3566_IRQ_PPI1         (17)
#define RK3566_IRQ_PPI2         (18)
#define RK3566_IRQ_PPI3         (19)
#define RK3566_IRQ_PPI4         (20)
#define RK3566_IRQ_PPI5         (21)
#define RK3566_IRQ_PPI6         (22)
#define RK3566_IRQ_PPI7         (23)
#define RK3566_IRQ_PPI8         (24)
#define RK3566_IRQ_PPI9         (25)
#define RK3566_IRQ_PPI10        (26)
#define RK3566_IRQ_PPI11        (27)
#define RK3566_IRQ_PPI12        (28)
#define RK3566_IRQ_PPI13        (29)
#define RK3566_IRQ_PPI14        (30)
#define RK3566_IRQ_PPI15        (31)

/* SPI Interrupts (32+)
 * From device tree: interrupts = <0x00 SPI_NUM 0x04>
 * GIC IRQ = SPI_NUM + 32
 */

#define RK3566_IRQ_UART0        (148)  /* SPI 116 */
#define RK3566_IRQ_UART1        (149)  /* SPI 117 */
#define RK3566_IRQ_UART2        (150)  /* SPI 118 */
#define RK3566_IRQ_UART3        (151)  /* SPI 119 */
#define RK3566_IRQ_UART4        (152)  /* SPI 120 */
#define RK3566_IRQ_UART5        (153)  /* SPI 121 */
#define RK3566_IRQ_UART6        (154)  /* SPI 122 */
#define RK3566_IRQ_UART7        (155)  /* SPI 123 */
#define RK3566_IRQ_UART8        (156)  /* SPI 124 */
#define RK3566_IRQ_UART9        (157)  /* SPI 125 */

#endif /* __ARCH_ARM64_INCLUDE_RK3566_IRQ_H */
