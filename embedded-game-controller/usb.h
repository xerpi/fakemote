#ifndef EGC_USB_H
#define EGC_USB_H

#include "types.h"

/* Constants */
#define EGC_USB_MAXPATH 64

#define EGC_USB_OK     0
#define EGC_USB_FAILED 1

#define EGC_USB_CLASS_HID         0x03
#define EGC_USB_SUBCLASS_BOOT     0x01
#define EGC_USB_PROTOCOL_KEYBOARD 0x01
#define EGC_USB_PROTOCOL_MOUSE    0x02
#define EGC_USB_REPTYPE_INPUT     0x01
#define EGC_USB_REPTYPE_OUTPUT    0x02
#define EGC_USB_REPTYPE_FEATURE   0x03
#define EGC_USB_REQTYPE_GET       0xA1
#define EGC_USB_REQTYPE_SET       0x21

/* Descriptor types */
#define EGC_USB_DT_DEVICE    0x01
#define EGC_USB_DT_CONFIG    0x02
#define EGC_USB_DT_STRING    0x03
#define EGC_USB_DT_INTERFACE 0x04
#define EGC_USB_DT_ENDPOINT  0x05
#define EGC_USB_DT_HID       0x21
#define EGC_USB_DT_REPORT    0x22

/* Standard requests */
#define EGC_USB_REQ_GETSTATUS     0x00
#define EGC_USB_REQ_CLEARFEATURE  0x01
#define EGC_USB_REQ_SETFEATURE    0x03
#define EGC_USB_REQ_SETADDRESS    0x05
#define EGC_USB_REQ_GETDESCRIPTOR 0x06
#define EGC_USB_REQ_SETDESCRIPTOR 0x07
#define EGC_USB_REQ_GETCONFIG     0x08
#define EGC_USB_REQ_SETCONFIG     0x09
#define EGC_USB_REQ_GETINTERFACE  0x0a
#define EGC_USB_REQ_SETINTERFACE  0x0b
#define EGC_USB_REQ_SYNCFRAME     0x0c

#define EGC_USB_REQ_GETPROTOCOL 0x03
#define EGC_USB_REQ_SETPROTOCOL 0x0B
#define EGC_USB_REQ_GETREPORT   0x01
#define EGC_USB_REQ_SETREPORT   0x09

/* Descriptor sizes per descriptor type */
#define EGC_USB_DT_DEVICE_SIZE         18
#define EGC_USB_DT_CONFIG_SIZE         9
#define EGC_USB_DT_INTERFACE_SIZE      9
#define EGC_USB_DT_ENDPOINT_SIZE       7
#define EGC_USB_DT_ENDPOINT_AUDIO_SIZE 9 /* Audio extension */
#define EGC_USB_DT_HID_SIZE            9
#define EGC_USB_DT_HUB_NONVAR_SIZE     7

/* control message request type bitmask */
#define EGC_USB_CTRLTYPE_DIR_HOST2DEVICE (0 << 7)
#define EGC_USB_CTRLTYPE_DIR_DEVICE2HOST (1 << 7)
#define EGC_USB_CTRLTYPE_TYPE_STANDARD   (0 << 5)
#define EGC_USB_CTRLTYPE_TYPE_CLASS      (1 << 5)
#define EGC_USB_CTRLTYPE_TYPE_VENDOR     (2 << 5)
#define EGC_USB_CTRLTYPE_TYPE_RESERVED   (3 << 5)
#define EGC_USB_CTRLTYPE_REC_DEVICE      0
#define EGC_USB_CTRLTYPE_REC_INTERFACE   1
#define EGC_USB_CTRLTYPE_REC_ENDPOINT    2
#define EGC_USB_CTRLTYPE_REC_OTHER       3

#define EGC_USB_REQTYPE_INTERFACE_GET                                                              \
    (EGC_USB_CTRLTYPE_DIR_DEVICE2HOST | EGC_USB_CTRLTYPE_TYPE_CLASS |                              \
     EGC_USB_CTRLTYPE_REC_INTERFACE)
