/*********************************************************************************************
 *
 * blink1mk3 firmware -- EFM32HG-based, based off work by Tomu project (github.com/im-tomu)
 *
 * 2018 Tod E. Kurt, http://todbot.com/blog/
 *
 * Differences from blink1mk3-test4:
 * - Implements serverdown logic
 * - 
 * - Uses PLAY_ enum
 *
 * Differences from blink1mk3-test5:
 * - 
 *
 * Differences from blink1mk3-test3:
 * - fixed two-report HID descriptor to work on Windows (and you know, be actually correct)
 * - fix usernotes to correctly save to flash
 * - fixes issue with usernotes killing pattern lines
 * - add startup_params
 * - cleaned up debug printing, increased dbgstr size from 30 to 50 bytes
 *
 * Differences from blink1mk3-test1:
 * - responds correctly to ReportId 2 (64-bytes), testable with "blink1-tool -i 2 --testtest" 
 * - code reformatted heavily to pull ws2812 driver and color_funcs into separate files
 * - code works on Tomu-class boards
 *
 *
 ********************************************************************************************/

/*
 * Flash Memory Layout
 * -------------
 * 0x0000 \ 
 * ...     |- DFU bootlaoder (16 kB)
 * 0x3FFF /
 * 0x4000 \ 
 * ..      -- blink1 program code (46 kB == 64kB - 1kB - 1kB - 16kB)
 * 0xF7FF /
 * 0xF800 \
 * ...     -- User-specified text notes (1 kB)
 * 0xFBFF /
 * 0xFC00 \
 * ...     -- User-specified color patterns (1 kB)
 * 0xFFFF /
 * 
 * See "blink1mk3.ld" for details
 */

#include "capsense.h"
#include "usbconfig.h"

#include <em_core.h>
#include <em_msc.h>
#include <em_usart.h>
#include <em_leuart.h>
#include <em_chip.h>
#include <em_cmu.h>
#include <em_device.h>
#include <em_emu.h>
#include <em_gpio.h>
#include <em_usb.h>
#include <em_wdog.h>
#include <em_system.h>

#include <stdint.h>
#include <stdlib.h> // rand()
#include <stdbool.h>

#define blink1_version_major '3'
#define blink1_version_minor '1'

#define DEBUG 1    // enable debug messages output via LEUART, see 'debug.h'
#define DEBUG_STARTUP 1
// define this to print out cmd+args in handleMessage()
#define DEBUG_HANDLEMESSAGE 0

#define BOARD_TYPE BOARD_TYPE_BLINK1MK3       // ws2812 data out on B7
//#define BOARD_TYPE BOARD_TYPE_TOMU          // ws2812 data out on E13
//#define BOARD_TYPE BOARD_TYPE_EFM32HGDEVKIT // ws2812 data out on E10

#define nLEDs 18   // number of LEDS

#include "utils.h"
#include "leuart.h"
#include "debug.h"

#include "toboot.h"
#include "tinyprintf.h"
#include "ws2812_spi.h"
#include "descriptors.h"
#include "color_types.h"


// forward decls
void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t n);
static inline void displayLEDs(void);
static void off();
#define setLEDsAll(r,g,b) { setLED(r,g,b, 255); } // 255 means all


// allocate faders, one per LED
rgbfader_t fader[nLEDs];

#include "color_funcs.h"   // needs setLED(), nLEDs, fader[] defined


extern struct toboot_runtime toboot_runtime;
/* Declare support for Toboot V2 */
/* To enable your code to run when you first plug in Tomu, pass
 * TOBOOT_CONFIG_FLAG_AUTORUN to this macro.  Otherwise, leave the
 * configuration value at 0 to use the defaults.
 */
TOBOOT_CONFIGURATION(0);
//TOBOOT_CONFIGURATION( TOBOOT_CONFIG_FLAG_AUTORUN );
//
// Note: Must also set "toboot_runtime.boot_count = 0"
// to prevent bootloader from running after 3 power-cycles
// Because apparently the RAM gets enough power to stay alive?x


// valid values for 'bootmode' in startup_param
enum {
    BOOT_NORMAL = 0,  // normal <v205 behavior
    BOOT_PLAY,        // play a script on startup (servertickle)
    BOOT_OFF,         // turn off on startup, even with no USB
};

// layout of the startup params bundle
// byte size of startup_params_t must be same as patternline_t (6 bytes))
// so in this mk2 hack, we have 6 bytes to play with {r,g,b, th,tl, ledn}
// The high bit of ledn is used to signal that it's a startup param: 0x80
// Thus the boot mode is ledn & 0x7F
typedef struct { 
    uint8_t playstart; // r from v206 mk2
    uint8_t playend;   // g
    uint8_t playcount; // b
    uint8_t unused1;   // th
    uint8_t unused2;   // tl
    uint8_t bootmode;  // ledn, bootmode_t enum above
    uint8_t bootloaderlock; // is programmatic triggering of bootloader allowed
} startup_params_t;

