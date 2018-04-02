#ifndef LEUART_TOMU_H
#define LEUART_TOMU

// Can define the following before including this file
#ifndef LEUART_LOCATION
// LOC1 is for tomu / blink1 
#define LEUART_LOCATION    LEUART_ROUTE_LOCATION_LOC1
#define LEUART_TXPORT      gpioPortB         // LEUART transmission port 
#define LEUART_TXPIN       13                // LEUART transmission pin  
#define LEUART_RXPORT      gpioPortB         // LEUART reception port    
#define LEUART_RXPIN       14                // LEUART reception pin     
#endif

// Write a single character out LEUART0
int write_char(int c) {
  while (!(LEUART0->STATUS & LEUART_STATUS_TXBL))
        ;
  LEUART0->TXDATA = (uint32_t)c & 0xffUL;
  return 0;
}

// Write a null-terminated string out LEUART0
int write_str(const char *s) {
  while (*s)
    write_char(*s++);
  return 0;
}

// Set up LEUART0 on Tomu. LEUART0 is only 9600bps.
void setupLeuart(void) {  
  LEUART_Init_TypeDef leuart_init = LEUART_INIT_DEFAULT;

  // Enable peripheral clocks 
  CMU_ClockEnable(cmuClock_HFPER, true);
  // Enable CORE LE clock in order to access LE modules 
  CMU_ClockEnable(cmuClock_CORELE, true);
  // Select LFXO for LEUARTs (and wait for it to stabilize) 
  // set to internal RC 32kHz oscillator (normally its LFXO in docs, but no ext LF crystal)
  CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFRCO); 
  CMU_ClockEnable(cmuClock_LEUART0, true);
  // Set location of TX & RX pins and enable them
  LEUART0->ROUTE = LEUART_LOCATION |
                   LEUART_ROUTE_TXPEN |
                   LEUART_ROUTE_RXPEN;
  // Enable the pins 
  GPIO_PinModeSet(LEUART_TXPORT, LEUART_TXPIN, gpioModePushPull, 1);
  GPIO_PinModeSet(LEUART_RXPORT, LEUART_RXPIN, gpioModeInput, 0);
  // Configure the LEUART
  LEUART_Init(LEUART0, &leuart_init);
}

#endif
