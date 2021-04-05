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
#include <stdint.h>
#include <pthread.h>
#include <json-c/json.h> // "json-c" must be installed via brew in /usr/local


#include "HAP.h"
#include "../Applications/Lock/App.h"
#include "GRM_Lock.h"

void GRM_Unlock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start unlocking...", __func__);
}
void GRM_Lock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start locking...", __func__);
}

void GRM_Pulse(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Pulse for unlocking/locking...", __func__);
}

void GRM_Blocked(void){
	HAPLogInfo(&kHAPLog_Default, "%s: Ringcode is blocked", __func__);
}

static int getKeyPress(void) {

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

    return c;

}

void GRM_SetVolume(uint8_t volume){
	HAPLogInfo(&kHAPLog_Default, "%s: Bell volume set to %u", __func__, volume);
}

void GRM_Ringcode(bool enable){
	HAPLogInfo(&kHAPLog_Default, "%s: Ringcode %s", __func__, enable?"enabled":"disabled");
}


volatile bool stopThreads = false;



static pthread_t mainThread;
static void* mainFunction(void *ptr) {

	char *message;
	message = (char*) ptr;
    HAPLogInfo(&kHAPLog_Default, "%s: Starting thread with message: %s", __func__, message);

	while (!stopThreads) {

		int c = getKeyPress(); // blocking!
		if (c =='r'){ // ring
			HAPError err = HAPPlatformRunLoopScheduleCallback(ringBell, NULL, 0); // required for IRQ/threads, but not for timers
			HAPLogInfo(&kHAPLog_Default, "%s: RING triggered with error = %u", __func__, err);
		} else if (c == 'c') { // code
			HAPError err = HAPPlatformRunLoopScheduleCallback(openForRingcode, NULL, 0); // required for IRQ/threads, but not for timers
			HAPLogInfo(&kHAPLog_Default, "%s: CODE triggered with error = %u", __func__, err);
		}
	}
	return NULL;
}


typedef enum {CODE_ANY=0, CODE_SHORT=1, CODE_LONG=2, CODE_VERYLONG=3} code_t;
long anyMin, anyMax, shortMin, shortMax, longMin, longMax, veryLongMin, veryLongMax;
code_t code[21];
int codeLength=0;


void GRM_ReadConfiguration(char * ptrValue){
	HAPLogInfo(&kHAPLog_Default, "%s: Reading configuration from file %s", __func__, ptrValue);

	struct json_object *jobj = json_object_from_file(ptrValue); // /home/pi/remote/dooropener.config



	HAPLogInfo(&kHAPLog_Default, "json object read from file and parsed:\n---\n%s\n---\n",	json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
//	HAPLogInfo(&kHAPLog_Default, "json object read from file and parsed:\n---\n%s\n---\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN));

	struct json_object *timeMinMax;

	timeMinMax = json_object_object_get(jobj, "anyMin");
	anyMin = json_object_get_int(timeMinMax);
	timeMinMax = json_object_object_get(jobj, "anyMax");
	anyMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "any = %ld ... %ld", anyMin, anyMax);

	timeMinMax = json_object_object_get(jobj, "shortMin");
	shortMin = json_object_get_int(timeMinMax);
	timeMinMax = json_object_object_get(jobj, "shortMax");
	shortMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "short = %ld ... %ld", shortMin, shortMax);

	timeMinMax = json_object_object_get(jobj, "longMin");
	longMin = json_object_get_int(timeMinMax);
	timeMinMax = json_object_object_get(jobj, "longMax");
	longMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "long = %ld ... %ld", longMin, longMax);

	timeMinMax = json_object_object_get(jobj, "veryLongMin");
	veryLongMin = json_object_get_int(timeMinMax);
	timeMinMax = json_object_object_get(jobj, "veryLongMax");
	veryLongMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "veryLong = %ld ... %ld", veryLongMin, veryLongMax);

	struct json_object *codeArray, *codeEntry;
	codeArray = json_object_object_get(jobj, "code");
	codeLength = json_object_array_length(codeArray);
	HAPLogInfo(&kHAPLog_Default, "code len=%d",(int) codeLength);

	if (codeLength <= (int) (sizeof(code) / sizeof(code[0]))) {
		for (int i = 0; i < codeLength; i++) {
			// get the i-th object in medi_array
			codeEntry = json_object_array_get_idx(codeArray, i);
			code[i] = (code_t) json_object_get_int(codeEntry);
			HAPLogInfo(&kHAPLog_Default, "code[%d]=%d", i, code[i]);
		}
	}
}


void GRM_Inititalize(HAPAccessoryServerOptions* hapAccessoryServerOptions HAP_UNUSED,
        HAPPlatform* hapPlatform HAP_UNUSED,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks HAP_UNUSED){

	int result = pthread_create(&mainThread, NULL, mainFunction, (void*) "Main thread started.");
	(void ) result;

}

void GRM_Deinititalize(void){

stopThreads = true;
	pthread_join(mainThread, NULL);

}