// array of LED data (sent to LEDs)
rgb_t leds[nLEDs];

// global which is active LED
uint8_t ledn;

// number of entries a color pattern can contain
#define PATT_MAX 32

// define what is in the user data section
typedef struct {
  startup_params_t startup_params;
  patternline_t pattern[PATT_MAX];
} userdata_t;

/*
 * "Notes" are user-writable and -readable blobs of data
 * report2 size is 60 bytes (must be mult of 4, must be <64 for feature report)
 * byte 0 reportid
 * byte 1 cmd
 * byte 3 noteid
 * byte 4-59 notedata == 55 bytes
 *
 * Note size = 50
 * Note count = 20
 * Total size = 1000 < FLASH_PAGE_SIZE = 1024
 * .userNotesFlashSection is in flash at address 0xf800 (64k - (2*1k)) 
 */
#define NOTE_SIZE 50
#define NOTE_COUNT 20

// define what is in a user note
typedef struct {
  char note[NOTE_SIZE];  // just a string for now
} usernote_t;

// just an idea, make the entire notes block its own struct
//typedef struct {
//  usernote_t notes[NOTE_COUNT]
//} usernotes_t;

/*
 * Flash / non-volatile user data & user color pattern
 *
 * 6 bytes / patternline (fade is 2bytes)
 * => 1024 bytes / 6 = 170 pattern lines potentially (or 10 16-line patterns =960 bytes)
 *
 * FLASH_PAGE_SIZE = 1024 (Gecko_SDK/platform/Device/SiliconLabs/EFM32HG/Include/efm32hg309f64.h)
 * FLASH_SIZE = 65536 (64k)
 * .userFlashSection is in flash at address 0xfc00 (64k - (1*1k)) 
 */
//uint32_t *patterFlashAddress = (uint32_t *)(FLASH_SIZE - (2*FLASH_PAGE_SIZE));

// Flash copy of user startup params and LED patterns
// can at most be FLASH_PAGE_SIZE big (1024 bytes)
__attribute__ ((section(".userFlashSection")))
const userdata_t userFlash = {
  {
    .playstart = 0,
    .playend = 0,
    .playcount = 0,
    .bootmode = BOOT_NORMAL,
    .bootloaderlock = 0,
  },
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
  },
};

// Flash copy of userNotes
// can at most be FLASH_PAGE_SIZE big (1024 bytes)
__attribute__ ((section(".userNotesFlashSection")))
const usernote_t userNotesFlash[NOTE_COUNT] = {
  {"Note0: This is a test note"},
  {"Note1: Here is another test note"},
  {"Note2: This is a max size note. Note is 50 bytes.!"},
  {"Note3:67890123456789012345678901234567890123456789"},
  //01234567890123456789012345678901234567890123456789
  //          1         2         3         4
};

// RAM copy of non-volatile notes
// must be word-aligned because that's what MSC_WriteWord() needs
SL_ALIGN(4)
usernote_t userNotes[NOTE_COUNT] SL_ATTRIBUTE_ALIGN(4);

// RAM copy of non-volatile startup params & color pattern
// must be word-aligned because that's what MSC_WriteWord() needs
SL_ALIGN(4)
userdata_t userData SL_ATTRIBUTE_ALIGN(4);


uint8_t playstart_serverdown = 0;        // start play position for serverdown
uint8_t playend_serverdown   = PATT_MAX; // end play position for serverdown 

uint8_t playpos   = 0; // current play position
uint8_t playstart = 0; // start play position
uint8_t playend   = PATT_MAX; // end play position
uint8_t playcount = 0; // number of times to play loop, or 0=infinite

// play modes, valid values for "playing"
enum { 
    PLAY_OFF       = 0,  // off
    PLAY_ON        = 1,  // normal playing pattern
    PLAY_POWERUP   = 2,  // playing from a powerup
    PLAY_DIRECTLED = 3   // direct LED addressing (FIXME: this is dumb)
};

uint8_t playing; // playing values: as above enum

bool doPatternWrite = false;

patternline_t ptmp;  // temp pattern holder
rgb_t ctmp;      // temp color holder
uint16_t ttmp;   // temp time holder
uint8_t ledn;    // temp ledn holder

// The uptime in milliseconds, maintained by the SysTick timer.
volatile uint32_t uptime_millis;

// next time led_update should run
const uint32_t led_update_millis = 10;  // tick msec
uint32_t led_update_next;

