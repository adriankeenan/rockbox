/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 * Copyright (C) 2026 Rockbox PSP port
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

#include "autoconf.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>

#include <pspkernel.h>
#include <pspthreadman.h>

#include "system-psp.h"
#include "thread-psp.h"
#include "../kernel-internal.h"
#include "core_alloc.h"

/* Each Rockbox "thread" is a real PSP kernel thread; this global recursive
 * lock hands exclusive execution to one of them at a time so that the
 * cooperative assumptions made by the rest of Rockbox's core code still
 * hold (i.e. only one Rockbox thread is ever logically "running"). */
typedef struct
{
    SceUID sema;
    SceUID owner;
    int count;
} psp_reclock_t;

static psp_reclock_t m;

static void reclock_init(psp_reclock_t *l)
{
    l->sema = sceKernelCreateSema("rb_scheduler", 0, 1, 1, NULL);
    l->owner = -1;
    l->count = 0;
}

static void reclock_lock(psp_reclock_t *l)
{
    SceUID self = sceKernelGetThreadId();
    if (l->owner == self)
    {
        l->count++;
        return;
    }
    sceKernelWaitSema(l->sema, 1, NULL);
    l->owner = self;
    l->count = 1;
}

static void reclock_unlock(psp_reclock_t *l)
{
    if (--l->count == 0)
    {
        l->owner = -1;
        sceKernelSignalSema(l->sema, 1);
    }
}

#define THREAD_PANICF(str...) \
    ({ fprintf(stderr, str); exit(-1); })

static jmp_buf thread_jmpbufs[MAXTHREADS];

#define THREADS_RUN                 0
#define THREADS_EXIT                1
#define THREADS_EXIT_COMMAND_DONE   2
static volatile int threads_status = THREADS_RUN;

extern long start_tick;

static inline SceUID sema_of(void *p) { return (SceUID)(intptr_t)p; }
static inline void * as_ptr(SceUID id) { return (void *)(intptr_t)id; }

static void psp_wait_and_delete_thread(SceUID tid)
{
    if (tid < 0)
        return;
    sceKernelWaitThreadEnd(tid, NULL);
    sceKernelDeleteThread(tid);
}

static int psp_sem_wait_timeout(SceUID sema, long tmo_ms)
{
    SceUInt to = (SceUInt)tmo_ms * 1000;
    int rc = sceKernelWaitSema(sema, 1, &to);
    return (rc == (int)SCE_KERNEL_ERROR_WAIT_TIMEOUT) ? 1 : 0;
}

void sim_thread_shutdown(void)
{
    int i;

    threads_status = THREADS_EXIT;

    reclock_lock(&m);

    for (i = 0; i < MAXTHREADS; i++)
    {
        struct thread_entry *thread = __thread_slot_entry(i);
        if (thread->context.s == NULL)
            continue;
        sceKernelSignalSema(sema_of(thread->context.s), 1);
    }

    for (i = 0; i < MAXTHREADS; i++)
    {
        struct thread_entry *thread = __thread_slot_entry(i);
        void *t = thread->context.t;

        if (t != NULL)
        {
            reclock_unlock(&m);
            psp_wait_and_delete_thread(sema_of(t));
            reclock_lock(&m);
            thread->context.told = NULL;
        }
        else
        {
            psp_wait_and_delete_thread(sema_of(thread->context.told));
        }
    }

    reclock_unlock(&m);

    threads_status = THREADS_EXIT_COMMAND_DONE;
}

void sim_thread_exception_wait(void)
{
    while (1)
    {
        sceKernelDelayThread((1000000 / HZ) / 10);
        if (threads_status != THREADS_RUN)
            thread_exit();
    }
}

void sim_thread_lock(void *me)
{
    reclock_lock(&m);
    __running_self_entry() = (struct thread_entry *)me;

    if (threads_status != THREADS_RUN)
        thread_exit();
}

void * sim_thread_unlock(void)
{
    struct thread_entry *current = __running_self_entry();
    reclock_unlock(&m);
    return current;
}

