	.set noreorder

#include "pspstub.s"

	STUB_START	"sceUmd",0x00090011,0x00040005
	STUB_FUNC	0x74145046,sceUmdWaitDriveStatCB620
	STUB_FUNC	0x8EF08FCE,sceUmdWaitDriveStat620
	STUB_FUNC	0xC6183D47,sceUmdActivate620
	STUB_FUNC	0xE70E8FFB,sceUmdCheckMedium620
	STUB_END