uint32_t pattern_update_next;
uint16_t serverdown_millis;
uint32_t serverdown_update_next;

uint32_t last_misc_millis;

// Set by 'G' "gobootload" command
bool shouldRebootToBootloader = false;
// Set when a Note write is issued
bool doNotesWrite = false;
// Set when USB is properly setup by host PC
bool usbHasBeenSetup = false;

// For sending back HID Descriptor in setupCmd
static void  *hidDescriptor = NULL;

// The USB report packet received from the host
// could be REPORT_COUNT or REPORT2_COUNT long
// first byte is reportId
SL_ALIGN(4)
static uint8_t  inbuf[REPORT2_COUNT] SL_ATTRIBUTE_ALIGN(4);

// The USB report packet to send to the host 
// generally it's a copy of the last report received, then modified
SL_ALIGN(4)
static uint8_t reportToSend[REPORT2_COUNT] SL_ATTRIBUTE_ALIGN(4);

// forward declaration for callbacks struct
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
 * main() further below).
 * It provides the equivalent of "millis()" in the variable "uptime_millis", 
 * but it does roll-over 
 * It must be called "SysTick_Handler"
 */
void SysTick_Handler() {
  uptime_millis++;
}
// ease-of-use function because I'm used to Arduino
#define millis() (uptime_millis)

/* simple delay() -- don't use this normally because it spinlocks the CPU*/
static void SpinDelay(uint32_t millis) {
  // Calculate the time at which we need to finish "sleeping".
  uint32_t sleep_until = uptime_millis + millis; 
  // Spin until the requested time has passed.
  while (uptime_millis < sleep_until);
}

/**********************************************************************
 * Set Toboot magic value to force bootloader and reset 
 **********************************************************************/
static void rebootToBootloader()
{
  dbg_printf("\nbootloading...\n");
  setLED(99,0,33, 0); // LED A
  setLED(33,0,99, 1); // LED B
  displayLEDs();
  SpinDelay(100);
  
  // set magic value to force bootloader
  toboot_runtime.magic = TOBOOT_FORCE_ENTRY_MAGIC;
  setLEDsAll(0,0,0);     // Turn off all LEDs
  displayLEDs();        
  USBD_Disconnect();     // Disconnect nicely from USB
  USBTIMER_DelayMs(100); // Wait a bit
  NVIC_SystemReset();    // Reset
}

// --------------------------------------------------------

/*********************************
 * Save current RAM pattern & startup params to flash 
 *********************************/
static void userDataSave()
{
  MSC_Init();
  MSC_ErasePage((uint32_t*)&userFlash); // must erase first
  MSC_WriteWord((uint32_t*)&userFlash, &userData, FLASH_PAGE_SIZE); 
  MSC_Deinit();
}

/*********************************
 *
 ********************************/
static void userDataLoad()
{
  // Load userdata from flash to RAM
  // includes startup params and color pattern
  memset( &userData, 0, sizeof(userdata_t)); // zero out just in case
  memcpy( &userData, &userFlash, sizeof(userdata_t)); // make this a loadUserDat() func?
}

/*********************************
 * Save RAM user notes to flash.
 *********************************/
//static void writeNotesFlash()
static void notesSave()
{
  MSC_Init();
  MSC_ErasePage((uint32_t*)userNotesFlash); // must erase first
  MSC_WriteWord((uint32_t*)userNotesFlash, userNotes, FLASH_PAGE_SIZE); // why is this freezing?!?!
  MSC_Deinit();
}

/**********************************************************************
 * Load all user notes from flash to RAM
 **********************************************************************/
static void notesLoadAll()
{
  memcpy( userNotes, userNotesFlash, (NOTE_COUNT*NOTE_SIZE)); //FLASH_PAGE_SIZE);
  // note: do not do "flash_page_size" or it overwrites other variables near 'userNotes'
  // because 'userNotes' size is smaller than flash_page_size
  // (NOTE_COUNT*NOTE_SIZE) = 1000, FLASH_PAGE_SIZE = 1024
}

/**********************************************************************
 * Write a note to RAM from USB.
 * - Uses global 'inbuf'
 *********************************************************************/
static void noteWrite(uint8_t pos )
{
  if( pos >= NOTE_COUNT ) {
    return;      // error
  }
  //  memcpy( userNotesData + (pos*NOTE_SIZE), inbuf+3, NOTE_SIZE);
  memcpy( &userNotes[pos], inbuf+3, NOTE_SIZE);
}

/**********************************************************************
 * Read a user note from RAM to USB.
 * - Uses global 'reportToSend'
 *********************************************************************/
