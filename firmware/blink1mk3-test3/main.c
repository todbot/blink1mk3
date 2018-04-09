/*********************************************************************************************
 *
 * blink1mk3 firmware -- EFM32HG-based, based off work by Tomu project (github.com/im-tomu)
 *
 * 2018 Tod E. Kurt, http://todbot.com/blog/
 *
 * Differences from blink1mk3-test1:
 * - responds correctly to ReportId 2 (64-bytes), testable with "blink1-tool -i 2 --testtest" 
 * - code reformatted heavily to pull ws2812 driver and color_funcs into separate files
 * - code works on Tomu-class boards
 *
 *
 ********************************************************************************************/

#include <stdint.h>
#include <stdbool.h>

#include "capsense.h"
#include "usbconfig.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_device.h"
#include "em_emu.h"
#include "em_gpio.h"
#include "em_usb.h"
#include "em_wdog.h"
#include "em_system.h"
#include <em_leuart.h>
#include <em_usart.h>
#include <em_core.h>
#include <em_msc.h>

#define BOARD_TYPE_BLINK1MK3
//#define BOARD_TYPE_TOMU 
//#define BOARD_TYPE_EFM32HGDEVKIT

//#define DEBUG_HANDLEMESSAGE

#include "toboot.h"

#include "leuart.h"
#include "tinyprintf.h"
#include "ws2812_spi.h"
#include "descriptors.h"
#include "color_types.h"


#define blink1_version_major '3'
#define blink1_version_minor '1'


extern struct toboot_runtime toboot_runtime;
/* Declare support for Toboot V2 */
/* To enable this program to run when you first plug in Tomu, pass
 * TOBOOT_CONFIG_FLAG_AUTORUN to this macro.  Otherwise, leave the
 * configuration value at 0 to use the defaults.
 */
//TOBOOT_CONFIGURATION(0);
TOBOOT_CONFIGURATION( TOBOOT_CONFIG_FLAG_AUTORUN );



// max number of LEDs
#define nLEDs 18
// array of LED data (sent to LEDs)
rgb_t leds[nLEDs];  
// foward decl for color_funcs.h (FIXME: make color_funcs a real lib)
static void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t n);
#define setLEDsAll(r,g,b) { setLED(r,g,b, 255); } // 255 means all
static inline void displayLEDs(void);

// global which is active LED
uint8_t ledn;
// allocate faders
rgbfader_t fader[nLEDs];

#include "color_funcs.h"  // included here becuase it needs setLed()


// used when sprintf()-ing to leuart
char dbgstr[30];

// for tiny printf
void myputc ( void* p, char c) {
  (void)p;
  write_char(c);
}

// number of entries a color pattern can contain
#define patt_max 32
#define patt_maxflash 16

uint8_t playpos   = 0; // current play position
uint8_t playstart = 0; // start play position
uint8_t playend   = patt_max; // end play position
uint8_t playcount = 0; // number of times to play loop, or 0=infinite
uint8_t playing; // playing values: 0 = off, 1 = normal, 2 == playing from powerup playing=3 direct led addressing FIXME: this is dumb
uint8_t do_pattern_write = 0;

patternline_t ptmp;  // temp pattern holder
rgb_t ctmp;      // temp color holder
uint16_t ttmp;   // temp time holder
uint8_t ledn;    // temp ledn holder

// in-memory copy of non-volatile pattern
patternline_t pattern[patt_max]; 

/*
 * non-volatile color pattern
 *
 * 6 bytes / patternline = 21.3 pattern lines (fade is 2bytes)
 *
 * FLASH_PAGE_SIZE = 1024 in Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Include/efm32hg309f64.h
 * FLASH_SIZE = 65536 (64k)
 * .patternSection is in flash at address 0xf800 (64k - (2*1k)) 
 */
