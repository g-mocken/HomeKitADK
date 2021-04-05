// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// The most basic HomeKit example: an accessory that represents a light bulb that
// only supports switching the light on and off. Actions are exposed as individual
// functions below.
//
// This header file is platform-independent.

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "HAP.h"

#if __has_feature(nullability)
#pragma clang assume_nonnull begin
#endif

/**
 * Identify routine. Used to locate the accessory.
 */
HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server,
        const HAPAccessoryIdentifyRequest* request,
        void* _Nullable context);

/**
 * Handle read request to the 'CurrentState' characteristic of the Lock Mechanism service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleLockMechanismLockCurrentStateRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        uint8_t* value,
        void* _Nullable context);

/**
 * Handle read request to the 'TargetState' characteristic of the Lock Mechanism service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleLockMechanismLockTargetStateRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        uint8_t* value,
        void* _Nullable context);

/**
 * Handle write request to the 'TargetState' characteristic of the Lock Mechanism service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleLockMechanismLockTargetStateWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        uint8_t value,
        void* _Nullable context);

/**
 * Handle write request to the 'ControlPoint' characteristic of the Lock Management service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleLockManagementLockControlPointWrite(
        HAPAccessoryServerRef* server,
        const HAPTLV8CharacteristicWriteRequest* request,
        HAPTLVReaderRef* requestReader,
        void* _Nullable context);

/**
 * Handle read request to the 'Version' characteristic of the Lock Management service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleLockManagementVersionRead(
        HAPAccessoryServerRef* server,
        const HAPStringCharacteristicReadRequest* request,
        char* value,
        size_t maxBytes,
        void* _Nullable context);


HAPError HandleLockManagementAutoSecurityTimeoutRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt32CharacteristicReadRequest* request HAP_UNUSED,
        uint32_t * value,
        void* _Nullable context HAP_UNUSED);

HAPError HandleLockManagementAutoSecurityTimeoutWrite(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt32CharacteristicWriteRequest* request HAP_UNUSED,
        uint32_t value,
        void* _Nullable context HAP_UNUSED);


/**
 * Handle read request to the 'ProgrammableSwitchEvent' characteristic of the Doorbell service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleProgrammableSwitchEventRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        uint8_t* value,
        void* _Nullable context);

/**
 * Handle read request to the 'Volume' characteristic of the Doorbell service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleVolumeRead(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicReadRequest* request,
        uint8_t* value,
        void* _Nullable context);

/**
 * Handle write request to the 'Volume' characteristic of the Doorbell service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleVolumeWrite(
        HAPAccessoryServerRef* server,
        const HAPUInt8CharacteristicWriteRequest* request,
        uint8_t value,
        void* _Nullable context);




/**
 * Handle read request to the 'On' characteristic of the Light Bulb service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleRingcodeOnRead(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicReadRequest* request,
        bool* value,
        void* _Nullable context);

/**
 * Handle write request to the 'On' characteristic of the Light Bulb service.
 */
HAP_RESULT_USE_CHECK
HAPError HandleRingcodeOnWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context);


/**
 * Initialize the application.
 */
void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore);

/**
 * Deinitialize the application.
 */
void AppRelease(void);

/**
 * Start the accessory server for the app.
 */
void AppAccessoryServerStart(void);

/**
 * Handle the updated state of the Accessory Server.
 */
void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context);

void AccessoryServerHandleSessionAccept(HAPAccessoryServerRef* server, HAPSessionRef* session, void* _Nullable context);

void AccessoryServerHandleSessionInvalidate(
        HAPAccessoryServerRef* server,
        HAPSessionRef* session,
        void* _Nullable context);

/**
 * Restore platform specific factory settings.
 */
void RestorePlatformFactorySettings(void);

/**
 * Returns pointer to accessory information
 */
const HAPAccessory* AppGetAccessoryInfo();


void ringBell(void* _Nullable context HAP_UNUSED, size_t contextSize HAP_UNUSED);
void openForRingcode(void* _Nullable context HAP_UNUSED, size_t contextSize HAP_UNUSED);


#if __has_feature(nullability)
#pragma clang assume_nonnull end
#endif

#ifdef __cplusplus
}
#endif

#endif