static void noteRead(uint8_t pos)
{
  dbg_printf("noteRead:%d\n",pos);
  //memcpy( reportToSend+3, userNotes + (pos*NOTE_SIZE), NOTE_SIZE );
  memcpy( reportToSend+3, &userNotes[pos], NOTE_SIZE );
}

// -----------------------------------------------------------


// -------- LED & color pattern handling -------------------------------
//

/**********************************************************************
 * @brief Send LED data out to LEDs
 * - used by "color_funcs.h"
 **********************************************************************/
static inline void displayLEDs(void)
{
  ws2812_sendLEDs( leds, nLEDs );    // ws2811_showRGB();
}

/*********************************************************************
 * set blink1 to not playing, and no LEDs lit
 *********************************************************************/
static void off(void)
{
    playing = PLAY_OFF;
    setRGBt(ctmp, 0,0,0);  // starting color
    rgb_setCurr( &ctmp );  // set all LEDs FIXME: better way to do this?
}

/*********************************************************************
 * Start playing the light pattern
 * playing values: 0 = off, 1 = normal, 2 == playing from powerup
 *********************************************************************/
static void startPlaying( void )
{
    playpos = playstart;
    pattern_update_next = millis(); //uptime_millis; // millis(); // now;
    //pattern_update_next = 0; // invalidate it so plays immediately
    //memcpy( pattern, patternflash, sizeof(patternline_t)*PATT_MAX);
}

/**********************************************************************
 * @brief Set the color of a particular LED, or all of them
 **********************************************************************/
void setLED(uint8_t r, uint8_t g, uint8_t b, uint8_t n)
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

#if 1
        // normal pre v206 behavior
        if( userData.startup_params.bootmode == BOOT_NORMAL ) { 
            // check for non-computer power up
            if( !usbHasBeenSetup ) {
                if( !playing && now > 500 ) {  // 500 msec wait
                    playing = PLAY_POWERUP;
                    startPlaying();
                }
            }
            else {  // else usb is setup...
                if( playing == PLAY_POWERUP ) { // ...but we started a powerup play, so reset
                    off();
                }
            }
        }
        else { 
            if( userData.startup_params.bootmode == BOOT_PLAY) { 
                if( !playing && now > 500 && now < 1000 ) { // 500 msec wait
                    playing = PLAY_ON;
                    startPlaying();
                }
            }
            // else do nothing
        }

#endif

#if 0
        // check for non-computer power up
        if( !usbHasBeenSetup ) {
            if( !playing && now > 500 ) {  // 500 msec wait
                playing = PLAY_POWERUP;
                startPlaying();
            }
        }
        else {  // else usb is setup...
            if( playing == PLAY_POWERUP ) { // ...but we started a powerup play, so reset
                off();
            }
        }
#endif
    } // if led_update_next

    // serverdown logic
    if( serverdown_millis != 0 ) {  // i.e. servermode has been turned on
        if( (long)(now - serverdown_update_next) > 0 ) {
            serverdown_millis = 0;  // disable this check
            playing = PLAY_ON;
            playstart = playstart_serverdown;
            playend   = playend_serverdown;
            startPlaying();
        }
    }

    // playing light pattern
    if( playing ) {
        if( (long)(millis() - pattern_update_next) > 0  ) { // time to get next line
            ctmp = userData.pattern[playpos].color;
            ttmp = userData.pattern[playpos].dmillis;
            ledn = userData.pattern[playpos].ledn;

            // special command handling
            if( ledn & 0x80 ) {    // special command bit
              ledn = ledn & 0x7f;  // mask off special command bit
              // random
              ledn = (rand() % ledn) +1 ; // 0 means all
              uint8_t h = rand() % 255;
              uint8_t s = ctmp.g;
              uint8_t v = ctmp.b;
              hsbtorgb( h,s,v, &ctmp.r, &ctmp.g, &ctmp.b );  // NOTE: writes to ctmp.{r,g,b}
            }
            
#if 0
            dbg_printf("%ld\n",millis());
#endif
#if 0            
            // enabling this causes a lag in pattern playing because of blocking LEUART writes
            dbg_printf("%ld patt %d rgb:%x %x %x t:%d l:%d\n", millis(),
                       playpos, ctmp.r, ctmp.g, ctmp.b, ttmp, ledn);
#endif            
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
                    playing = PLAY_OFF; // done!
                }
                else if(playcount==255) {
                    playcount = 0; // infinite playing
                }
            }
            pattern_update_next += ttmp*10;  // okay if ttmp is zero
        }
    } // playing
    
}

//
// Hardawre bootloader trigger support
//
#define SWD_PORT  gpioPortF
#define SWDIO_PIN 1
#define SWCLK_PIN 0
uint8_t bootloadPinTest = 0;