const patternline_t patternFlash[patt_maxflash]  __attribute__((section (".patternSection"))) = 
{
    //    G     R     B    fade ledn
    { { 0x00, 0xff, 0x00 },  50, 1 }, // 0  red A
    { { 0x00, 0xff, 0x00 },  50, 2 }, // 1  red B
    { { 0x00, 0x00, 0x00 },  50, 0 }, // 2  off both
    { { 0xff, 0x00, 0x00 },  50, 1 }, // 3  grn A
    { { 0xff, 0x00, 0x00 },  50, 2 }, // 4  grn B
    { { 0x00, 0x00, 0x00 },  50, 0 }, // 5  off both
    { { 0x00, 0x00, 0xff },  50, 1 }, // 6  blu A
    { { 0x00, 0x00, 0xff },  50, 2 }, // 7  blu B
    { { 0x00, 0x00, 0x00 },  50, 0 }, // 8  off both
    { { 0x80, 0x80, 0x80 }, 100, 0 }, // 9  half-bright, both LEDs
    { { 0x00, 0x00, 0x00 }, 100, 0 }, // 10 off both
    { { 0xff, 0xff, 0xff },  50, 1 }, // 11 white A
    { { 0x00, 0x00, 0x00 },  50, 1 }, // 12 off A
    { { 0xff, 0xff, 0xff },  50, 2 }, // 13 white B
    { { 0x00, 0x00, 0x00 }, 100, 2 }, // 14 off B
    { { 0x00, 0x00, 0x00 }, 100, 0 }, // 15 off everyone
};

/*
 * "Notes" are user-writable and -readable blobs of data
 *
 * Note size = 100
 * Note count = 10
 * Total size = 1000 < FLASH_PAGE_SIZE
 */
uint8_t notesdata[FLASH_PAGE_SIZE] ;
//uint32_t *notesFlashStartAddress = (uint32_t *)(FLASH_SIZE - FLASH_PAGE_SIZE);
uint32_t* notesFlashStartAddress  __attribute__ ((used, section(".userNoteSection"))) ;

#define NOTE_SIZE 100
#define NOTE_COUNT 10


// The uptime in milliseconds, maintained by the SysTick timer.
volatile uint32_t uptime_millis;

// next time led_update should run
const uint32_t led_update_millis = 10;  // tick msec
uint32_t led_update_next;

uint32_t pattern_update_next;
uint16_t serverdown_millis;
uint32_t serverdown_update_next;

uint32_t last_misc_millis;

// set by 'G' "gobootload" command
bool shouldRebootToBootloader = false;

// for sending back HID Descriptor in setupCmd
static void  *hidDescriptor = NULL;

// The report packet received from the host
// could be REPORT_COUNT or REPORT2_COUNT long
// first byte is reportId
static uint8_t  inbuf[REPORT2_COUNT];

// The report packet to send to the host 
// generally it's a copy of the last report received, then modified
SL_ALIGN(4)
static uint8_t reportToSend[REPORT2_COUNT] SL_ATTRIBUTE_ALIGN(4);

// forward decl for callbacks struct
int setupCmd(const USB_Setup_TypeDef *setup);
void stateChange(USBD_State_TypeDef oldState, USBD_State_TypeDef newState);

/* Define callbacks that are called by the USB stack on different events. */
static const USBD_Callbacks_TypeDef callbacks =
{
  .usbReset       = NULL,         // Called whenever USB reset signalling is detected  
  .usbStateChange = stateChange,  // Called whenever the device change state.  
  .setupCmd       = setupCmd,     // Called on each setup request received from host. 
  .isSelfPowered  = NULL,         // Called when the device stack needs to query if the device is currently self- or bus-powered. 
  .sofInt         = NULL          // Called at each SOF interrupt. If NULL, device stack will not enable the SOF interrupt. 
};

/* Fill the init struct. This struct is passed to USBD_Init() in order 
 * to initialize the USB Stack */
static const USBD_Init_TypeDef initstruct =
{
  .deviceDescriptor    = &deviceDesc,
  .configDescriptor    = configDesc,
  .stringDescriptors   = strings,
  .numberOfStrings     = sizeof(strings)/sizeof(void*),
  .callbacks           = &callbacks,
  .bufferingMultiplier = bufferingMultiplier,
  .reserved            = 0
};


/* This functions is injected into the Interrupt Vector Table, and will be
 * called whenever the SysTick timer fires (whose interval is configured inside
 * main() further below). */
void SysTick_Handler() {
  uptime_millis++;
}

/* simple delay() */
static void SpinDelay(uint32_t millis) {
  // Calculate the time at which we need to finish "sleeping".
  uint32_t sleep_until = uptime_millis + millis;

  // Spin until the requested time has passed.
  while (uptime_millis < sleep_until);
}

static void off();
/* Set Toboot magic value to force bootloader and reset */
static void rebootToBootloader() {
  toboot_runtime.magic = TOBOOT_FORCE_ENTRY_MAGIC;
  setLEDsAll(0,0,0);
  displayLEDs();
  USBD_Disconnect();
  USBTIMER_DelayMs(100); 
  NVIC_SystemReset();    
}

