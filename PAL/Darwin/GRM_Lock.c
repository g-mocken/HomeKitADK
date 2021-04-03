/*
 * GRM_Lock.c
 *
 *  Created on: 3 Apr 2021
 *      Author: mocken
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>

#include "HAP.h"

#include "GRM_Lock.h"

void GRM_Unlock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start unlocking...\n", __func__);
}
void GRM_Lock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start locking...\n", __func__);
}

void GRM_Pulse(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Pulse for unlocking/locking...\n", __func__);
}

bool GRM_Ring(void) {

    int c;

    static struct termios new_io;
    static struct termios old_io;

    tcgetattr(STDIN_FILENO, &old_io);
    new_io = old_io;
    new_io.c_lflag = new_io.c_lflag & ~(ECHO|ICANON);
    new_io.c_cc[VMIN] = 1;
    new_io.c_cc[VTIME]= 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &new_io);

    c=getchar(); // works in Terminal, but still requires extra CR in Eclipse console
	HAPLogInfo(&kHAPLog_Default, "%s: Key pressed = %c\n", __func__, c);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &old_io);
	return (c == 'r');

}
