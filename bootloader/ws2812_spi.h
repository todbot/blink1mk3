#ifndef WS2812_SPI_H
#define WS2812_SPI_H

// what makes up the data sent to one LED
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;
// (main code creates an "leds" array of rgb_t)


/**
 *
 */
void ws2812_setup()
{
  // make sure disabled first
  USART0->CTRL = USART_CMD_RXDIS | USART_CMD_TXDIS | USART_CMD_MASTERDIS
               | USART_CMD_RXBLOCKDIS | USART_CMD_TXTRIDIS | USART_CMD_CLEARTX
               | USART_CMD_CLEARRX;
  // reset everyone to know state
  USART0->CTRL      = _USART_CTRL_RESETVALUE;
  USART0->FRAME     = _USART_FRAME_RESETVALUE;
  USART0->TRIGCTRL  = _USART_TRIGCTRL_RESETVALUE;
  USART0->CLKDIV    = _USART_CLKDIV_RESETVALUE;
  USART0->IEN       = _USART_IEN_RESETVALUE;
  USART0->IFC       = _USART_IFC_MASK;
  USART0->ROUTE     = _USART_ROUTE_RESETVALUE;

  // set master mode
  USART0->CMD = USART_CMD_MASTEREN;
  
  // set sync mode and MostSigBitFirst
  USART0->CTRL |=
    (USART_CTRL_CLKPOL_IDLELOW | USART_CTRL_CLKPHA_SAMPLELEADING) | // usartClockMode0
    USART_CTRL_SYNC |
    USART_CTRL_MSBF;

  // set data bits, stop bits, parity
  USART0->FRAME =
    USART_FRAME_DATABITS_TWELVE |
    USART_FRAME_STOPBITS_DEFAULT |
    USART_FRAME_PARITY_DEFAULT;

  // set data rate (clkval)
  // usartInit.baudrate = 3000000;  // 3.0 MHz for SK6812mini (works for WS2812 too apparently)
  // clkdiv  = 2 * refFreq; // refFreq = 21000000
  // clkdiv += baudrate - 1;
  // clkdiv /= baudrate;
  // clkdiv -= 4;
  // clkdiv *= 64;
  // // Make sure we don't use fractional bits by rounding CLKDIV 
  // // up (and thus reducing baudrate, not increasing baudrate above 
  // // specified value). 
  // clkdiv += 0xc0;
  // clkdiv &= 0xffffff00;
  // 
  USART0->CLKDIV = 768;  // trust me on this
  
  // enable master and transmitter (no receiver tho)
  USART0->CMD = USART_CMD_MASTEREN | USART_CMD_TXEN;

  // from blink1 codebase
  // #define USART0_LOCATION USART_ROUTE_LOCATION_LOC4
  // #define USART0_TXPORT   gpioPortB
  // #define USART0_TXPIN    7

  // Set PB7 to be output (pin that blink(1) LEDs are on)
  GPIO->P[1].MODEL &= ~_GPIO_P_MODEL_MODE7_MASK;
  GPIO->P[1].MODEL |=   GPIO_P_MODEL_MODE7_PUSHPULL;
  // Route USART clock and USART TX to LOC0 (see defines above)
  USART0->ROUTE = USART_ROUTE_LOCATION_LOC4 | USART_ROUTE_TXPEN;

}

// Convert nibble to WS2812 bitstream
// @ 2.4MHz, 3 SPI bits for each ws2812 bit
// ws2812 0 bit = 0b100 
// ws2812 1 bit = 0b110
// => 12 bits carry 4 ws2812 bits
// To send one ws2812 byte, send two 12-bit transfers
// concept from: https://jeelabs.org/book/1450d/
static const uint16_t bits[] =
  {
   0b100100100100, // => 0b0000 in ws2812 bits
   0b100100100110, // => 0b0001 in ws2812 bits
   0b100100110100, // => 0b0010 in ws2812 bits
   0b100100110110, // => 0b0011 in ws2812 bits
   0b100110100100, // => 0b0100 in ws2812 bits
   0b100110100110, // => 0b0101 in ws2812 bits
   0b100110110100, // => 0b0110 in ws2812 bits
   0b100110110110, // => 0b0111 in ws2812 bits
   0b110100100100, // => 0b1000 in ws2812 bits
   0b110100100110, // => 0b1001 in ws2812 bits
   0b110100110100, // => 0b1010 in ws2812 bits
   0b110100110110, // => 0b1011 in ws2812 bits
   0b110110100100, // => 0b1100 in ws2812 bits
   0b110110100110, // => 0b1101 in ws2812 bits
   0b110110110100, // => 0b1110 in ws2812 bits
   0b110110110110, // => 0b1111 in ws2812 bits
  };


// note double-wide with TxDouble, sending 12-bit words
// equivalent to USART_TxDouble( USART0, x)
static inline void ws2812_spiSend(uint32_t data) {
  // Check that transmit buffer is empty
  while (!(USART0->STATUS & USART_STATUS_TXBL))
    ;
  USART0->TXDOUBLE = (uint32_t)data;
}

//
static inline void ws2812_sendByte (int value)
{
    ws2812_spiSend( bits[value >> 4] );
    ws2812_spiSend( bits[value & 0xF] );
}

/**********************************************************************
 * @brief Send LED data out via SPI, disables interrupts
 **********************************************************************/
static inline void ws2812_sendLEDs(rgb_t* leds, int num)
{
  //CORE_irqState_t is = CORE_EnterCritical();
  for( int i=0; i<num; i++ ) {
    // send out GRB data
    ws2812_sendByte( leds[i].g );
    ws2812_sendByte( leds[i].r );
    ws2812_sendByte( leds[i].b );
  }
  //CORE_ExitCritical(is);
  // delay at least 50usec before sending again
}


#endif
