/***************************************************************************
 *             __________               __   ___.                  
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___  
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /  
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <   
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \  
 *                     \/            \/     \/    \/            \/ 
 *
 * Copyright (C) 2010 by Thomas Martitz
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __SYSTEM_HOSTED_H__
#define __SYSTEM_HOSTED_H__

#ifndef __PCTOOL__

#if defined(PSP)
/* Only PSP actually loads code at runtime (CONFIG_BINFMT == BINFMT_ROCK,
 * via lc-psp.c's raw memcpy-style loader) among hosted targets, so it's
 * the only one where a real icache sync is needed after writing fresh
 * code into RAM -- MIPS doesn't keep icache/dcache coherent on its own. */
#include <psputils.h>
static inline void commit_dcache(void)
{
    sceKernelDcacheWritebackAll();
}
static inline void commit_discard_dcache(void)
{
    sceKernelDcacheWritebackInvalidateAll();
}
static inline void commit_discard_idcache(void)
{
    sceKernelDcacheWritebackInvalidateAll();
    sceKernelIcacheInvalidateAll();
}
#else
static inline void commit_dcache(void) {}
static inline void commit_discard_dcache(void) {}
static inline void commit_discard_idcache(void) {}
#endif

static inline void core_sleep(void)
{
    enable_irq();
    wait_for_interrupt();
}

#endif /* __PCTOOL__ */

#if defined(WIN32) || defined(__PCTOOL__)

#ifndef alloca
#define alloca __builtin_alloca
#endif

#endif /* WIN32 || __PCTOOL__ */

#endif