/**********************************************************************
 * 
 **********************************************************************/
static void updateMisc()
{
  if( (uptime_millis - last_misc_millis) > 500 ) {
    last_misc_millis = uptime_millis;
    write_char('.');

    if( shouldRebootToBootloader ) {
      //write_str("rebooting to bootloader...\n");
      rebootToBootloader();
      //write_str("why am I still here\n");
    }
    
  }
  
  /*
    // Capture/sample the state of the capacitive touch sensors.
    CAPSENSE_Sense();

    // Analyse the sample, and if the touch-pads on the green LED side is
    // touched, rapid blink the green LED ten times.
    if (CAPSENSE_getPressed(BUTTON0_CHANNEL) &&
        !CAPSENSE_getPressed(BUTTON1_CHANNEL)) {
      int i;
      for (i = 10; i > 0; i--) {
        GPIO_PinOutClear(gpioPortA, 0);
        SpinDelay(100);
        GPIO_PinOutSet(gpioPortA, 0);
        SpinDelay(100);
      }
    // Analyse the same sample, and if the touch-pads on the red LED side is
    // touched, rapid blink the red LED ten times.
    } else if (CAPSENSE_getPressed(BUTTON1_CHANNEL) &&
               !CAPSENSE_getPressed(BUTTON0_CHANNEL)) {
      int i;
      for (i = 10; i > 0; i--) {
        GPIO_PinOutClear(gpioPortB, 7);
        SpinDelay(100);
        GPIO_PinOutSet(gpioPortB, 7);
        SpinDelay(100);
      }
    }
    SpinDelay(500);
  */
}

// -------- LED & color pattern handling -------------------------------------
//

/**********************************************************************
 * @brief Send LED data out to LEDs
 **********************************************************************/
static inline void displayLEDs(void)
{
  ws2812_sendLEDs( leds, nLEDs );    // ws2811_showRGB();
}
/*
 * set blink1 to not playing, and no LEDs lit
 */
static void off(void)
{
    playing = 0;
    setRGBt(ctmp, 0,0,0);      // starting color
    rgb_setCurr( &ctmp );  // FIXME: better way to do this?
}

/*
 * Start playing the light pattern
 * playing values: 0 = off, 1 = normal, 2 == playing from powerup
 */
static void startPlaying( void )
{
    playpos = playstart;
    pattern_update_next = uptime_millis; // millis(); // now;
    //pattern_update_next = 0; // invalidate it so plays immediately
    //memcpy( pattern, patternflash, sizeof(patternline_t)*patt_max);
}

/**********************************************************************
 * @brief Set the color of a particular LED, or all of them
 **********************************************************************/
static void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t n)
{
    if (n == 255) { // all of them  // FIXME: look into why 255
        for (int i = 0; i < nLEDs; i++) {
            leds[i].r = r;  leds[i].g = g; leds[i].b = b;
        }
    }
    else {    // else just one LED, not all of them
        leds[n].r = r; leds[n].g = g; leds[n].b = b;
    }
}

/**********************************************************************
 * updateLEDs() is the main user-land function that:
 * - periodically calls the rgb fader code to fade any actively moving colors
 * - controls sequencing of a light pattern, if playing
 * - triggers pattern playing on USB disconnect
 *
 **********************************************************************/
static void updateLEDs(void)
{
    uint32_t now = uptime_millis;

    // update LEDs every led_update_millis
    if( (long)(now - led_update_next) > 0 ) {
        led_update_next += led_update_millis;

        rgb_updateCurrent();  // playing=3 => direct LED addressing (not anymore)
        displayLEDs();
#if 0
        // check for non-computer power up
        if( !usbHasBeenSetup ) {
            if( !playing && now > 500 ) {  // 500 msec wait
                playing = 2;
                startPlaying();
            }
        }
        else {  // else usb is setup...
            if( playing == 2 ) { // ...but we started a powerup play, so reset
                off();
            }
        }
#endif
    } // if led_update_next

    // playing light pattern
    if( playing ) {
        if( (long)(now - pattern_update_next) > 0  ) { // time to get next line
            ctmp = pattern[playpos].color;
            ttmp = pattern[playpos].dmillis;
            ledn = pattern[playpos].ledn;
            if( ttmp == 0 && ctmp.r == 0 && ctmp.g == 0 && ctmp.b == 0) {
                // skip lines set to zero
            } else {
                rgb_setDest( &ctmp, ttmp, ledn );
            }
            playpos++;
            if( playpos == playend ) {
                playpos = playstart; // loop the pattern
                playcount--;
                if( playcount == 0 ) {
                    playing=0; // done!
                }
                else if(playcount==255) {
                    playcount = 0; // infinite playing
                }
            }
            pattern_update_next += ttmp*10;
        }
    } // playing
    
}


