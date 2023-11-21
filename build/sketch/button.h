#line 1 "C:\\Users\\christoph.bubeck\\Documents\\GitHub\\DPVControl\\DPVController\\button.h"
#ifndef button_h
#define button_h

// Button Values
const int PRESSED = 0;
const int DEPRESSED = 1;

extern unsigned long lastActionTime;

void buttonSetup();
void buttonLoop();

#endif