/**
 * Check SWDAT & SWCLK pins for shorting
 *  to boot to bootloader
 * FIXME: does this all need to be in an atomic block?
 */
static void checkSWDPins()
{
  // SWCLK is output, SWDIO is input
  GPIO_DbgSWDIOEnable(false);  // turn OFF SWD (GPIO->ROUTE = 0)
  // begin: do this part as fast as possible
  GPIO_PinModeSet(SWD_PORT, SWCLK_PIN, gpioModePushPull, 0); // set LOW 
  GPIO_PinModeSet(SWD_PORT, SWDIO_PIN, gpioModeInputPull, 1); // pull-up
  //GPIO_PinOutClear(SWD_PORT, SWCLK_PIN); // set SWCLK low
  int v = GPIO_PinInGet(SWD_PORT, SWDIO_PIN); // read SWDIO
  // end: do this part as fast as possible
  GPIO_DbgSWDIOEnable(true);   // turn ON SWD  (GPIO->ROUTE = 0)
  
  if( v==0 ) { // pin connected
    bootloadPinTest += 5;
    if( bootloadPinTest > 10 ) {
      rebootToBootloader();
    }
  }
  bootloadPinTest = constrain(bootloadPinTest, 1, 200);  // limit the value between 1-200
  bootloadPinTest--; // decay away
  //dbg_printf("swdt:%d %d\n", bootloadPinTest,v);
}

/**********************************************************************
 * Tend to various housekeeping
 **********************************************************************/
static void updateMisc()
{
  if( (uptime_millis - last_misc_millis) > 250 ) {  // only run this every 250 msecs
    last_misc_millis = millis(); //uptime_millis;
    //write_char('.');  //write_char('0'+usbState);
    // print out heartbeats that are also our USB state:
    // '.' == connected to computer
    // ':' == powered but no computer
    // a number is another USBD_State_TypeDef
    //write_char((usbState==USBD_STATE_CONFIGURED) ? '.' : (usbState==USBD_STATE_DEFAULT) ? ':': 0+usbState);
    write_char((usbHasBeenSetup) ? '.' : ':');

    if( shouldRebootToBootloader ) {
      rebootToBootloader();  // and now we die
    }
    
    if( userData.startup_params.bootloaderlock ) {
      checkSWDPins();
    }
  }
  
  if( doNotesWrite ) {
    doNotesWrite = false;
    dbg_str("writing userNotes...");
    notesSave();
    dbg_str("wrote userNotes");
  }
  
  if( doPatternWrite ) {
    doPatternWrite = false;
    userDataSave();
    dbg_str("wrote userFlash");
  }

  // usbState: '5' is CONFIGURED, '3' is DEFAULT.  See em_usb.h
  //USBD_State_TypeDef usbState = USBD_GetUsbState();
  //if( usbState == USBD_STATE_CONFIGURED ) {
  // usbHasBeenSetup = true;
  //}

  // enabling this makes it impossible to go into bootloader mode, why?
#if 0
  // Capture/sample the state of the capacitive touch sensors.
  CAPSENSE_Sense();

  if( CAPSENSE_getPressed(BUTTON0_CHANNEL) ) {
    playing = PLAY_DIRECTLED ; // sigh
    setLED( 0, 255, 0, 2 );
    
  }
#endif
  
  /*
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
  */

}


/**********************************************************************
 * @brief Modify 'iSerialNumber' string to be based on chip's unique Id
 * iSerialNumber is defined in 'descriptors.c'
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
 * Main, called by ResetHandler
 **********************************************************************/