/*
 * Save all user notes to flash.
 */
static void notesSaveAll()
{
  MSC_Init();  // done only in setup?
  MSC_ErasePage(notesFlashStartAddress); // erase first
  MSC_WriteWord(notesFlashStartAddress, &notesdata, FLASH_PAGE_SIZE);
}

/*
 * Load all user notes from flash to RAM
 */
static void notesLoadAll()
{
  memcpy( notesdata, notesFlashStartAddress, FLASH_PAGE_SIZE);
}

/*
 * Write a note to RAM.
 * - Uses global 'inbuf'
 */
static void noteWrite(uint8_t pos )
{
  if( pos >= NOTE_COUNT ) {
    // error
    return;
  }
  memcpy( notesdata + (pos*NOTE_SIZE), inbuf+3, NOTE_SIZE);

  notesSaveAll(); // FIXME: don't do this every write
}

/*
 * Read a user note from RAM into 'reportToSend' buffer.
 * - Uses global 'reportToSend'
 */
static void noteRead(uint8_t pos)
{
  memcpy( reportToSend+3, notesdata + (pos*NOTE_SIZE), NOTE_SIZE );
}


/**********************************************************************
 * @brief Modify 'iSerialNumber' string to be based on chip's unique Id
 **********************************************************************/
static void makeSerialNumber()
{
  uint64_t uniqid = SYSTEM_GetUnique(); // is 64-bit but we'll only use lower 32-bits
  uint8_t* uniqp = (uint8_t*) &uniqid;
  
  // Hack to map ASCII hex digits to UTF-16LE
  // this way we don't need sprintf
  static const char table[] = "0123456789abcdef";
  iSerialNumber[2]  = '3'; // mk3
  iSerialNumber[4]  = table[ uniqp[0] >> 4 ];
  iSerialNumber[6]  = table[ uniqp[0] & 0x0f ];
  iSerialNumber[8]  = table[ uniqp[1] >> 4 ];
  iSerialNumber[10] = table[ uniqp[1] & 0x0f ];
  iSerialNumber[12] = table[ uniqp[2] >> 4 ];
  iSerialNumber[14] = table[ uniqp[2] & 0x0f ];
  iSerialNumber[16] = table[ uniqp[3] >> 4 ];
}

/**********************************************************************
 * 
 **********************************************************************/
int main()
{
  // Runs the Silicon Labs chip initialisation stuff, that also deals with
  // errata (implements workarounds, etc).
  CHIP_Init();
  
  // Disable the watchdog that the bootloader started.
  WDOG->CTRL = 0;
  
  //CMU_ClockEnable(cmuClock_DMA, true);  
  CMU_ClockEnable(cmuClock_USART0, true);  
  // Switch on the clock for GPIO. Even though there's no immediately obvious
  // timing stuff going on beyond the SysTick below, it still needs to be
  // enabled for the GPIO to work.
  CMU_ClockEnable(cmuClock_GPIO, true);
  
  // Sets up and enable the `SysTick_Handler' interrupt to fire once every 1ms.
  // ref: http://community.silabs.com/t5/Official-Blog-of-Silicon-Labs/Chapter-5-MCU-Clocking-Part-2-The-SysTick-Interrupt/ba-p/145297
  if (SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000)) {
    // Something went wrong.
    while (1);
  }

  setupLeuart();

  ws2812_setupSpi();
  
  write_str("blink1mk3-test2 startup...\n");

  // load pattern from flash to RAM
  memset( pattern, 0, sizeof(patternline_t)*patt_max); // zero out just in case
  memcpy( pattern, patternFlash, sizeof(patternline_t)*patt_maxflash);

#if 0
  notesLoadAll();
#endif

  // Enable the capacitive touch sensor.
  // Remember, this consumes TIMER0 and TIMER1, so those are off-limits to us.
  //CAPSENSE_Init();

  makeSerialNumber();
  
  hidDescriptor = (void*) USBDESC_HidDescriptor; // FIXME
  
  // Enable the USB controller.
  // Remember, this consumes TIMER2 as per -DUSB_TIMER=USB_TIMER2 in Makefile
  // because TIMER0 & TIMER1 are already taken by the capacitive touch sensors.
  USBD_Init(&initstruct);

  // When using a debugger it is practical to uncomment the following three
  // lines to force host to re-enumerate the device.
