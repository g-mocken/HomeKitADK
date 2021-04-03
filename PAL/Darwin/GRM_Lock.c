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

	// both variants require 'r' + CR
	char c = '.';
	//int s = read(0, &c, 1);
	int s=1; c=getchar();
	HAPLogInfo(&kHAPLog_Default, "%s: Key pressed = %c\n", __func__, c);

	return ((s == 1) && (c == 'r'));

}
