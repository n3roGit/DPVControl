
/*
Its working but while beeping the esp doesnt response
*/
void beep(const String& sequence) {
  for (char c : sequence) {
    int toneDuration = (c == '1') ? 200 : 600;
    digitalWrite(PIN_BEEP, HIGH);
    float startMicros = micros();
    while (micros() - startMicros < toneDuration * 1000) {
      // Wait until the desired duration is reached
    }
    digitalWrite(PIN_BEEP, LOW);
    lastBeepTime = micros();
    while (micros() - lastBeepTime < 400000) {
      // Pause between tones
    }
  }
}

void BeepForLeak() {
  if (leakSensorState == 1 && micros() - lastBeepTime >= (10 * 1000 * 1000)) {  // Every 10 seconds
    beep("22222");                                                              // Here is the desired sequence for the sound
    log("WARNING LEAK", 22222, true);
    lastLeakBeepTime = micros();  // update the time of the last call
  }
}
void BeepForStandby() {
  if (motorState == MOTOR_STANDBY && micros() - lastStandbyBeepTime >= (1 * 60 * 1000000)) {
    beep("1");  // Hier die gewünschte Sequenz für den Ton
    log("still in standby", 1, true);
    lastStandbyBeepTime = micros();  // Update the time of the last call
  }
}
