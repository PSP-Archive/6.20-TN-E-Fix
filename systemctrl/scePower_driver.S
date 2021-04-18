	.set noreorder

#include "pspstub.s"

	STUB_START	"scePower_driver",0x00090011,0x00010005
	STUB_FUNC	0xFDB5BFE9,scePowerGetCpuClockFrequency620
	STUB_END
