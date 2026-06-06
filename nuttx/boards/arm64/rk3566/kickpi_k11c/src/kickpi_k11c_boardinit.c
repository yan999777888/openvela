/****************************************************************************
 * boards/arm64/rk3566/kickpi_k11c/src/kickpi_k11c_boardinit.c
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

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <debug.h>

#include <nuttx/board.h>

#include "kickpi_k11c.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: rk3566_board_initialize
 *
 * Description:
 *   RK3566 arch-level custom initialization.  This function is called
 *   from arm64_chip_boot() after the MMU has been initialized.
 *   All RK3566 architecture-specific initialization should be performed
 *   here, including clock configuration, pin muxing, etc.
 *
 ****************************************************************************/

void rk3566_board_initialize(void)
{
  /* NOTE: Most hardware initialization (clocks, pin muxing, etc.)
   * is done by the bootloader (U-Boot).  We only need to do
   * initialization that U-Boot doesn't do, or that we need to
   * reconfigure differently for NuttX.
   */

#ifdef CONFIG_ARCH_LEDS
  /* Configure on-board LEDs if present */

  board_autoled_initialize();
#endif
}
