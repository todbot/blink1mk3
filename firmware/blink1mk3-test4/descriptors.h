/***************************************************************************//**
 * @file descriptors.h
 * @brief USB descriptors
 * @author Energy Micro AS
 * @version 1.01
 *******************************************************************************
 * @section License
 * <b>(C) Copyright 2012 Energy Micro AS, http://www.energymicro.com</b>
 *******************************************************************************
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 * 4. The source and compiled code may only be used on Energy Micro "EFM32"
 *    microcontrollers and "EFR4" radios.
 *
 * DISCLAIMER OF WARRANTY/LIMITATION OF REMEDIES: Energy Micro AS has no
 * obligation to support this Software. Energy Micro AS is providing the
 * Software "AS IS", with no express or implied warranties of any kind,
 * including, but not limited to, any implied warranties of merchantability
 * or fitness for any particular purpose or warranties against infringement
 * of any proprietary rights of a third party.
 *
 * Energy Micro AS will not be liable for any consequential, incidental, or
 * special damages, or any other relief, or for any claim by any third party,
 * arising from your use of this Software.
 *
 *****************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif

#define VENDOR_ID     0x27B8 // real one
#define PRODUCT_ID    0x01ED // real one
//#define VENDOR_ID     0x27B8
//#define PRODUCT_ID    0x01Ef
#define DEVICE_VER		0x0101	/* Program version */

#define REPORT_ID  1
#define REPORT2_ID  2
#define REPORT_COUNT 8
#define REPORT2_COUNT 128

SL_ALIGN(4)
const char MyHIDReportDescriptor[48] SL_ATTRIBUTE_ALIGN(4) =
{
    0x06, 0xAB, 0xFF,
    0x0A, 0x00, 0x20,
    
    //0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    //0x09, 0x01,                    // USAGE (Vendor Usage 1)
    
    0xA1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x85, REPORT_ID,               //   REPORT_ID (1)
    0x95, REPORT_COUNT,            //   REPORT_COUNT (8)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0,                          // END_COLLECTION
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x85, REPORT2_ID,              //   REPORT_ID (2)
    0x95, REPORT2_COUNT,            //   REPORT_COUNT (64)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};


/* Device Descriptor. Refer to the USB 2.0 Specification, chapter 9.6 */
SL_ALIGN(4)
static const USB_DeviceDescriptor_TypeDef deviceDesc SL_ATTRIBUTE_ALIGN(4) =
{
  .bLength            = USB_DEVICE_DESCSIZE,            /* Size of the Descriptor in Bytes   */  // 18
  .bDescriptorType    = USB_DEVICE_DESCRIPTOR,          /* Device Descriptor type            */  // 1
  .bcdUSB             = 0x0200,                         /* USB 2.0 compliant                 */  //
  .bDeviceClass       = 0x00,                           /* Vendor unique device              */
  .bDeviceSubClass    = 0,                              /* Ignored for vendor unique device  */            
  .bDeviceProtocol    = 0,                              /* Ignored for vendor unique device  */  
  .bMaxPacketSize0    = USB_FS_CTRL_EP_MAXSIZE,         /* Max packet size for EP0           */  // 64
  .idVendor           = VENDOR_ID,     
  .idProduct          = PRODUCT_ID,                     /* PID */
  .bcdDevice          = DEVICE_VER,                     /* Device Release number             */
  .iManufacturer      = 1,                              /* Index of Manufacturer String Desc */
  .iProduct           = 2,                              /* Index of Product String Desc      */
  .iSerialNumber      = 3,                              /* Index of Serial Number String Desc*/
  .bNumConfigurations = 1                               /* Number of Possible Configurations */
};


/* wTotalLength (LSB)  = 9 + 9 + 9 + (7 * 1)*/
#define CONFIGDESC_SIZE   (USB_CONFIG_DESCSIZE +                  \
                           USB_INTERFACE_DESCSIZE +               \
                           USB_HID_DESCSIZE +                     \
                           (USB_ENDPOINT_DESCSIZE * NUM_EP_USED))

/* This array contains the Configuration Descriptor and all
 * Interface and Endpoint Descriptors for the device.
 * Refer to the USB 2.0 Specification, chapter 9.6. */
