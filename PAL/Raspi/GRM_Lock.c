/*
 * GRM_Lock.c / Raspi
 *
 *  Created on: 3 Apr 2021
 *      Author: mocken
 */
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/time.h> // gettimeofday
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <stdint.h>
#include <pthread.h>
#include <string.h> // strcmp

// for reading the configuration file:
#include <json-c/json.h>

// for GPIO:
#include <wiringPi.h>

// for audio playback:
#include <math.h>
#define BUFFER_SIZE 8192
#include <ao/ao.h>
#include <sndfile.h>
#include <signal.h>
static uint8_t vol;


#include "HAP.h"
#include "../Applications/Lock/App.h"
#include "GRM_Lock.h"


/*
	Requirements:
	
	/usr/bin/gpio -g mode 20 tri
	/usr/bin/gpio export 21 out
*/

static const int opener = 21; // == 29 in wPi numbering
static const int trigger = 20;
static const char * unlocked = "/var/www/dooropener/unlocked.wav";
static const char * doorbell = "/var/www/dooropener/doorbell.wav";
static const char * locked = "/var/www/dooropener/locked.wav";

bool block = false;
static pthread_mutex_t blockMutex = PTHREAD_MUTEX_INITIALIZER;


typedef enum {CODE_ANY=0, CODE_SHORT=1, CODE_LONG=2, CODE_VERYLONG=3} code_t;
static long anyMin, anyMax, shortMin, shortMax, longMin, longMax, veryLongMin, veryLongMax;
static code_t code[21];
static int codeLength=0;





typedef enum {
	EVENT_BELL, EVENT_UNLOCKED, EVENT_LOCKED
} event_t;

static struct {
	int64_t time;
	event_t event;
} logfile[10];

static const int logfilesize = sizeof(logfile) / sizeof(logfile[0]);

static int pointer;

void logEvent(event_t e) {
	struct timeval t;
	gettimeofday(&t, NULL);

	logfile[pointer].event = e;
	logfile[pointer].time = t.tv_sec;

	pointer++;
	if (pointer >= logfilesize)
		pointer = 0;
}





/*
void delayTenths(int t) {
	const long tenthsOfSecond = 100000000;
	struct timespec time = { .tv_sec = t / 10, .tv_nsec = (t % 10)
			* tenthsOfSecond };
	nanosleep(&time, NULL);
}
*/

// is it unlocked?
bool isOn(void) {
	return digitalRead(opener) == HIGH;
}

// unlock
void on(void) {
	digitalWrite(opener, HIGH);
}

// lock
void off(void) {
	digitalWrite(opener, LOW);
}

// unlock, then lock again
void pulse(void) {
	digitalWrite(opener, HIGH);
	delay(200); // blocking HAP this long should be avoided?
	digitalWrite(opener, LOW);
}

static void clean(ao_device *device, SNDFILE *file) {
	ao_close(device);
	sf_close(file);
	ao_shutdown();
}

int playAudio(char * filename, float volume) {
	ao_device *device;
	ao_sample_format format;
	SF_INFO sfinfo;

	int default_driver;

	short *buffer;

	SNDFILE *file = sf_open(filename, SFM_READ, &sfinfo);

	//   printf("Samples: %lld\n", sfinfo.frames);
	//   printf("Sample rate: %d\n", sfinfo.samplerate);
	//   printf("Channels: %d\n", sfinfo.channels);

	ao_initialize();

	default_driver = ao_driver_id("alsa");
	//= ao_default_driver_id();

	switch (sfinfo.format & SF_FORMAT_SUBMASK) {
	case SF_FORMAT_PCM_16:
		format.bits = 16;
		break;
	case SF_FORMAT_PCM_24:
		format.bits = 24;
		break;
	case SF_FORMAT_PCM_32:
		format.bits = 32;
		break;
	case SF_FORMAT_PCM_S8:
		format.bits = 8;
		break;
	case SF_FORMAT_PCM_U8:
		format.bits = 8;
		break;
	default:
		format.bits = 16;
		break;
	}

	format.channels = sfinfo.channels;
	format.rate = sfinfo.samplerate;
	format.byte_format = AO_FMT_NATIVE;
	format.matrix = 0;

	//struct ao_option option = {.key="id", .value="1", .next=NULL}; // optionally choose audio driver

	device = ao_open_live(default_driver, &format, NULL/*&option*/); // use default audio, setup alsa for dmix and mono!

	if (device == NULL) {
		fprintf(stderr, "Error opening device.\n");
		return 1;
	}

	buffer = calloc(BUFFER_SIZE, sizeof(short));
	long int read = 1;
	while (read != 0) {
		read = sf_read_short(file, buffer, BUFFER_SIZE);

		// adjust volume
		for (long int i = 0; i < BUFFER_SIZE; i++) {
			long int sample = lroundf(buffer[i] * volume);
			if (sample > SHRT_MAX)
				sample = SHRT_MAX; // clip at max. volume
			if (sample < SHRT_MIN)
				sample = SHRT_MIN;
			buffer[i] = sample;
		}
		// end of volume adjustment

		if (ao_play(device, (char *) buffer, (uint_32) (read * sizeof(short)))
				== 0) {
			fprintf(stderr, "ao_play: failed.\n");
			clean(device, file);
			break;
		}
	}

	clean(device, file);

	return 0;
}

