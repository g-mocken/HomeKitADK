/*
 * GRM_Dooropener.h / Raspi
 *
 *  Created on: 3 Apr 2021
 *      Author: mocken
 */

#ifndef DARWIN_GRM_LOCK_H_
#define DARWIN_GRM_LOCK_H_

#include <stdbool.h>
#include <stdint.h>
#include "HAP.h"

typedef enum {LOCKED, UNLOCKED} GRM_state_t;

GRM_state_t GRM_GetState(void);

void GRM_Unlock(void);
void GRM_Lock(void);
void GRM_Pulse(void);
void GRM_Blocked(void);

void GRM_Ringcode(bool enable);
void GRM_SetVolume(uint8_t volume);

void GRM_ReadConfiguration(char * ptrValue);

void GRM_Inititalize(HAPAccessoryServerOptions* hapAccessoryServerOptions,
        HAPPlatform* hapPlatform,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks);

void GRM_Deinititalize(void);

#endif /* DARWIN_GRM_LOCK_H_ */
