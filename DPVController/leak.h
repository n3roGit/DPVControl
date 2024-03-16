#ifndef leak_h
#define leak_h

extern bool hasLeak;

void checkForLeak();

void leakSetup();

bool getLeakFront();
bool getLeakBack();


#endif