void * playAudioThread(void *ptr) {
	static bool playing;
	const float lowVolume = 1. / 2.; //1./8.;
	const float maxVolume = 1.0;
	static float volume = 1. / 2.; // start very soft
	static struct timeval timeStart, timeEnd;

	pthread_mutex_lock(&blockMutex);
	float relativeVolume = ((float) vol)/100.0;
	pthread_mutex_unlock(&blockMutex);


	gettimeofday(&timeEnd, NULL);
	long ms = ((timeEnd.tv_sec - timeStart.tv_sec) * 1000
			+ (timeEnd.tv_usec - timeStart.tv_usec) / 1000);
	if (ms > 10000)
		volume = lowVolume; // if there is a pause of 10s, reset volume to low

	gettimeofday(&timeStart, NULL);

	if (!playing) {
		playing = true;
		if (strcmp((char*) ptr, unlocked) == 0
				|| strcmp((char*) ptr, locked) == 0)
			volume = maxVolume; // alway play the "Unlocked" sound at this volume
		fprintf(stderr, "volume = %f\n", volume);
		playAudio((char*) ptr, volume * relativeVolume); // weighted with global volume
		if (strcmp((char*) ptr, unlocked) == 0
				|| strcmp((char*) ptr, locked) == 0)
			volume = lowVolume; // after "Unlocked" start over at this volume

		playing = false;

		if (volume < maxVolume)
			volume *= 2.0;
		else
			volume = maxVolume; // max
	} else {
		fprintf(stderr, "IGNORING - audio still playing\n");

	} // with a mutex, other calls would be kind of queued, this way, they return immediately

	return NULL;
}


bool blockBruteForce(long pressTimeMs) { // call only on press (not release)!
	bool allow = true;
	static int count = 0;
	if (pressTimeMs > 30000) // if released for 30s, re-allow unlocking
			{
		allow = true;
		count = 0;
		fprintf(stderr, "Unlocking re-allowed for 3 attempts!\n");
	}
	count++;
	if (count > 3) {

		allow = false; // after the third (i.e. at the fourth) ring, disallow unlocking the door for some time
		count--; // prevent overflow, i.e. go back to 2
		fprintf(stderr, "Unlocking disallowed for 30s!\n"); // called after start of pressing, so pressTimeMs is actually a release time

	}

	return allow;
}

void sendPushNotification(const char * push_message) {

		fprintf(stderr, "Push: %s\n", push_message);

}

void ringBell2(long pressTimeMs) {

	(void) pressTimeMs; // unused, suppress warning

	fprintf(stderr, "DINGDONG!\n"); // called after start of pressing, so pressTimeMs is actually a release time

	static struct timeval timeStart, timeEnd;
	gettimeofday(&timeEnd, NULL);
	long dt = ((timeEnd.tv_sec - timeStart.tv_sec) * 1000
			+ (timeEnd.tv_usec - timeStart.tv_usec) / 1000);
	timeStart = timeEnd;

	if (dt > 10000)
		sendPushNotification("Es hat an der Haustür geklingelt."); // combine multiple rings that occur within 10s into a single push

	// but log them all
	logEvent(EVENT_BELL);

	pthread_t audioThread;
	int result = pthread_create(&audioThread, NULL, playAudioThread,
			(void*) doorbell);
	if (result) {
		fprintf(stderr, "Error - pthread_create(playAudioThread) return code: %d\n", result);
	} else{
		pthread_detach(audioThread);
	}
}

/*
 * unlocks the door unless it is excplicitly blocked
 */
