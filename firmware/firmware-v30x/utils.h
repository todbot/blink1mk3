
#ifndef BLINK1_UTILS_H
#define BLINK1_UTILS_H

// some defs from Arduino
#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

// http://www.cse.yorku.ca/~oz/hash.html
uint32_t djb_hash(uint8_t *str, int len)
{
  uint32_t hash = 5381;
  for( int i=0; i< len; i++ ) {
    //hash = ((hash << 5) ^ hash) ^ str[i]; /* hash * 33 + c */
    hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
  }
  return hash;
}

#endif
