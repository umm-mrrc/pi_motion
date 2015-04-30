#ifndef PI_STEPPER_PINS_H
#define PI_STEPPER_PINS_H

// Stepper motor GPIO pin assignents on the Raspberry PI:
int stepPins[] = {0, 4};    // BCM_GPIO pins {17, 23}, Header {11, 16}
int dirPins[] = {1, 5};     // BCM_GPIO pins {18, 24}, Header {12, 18}
int llPins[] = {2, 6};      // BCM_GPIO pins {27, 25}, Header {13, 22}
int ulPins[] = {3, 7};      // BCM_GPIO pins {22,  4}, Header {15,  7}
int enablePins[] = {8, 9};  // BCM_GPIO pins { 2,  3}, Header { 3,  5}

#endif // PI_STEPPER_PINS_H