void puzzleSolved(void){

	pthread_mutex_lock(&blockMutex);
	bool b = block;
	pthread_mutex_unlock(&blockMutex);

	if (b) {
		fprintf(stderr, "LOCKED!\n");
		pthread_t audioThread;
		int result = pthread_create(&audioThread, NULL, playAudioThread,
				(void*) locked);
		if (result) {
			fprintf(stderr,
					"Error - pthread_create(playAudioThread, locked) return code: %d\n",
					result);
		} else{
			pthread_detach(audioThread);
		}

		sendPushNotification(
				"Das Klingelzeichen war korrekt, aber die Haustür ist verriegelt.");
		logEvent(EVENT_LOCKED);

	} else {
		fprintf(stderr, "UNLOCKED!\n");
		pulse();
		pthread_t audioThread;
		int result = pthread_create(&audioThread, NULL, playAudioThread,
				(void*) unlocked);
		if (result) {
			fprintf(stderr,
					"Error - pthread_create(playAudioThread, unlocked) return code: %d\n",
					result);
		} else{
			pthread_detach(audioThread);
		}

		sendPushNotification(
				"Die Haustür wurde per Klingelzeichen entriegelt.");
		logEvent(EVENT_UNLOCKED);
	}
}



void decodePattern(bool press) { // bei press (true) und release (false) aufrufen
	static struct timeval timeStart, timeEnd;


	gettimeofday(&timeEnd, NULL);
	long pressTimeMs = ((timeEnd.tv_sec - timeStart.tv_sec) * 1000
			+ (timeEnd.tv_usec - timeStart.tv_usec) / 1000);
	timeStart = timeEnd;

	fprintf(stderr, "%s time was = %ld ms = ", !press ? "press" : "release",
			pressTimeMs); // !press, because we lag behind
	// NOTE: first value of pressTimeMs will be large negative

	/* NEED TO DEBOUNCE! And need to distinguish falling and rising IRQs properly.
	 * Reading the input at the beginning of the ISR may be too slow, i.e. the level is no longer the one that triggered the IRQ.
	 * Workaround: use two inputs in parallel, one ofr rising, another one for falling edge detection. */

	

	bool isAny = (pressTimeMs <= anyMax) && (pressTimeMs >= anyMin); // nach oben begrenzt, damit kein Zustand ewig stehenbleiben kann! Nach spätestens 10s matched auch Any nicht mehr, d.h. ein press-Event führt zum state RING.

	bool isShort = (pressTimeMs <= shortMax) && (pressTimeMs >= shortMin); // less than half a second
	bool isLong = (pressTimeMs <= longMax) && (pressTimeMs >= longMin); // about one seconds
	bool isVeryLong = (pressTimeMs <= veryLongMax) && (pressTimeMs >= veryLongMin); // about two seconds


	fprintf(stderr, "%s \n",
			isVeryLong ?
					"very long" :
					(isLong ? "long" : (isShort ? "short" : "other")));

	bool release = !press;
	static bool allow = true; // set to false if too many tried in too short a time (prevents brute force attack)

// flexible new variant

	bool checks[4]={[CODE_ANY]=isAny, [CODE_SHORT]=isShort,[CODE_LONG]=isLong,[CODE_VERYLONG]=isVeryLong};

	static int numericalState = 0;

	if (numericalState <= codeLength) {

		if (numericalState % 2 == 0) {
			// even states: press

			if (numericalState == 0) { // special state 0: IDLE
				if (press) { // nothing to be checked for the first beginning press
					numericalState = 1;
#if ALWAYS_RING_ON_FIRST_PRESS
					ringBell2(pressTimeMs);
#endif
					allow = blockBruteForce(pressTimeMs); // this call is needed to re-allow! It still counts as a ring.
				}

			} else { // states 2, 4, 6, ...

				if (checks[code[numericalState - 1]] && press && allow) {
					numericalState++; // correctly timed release and beginning press detected
				} else {
					numericalState = 1; // on error, can retry immediately (but only 3 times without delay, see above)
					ringBell2(pressTimeMs);
					allow = blockBruteForce(pressTimeMs);
				}
			}
		} else {
			// odd states: release

			if (numericalState == 1) { // special state 1: RING

				if (checks[code[numericalState - 1]] && release && allow) {
					numericalState++; //  correctly timed press and beginning release detected
				} else {
					numericalState = 0;
#if !ALWAYS_RING_ON_FIRST_PRESS
					ringBell2(pressTimeMs);
#endif
				}

			} else if (numericalState == codeLength) { // special last state 7:
				if (checks[code[numericalState - 1]] && release && allow) {
					numericalState = 0; // correctly timed press and beginning release detected
					puzzleSolved();
				} else {
					numericalState = 0;
				}

			} else { // states 3, 5, ...

				if (checks[code[numericalState - 1]] && release && allow)
					numericalState++; // correctly timed press and beginning release detected
				else
					numericalState = 0;

			}
		}
	} else {
		fprintf(stderr, "state overflow error"); // this should not ever happen - sizeof(code) must be odd (odd number of press times plus even number of release times)
		numericalState = 0;
	}
	fprintf(stderr, "state=%d, allow=%d\n", numericalState, allow);


}




