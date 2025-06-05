#ifndef button_h
#define button_h

// Button Values
const int PRESSED = 0;
const int DEPRESSED = 1;

extern unsigned long lastActionTime;
extern int leftButtonState;
extern int rightButtonState;

void buttonSetup();
void buttonLoop();

#endif