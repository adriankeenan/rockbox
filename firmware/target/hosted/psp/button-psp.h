#ifndef __BUTTON_PSP_H__
#define __BUTTON_PSP_H__

#include <stdbool.h>
#include "config.h"

bool button_hold(void);
#undef button_init_device
void button_init_device(void);
int button_read_device(int *data);

#endif /* __BUTTON_PSP_H__ */
