#pragma once
#include "Arduino.h"

// Long display operations (scrolling messages, animations, static-notification
// holds) run to completion inside loop() and would otherwise block the network
// stack for their whole duration. These helpers let those blocking sections
// keep the background services alive cooperatively:
//
//   serviceBackground() - feed the watchdog and pump OTA / web / MQTT once.
//   serviceDelay(ms)    - like delay(ms), but services the background roughly
//                         every few milliseconds while it waits.
//
// serviceBackground() is a no-op for the network services until setup() has
// finished initializing them (guarded internally), so it is safe to call from
// the very first boot-time scroll.
void serviceBackground();
void serviceDelay(unsigned long ms);
