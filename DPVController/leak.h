#ifndef leak_h
#define leak_h

extern int leakSensorState;

void checkForLeak();

void leakSetup();

bool getLeakFront();
bool getLeakBack();


#endif