#define EGC_USB_REQTYPE_INTERFACE_SET                                                              \
    (EGC_USB_CTRLTYPE_DIR_HOST2DEVICE | EGC_USB_CTRLTYPE_TYPE_CLASS |                              \
     EGC_USB_CTRLTYPE_REC_INTERFACE)
#define EGC_USB_REQTYPE_ENDPOINT_GET                                                               \
    (EGC_USB_CTRLTYPE_DIR_DEVICE2HOST | EGC_USB_CTRLTYPE_TYPE_CLASS | EGC_USB_CTRLTYPE_REC_ENDPOINT)
#define EGC_USB_REQTYPE_ENDPOINT_SET                                                               \
    (EGC_USB_CTRLTYPE_DIR_HOST2DEVICE | EGC_USB_CTRLTYPE_TYPE_CLASS | EGC_USB_CTRLTYPE_REC_ENDPOINT)

#define EGC_USB_FEATURE_ENDPOINT_HALT 0

#define EGC_USB_ENDPOINT_INTERRUPT 0x03
#define EGC_USB_ENDPOINT_IN        0x80
#define EGC_USB_ENDPOINT_OUT       0x00

/* Structures */
typedef struct egc_usb_endpointdesc_t {
    u8 bLength;
    u8 bDescriptorType;
    u8 bEndpointAddress;
    u8 bmAttributes;
    u16 wMaxPacketSize;
    u8 bInterval;
} ATTRIBUTE_PACKED egc_usb_endpointdesc_t;

typedef struct egc_usb_interfacedesc_t {
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
    struct egc_usb_endpointdesc_t *endpoints;
} ATTRIBUTE_PACKED egc_usb_interfacedesc_t;

typedef struct egc_usb_confdesc_t {
    u8 bLength;
    u8 bDescriptorType;
    u16 wTotalLength;
    u8 bNumInterfaces;
    u8 bConfigurationValue;
    u8 iConfiguration;
    u8 bmAttributes;
    u8 bMaxPower;
    struct egc_usb_interfacedesc_t *interfaces;
} ATTRIBUTE_PACKED egc_usb_configurationdesc_t;

typedef struct egc_usb_devdesc_t {
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdUSB;
    u8 bDeviceClass;
    u8 bDeviceSubClass;
    u8 bDeviceProtocol;
    u8 bMaxPacketSize0;
    u16 idVendor;
    u16 idProduct;
    u16 bcdDevice;
    u8 iManufacturer;
    u8 iProduct;
    u8 iSerialNumber;
    u8 bNumConfigurations;
} ATTRIBUTE_PACKED egc_usb_devdesc_t;

typedef struct egc_usb_hiddesc_t {
    u8 bLength;
    u8 bDescriptorType;
    u16 bcdHID;
    u8 bCountryCode;
    u8 bNumDescriptors;
    struct {
        u8 bDescriptorType;
        u16 wDescriptorLength;
    } descr[1];
} ATTRIBUTE_PACKED egc_usb_hiddesc_t;

typedef struct egc_usb_device_t egc_usb_device_t;

typedef enum egc_usb_transfer_type_e {
    EGC_USB_TRANSFER_CONTROL,
    EGC_USB_TRANSFER_ISOCHRONOUS,
    EGC_USB_TRANSFER_BULK,
    EGC_USB_TRANSFER_INTERRUPT,
} egc_usb_transfer_type_e;

typedef enum egc_usb_transfer_status_e {
    EGC_USB_TRANSFER_STATUS_UNSET = 0,
    EGC_USB_TRANSFER_STATUS_COMPLETED,
    EGC_USB_TRANSFER_STATUS_ERROR,
} egc_usb_transfer_status_e;

typedef struct egc_usb_transfer_t egc_usb_transfer_t;

struct egc_usb_transfer_t {
    egc_usb_device_t *device;
    egc_usb_transfer_type_e transfer_type;
    egc_usb_transfer_status_e status;
    u8 endpoint;
    u16 length;  /* length of actually used data */
    u16 bufsize; /* data buffer size */
    u8 *data;
} ATTRIBUTE_PACKED;

typedef void (*egc_transfer_cb)(egc_usb_transfer_t *transfer);

#endif