int main()
{
  // Runs the Silicon Labs chip initialisation stuff, that also deals with
  // errata (implements workarounds, etc).
  CHIP_Init();
  
  // Disable the watchdog that the bootloader started.
  WDOG->CTRL = 0;
  
  //CMU_ClockEnable(cmuClock_DMA, true);  
  // USART is a HFPERCLK peripheral. Enable HFPERCLK domain and USART0.
  CMU_ClockEnable(cmuClock_HFPER, true);  
  CMU_ClockEnable(cmuClock_USART0, true);  
  // Switch on the clock for GPIO. Even though there's no immediately obvious
  // timing stuff going on beyond the SysTick below, it still needs to be
  // enabled for the GPIO to work.
  CMU_ClockEnable(cmuClock_GPIO, true);

  // Fix the fact that Tomu bootloader doesn't reset pin state
  GPIO_PinModeSet(gpioPortA, 0, gpioModeDisabled, 0);
  GPIO_PinModeSet(gpioPortB, 7, gpioModeDisabled, 0);

  // Enable the capacitive touch sensor. Remember, this consumes TIMER0 and
  // TIMER1, so those are off-limits to us.
  CAPSENSE_Init();

  // Sets up and enable the `SysTick_Handler' interrupt to fire once every 1ms.
  // ref: http://community.silabs.com/t5/Official-Blog-of-Silicon-Labs/Chapter-5-MCU-Clocking-Part-2-The-SysTick-Interrupt/ba-p/145297
  if (SysTick_Config(CMU_ClockFreqGet(cmuClock_CORE) / 1000)) {
    // Something went wrong.
    while (1);
  }

  setupLeuart();

  dbg_str("\nblink1mk3-firmware-v30x startup...\n");
  dbg_printf("toboot_runtime: count:%d model:%x\n",
             toboot_runtime.boot_count, toboot_runtime.board_model);

  toboot_runtime.boot_count = 0; // reset boot count to say we're alive
  // FIXME: how is the toboot_runtime RAM being preserved on power cycle?

  ws2812_setupSpi();

  uint32_t refFreq = CMU_ClockFreqGet(cmuClock_HFPER);
  dbg_printf("refFreq:%ld\n", refFreq);         // 21000000
  dbg_printf("CTRL   :%lx\n", USART0->CTRL);    // 1025 = 0x0401
  dbg_printf("FRAME  :%lx\n", USART0->FRAME);   // 4105 = 0x1009
  dbg_printf("CMD    :%lx\n", USART0->CMD);     // 0
  dbg_printf("STATUS :%lx\n", USART0->STATUS);  // 71   = 0x0047
  dbg_printf("CLKDIV :%lx\n", USART0->CLKDIV);  // 768  = 0x300
  dbg_printf("ROUTE  :%lx\n", USART0->ROUTE);   // 0x0402
  dbg_printf("CMU_CTRL         : %lx\n", CMU->CTRL );         // 0xc262c 796204 ?
  dbg_printf("CMU_HFCORECLKDIV : %lx\n", CMU->HFCORECLKDIV ); // 0
  dbg_printf("CMU_HFPERCLKDIV  : %lx\n", CMU->HFPERCLKDIV );  // 0x0100
  dbg_printf("CMU_HFCORECLKEN0 : %lx\n", CMU->HFCORECLKEN0 ); // 0x0004
  dbg_printf("CMU_HFPERCLKEN0  : %lx\n", CMU->HFPERCLKEN0 );  // 0x016b

  userDataLoad();

#if DEBUG_STARTUP
  // debug: dump out loaded pattern
  for( int i=0; i<PATT_MAX; i++) {
    ctmp = userData.pattern[i].color;
    ttmp = userData.pattern[i].dmillis;
    ledn = userData.pattern[i].ledn;
    dbg_printf("patt[%d] rgb:%2x%2x%2x t:%d l:%d\n",
               i, ctmp.r, ctmp.g, ctmp.b, ttmp, ledn);
  }
#endif
  
  notesLoadAll();
  
  makeSerialNumber();  // Make USB serial number from chip unique Id
  
  hidDescriptor = (void*) USBDESC_HidDescriptor; // FIXME
  
  // Enable the USB controller.
  // Remember, this consumes TIMER2 as per -DUSB_TIMER=USB_TIMER2 in Makefile
  // because TIMER0 & TIMER1 are already taken by the capacitive touch sensors.
  //
  int rc = USBD_Init(&initstruct);
  UNUSED(rc);
  dbg_printf("usbd_init rc:%d\n", rc);

  // When using a debugger it is practical to uncomment the following three
  // lines to force host to re-enumerate the device.
#if 0
  USBD_Disconnect();      
  USBTIMER_DelayMs(1000); 
  USBD_Connect();         
#endif

  // standard blink1 startup white fadeout
  for( uint8_t i=255; i>0; i-- ) {
    SpinDelay(1);
    uint8_t j = i>>4;      // not so bright, please
    //setLEDsAll(j,j,j);
    setLED(j,j,j, 0); // LED A
    setLED(j,j,j, 1); // LED B
    displayLEDs();
  }
  
  //startPlaying();  // to load pattern up
  off();
  displayLEDs(); // why this here, to prime the system?

  // main loop 
  while(1) {

    updateLEDs();
    updateMisc();
    
  }
  
}

