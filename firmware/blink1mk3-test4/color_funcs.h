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


// integer-only hsbtorgb
// from: http://web.mit.edu/storborg/Public/hsvtorgb.c
void hsbtorgb( uint8_t h, uint8_t s, uint8_t v,  uint8_t* nr, uint8_t* ng, uint8_t* nb )
{
  //uint8_t h = hsb[0];
  //  uint8_t s = hsb[1];
  //  uint8_t v = hsb[2];

    unsigned char region, fpart, p, q, t;
    uint8_t r,g,b;

    if(s == 0) {          // color is grayscale 
        r = g = b = v;
        return;
    }
    
    region = h / 43;      // make hue 0-5 
    fpart = (h - (region * 43)) * 6; // find remainder part, make it from 0-255 
    
    // calculate temp vars, doing integer multiplication 
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * fpart) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - fpart)) >> 8))) >> 8;
        
    // assign temp vars based on color cone region 
    switch(region) {
        case 0:   r = v; g = t; b = p; break;
        case 1:   r = q; g = v; b = p; break;
        case 2:   r = p; g = v; b = t; break;
        case 3:   r = p; g = q; b = v; break;
        case 4:   r = t; g = p; b = v; break;
        default:  r = v; g = p; b = q; break;
    }    
    *nr = r;
    *ng = g;
    *nb = b;
}

void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v,  uint8_t* r, uint8_t* g, uint8_t* b)
{
  uint8_t red_dest, grn_dest, blu_dest;
  
  if ( s == 0 ) {
    red_dest = grn_dest = blu_dest = v;
    return;
  }
  int16_t var_i = (h/16) / 42;
  int16_t var_h = ((h*6)/16)%16;
  int16_t var_1 = v/16 * ( 16 - s/16 );
  int16_t var_2 = v/16 * ( 16 - ((s/16) * var_h)/16 );
  int16_t var_3 = v/16 * ( 16 - ((s/16) * (16-var_h))/16 );

  if ( var_i == 0 ) {
    red_dest = v     ; grn_dest = var_3 ; blu_dest = var_1; }
  else if ( var_i == 1 ) {
    red_dest = var_2 ; grn_dest = v     ; blu_dest = var_1; }
  else if ( var_i == 2 ) {
    red_dest = var_1 ; grn_dest = v     ; blu_dest = var_3; }
  else if ( var_i == 3 ) {
    red_dest = var_1 ; grn_dest = var_2 ; blu_dest = v;     }
  else if ( var_i == 4 ) {
    red_dest = var_3 ; grn_dest = var_1 ; blu_dest = v;     }
  else                   {
    red_dest = v     ; grn_dest = var_1 ; blu_dest = var_2; }

  *r = red_dest; *g = grn_dest; *b = blu_dest;
}


#endif
