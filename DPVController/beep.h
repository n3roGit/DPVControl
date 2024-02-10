#ifndef beep_h
#define beep_h
#include "Arduino.h"

/**
* Code for making beeps. Surprisingly hard, because we avoid
* using delay() so that we do not block all execution while
* we beep. We achieve this by using running a loop that checks the current time with
* a timestamp to determine if we need to stop beeping or continue the next beep
* in a series of beeps. 
*
* Since we use global variables for this, we can only do one beep or beepSeuence at a time. Extra calls will override the current one. 
* We could instead queue them using a ... queue.
*/

/**
* Perform a beep for the given time. Works asynchronously. 
*/
void beep(long length_ms);
/**
* Perform a beep for each character in the string with a pause between beeps. 
* a "1" in the string will be a short beep. All others long beeps. 
* Works asynchronously(does not block).
*/
void beep(const String& sequence);

void beepLoop();
void BeepForLeak();

void beepSetup();

#endif