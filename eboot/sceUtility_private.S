	.set noreorder

#include "pspstub.s"

	STUB_START	"sceUtility_private",0x40010011,0x00010005
	STUB_FUNC	0x764F5A3C,sceUtilityAppletRegisterPowerCallback
	STUB_END
