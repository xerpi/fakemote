#ifndef _USB2_H_
#define _USB2_H_

#include "types.h"

/* Constants */
#define USB_MAXPATH			64

#define USB_OK				0
#define USB_FAILED			1

#define USB_CLASS_HID			0x03
#define USB_SUBCLASS_BOOT		0x01
#define USB_PROTOCOL_KEYBOARD		0x01
#define USB_PROTOCOL_MOUSE		0x02
#define USB_REPTYPE_INPUT		0x01
#define USB_REPTYPE_OUTPUT		0x02
#define USB_REPTYPE_FEATURE		0x03
#define USB_REQTYPE_GET			0xA1
#define USB_REQTYPE_SET			0x21

/* Descriptor types */
#define USB_DT_DEVICE			0x01
#define USB_DT_CONFIG			0x02
#define USB_DT_STRING			0x03
#define USB_DT_INTERFACE		0x04
#define USB_DT_ENDPOINT			0x05
#define USB_DT_HID				0x21
#define USB_DT_REPORT			0x22

/* Standard requests */
#define USB_REQ_GETSTATUS		0x00
#define USB_REQ_CLEARFEATURE		0x01
#define USB_REQ_SETFEATURE		0x03
#define USB_REQ_SETADDRESS		0x05
#define USB_REQ_GETDESCRIPTOR		0x06
#define USB_REQ_SETDESCRIPTOR		0x07
#define USB_REQ_GETCONFIG		0x08
#define USB_REQ_SETCONFIG		0x09
#define USB_REQ_GETINTERFACE		0x0a
#define USB_REQ_SETINTERFACE		0x0b
#define USB_REQ_SYNCFRAME		0x0c

#define USB_REQ_GETPROTOCOL		0x03
#define USB_REQ_SETPROTOCOL		0x0B
#define USB_REQ_GETREPORT		0x01
#define USB_REQ_SETREPORT		0x09

/* Descriptor sizes per descriptor type */
#define USB_DT_DEVICE_SIZE		18
#define USB_DT_CONFIG_SIZE		9
#define USB_DT_INTERFACE_SIZE		9
#define USB_DT_ENDPOINT_SIZE		7
#define USB_DT_ENDPOINT_AUDIO_SIZE	9	/* Audio extension */
#define USB_DT_HID_SIZE				9
#define USB_DT_HUB_NONVAR_SIZE		7

/* control message request type bitmask */
#define USB_CTRLTYPE_DIR_HOST2DEVICE	(0<<7)
#define USB_CTRLTYPE_DIR_DEVICE2HOST	(1<<7)
#define USB_CTRLTYPE_TYPE_STANDARD	(0<<5)
#define USB_CTRLTYPE_TYPE_CLASS		(1<<5)
#define USB_CTRLTYPE_TYPE_VENDOR	(2<<5)
#define USB_CTRLTYPE_TYPE_RESERVED	(3<<5)
#define USB_CTRLTYPE_REC_DEVICE		0
#define USB_CTRLTYPE_REC_INTERFACE	1
#define USB_CTRLTYPE_REC_ENDPOINT	2
#define USB_CTRLTYPE_REC_OTHER		3

#define USB_FEATURE_ENDPOINT_HALT	0

#define USB_ENDPOINT_INTERRUPT		0x03
#define USB_ENDPOINT_IN			0x80
#define USB_ENDPOINT_OUT		0x00

#define USB_OH0_DEVICE_ID		0x00000000				// for completion
#define USB_OH1_DEVICE_ID		0x00200000


/* Structures */
typedef struct _usbendpointdesc
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bEndpointAddress;
	u8 bmAttributes;
	u16 wMaxPacketSize;
	u8 bInterval;
} ATTRIBUTE_PACKED usb_endpointdesc;

typedef struct _usbinterfacedesc
{
	u8 bLength;
	u8 bDescriptorType;
	u8 bInterfaceNumber;
	u8 bAlternateSetting;
	u8 bNumEndpoints;
	u8 bInterfaceClass;
	u8 bInterfaceSubClass;
	u8 bInterfaceProtocol;
	u8 iInterface;
	u8 *extra;
	u8 extra_size;
	struct _usbendpointdesc *endpoints;
} ATTRIBUTE_PACKED usb_interfacedesc;

typedef struct _usbconfdesc
{
	u8 bLength;
	u8 bDescriptorType;
	u16 wTotalLength;
	u8 bNumInterfaces;
	u8 bConfigurationValue;
	u8 iConfiguration;
	u8 bmAttributes;
	u8 bMaxPower;
	struct _usbinterfacedesc *interfaces;
} ATTRIBUTE_PACKED usb_configurationdesc;

typedef struct _usbdevdesc
{
	u8  bLength;
	u8  bDescriptorType;
	u16 bcdUSB;
	u8  bDeviceClass;
	u8  bDeviceSubClass;
	u8  bDeviceProtocol;
	u8  bMaxPacketSize0;
	u16 idVendor;
	u16 idProduct;
	u16 bcdDevice;
	u8  iManufacturer;
	u8  iProduct;
	u8  iSerialNumber;
	u8  bNumConfigurations;
	struct _usbconfdesc *configurations;
} ATTRIBUTE_PACKED usb_devdesc;

typedef struct _usbhiddesc
{
	u8 bLength;
	u8 bDescriptorType;
	u16 bcdHID;
	u8 bCountryCode;
	u8 bNumDescriptors;
	struct {
		u8 bDescriptorType;
		u16 wDescriptorLength;
	} descr[1];
} ATTRIBUTE_PACKED usb_hiddesc;

typedef struct _usb_device_entry {
	s32 device_id;
	u16 vid;
	u16 pid;
	u32 token;
} usb_device_entry;

#endif
