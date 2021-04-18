	.set noreorder
	
#include "pspstub.s"

	STUB_START "scePaf",0x00090011,0x00120005
	STUB_FUNC  0x03A0E8C2,sce_paf_private_wcslen
	STUB_FUNC  0xE8473E80,sce_paf_private_sprintf
	STUB_FUNC  0xF7A832C8,sce_paf_private_memcpy
	STUB_FUNC  0xBF48C1FC,sce_paf_private_memset
	STUB_FUNC  0x58189108,sce_paf_private_strlen
	STUB_FUNC  0xE5A74996,sce_paf_private_strcpy
	STUB_FUNC  0x4F487FBC,sce_paf_private_strncpy
	STUB_FUNC  0x4900119B,sce_paf_private_strcmp
	STUB_FUNC  0xE00E38F8,sce_paf_private_strncmp
	STUB_FUNC  0x3029535C,sce_paf_private_strchr
	STUB_FUNC  0x42D04DD2,sce_paf_private_strrchr
	STUB_FUNC  0x2C433251,sce_paf_private_strpbrk
	STUB_FUNC  0x0C962B6E,sce_paf_private_strtoul
	STUB_FUNC  0xF0D98BD1,sce_paf_private_malloc
	STUB_FUNC  0xE0E8820F,sce_paf_private_free
	STUB_FUNC  0xCB608DE5,scePafGetText
	STUB_FUNC  0x511991AE,PAF_Resource_GetPageNodeByID
	STUB_FUNC  0xC8C59436,PAF_Resource_ResolveRefWString
	STUB_END
