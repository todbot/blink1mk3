//
// color_funcs.h -- color sliding
//
// 2012-2018, Tod E. Kurt, http://todbot.com/blog/
//
//
// also see:
//   http://meyerweb.com/eric/tools/color-blend/ 
//
// current memory usage for one LED is:
// 6 bytes - curr100x
// 6 bytes - dest100x
// 6 bytes - step100x
// 1 byte  - stepcnt
// === 19 bytes
// => 8 LEDs = 8*19 = 152 bytes
// => 18 LEDS = 18*19 = 342 bytes
//
//

#ifndef COLOR_FUNCS_H
#define COLOR_FUNCS_H

#include <stdint.h>

#include "color_types.h"

// max number of LEDs
//#define nLEDs 18

// set the current color OF ALL LEDs
void rgb_setCurr( rgb_t* newcolor );

void rgb_setDestN( rgb_t* newcolor, int steps, int16_t ledn );

// set a new destination color
void rgb_setDest( rgb_t* newcolor, int steps, int16_t ledn  );

// call at every tick
void rgb_updateCurrent(void);

// set the current color OF ALL LEDs
void rgb_setCurr( rgb_t* newcolor )
{
    for( uint8_t i=0; i<nLEDs; i++ ) { 
        rgbfader_t* f = &fader[i];
        //rgbfader_t f = fader[i];
        f->curr100x.r = newcolor->r * 100;
        f->curr100x.g = newcolor->g * 100;
        f->curr100x.b = newcolor->b * 100;

        f->dest100x.r = f->curr100x.r;
        f->dest100x.g = f->curr100x.g;
        f->dest100x.b = f->curr100x.b;
        f->stepcnt = 0;

        //setRGBOutN( newcolor->r, newcolor->g, newcolor->b, i );
        setLED( newcolor->r, newcolor->g, newcolor->b, i );
    }
    //displayLEDs();
}

// set a 
void rgb_setDestN( rgb_t* newcolor, int steps, int16_t ledn )
{
    rgbfader_t* f = &fader[ledn];
    f->dest100x.r = newcolor->r * 100;
    f->dest100x.g = newcolor->g * 100;
    f->dest100x.b = newcolor->b * 100;

    f->stepcnt = steps + 1;

    f->step100x.r = (f->dest100x.r - f->curr100x.r) / steps;
    f->step100x.g = (f->dest100x.g - f->curr100x.g) / steps;
    f->step100x.b = (f->dest100x.b - f->curr100x.b) / steps;
}

// set a new destination color
// if ledn == 0 then set all
void rgb_setDest( rgb_t* newcolor, int steps, int16_t ledn  )
{
    if (ledn > 0) {
        rgb_setDestN(newcolor, steps, ledn - 1);
    } else {
        for (uint8_t i = 0; i < nLEDs; i++) {
            rgb_setDestN( newcolor, steps, i);
        }
    }
}

// call at every tick
void rgb_updateCurrent(void)
{
    for( uint8_t i=0; i<nLEDs; i++ ) {
        //rgbfader_t f = fader[i];
        rgbfader_t* f = &fader[i];
        if( f->stepcnt == 0 ) { // no more steps left
            continue;
        }
        f->stepcnt--;
        if( f->stepcnt ) {
            f->curr100x.r += f->step100x.r;
            f->curr100x.g += f->step100x.g;
            f->curr100x.b += f->step100x.b;
        } else {  // at destination, so set current
            f->curr100x.r = f->dest100x.r;
            f->curr100x.g = f->dest100x.g;
            f->curr100x.b = f->dest100x.b;
        }
        
        //setRGBOutN( f->curr100x.r/100, f->curr100x.g/100, f->curr100x.b/100, i );
        setLED( f->curr100x.r/100, f->curr100x.g/100, f->curr100x.b/100, i );
    }
    //displayLEDs();
}


#endif
