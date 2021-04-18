#ifndef __USBDEVICE_H__
#define __USBDEVICE_H__

int pspUsbDeviceSetDevice(u32 device, int ronly, int unassign_mask);
int pspUsbDeviceFinishDevice(void);
int pspUsbDeviceInit();

#endif