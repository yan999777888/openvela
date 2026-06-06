/****************************************************************************
 * arch/arm64/src/rk3566/rk3566_serial.h
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

#ifndef __ARCH_ARM64_SRC_RK3566_RK3566_SERIAL_H
#define __ARCH_ARM64_SRC_RK3566_RK3566_SERIAL_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include "arm64_internal.h"
#include "arm64_gic.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Inline Functions
 ****************************************************************************/

#ifndef __ASSEMBLY__

/****************************************************************************
 * Public Data
 ****************************************************************************/

#ifdef CONFIG_ARCH_CHIP_RK3566

/* UART addresses */

#define RK3566_UART0_ADDR      0xfdd50000
#define RK3566_UART1_ADDR      0xfe650000
#define RK3566_UART2_ADDR      0xfe660000
#define RK3566_UART3_ADDR      0xfe670000
#define RK3566_UART4_ADDR      0xfe680000
#define RK3566_UART5_ADDR      0xfe690000
#define RK3566_UART6_ADDR      0xfe6a0000
#define RK3566_UART7_ADDR      0xfe6b0000
#define RK3566_UART8_ADDR      0xfe6c0000
#define RK3566_UART9_ADDR      0xfe6d0000

/* UART IRQ numbers (SPI + 32 for GICv3)
 * From device tree: interrupts = <0x00 IRQ_NUM 0x04>
 * GIC SPI offset = 32
 */

#define RK3566_UART0_IRQ       148         /* SPI 116 + 32 = 148 */
#define RK3566_UART1_IRQ       149         /* SPI 117 + 32 = 149 */
#define RK3566_UART2_IRQ       150         /* SPI 118 + 32 = 150 */
#define RK3566_UART3_IRQ       151         /* SPI 119 + 32 = 151 */
#define RK3566_UART4_IRQ       152         /* SPI 120 + 32 = 152 */
#define RK3566_UART5_IRQ       153         /* SPI 121 + 32 = 153 */
#define RK3566_UART6_IRQ       154         /* SPI 122 + 32 = 154 */
#define RK3566_UART7_IRQ       155         /* SPI 123 + 32 = 155 */
#define RK3566_UART8_IRQ       156         /* SPI 124 + 32 = 156 */
#define RK3566_UART9_IRQ       157         /* SPI 125 + 32 = 157 */

#endif /* CONFIG_ARCH_CHIP_RK3566 */

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

#endif /* __ASSEMBLY__ */
#endif /* __ARCH_ARM64_SRC_RK3566_RK3566_SERIAL_H */