SL_ALIGN(4)
static const uint8_t configDesc[] SL_ATTRIBUTE_ALIGN(4) =
{
  /*** Configuration descriptor ***/
  USB_CONFIG_DESCSIZE,                  /* bLength              */  // 9
  USB_CONFIG_DESCRIPTOR,                /* bDescriptorType      */  // 2

  CONFIGDESC_SIZE,                      /* wTotalLength (LSB)  = 9 + 9 + 9 + (7 * 1)*/
  CONFIGDESC_SIZE>>8,                   /* wTotalLength (MSB)   */

  1,                                    /* bNumInterfaces       */
  1,                                    /* bConfigurationValue  */
  0,                                    /* iConfiguration       */
  0xC0,                                 /* bmAttrib */  // 0x80 = buspowered | 0x40 = self-powered = 0xc0
  CONFIG_DESC_MAXPOWER_mA( 100 ),       /* bMaxPower: 100 mA    */

  /*** Interface descriptor ***/
  USB_INTERFACE_DESCSIZE,               /* bLength              */  // 9
  USB_INTERFACE_DESCRIPTOR,             /* bDescriptorType      */  // 4
  0,                                    /* bInterfaceNumber     */
  0,                                    /* bAlternateSetting    */
  NUM_EP_USED,                          /* bNumEndpoints        */  // 2
  USB_CLASS_HID,                        /* bInterfaceClass      */  // 3
  0,                                    /* bInterfaceSubClass   */  
  0,                                    /* bInterfaceProtocol   */
  4,                                    /* iInterface           */ // (string index)

  /*** HID descriptor ***/
  USB_HID_DESCSIZE,                     /* bLength               */ // 9
  USB_HID_DESCRIPTOR,                   /* bDescriptorType       */ // 0x21
  0x11,                                 /* bcdHID (LSB)          */
  0x01,                                 /* bcdHID (MSB)          */
  0,                                    /* bCountryCode          */
  1,                                    /* bNumDescriptors       */
  USB_HID_REPORT_DESCRIPTOR,            /* bDecriptorType        */ // 0x22
  sizeof(MyHIDReportDescriptor),        /* wDescriptorLength(LSB)*/
  sizeof(MyHIDReportDescriptor) >> 8,   /* wDescriptorLength(MSB)*/
  
  /*** Endpoint Descriptor (IN) ***/
  USB_ENDPOINT_DESCSIZE,                /* bLength              */  // 7
  USB_ENDPOINT_DESCRIPTOR,              /* bDescriptorType      */  // 5
  EP_IN,                                /* bEndpointAddress     */  // 0x81 == 0x80 | 0x01
  USB_EPTYPE_INTR,                      /* bmAttributes         */  // 0x03
  USB_FS_INTR_EP_MAXSIZE,               /* wMaxPacketSize (LSB) */
  0,                                    /* wMaxPacketSize (MSB) */
  1,                                    /* bInterval            */
  
  /*** Endpoint Descriptor (OUT) ***/
  USB_ENDPOINT_DESCSIZE,                /* bLength              */   // 7
  USB_ENDPOINT_DESCRIPTOR,              /* bDescriptorType      */   // 5
  EP_OUT,                               /* bEndpointAddress     */   // 0x01 = 0x00 | 0x01
  USB_EPTYPE_INTR,                      /* bmAttributes         */   // 0x03
  USB_FS_INTR_EP_MAXSIZE,               /* wMaxPacketSize (LSB) */
  0,                                    /* wMaxPacketSize (MSB) */
  1,                                    /* bInterval            */
   
};

const void *USBDESC_HidDescriptor = (void*)  &configDesc[USB_CONFIG_DESCSIZE + USB_INTERFACE_DESCSIZE];

/* Define the String Descriptor for the device. String must be properly
 * aligned and unicode encoded. The first element defines the language id. 
 * Here 0x04 = United States, 0x09 = English. 
 * Refer to the USB Language Identifiers documentation. */
STATIC_CONST_STRING_DESC_LANGID( langID, 0x04, 0x09 );
STATIC_CONST_STRING_DESC( iManufacturer, 'T','h','i','n','g','M');
                          
STATIC_CONST_STRING_DESC( iProduct     , 'b','l','i','n','k','(','1',')',' ','m','k','3',
                                         ' ','0','0','4');
STATIC_CONST_STRING_DESC( iInterface   , 'b','l','i','n','k','1',' ', 'h','i','d');

//STATIC_CONST_STRING_DESC( iSerialNumber, '2','2','0','0','1','2','3','4' );

SL_ALIGN(4)
static uint8_t iSerialNumber[] =  { 18,USB_STRING_DESCRIPTOR,
                                    '1',0,'2',0,'3',0,'4',0,'5',0,'6',0,'7',0,'8',0,
                                    0,0};
////                                len,type,bytes...,null

static const void * const strings[] =
{
  &langID,
  &iManufacturer,
  &iProduct,
  &iSerialNumber,
  &iInterface
};

/* Endpoint buffer sizes. Use 1 for Control/Interrupt
 * endpoints and 2 for Bulk endpoints. */
static const uint8_t bufferingMultiplier[ NUM_EP_USED + 1 ] = 
{ 
  1,  /* Control */
  1,  /* interrupt */
  1   /* interrupt */
  //2,  /* Bulk */
  //2   /* Bulk */
};



#ifdef __cplusplus
}
#endif

