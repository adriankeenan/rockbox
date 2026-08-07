#ifndef _BUTTON_TARGET_H_
#define _BUTTON_TARGET_H_

/* Logical buttons key codes */
#define BUTTON_UP           0x00000001
#define BUTTON_DOWN         0x00000002
#define BUTTON_LEFT         0x00000004
#define BUTTON_RIGHT        0x00000008
#define BUTTON_TRIANGLE     0x00000010
#define BUTTON_CIRCLE       0x00000020
#define BUTTON_CROSS        0x00000040
#define BUTTON_SQUARE       0x00000080
#define BUTTON_L            0x00000100
#define BUTTON_R            0x00000200
#define BUTTON_START        0x00000400
#define BUTTON_SELECT       0x00000800
#define BUTTON_HOME         0x00001000

#define BUTTON_MAIN         0x00001fff

/* Software power-off (held HOME) */
#define POWEROFF_BUTTON BUTTON_HOME
/* About 3 seconds */
#define POWEROFF_COUNT 30

#endif /* _BUTTON_TARGET_H_ */
