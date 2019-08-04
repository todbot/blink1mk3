/***************************************************************************//**
 * @file    usbconfig.h
 * @brief   USB protocol stack library, application supplied configuration
 *          options.
 * @version 5.0
 *******************************************************************************
 * @section License
 * <b>Copyright 2016 Silicon Laboratories, Inc. http://www.silabs.com</b>
 *******************************************************************************
 *
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 *
 ******************************************************************************/

#ifndef SILICON_LABS_USBCONFIG_H
#define SILICON_LABS_USBCONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

#define USB_DEVICE        // Compile stack for device mode.
#define USB_TIMER USB_TIMER2
#define USB_CORECLK_HFRCO
#define USB_USBC_32kHz_CLK USB_USBC_32kHz_CLK_LFRCO

/*
According to the docs, USB_PWRSAVE_MODE is undefined by default, 
so no powersaving is performed.  However in practice, it appears
if you set device to be Bus Powered it will halt the CPU when 
a USB Suspend is done.

// value for fw 302
#define USB_PWRSAVE_MODE (USB_PWRSAVE_MODE_ONVBUSOFF   \
                          | USB_PWRSAVE_MODE_ONSUSPEND \
                          | USB_PWRSAVE_MODE_ENTEREM2 )
//#define USB_PWRSAVE_MODE (USB_PWRSAVE_MODE_OFF)
*/

#if 0
// FIXME: this did not seem to work
extern int write_char(int p);
#define USER_PUTCHAR  write_char
// Debug USB API functions (illegal input parameters etc.) 
#define DEBUG_USB_API
#endif

/****************************************************************************
**                                                                         **
** Specify number of endpoints used (in addition to EP0).                  **
**                                                                         **
*****************************************************************************/
#define NUM_EP_USED 2
#define EP_IN  0x81
#define EP_OUT 0x01


/****************************************************************************
**                                                                         **
** Specify number of application timers you need.                          **
**                                                                         **
*****************************************************************************/
#define NUM_APP_TIMERS 1


#ifdef __cplusplus
}
#endif

#endif // SILICON_LABS_USBCONFIG_H
