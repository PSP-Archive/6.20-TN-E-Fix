	.set noreorder
	
#include "pspstub.s"

	STUB_START "vsh",0x00090011,0x00020005
	STUB_FUNC  0xE73C355B,vshGetRegistryValue
	STUB_FUNC  0x2375A440,vshSetRegistryValue
	STUB_END