#if 0
  USBD_Disconnect();      
  USBTIMER_DelayMs(1000); 
  USBD_Connect();         
#endif

  // startup white fadeout
  for( uint8_t i=255; i>0; i-- ) {
    SpinDelay(1);
    uint8_t j = i>>4;      // not so bright, please
    setLEDsAll(j,j,j);
    displayLEDs();
  }
  
  //startPlaying();  // to load pattern up
  off();
  displayLEDs(); // why this here, to prime the system?

  while(1) {

    updateLEDs();
    updateMisc();
    
  }
}


/**
 * handleMessage(char* inbuf) -- main command router
 *
 * inbuf[] is 8 bytes long
 *  byte0 = report-id
 *  byte1 = command
 *  byte2..byte7 = args for command
 *
 * Available commands:
 *    - Fade to RGB color       format: { 1, 'c', r,g,b,     th,tl, n }
 *    - Set RGB color now       format: { 1, 'n', r,g,b,       0,0, n } (*)
 *    - Read current RGB color  format: { 1, 'r', n,0,0,       0,0, n } (2)
 *    - Serverdown tickle/off   format: { 1, 'D', on,th,tl,  st,sp,ep } (*)
 *    - PlayLoop                format: { 1, 'p', on,sp,ep,c,    0, 0 } (2)
 *    - Playstate readback      format: { 1, 'S', 0,0,0,       0,0, 0 } (2)
 *    - Set color pattern line  format: { 1, 'P', r,g,b,     th,tl, p }
 *    - Save color patterns     format: { 1, 'W', 0,0,0,       0,0, 0 } (2)
 *    - read color pattern line format: { 1, 'R', 0,0,0,       0,0, p }
 *    - Set ledn                format: { 1, 'l', n,0,0,       0,0, 0 } (2+)
 *    - Read EEPROM location    format: { 1, 'e', ad,0,0,      0,0, 0 } (1)
 *    - Write EEPROM location   format: { 1, 'E', ad,v,0,      0,0, 0 } (1)
 *    - Get version             format: { 1, 'v', 0,0,0,       0,0, 0 }
 *    - Test command            format: { 1, '!', 0,0,0,       0,0, 0 }
 *    - Write 100-byte note     format: { 1, 'F', noteid, data0 ... data99 } (3)
 *    - Read 100-byte note      format: { 1, 'F', noteid, data0 ... data99 } (3)
 *    - Go to bootloader        format: { 1, 'G', 'o','B','o','o','t',0 } (3)
 *
 *  Fade to RGB color        format: { 1, 'c', r,g,b,      th,tl, ledn }
 *  Set RGB color now        format: { 1, 'n', r,g,b,        0,0, ledn }
 *  Play/Pause, with pos     format: { 1, 'p', {1/0},pos,0,  0,0,    0 }
 *  Play/Pause, with pos     format: { 1, 'p', {1/0},pos,endpos, 0,0,0 }
 *  Write color pattern line format: { 1, 'P', r,g,b,      th,tl,  pos }
 *  Read color pattern line  format: { 1, 'R', 0,0,0,        0,0, pos }
 *  Server mode tickle       format: { 1, 'D', {1/0},th,tl, {1,0},0, 0 }
 *  Get version              format: { 1, 'v', 0,0,0,        0,0, 0 }
 *
 **/
