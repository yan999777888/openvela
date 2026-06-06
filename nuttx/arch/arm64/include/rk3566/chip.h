/****************************************************************************
 * arch/arm64/include/rk3566/chip.h
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

#ifndef __ARCH_ARM64_INCLUDE_RK3566_CHIP_H
#define __ARCH_ARM64_INCLUDE_RK3566_CHIP_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Number of bytes in x kibibytes/mebibytes/gibibytes */

#define KB(x)           ((x) << 10)
#define MB(x)           (KB(x) << 10)
#define GB(x)           (MB(UINT64_C(x)) << 10)

/* Rockchip RK3566 GICv3: Distributor and Redistributor */

#define CONFIG_GICD_BASE          0xfd400000
#define CONFIG_GICR_BASE          0xfd460000
#define CONFIG_GICR_OFFSET        0x20000

/* Rockchip RK3566 Memory Map: RAM and Device I/O
 *
 * From DTS: two memory regions with a gap:
 *   Region 1: 0x00200000 - 0x08400000 (130MB)
 *   Region 2: 0x09400000 - 0x80000000 (1900MB)
 * Total usable RAM: ~2030MB
 *
 * We map from 0x02000000 to 0x80000000 (2016MB) as a single flat region.
 * The gap (0x08400000-0x09400000) is not real RAM but is small enough
 * to be covered by the flat mapping without issues.
 */

#define CONFIG_RAMBANK1_ADDR      0x02000000
#define CONFIG_RAMBANK1_SIZE      0x7e000000   /* 0x02000000 to 0x80000000 = 2016MB */
#define CONFIG_DEVICEIO_BASEADDR  0xF8000000
#define CONFIG_DEVICEIO_SIZE      MB(128)

/* Override CONFIG_RAM_END to prevent sign-extension bug.
 * 0x80000000 has bit 31 set, which sign-extends to 0xFFFFFFFF80000000
 * when used in 64-bit arithmetic. Define as explicit 64-bit constant.
 */

#ifdef CONFIG_RAM_START
#  undef CONFIG_RAM_END
#  define CONFIG_RAM_END 0x80000000U
#endif

/* U-Boot loads NuttX at this address (kernel_addr_r) */

#define CONFIG_LOAD_BASE          0x02080000

#define MPID_TO_CLUSTER_ID(mpid)  ((mpid) & ~0xff)

/****************************************************************************
 * Assembly Macros
 ****************************************************************************/

#ifdef __ASSEMBLY__

.macro  get_cpu_id xreg0
  mrs    \xreg0, mpidr_el1
  ubfx   \xreg0, \xreg0, #0, #8
.endm

#endif /* __ASSEMBLY__ */

#endif /* __ARCH_ARM64_INCLUDE_RK3566_CHIP_H */