void switch_thread(void)
{
    struct thread_entry *current = __running_self_entry();

    enable_irq();

    switch (current->state)
    {
    case STATE_RUNNING:
    {
        reclock_unlock(&m);
        reclock_lock(&m);
        break;
    }

    case STATE_BLOCKED:
    {
        int oldlevel;

        reclock_unlock(&m);
        sceKernelWaitSema(sema_of(current->context.s), 1, NULL);
        reclock_lock(&m);

        oldlevel = disable_irq_save();
        current->state = STATE_RUNNING;
        restore_irq(oldlevel);
        break;
    }

    case STATE_BLOCKED_W_TMO:
    {
        int result, oldlevel;

        reclock_unlock(&m);
        result = psp_sem_wait_timeout(sema_of(current->context.s), current->tmo_tick);
        reclock_lock(&m);

        oldlevel = disable_irq_save();

        current->state = STATE_RUNNING;

        if (result == 1)
        {
            while (1)
            {
                SceKernelSemaInfo info;
                info.size = sizeof(info);
                if (sceKernelReferSemaStatus(sema_of(current->context.s), &info) < 0)
                    break;
                if (info.currentCount <= 0)
                    break;
                sceKernelWaitSema(sema_of(current->context.s), 1, NULL);
            }
        }

        restore_irq(oldlevel);
        break;
    }

    case STATE_SLEEPING:
    {
        reclock_unlock(&m);
        psp_sem_wait_timeout(sema_of(current->context.s), current->tmo_tick);
        reclock_lock(&m);
        current->state = STATE_RUNNING;
        break;
    }
    }

#ifdef BUFLIB_DEBUG_CHECK_VALID
    core_check_valid();
#endif
    __running_self_entry() = current;

    if (threads_status != THREADS_RUN)
        thread_exit();
}

void sleep_thread(int ticks)
{
    struct thread_entry *current = __running_self_entry();
    int rem;

    current->state = STATE_SLEEPING;

    rem = ((long)(sceKernelGetSystemTimeLow() / 1000) - start_tick) % (1000/HZ);
    if (rem < 0)
        rem = 0;

    current->tmo_tick = (1000/HZ) * ticks + ((1000/HZ)-1) - rem;
}

void block_thread_(struct thread_entry *current, int ticks)
{
    if (ticks < 0)
        current->state = STATE_BLOCKED;
    else
    {
        current->state = STATE_BLOCKED_W_TMO;
        current->tmo_tick = (1000/HZ)*ticks;
    }

    wait_queue_register(current);
}

unsigned int wakeup_thread_(struct thread_entry *thread
                            IF_PRIO(, enum wakeup_thread_protocol proto))
{
    switch (thread->state)
    {
    case STATE_BLOCKED:
    case STATE_BLOCKED_W_TMO:
        wait_queue_remove(thread);
        thread->state = STATE_RUNNING;
        sceKernelSignalSema(sema_of(thread->context.s), 1);
        return THREAD_OK;
    }

    return THREAD_NONE;
    (void) proto;
}

void thread_thaw(unsigned int thread_id)
{
    struct thread_entry *thread = __thread_id_entry(thread_id);

    if (thread->id == thread_id && thread->state == STATE_FROZEN)
    {
        thread->state = STATE_RUNNING;
        sceKernelSignalSema(sema_of(thread->context.s), 1);
    }
}

static int psp_runthread(SceSize args, void *argp)
{
    (void)args;
    struct thread_entry *current = *(struct thread_entry **)argp;

    reclock_lock(&m);
    __running_self_entry() = current;

    jmp_buf *current_jmpbuf = &thread_jmpbufs[THREAD_ID_SLOT(current->id)];

    if (setjmp(*current_jmpbuf) == 0)
    {
        if (current->state == STATE_FROZEN)
        {
            reclock_unlock(&m);
            sceKernelWaitSema(sema_of(current->context.s), 1, NULL);
            reclock_lock(&m);
            __running_self_entry() = current;
        }

        if (threads_status == THREADS_RUN)
        {
            current->context.start();
        }

        thread_exit();
    }
    else
    {
        reclock_unlock(&m);
    }

    return 0;
}