static void handleMessage(uint8_t reportId)
{
#ifdef DEBUG_HANDLEMESSAGE
  sprintf(dbgstr, "%d:%x,%x,%x,%x,%x,%x,%x,%x\n", reportId,
          inbuf[0],inbuf[1],inbuf[2],inbuf[3],inbuf[4],inbuf[5],inbuf[6],inbuf[7] );
  write_str(dbgstr);
#endif
  
  // pre-load response with request, contains report id
  uint8_t count = (reportId==REPORT_ID) ? REPORT_COUNT : REPORT2_COUNT;
  memcpy( (void*)reportToSend, (void*)inbuf, count);

  uint8_t rId;
  uint8_t cmd;
  rgb_t c; // we need this for many commands so pre-parse it
  rId = inbuf[0];
  cmd = inbuf[1];
  c.r = inbuf[2];
  c.g = inbuf[3];
  c.b = inbuf[4];

  //
  // Fade to RGB color - { 1,'c', r,g,b, th,tl, ledn }
  //   where t = number of 10msec ticks
  if(      cmd == 'c' ) {
    uint16_t dmillis = (inbuf[5] << 8) | inbuf[6];
    uint8_t ledn = inbuf[7];          // which LED to address
    playing = 0;
    rgb_setDest(&c, dmillis, ledn);
  }
  //
  // set RGB color immediately  - {1,'n', r,g,b, 0,0,0 }
  //
  else if( cmd == 'n' ) {
    uint8_t iledn = inbuf[7];          // which LED to address
     playing = 0;
    if( iledn > 0 ) {
      playing = 3;                   // FIXME: wtf non-semantic 3
      setLED( c.r, c.g, c.b, iledn ); // FIXME: no fading
    }
    else {
      rgb_setDest( &c, 0, 0 );
      rgb_setCurr( &c );  // FIXME: no LED arg
    }    
  }
  //
  //  Read current color        - { 1,'r', 0,0,0,   0,0, 0}
  //
  else if( cmd == 'r' ) {
    uint8_t iledn = inbuf[7];          // which LED to address
    if( iledn > 0 ) iledn--;
    reportToSend[2] = leds[ledn].r;
    reportToSend[3] = leds[ledn].g;
    reportToSend[4] = leds[ledn].b;
    reportToSend[5] = 0;
    reportToSend[6] = 0;
    reportToSend[7] = iledn;
  }
  //
  //  Play/Pause, with pos     - { 1, 'p', {1/0},startpos,endpos,  0,0, 0 }
  //
  else if( cmd == 'p' ) { 
    playing   = inbuf[2];
    playstart = inbuf[3];
    playend   = inbuf[4];
    playcount = inbuf[5];
    if( playend == 0 || playend > patt_max )
      playend = patt_max;
    else playend++;  // so that it's equivalent to patt_max, if you know what i mean
    startPlaying();
  }
  //
  // Play state readback      - { 1, 'S', 0,0,0, 0,0,0 }
  //   resopnse format:
  //
  else if( cmd == 'S' ) {
    reportToSend[2] = playing;
    reportToSend[3] = playstart;
    reportToSend[4] = playend;
    reportToSend[5] = playcount;
    reportToSend[6] = playpos;
    reportToSend[7] = 0;
  }
  //
  // Write color pattern line  - {1,'P', r,g,b, th,tl, pos}
  //
  else if( cmd == 'P' ) {
    // was doing this copy with a cast, but broke it out for clarity
    ptmp.color.r = inbuf[2];
    ptmp.color.g = inbuf[3];
    ptmp.color.b = inbuf[4];
    ptmp.dmillis = ((uint16_t)inbuf[5] << 8) | inbuf[6];
    ptmp.ledn    = ledn;
    uint8_t pos  = inbuf[7];
    if( pos >= patt_max ) pos = 0;  // just in case
    // save pattern line to RAM
    memcpy( &pattern[pos], &ptmp, sizeof(patternline_t) );
    //if( pos == (patt_max-1) ) {  // NOTE: writing last position causes write to flash
    //do_pattern_write = 1;
    //writePatternFlash();
    //}    
  }
  //
  // Read color pattern entry - {1,'R', 0,0,0, 0,0, pos}
  //
  else if( cmd == 'R' ) {
    uint8_t pos = inbuf[7];
    if( pos >= patt_max ) pos = 0;
    patternline_t patt = pattern[pos];
    reportToSend[2] = patt.color.r;
    reportToSend[3] = patt.color.g;
    reportToSend[4] = patt.color.b;
    reportToSend[5] = (patt.dmillis >> 8);
    reportToSend[6] = (patt.dmillis & 0xff);
    reportToSend[7] = patt.ledn;
  }
  //
  // Write color pattern to flash memory: { 1, 'W', 0x55,0xAA, 0xCA,0xFE, 0,0}
  //
  else if( cmd == 'W' ) {
    
  }
  //
  // Set ledn : { 1, 'l', n, 0...}
  //
  else if( cmd == 'l' ) { 
    ledn = inbuf[2];
  }
  //
  //  Server mode tickle      - { 1, 'D', {1/0}, th,tl, {1,0}, sp, ep }
  //
  else if( cmd == 'D' ) {
    
  }
  //
  //  Get version               format: { 1, 'v', 0,0,0,        0,0, 0 }
  //
  else if( cmd == 'v' ) {
    //GPIO_PinOutSet(gpioPortF, 5);  // debug
    reportToSend[3] = blink1_version_major;
    reportToSend[4] = blink1_version_minor;
  }
  //
  // Go to bootloader
  //
  else if( cmd == 'G' ) {
    if( inbuf[2] == 'o' && inbuf[3] == 'B' && inbuf[4] == 'o' && inbuf[5] == 'o' && inbuf[6] == 't' ) {
      shouldRebootToBootloader = true;
      reportToSend[2] = 'O'; // send acknowledge
      reportToSend[3] = 'B';
      reportToSend[4] = 'O';
      reportToSend[5] = 'O';
      reportToSend[6] = 'T'; 
    }
  }
  //
  //
  //
  else if( cmd == '!' ) {  // testtest
    reportToSend[2] = 0x55;
    reportToSend[3] = 0xAA;
    reportToSend[4] = rId; //(uint8_t)(uptime_millis >> 24);
    reportToSend[5] = (uint8_t)(uptime_millis >> 16);
    reportToSend[6] = (uint8_t)(uptime_millis >> 8);
    reportToSend[7] = (uint8_t)(uptime_millis >> 0);

    //test2Flash(); // FIXME: this will get removed
  }
  else if( cmd == 'f' && rId == 2 ) {  // read note
    uint8_t noteid = inbuf[2];
    noteRead( noteid ); // fills out reportToSend
  }
  else if( cmd == 'F' && rId == 2 ) { // write note
    uint8_t noteid = inbuf[2];
    noteWrite( noteid ); // reads from global inbuf+3
  }
  
}

