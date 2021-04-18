	.set noreorder

#include "pspstub.s"

	STUB_START	"sceCtrl_driver",0x00090011,0x00010005
	STUB_FUNC	0xD073ECA4,sceCtrlReadBufferPositive620
	STUB_END