/**********************************************************************
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
 *    - Write 50-byte note      format: { 2, 'F', noteid, data0 ... data99 } (3)
 *    - Read  50-byte note      format: { 2, 'f', noteid, data0 ... data99 } (3)
 *    - Go to bootloader        format: { 1, 'G', 'o','B','o','o','t',0 } (3)
 *    - Lock go to bootload     format: { 2','L'  'o','c','k','B','o','o','t','l','o','a','d'} (3)
 *    - Set startup params      format: { 1, 'B', bootmode, playstart,playend,playcnt,0,0} (3)
 *    - Get startup params      format: { 1, 'b', 0,0,0, 0,0,0        } (3)
 *    - Server mode tickle      format: { 1, 'D', {1/0},th,tl, {1,0},sp, ep }
 *
 * x Fade to RGB color        format: { 1, 'c', r,g,b,      th,tl, ledn }
 * x Set RGB color now        format: { 1, 'n', r,g,b,        0,0, ledn }
 * x Play/Pause, with pos     format: { 1, 'p', {1/0},pos,0,  0,0,    0 }
 * x Play/Pause, with pos     format: { 1, 'p', {1/0},pos,endpos, 0,0,0 }
 * x Write color pattern line format: { 1, 'P', r,g,b,      th,tl,  pos }
 * x Read color pattern line  format: { 1, 'R', 0,0,0,        0,0, pos }
 *
 *********************************************************************/