void risingISR(void) {
	fprintf(stderr, "rising IRQ / button released\n");
}

void fallingISR(void) {
	fprintf(stderr, "falling IRQ / button pressed\n");
	// triggerISR();
}

/*
 * DEBOUNCE:
 * The discussion here:
 * https://www.raspberrypi.org/forums/viewtopic.php?f=29&t=133740
 * https://www.raspberrypi.org/forums/viewtopic.php?f=28&t=134394
 * suggests, that the inputs are not Schmidt-Trigger type, so my simple debouncing circuit is probably not good enough (and has too much capacity).
 *
 * Nevertheless, with 1uF and 6k6 it *used* to work, but after integrating it into main board, it no longer does, unless I am additionally ignoring certain edges in software (see below).
 * Turned off internal pullup (in gpio config via startup script) because it raises the input pin level. External pullup should be enough.
 */

void dispatchISR(void) {
	static bool last;
	static bool init = true;

	bool current;
	current = digitalRead(trigger);

	if ((current != last) || init) { // only accept consecutive edges of different direction
		if (current)
			risingISR();
		else
			fallingISR();
		decodePattern(!current);
	}
	else { // ignore falling-falling and rising-rising

	}
	last = current;
	init = false;
}




GRM_state_t GRM_GetState(void){
	if (isOn()) return UNLOCKED;
	else return LOCKED;
}


void GRM_Unlock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start unlocking...", __func__);
	on();
}
void GRM_Lock(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Start locking...", __func__);
	off();
}

void GRM_Pulse(void) {
	HAPLogInfo(&kHAPLog_Default, "%s: Pulse for unlocking/locking...", __func__);
	pulse();
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
	
	pthread_mutex_lock(&blockMutex);
	vol = volume;
	pthread_mutex_unlock(&blockMutex);

}

void GRM_Ringcode(bool enable){
	HAPLogInfo(&kHAPLog_Default, "%s: Ringcode %s", __func__, enable?"enabled":"disabled");
	
	pthread_mutex_lock(&blockMutex);
	block = !enable;
	pthread_mutex_unlock(&blockMutex);

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



void GRM_ReadConfiguration(char * ptrValue){
	HAPLogInfo(&kHAPLog_Default, "%s: Reading configuration from file %s", __func__, ptrValue);

	struct json_object *jobj = json_object_from_file(ptrValue); // /home/pi/remote/dooropener.config



	HAPLogInfo(&kHAPLog_Default, "json object read from file and parsed:\n---\n%s\n---\n",	json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_SPACED | JSON_C_TO_STRING_PRETTY));
//	HAPLogInfo(&kHAPLog_Default, "json object read from file and parsed:\n---\n%s\n---\n", json_object_to_json_string_ext(jobj, JSON_C_TO_STRING_PLAIN));

	struct json_object *timeMinMax;

	json_object_object_get_ex(jobj, "anyMin", &timeMinMax);
	anyMin = json_object_get_int(timeMinMax);
	json_object_object_get_ex(jobj, "anyMax", &timeMinMax);
	anyMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "any = %ld ... %ld", anyMin, anyMax);

	json_object_object_get_ex(jobj, "shortMin", &timeMinMax);
	shortMin = json_object_get_int(timeMinMax);
	json_object_object_get_ex(jobj, "shortMax", &timeMinMax);
	shortMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "short = %ld ... %ld", shortMin, shortMax);

	json_object_object_get_ex(jobj, "longMin", &timeMinMax);
	longMin = json_object_get_int(timeMinMax);
	json_object_object_get_ex(jobj, "longMax", &timeMinMax);
	longMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "long = %ld ... %ld", longMin, longMax);

	json_object_object_get_ex(jobj, "veryLongMin", &timeMinMax);
	veryLongMin = json_object_get_int(timeMinMax);
	json_object_object_get_ex(jobj, "veryLongMax", &timeMinMax);
	veryLongMax = json_object_get_int(timeMinMax);
	HAPLogInfo(&kHAPLog_Default, "veryLong = %ld ... %ld", veryLongMin, veryLongMax);

	struct json_object *codeArray, *codeEntry;
	json_object_object_get_ex(jobj, "code", &codeArray);
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

	wiringPiSetupSys(); // gpio export 21 out
	wiringPiISR(trigger, INT_EDGE_BOTH, dispatchISR); // actually, this is a pthread!

	GRM_Lock(); // make sure it is locked initially


	int result = pthread_create(&mainThread, NULL, mainFunction, (void*) "Main thread started.");
	(void ) result;

}

void GRM_Deinititalize(void){

	stopThreads = true;
	pthread_join(mainThread, NULL);

}