/**************************************************************************//**
 * @brief
 *   Callback function called when the data stage of a USB_HID_SET_REPORT
 *   setup command has completed.
 *
 * @param[in] status    Transfer status code.
 * @param[in] xferred   Number of bytes transferred.
 * @param[in] remaining Number of bytes not transferred.
 *
 * @return USB_STATUS_OK.
 *****************************************************************************/
static int ReportReceived(USB_Status_TypeDef status, uint32_t xferred, uint32_t remaining)
{
  (void) remaining;
  
  if ((status   == USB_STATUS_OK) &&
      (xferred  == REPORT_COUNT) ) {
    //      && (setReportFunc != NULL) ) {
    //setReportFunc( (uint8_t)tmpBuffer);
    handleMessage(REPORT_ID);
  }
  
  return USB_STATUS_OK;
}

/*****************************************************************************
 * when report id 2 is received
 *****************************************************************************/
static int Report2Received(USB_Status_TypeDef status, uint32_t xferred, uint32_t remaining)
{
  (void) remaining;
  //(void) xferred;  (void) status;
  
  if ((status   == USB_STATUS_OK) &&
      (xferred  == REPORT2_COUNT) ) {
    //GPIO_PinOutSet(gpioPortF, 4);
    handleMessage(REPORT2_ID);
  }
  
  return USB_STATUS_OK;
}

/**************************************************************************//**
 * @brief
 *   Handle USB setup commands. Implements HID class specific commands.
 *   This function must be called each time the device receive a setup command.
 *
 *
 * @param[in] setup Pointer to the setup packet received.
 *
 * @return USB_STATUS_OK if command accepted,
 *         USB_STATUS_REQ_UNHANDLED when command is unknown. In the latter case
 *         the USB device stack will handle the request. 
 ****************************************************************************/
