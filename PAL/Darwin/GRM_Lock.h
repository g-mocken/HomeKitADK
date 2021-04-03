/*
 * GRM_Dooropener.h
 *
 *  Created on: 3 Apr 2021
 *      Author: mocken
 */

#ifndef DARWIN_GRM_LOCK_H_
#define DARWIN_GRM_LOCK_H_

#include <stdbool.h>

void GRM_Unlock(void);
void GRM_Lock(void);
void GRM_Pulse(void);
bool GRM_Ring(void);

#endif /* DARWIN_GRM_LOCK_H_ */
