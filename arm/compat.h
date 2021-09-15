#ifndef _COMPAT_H_
#define _COMPAT_H_

#define iosAlloc(hid, len)	Mem_Alloc(len)
#define iosFree(ptr)		Mem_Free(ptr)
#define IOS_Ioctl		os_ioctl
#define IOS_IoctlAsync		os_ioctl_async
#define IOS_Ioctlv		os_ioctlv
#define IOS_IoctlvAsync		os_ioctlv_async
#define IOS_Open		os_open
#define IOS_Close		os_close

#endif
