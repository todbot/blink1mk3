#ifndef COLOR_TYPES_H
#define COLOR_TYPES_H

#include <stdint.h>

// RGB triplet of 8-bit vals for input/output use
typedef struct {
    uint8_t g;
    uint8_t r;
    uint8_t b;
} rgb_t;

// RGB triplet unsigned ints for internal use of 100x scale
// used instead of floating point
typedef struct {
    int16_t g;
    int16_t r;
    int16_t b;
} rgbint_t;

typedef struct {
    rgbint_t dest100x;  // the eventual destination color we want to hit
    rgbint_t step100x;  // the amount of to move each tick
    rgbint_t curr100x;  // the current color, times 10 (to lessen int trunc issue)
    int16_t stepcnt;
} rgbfader_t;

typedef struct {
    rgb_t color;
    uint16_t dmillis; // hundreths of a sec
    uint8_t ledn;     // number of led, or 0 for all
} patternline_t;

// note: this is for doing ROM size address calcs that can't use sizeof()
//#define patternline_size (3 + 2 + 1)

// what is this for exactly? only used in off()?
#define setRGBt(rgbt,x,y,z) { rgbt.r=x; rgbt.g=y; rgbt.b=z; }


#endif
