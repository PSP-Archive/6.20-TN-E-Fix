	.set noreorder

#include "pspstub.s"

	STUB_START "scePaf",0x00090011,0x00040005
	STUB_FUNC  0x58189108,sce_paf_private_strlen
	STUB_FUNC  0xE8473E80,sce_paf_private_sprintf
	STUB_FUNC  0xF7A832C8,sce_paf_private_memcpy
	STUB_FUNC  0xE5A74996,sce_paf_private_strcpy
	STUB_END
