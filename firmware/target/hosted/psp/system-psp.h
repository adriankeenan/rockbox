#ifndef _SYSTEM_PSP_H_
#define _SYSTEM_PSP_H_

#include <stdbool.h>

#include "config.h"
#include "gcc_extensions.h"

#define HIGHEST_IRQ_LEVEL 1

int set_irq_level(int level);

#define disable_irq() \
    ((void)set_irq_level(HIGHEST_IRQ_LEVEL))

#define enable_irq()  \
    ((void)set_irq_level(0))

#define disable_irq_save() \
    set_irq_level(HIGHEST_IRQ_LEVEL)

#define restore_irq(level) \
    ((void)set_irq_level(level))

#define wait_for_interrupt()

#include "system-hosted.h"

void sim_enter_irq_handler(void);
void sim_exit_irq_handler(void);
void sim_kernel_shutdown(void);
void sys_poweroff(void);
void sim_do_exit(void) NORETURN_ATTR;
void psp_sys_quit(void);

extern long start_tick;

#endif /* _SYSTEM_PSP_H_ */