int setupCmd(const USB_Setup_TypeDef *setup)
{
  STATIC_UBUF(hidDesc, USB_HID_DESCSIZE);

  int retVal = USB_STATUS_REQ_UNHANDLED;

  //setup->bmRequestType == 0x81) {
  if (  (setup->Type         == USB_SETUP_TYPE_STANDARD)
        && (setup->Direction == USB_SETUP_DIR_IN)
        && (setup->Recipient == USB_SETUP_RECIPIENT_INTERFACE)    ) {
    
    /* A HID device must extend the standard GET_DESCRIPTOR command   */
    /* with support for HID descriptors.                              */
    switch (setup->bRequest) {
    case GET_DESCRIPTOR:
      
      if ( (setup->wValue >> 8) == USB_HID_REPORT_DESCRIPTOR ) {
        USBD_Write(0, (void*)MyHIDReportDescriptor,
                   SL_MIN(sizeof(MyHIDReportDescriptor), setup->wLength),
                   NULL);
        retVal = USB_STATUS_OK;
      }
      else if ( (setup->wValue >> 8) == USB_HID_DESCRIPTOR ) {
        /* The HID descriptor might be misaligned ! */
        memcpy(hidDesc, hidDescriptor, USB_HID_DESCSIZE);
        USBD_Write(0, hidDesc, SL_MIN(USB_HID_DESCSIZE, setup->wLength),
                   NULL);
        retVal = USB_STATUS_OK;
      }
      break;
    }
  }
  else {
    
    if ( (setup->Type         == USB_SETUP_TYPE_CLASS)
         && (setup->Recipient == USB_SETUP_RECIPIENT_INTERFACE) ) { 
      // && (setup->wIndex    == HIDKBD_INTERFACE_NO)    ) {
      
      // Implement the necessary HID class specific commands.           
      switch ( setup->bRequest ) {
        
      case USB_HID_SET_REPORT:           // 0x09, receive data from host
        /*
          if ( ( (setup->wValue >> 8)      == 3)              // FEATURE report 
          if ( ( (setup->wValue >> 8)      == 2)              // OUTPUT report 
          && ( (setup->wValue & 0xFF) == 1)              // Report ID  
          && (setup->wLength          == 1)              // Report length 
          && (setup->Direction        != USB_SETUP_DIR_OUT)    ) { // 
        */
        
        if( (setup->wValue & 0xFF) == REPORT_ID ) { 
          USBD_Read(0, (void*)&inbuf, REPORT_COUNT, ReportReceived);
          retVal = USB_STATUS_OK;
        }
        else if( (setup->wValue & 0xFF) == REPORT2_ID ) {
          USBD_Read(0, (void*)&inbuf, REPORT2_COUNT, Report2Received);
          retVal = USB_STATUS_OK;            
        }
        
        break;
        
      case USB_HID_GET_REPORT:           // 0x01, send data to host
          /*
          if ( ( (setup->wValue >> 8)       == 1)             // INPUT report  
               && ( (setup->wValue & 0xFF)  == 1)             // Report ID     
               //               && (setup->wLength           == 8)             // Report length 
               //               && (setup->Direction         == USB_SETUP_DIR_IN)    ) {
               ) {
          */
        if( ((setup->wValue & 0xFF) == REPORT_ID)  ) {
          USBD_Write(0, &reportToSend, REPORT_COUNT, NULL);
          retVal = USB_STATUS_OK;
        }
        else if( ((setup->wValue & 0xFF) == REPORT2_ID) ) {
          USBD_Write(0, &reportToSend, REPORT2_COUNT, NULL);
          retVal = USB_STATUS_OK;
        }
        
        break;
          
      /*
      case USB_HID_SET_IDLE:
        // ********************
          if ( ( (setup->wValue & 0xFF)    == 0)              // Report ID     
             && (setup->wLength          == 0)
             && (setup->Direction        != USB_SETUP_DIR_IN)    ) {
          idleRate = setup->wValue >> 8;
          if ( (idleRate != 0) && (idleRate < (HIDKBD_POLL_RATE / 4) ) ) {
            idleRate = HIDKBD_POLL_RATE / 4;
          }
          USBTIMER_Stop(HIDKBD_IDLE_TIMER);
          if ( idleRate != 0 ) {
            IdleTimeout();
          }
          retVal = USB_STATUS_OK;
        }
        break;

      case USB_HID_GET_IDLE:
        // ******************
          if ( (setup->wValue       == 0)                     // Report ID     
             && (setup->wLength   == 1)
             && (setup->Direction == USB_SETUP_DIR_IN)    ) {
          *(uint8_t*)&tmpBuffer = idleRate;
          USBD_Write(0, (void*)&tmpBuffer, 1, NULL);
          retVal = USB_STATUS_OK;
        }
        break;
          */
        } // swtich bRequest
        
      } // if

  } // else
  
  
  return retVal;
}

// unused for now
void stateChange(USBD_State_TypeDef oldState, USBD_State_TypeDef newState)
{
  (void)oldState;
  if (newState == USBD_STATE_CONFIGURED) {
    //GPIO_PinOutClear(gpioPortA, 0);
    //USBD_Read(EP_OUT, receiveBuffer, BUFFERSIZE, dataReceivedCallback);
  }
  else if ( newState == USBD_STATE_SUSPENDED ) {
    //    GPIO_PinOutSet(gpioPortA, 0);
  }
}



