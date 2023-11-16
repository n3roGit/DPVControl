#ifndef Blinker_h
#define Blinker_h

/**
*  This class helps us to "blink". Which means to turn something on(onfunction), wait a while, then turn something
* on. And we do not want to wait, so we do it asnchronously. 
*
*/
class Blinker {
public:
    Blinker(void (*onFunction)(), void (*offFunction)());
    void blink(long ms);
    void loop();

private:
    void (*turnOnFunction)();
    void (*turnOffFunction)();
    /**
    * Time in ms at which we stop blinking.
    */
    long stopAt; 
};

#endif