unsigned int create_thread(void (*function)(void),
                           void* stack, size_t stack_size,
                           unsigned flags, const char *name
                           IF_PRIO(, int priority)
                           IF_COP(, unsigned int core))
{
    struct thread_entry *thread = thread_alloc();
    if (thread == NULL)
        return 0;

    SceUID s = sceKernelCreateSema(name ? name : "rb_thread", 0, 0, 255, NULL);
    if (s < 0)
        return 0;

    thread->name = name;
    thread->state = (flags & CREATE_THREAD_FROZEN) ?
        STATE_FROZEN : STATE_RUNNING;
    thread->context.start = function;
    thread->context.s = as_ptr(s);
    thread->context.t = as_ptr(-1);
    thread->context.told = NULL;
    thread->priority = priority;

    /* DEFAULT_STACK_SIZE (thread-internal.h) is 256 bytes -- fine on
     * SDL/CTRU where it's ignored and a real OS-thread stack is used
     * underneath, but sceKernelCreateThread() rejects anything that
     * small outright (ILLEGAL_STACK_SIZE). Since our "thread" IS a real
     * PSP thread, always give it a real stack. */
    size_t real_stack_size = stack_size > 16*1024 ? stack_size : 16*1024;

    SceUID tid = sceKernelCreateThread(name ? name : "rockbox_thread",
                                       psp_runthread, 0x20,
                                       real_stack_size, 0, NULL);
    if (tid < 0)
    {
        sceKernelDeleteSema(s);
        return 0;
    }

    thread->context.t = as_ptr(tid);

    struct thread_entry *arg = thread;
    sceKernelStartThread(tid, sizeof(arg), &arg);

    return thread->id;
    (void)stack;
}

void thread_exit(void)
{
    struct thread_entry *current = __running_self_entry();

    int oldlevel = disable_irq_save();

    void *t = current->context.t;
    void *s = current->context.s;

    psp_wait_and_delete_thread(sema_of(current->context.told));

    current->context.t = NULL;
    current->context.s = NULL;
    current->context.told = t;

    unsigned int id = current->id;
    new_thread_id(current);
    current->state = STATE_KILLED;
    wait_queue_wake(&current->queue);

    if (s != NULL)
        sceKernelDeleteSema(sema_of(s));

    restore_irq(oldlevel);

    thread_free(current);

    longjmp(thread_jmpbufs[THREAD_ID_SLOT(id)], 1);

    THREAD_PANICF("thread_exit->K:*R (ID: %d)", id);
    while (1);
}

void thread_wait(unsigned int thread_id)
{
    struct thread_entry *current = __running_self_entry();
    struct thread_entry *thread = __thread_id_entry(thread_id);

    if (thread->id == thread_id && thread->state != STATE_KILLED)
    {
        block_thread(current, TIMEOUT_BLOCK, &thread->queue, NULL);
        switch_thread();
    }
}

int thread_set_priority(unsigned int thread_id, int priority)
{
    struct thread_entry *thread = __thread_id_entry(thread_id);
    SceUID tid = sema_of(thread->context.t);
    if (tid >= 0)
        sceKernelChangeThreadPriority(tid, 0x20);
    thread->priority = priority;
    return thread->priority;
}

int thread_get_priority(unsigned int thread_id)
{
    struct thread_entry *thread = __thread_id_entry(thread_id);
    return thread->priority;
}

void init_threads(void)
{
    reclock_init(&m);
    reclock_lock(&m);

    thread_alloc_init();

    struct thread_entry *thread = thread_alloc();
    if (thread == NULL)
    {
        fprintf(stderr, "Main thread alloc failed\n");
        return;
    }

    thread->name = __main_thread_name;
    thread->state = STATE_RUNNING;
    SceUID s = sceKernelCreateSema("rb_main_thread", 0, 0, 255, NULL);
    thread->context.s = as_ptr(s);
    thread->context.t = NULL; /* NULL for the implicit main thread */
    thread->context.told = NULL;
    __running_self_entry() = thread;

    if (s < 0)
    {
        fprintf(stderr, "Failed to create main semaphore\n");
        return;
    }

    if (setjmp(thread_jmpbufs[THREAD_ID_SLOT(thread->id)]) == 0)
    {
        return;
    }

    reclock_unlock(&m);

    while (threads_status < THREADS_EXIT_COMMAND_DONE)
        sceKernelDelayThread(10000);

    sim_do_exit();
}
