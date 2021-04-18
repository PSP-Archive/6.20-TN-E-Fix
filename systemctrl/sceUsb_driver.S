	.set noreorder

#include "pspstub.s"

	STUB_START	"sceUsb_driver",0x00090011,0x00020005
	STUB_FUNC	0xAE5DE6AF,sceUsbStart620
	STUB_FUNC	0xC2464FA0,sceUsbStop620
	STUB_END