static void handleMessage(uint8_t reportId)
{
#if DEBUG_HANDLEMESSAGE 
  dbg_printf("%d:%x,%x,%x,%x,%x,%x,%x,%x\n", reportId,
          inbuf[0],inbuf[1],inbuf[2],inbuf[3],inbuf[4],inbuf[5],inbuf[6],inbuf[7] );
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
    playing = PLAY_OFF;
    rgb_setDest(&c, dmillis, ledn);
  }
  //
  // set RGB color immediately  - {1,'n', r,g,b, 0,0,0 }
  //
  else if( cmd == 'n' ) {
    uint8_t iledn = inbuf[7];          // which LED to address
    playing = PLAY_OFF;
    if( iledn > 0 ) {
      playing = PLAY_DIRECTLED;       // FIXME: wtf non-semantic 3
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
    if( playend == 0 || playend > PATT_MAX )
      playend = PATT_MAX;
    else playend++;  // so that it's equivalent to PATT_MAX, if you know what i mean
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
    if( pos >= PATT_MAX ) pos = 0;  // just in case
    // save pattern line to RAM
    memcpy( &userData.pattern[pos], &ptmp, sizeof(patternline_t) );
  }
  //
  // Read color pattern entry - {1,'R', 0,0,0, 0,0, pos}
  //
  else if( cmd == 'R' ) {
    uint8_t pos = inbuf[7];
    if( pos >= PATT_MAX ) pos = 0;
    patternline_t patt = userData.pattern[pos];
    reportToSend[2] = patt.color.r;
    reportToSend[3] = patt.color.g;
    reportToSend[4] = patt.color.b;
    reportToSend[5] = (patt.dmillis >> 8);
    reportToSend[6] = (patt.dmillis & 0xff);
    reportToSend[7] = patt.ledn;
  }
  //
  // Save color pattern to flash memory: { 1, 'W', 0x55,0xAA, 0xCA,0xFE, 0,0}
  //
  else if( cmd == 'W' ) {
    if( inbuf[2] == 0xBE &&
        inbuf[3] == 0xEF &&
        inbuf[4] == 0xCA &&
        inbuf[5] == 0xFE ) {
      doPatternWrite = true; 
      // we write in main loop, not in this callback
    }
  }
  //
  // Set ledn : { 1, 'l', n, 0...}
  //
  else if( cmd == 'l' ) { 
    ledn = inbuf[2];
  }
  //
  //  Server mode tickle        format: { 1, 'D', {1/0}, th,tl, st, sp, ep }
  //     1/0 == on/off
  //     t == tickle time
  //     st == stop any current playing pattern and turn off (boolean)
  //     sp == pattern play start point
  //     ep == pattern play end point
  //
  else if( cmd == 'D' ) {
    uint8_t serverdown_on = inbuf[2];
    uint16_t t = ((uint16_t)inbuf[3] << 8) | inbuf[4];
    uint8_t stop = inbuf[5];
    playstart  = inbuf[6];
    playend    = inbuf[7];
    if( playend == 0 || playend > PATT_MAX )
      playend = PATT_MAX;
    
    if( serverdown_on ) {
      serverdown_millis = t;
      serverdown_update_next = millis() + (t*10); //uptime_millis + (t*10);
    } else {
      serverdown_millis = 0; // turn off serverdown mode
    }
    if( stop == 0 ) {   // agreed, confusing
      off();
    }
  }

  // Set startup parameters     format: {1, 'B', bootmode, playstart, playend, playcount, 0,0} 
  //
  else if( cmd == 'B' ) { 
    userData.startup_params.bootmode  = inbuf[2]; // ledn
    userData.startup_params.playstart = inbuf[3]; // r
    userData.startup_params.playend   = inbuf[4]; // g
    userData.startup_params.playcount = inbuf[5]; // b
    userData.startup_params.unused1   = inbuf[6]; // th
    userData.startup_params.unused2   = inbuf[7]; // tl

    // fixup playend, copied from 'p'lay/pause
    // FIXME: find a real way to handle this 
    if( userData.startup_params.playend == 0 || userData.startup_params.playend > PATT_MAX )
      userData.startup_params.playend = PATT_MAX;
    else
      userData.startup_params.playend++; // so it's equiv to patt_max, if you know what i mean

    doPatternWrite = true;; // causes all userData to be saved // FIXME: rename
  }

  // Get Startup Params      format: { 1, 'b', 0,0,0,0, 0,0 }
  //
  else if( cmd == 'b' ) {
    reportToSend[2] = userData.startup_params.bootmode;
    reportToSend[3] = userData.startup_params.playstart;
    reportToSend[4] = userData.startup_params.playend;
    reportToSend[5] = userData.startup_params.playcount;
    reportToSend[6] = userData.startup_params.unused1;
    reportToSend[7] = userData.startup_params.unused2;
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
  // Check against command "GoBoot"
  //
  else if( cmd == 'G' ) {
    if( inbuf[2] == 'o' && inbuf[3] == 'B' && inbuf[4] == 'o' && inbuf[5] == 'o' && inbuf[6] == 't' ) {
      dbg_str("GoBoot");
      if( userData.startup_params.bootloaderlock ) {
        reportToSend[1] = 'L'; // send ack but fail because lock
        reportToSend[2] = 'O';
        reportToSend[3] = 'C';
        reportToSend[4] = 'K';
        reportToSend[5] = 'E';
        reportToSend[6] = 'D';
      }
      else { 
        shouldRebootToBootloader = true;
        reportToSend[2] = 'O'; // send acknowledge
        reportToSend[3] = 'B';
        reportToSend[4] = 'O';
        reportToSend[5] = 'O';
        reportToSend[6] = 'T'; 
      }
    }
  }
  //
  // 
  else if( cmd == 'L' && rId == 2 ) {
    char* bp = (char*) (&inbuf[2]);
    if( strncmp( bp, "ockBootload",11) == 0 ) {
      dbg_str("LockBootload");
      userData.startup_params.bootloaderlock = 1;
      // save user data
      doPatternWrite = true;
    }
  }
  //
  // test test
  //
  else if( cmd == '!' ) {  // testtest
    uint32_t now = millis();
    reportToSend[2] = 0x55;
    reportToSend[3] = 0xAA;
    reportToSend[4] = rId; //(uint8_t)(uptime_millis >> 24);
    reportToSend[5] = (uint8_t)(now >> 16);
    reportToSend[6] = (uint8_t)(now >> 8);
    reportToSend[7] = (uint8_t)(now >> 0);

    //test2Flash(); // FIXME: this will get removed
  }
  //
  // Read User Note       format: { 1, 'f', noteid, 0, 0, 0, 0 }  
  // NOTE: must be sent on reportId 2!
  // 
  else if( cmd == 'f' && rId == 2 ) {  // read note
    uint8_t noteid = inbuf[2];
    noteRead( noteid ); // fills out reportToSend from RAM note
  }
  //
  // Write User Note      format: { 2, 'F', noteid, data0,data1,...,data99 }
  // NOTE: must be sent on reportId 2!
  //
  else if( cmd == 'F' && rId == 2 ) { // write note
    uint8_t noteid = inbuf[2];
    noteWrite( noteid ); // reads from global inbuf+3, writes to RAM note
    doNotesWrite = true; // trigger save all notes
    // we write in main loop, not in this callback
  }
  
}

/****************************************************************************
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

/******************************************************************************
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

/******************************************************************************
 * Used to detect USB state change
 *
 * Sets global variable "usbHasBeenSetup"
 *
 *****************************************************************************/
void stateChange(USBD_State_TypeDef oldState, USBD_State_TypeDef newState)
{
  (void)oldState;
  dbg_printf(" USBst:%d ",newState);
  if (newState == USBD_STATE_CONFIGURED) {
    dbg_str(" USBconfigured ");
    usbHasBeenSetup = true;
    //GPIO_PinOutClear(gpioPortA, 0);
    //USBD_Read(EP_OUT, receiveBuffer, BUFFERSIZE, dataReceivedCallback);
  }
  else if ( newState == USBD_STATE_SUSPENDED ) {
    dbg_str(" USBsuspended ");
    //    GPIO_PinOutSet(gpioPortA, 0);
  }
}



