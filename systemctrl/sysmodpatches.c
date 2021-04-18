#include <pspsdk.h>
#include <pspkernel.h>
#include <pspctrl.h>
#include <pspusb.h>
#include <systemctrl.h>
#include <string.h>

#include "sysmodpatches.h"
#include "main.h"
#include "usbdevice.h"
#include "conf.h"
#include "vshmenu.h"

SceUID vshmenu_mod;

int up = 0, dn = 0;

u8 set;
int firsttime, curtime;

int (* ctrl_handler)(SceCtrlData *pad_data, int count);

int unk_A7C0 = 0; //0x0000A7C0
int unk_A7C4 = 0; //0x0000A7C4

char *reboot_module_after; //0x0000AB88
int reboot_size; //0x0000AB94

int (* scePowerSetClockFrequency_k)(int cpufreq, int ramfreq, int busfreq); //0x0000AB98
int (* sfoRead)(void *buf, void *a1, void *a2); //0x0000AB9C

u32 set_p2 = 0; //0x0000ABA0;
u32 set_p8 = 0; //0x0000ABA8;

void *reboot_buf; //0x0000ABB4
int reboot_flags; //0x0000ABC0

int (* decompress_kle)(void *outbuf, u32 outcapacity, void *inbuf, void *unk); //0x0000ABC4

TNConfig config; //0x0000ABEC

//0x00001764
void sctrlHENLoadModuleOnReboot(char *module_after, void *buf, int size, int flags)
{
	reboot_module_after = module_after;
	reboot_buf = buf;
	reboot_size = size;
	reboot_flags = flags;
}

//0x00001794
void sctrlHENSetP2P8()
{
	if(set_p2 != 0 && (set_p2 + set_p8) <= 51)
	{
		memset((void *)0xBC000040, -1, 0x40);

		PartitionInfo *(* GetPartition)(int pid) = (void *)0x88003E2C;

		int size_p2 = set_p2 * 1024 * 1024;
		PartitionInfo *part2 = GetPartition(2);
		part2->size = size_p2;
		part2->head->subblocks[1].pos = 0xFC | (size_p2 << 1);

		int size_p8 = set_p8 * 1024 * 1024;
		PartitionInfo *part8 = GetPartition(9);
		part8->addr = 0x88800000 + size_p2;
		part8->size = size_p8;
		part8->head->subblocks[1].pos = 0xFC | (size_p8 << 1);
	}
}

//0x0000183C
int sceChkregGetPsCodePatched(u8 *pscode)
{
	pscode[0] = 1;
	pscode[1] = 0;
	pscode[2] = config.fakeregion >= 12 ? config.fakeregion - 11 : config.fakeregion + 2;
	if(pscode[2] == 2) pscode[2]++;
	pscode[3] = 0;
	pscode[4] = 1;
	pscode[5] = 0;
	pscode[6] = 1;
	pscode[7] = 0;

	return 0;
}

//0x00001980
__attribute__((noinline)) u32 FindPowerDriverFunction(u32 nid)
{
	return FindProc("scePower_Service", "scePower_driver", nid);
}

//0x00001998
__attribute__((noinline)) u32 FindPowerFunction(u32 nid)
{
	return FindProc("scePower_Service", "scePower", nid);
}

//0x000019B0
SceUInt time_handler()
{
	int (* scePowerIsBatteryCharging620)() = (void *)FindPowerFunction(0x1E490401);
	if(!scePowerIsBatteryCharging620())
	{
		if(unk_A7C0)
		{
			if(unk_A7C4)
			{
				int (* scePowerBatteryDisableUsbCharging620)() = (void *)FindPowerDriverFunction(0x90285886);
				scePowerBatteryDisableUsbCharging620(0);
			}
			unk_A7C0 = 0;
			unk_A7C4 = !unk_A7C4;
			return 5000000;
		}
		else
		{
			int (* scePowerBatteryEnableUsbCharging620)() = (void *)FindPowerDriverFunction(0x733F973B);
			scePowerBatteryEnableUsbCharging620(1);
			unk_A7C0 = 1;
			return 15000000;
		}
	}
	return 2000000;
}

//0x00001A70
int sctrlHENSetSpeed(int cpu, int bus)
{
	int (* scePowerSetClockFrequency2)(int cpufreq, int ramfreq, int busfreq) = (void *)FindPowerFunction(0x545A7F3C);
	scePowerSetClockFrequency_k = scePowerSetClockFrequency2;
	return scePowerSetClockFrequency2(cpu, cpu, bus);
}

//0x00001CE8
__attribute__((noinline)) void SetConfig(TNConfig *conf)
{
	memcpy(&config, conf, sizeof(TNConfig));
}

//0x00001DB0
void PatchUmdMan(u32 text_addr)
{
	/* Patch sceKernelBootFrom to return PSP_BOOT_DISC */
	if(sceKernelBootFrom() == PSP_BOOT_MS)
	{
		_sw(0x24020000 | PSP_BOOT_DISC, text_addr + 0x431C);
		ClearCaches();
	}
}

//0x00001DE0
int sctrlHENSetMemory(u32 p2, u32 p8)
{
	if(p2 == 0 || (p2 + p8) > 51) return 0x80000107;
	u32 k1 = pspSdkSetK1(0);
	set_p2 = p2;
	set_p8 = p8;
	pspSdkSetK1(k1);
	return 0;
}

//0x00001FB4
int sfoReadPatched(void *buf, void *a1, void *a2)
{
	int memsize = 0;

	if(set_p2 == 0)
	{
		SFOHeader *header = (SFOHeader *)buf;
		SFOEntry *entries = (SFOEntry *)((u32)header + 0x14);

		int i;
		for(i = 0; i < header->count; i++)
		{
			if(strcmp((char *)((u32)header + header->keyofs + entries[i].nameofs), "MEMSIZE") == 0)
			{
				memcpy(&memsize, (void *)((u32)header + header->valofs + entries[i].dataofs), sizeof(memsize));
			}
		}

		if(memsize)
		{
			sctrlHENSetMemory(51, 0);
			sctrlHENSetP2P8();
		}
	}
	else
	{
		sctrlHENSetP2P8();
		set_p2 = 0;
	}

	return sfoRead(buf, a1, a2);
}

//0x000020DC
void SetSpeed(int cpu, int bus)
{
	if(cpu == 20 || cpu == 75 || cpu == 100 || cpu == 133 || cpu == 222 || cpu == 266 || cpu == 300 || cpu == 333)
	{
		scePowerSetClockFrequency_k = (void *)FindPowerFunction(0x737486F2);
		scePowerSetClockFrequency_k(cpu, cpu, bus);

		if(sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_VSH)
		{
			MAKE_DUMMY_FUNCTION0((u32)scePowerSetClockFrequency_k);
			MAKE_DUMMY_FUNCTION0((u32)FindPowerFunction(0x545A7F3C));
			MAKE_DUMMY_FUNCTION0((u32)FindPowerFunction(0xB8D7B3FB));
			MAKE_DUMMY_FUNCTION0((u32)FindPowerFunction(0x843FBF43));
			MAKE_DUMMY_FUNCTION0((u32)FindPowerFunction(0xEBD177D6));
			ClearCaches();
		}
	}
}

//0x00002370
void PatchChkreg()
{
	u32 pscode = FindProc("sceChkreg", "sceChkreg_driver", 0x59F8491D);
	if(pscode)
	{
		if(config.fakeregion)
		{
			REDIRECT_FUNCTION(pscode, sceChkregGetPsCodePatched);
		}
	}
	ClearCaches();
}

//0x000025F8
void OnImposeLoad()
{
	sctrlSEGetConfig(&config);

	PatchChkreg();

	if(psp_model != 0 && config.usbcharge_slimcolors)
	{
		SceUID timer = sceKernelCreateVTimer("", NULL);
		if(timer >= 0)
		{
			sceKernelStartVTimer(timer);
			sceKernelSetVTimerHandlerWide(timer, 5000000, time_handler, NULL);

			SceModule2 *mod = sceKernelFindModuleByName620("sceUSB_Driver");
			if(mod)
			{
				MAKE_DUMMY_FUNCTION0(mod->text_addr + 0x8FE8);
				MAKE_DUMMY_FUNCTION0(mod->text_addr + 0x8FF0);
				ClearCaches();
			}
		}
	}

	ClearCaches();
}

//0x00002790
void PatchWlan(u32 text_addr)
{
	_sw(0, text_addr + 0x2690);
	ClearCaches();
}

//0x0000279C
void PatchPower(u32 text_addr)
{
	_sw(0, text_addr + 0xCC8);
	ClearCaches();
}

//0x00002880
void PatchUmdCache(u32 text_addr)
{
	if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME)
	{
		if(sceKernelBootFrom() == PSP_BOOT_MS)
		{
			MAKE_DUMMY_FUNCTION1(text_addr + 0x9C8);
			ClearCaches();
		}
	}
}

//0x00002904
int decompress_kle_patched(void *outbuf, u32 outcapacity, void *inbuf, void *unk)
{
	memcpy((void *)0x88FC0000, rebootex, size_rebootex);

	memset((void *)0x88FB0000, 0, 0x20);
	*(u32 *)0x88FB0000 = psp_model;
	*(u32 *)0x88FB0004 = size_rebootex;
	*(u32 *)0x88FB0008 = set_p2;
	*(u32 *)0x88FB000C = set_p8;
	*(u32 *)0x88FB0010 = (u32)reboot_module_after;
	*(u32 *)0x88FB0014 = (u32)reboot_buf;
	*(u32 *)0x88FB0018 = reboot_size;
	*(u32 *)0x88FB001C = reboot_flags;

	return decompress_kle(outbuf, outcapacity, inbuf, unk);
}

//0x00002ABC
void PatchLoadExec(u32 text_addr)
{
	static u32 ofs_go[] = { 0x2F28, 0x2F74, 0x25A4, 0x25E8 };
	static u32 ofs_other[] = { 0x2CD8, 0x2D24, 0x2350, 0x2394 };

	u32 *ofs = ofs_other;
	if(psp_model == 4) ofs = ofs_go;

	decompress_kle = (void *)text_addr;
	MAKE_CALL(text_addr + ofs[0], decompress_kle_patched);
	_sw(0x3C0188FC, text_addr + ofs[1]);

	/* Allow sceKernelLoadExecVSH in whatever user level */
	_sw(0x1000000B, text_addr + ofs[2]);
	_sw(0, text_addr + ofs[3]);

	/* Allow sceKernelExitVSHVSH in whatever user level */
	_sw(0x10000008, text_addr + 0x1674);
	_sw(0, text_addr + 0x16A8);
}

//0x00002B18
void PatchMediaSync(u32 text_addr)
{
	char *filename = sceKernelInitFileName();
	if(filename && strstr(filename, ".PBP"))
	{
		/* Avoid 0x80120005 sfo error */
		_sw(0x00008821, text_addr + 0x960);
		_sw(0x00008821, text_addr + 0x83C);

		if(psp_model != 0)
		{
			MAKE_CALL(text_addr + 0x954, sfoReadPatched);
			sfoRead = (void *)text_addr + 0xF00;
		}

		ClearCaches();
	}

	SceModule2 *mod = sceKernelFindModuleByName620("sceLoadExec");
	PatchLoadExec(mod->text_addr);

	PatchMesgLed();

	ClearCaches();
}

int vctrlVSHExitVSHMenu(TNConfig *conf)
{
	int k1 = pspSdkSetK1(0);

	ctrl_handler = NULL;

	int prev_vshcpuspeed = config.vshcpuspeed;

	SetConfig(conf);

	if(set)
	{
		if(config.vshcpuspeed != prev_vshcpuspeed)
		{
			if(config.vshcpuspeed) SetSpeed(config.vshcpuspeed, config.vshbusspeed);
			else SetSpeed(222, 111);
			curtime = sceKernelGetSystemTimeLow();
		}
	}

	pspSdkSetK1(k1);
	return 0;
}

int vctrlVSHRegisterVshMenu(int (* ctrl)(SceCtrlData *, int))
{
	int k1 = pspSdkSetK1(0);
	ctrl_handler = (void *)((u32)ctrl | 0x80000000);
	pspSdkSetK1(k1);
	return 0;
}

int sceCtrlReadBufferPositivePatched(SceCtrlData *pad_data, int count)
{
	int res = sceCtrlReadBufferPositive620(pad_data, count);
	int k1 = pspSdkSetK1(0);

	if(!set)
	{
		if(config.vshcpuspeed)
		{
			int timelow = sceKernelGetSystemTimeLow();
			if(timelow - firsttime >= 10 * 1000 * 1000)
			{
				set = 1;
				SetSpeed(config.vshcpuspeed, config.vshbusspeed);
				curtime = timelow;
			}
		}
	}
	else
	{
		if(config.vshcpuspeed && config.vshcpuspeed != 222 && scePowerGetCpuClockFrequency620() == 222)
		{
			int timelow = sceKernelGetSystemTimeLow();
			if(timelow - curtime >= 1 * 1000 * 1000)
			{
				SetSpeed(config.vshcpuspeed, config.vshbusspeed);
				curtime = timelow;
			}
		}
	}

	if(!sceKernelFindModuleByName620("TNVshMenu"))
	{
		if(config.fastscrollmusic && sceKernelFindModuleByName620("music_browser_module"))
		{
			int i;
			for(i = 0; i < count; i++)
			{
				if(pad_data[i].Buttons & PSP_CTRL_UP)
				{
					if(up > 7)
					{
	  					pad_data[i].Buttons ^= PSP_CTRL_UP;
						up = 7;
					}
					else up++;
				}
				else up = 0;

				if(pad_data[i].Buttons & PSP_CTRL_DOWN)
				{
					if(dn > 7)
					{
						pad_data[i].Buttons ^= PSP_CTRL_DOWN;
						dn = 7;
					}
					else dn++;
				}
				else dn = 0;

				if(up || dn)
				{
					int side = 0;

					if(pad_data[i].Buttons & PSP_CTRL_LEFT) side |= PSP_CTRL_LEFT;
					if(pad_data[i].Buttons & PSP_CTRL_RIGHT) side |= PSP_CTRL_RIGHT;

	 		 		pad_data[i].Buttons ^= side;
				}
			}
		}

		if(!config.novshmenu)
		{
			if(!sceKernelFindModuleByName620("htmlviewer_plugin_module") &&
			   !sceKernelFindModuleByName620("sceVshOSK_Module") &&
			   !sceKernelFindModuleByName620("camera_plugin_module"))
			{
				if(pad_data->Buttons & PSP_CTRL_SELECT)
				{
					vshmenu_mod = sceKernelLoadModuleBuffer620(size_vshmenu, vshmenu, 0, NULL);
					if(vshmenu_mod >= 0)
					{
						sceKernelStartModule620(vshmenu_mod, 0, NULL, NULL, NULL);
						pad_data->Buttons &= ~PSP_CTRL_SELECT;
					}
				}
			}
		}
	}
	else
	{
		if(ctrl_handler) ctrl_handler(pad_data, count);
		else
		{
			if(vshmenu_mod >= 0) if(sceKernelStopModule620(vshmenu_mod, 0, NULL, NULL, NULL) >= 0) sceKernelUnloadModule620(vshmenu_mod);
		}
	}

	pspSdkSetK1(k1);
	return res;
}

SceUID sceKernelLoadModuleVSHPatched(const char *file, int flags, SceKernelLMOption *option)
{
	SceUID modid = sceKernelLoadModuleVSH620(file, flags, option);
	if(modid >= 0)
	{
		int k1 = pspSdkSetK1(0);
		if(!config.notusetnnetupd)
		{
			SceModule2 *mod;
			if((mod = sceKernelFindModuleByName620("SceUpdateDL_Library")))
			{
				if(!sceKernelFindModuleByName620("sceVshNpSignin_Module"))
				{
					if(!sceKernelFindModuleByName620("sceNpSignupPlugin_Module"))
					{
						strcpy((char *)mod->text_addr + 0x32C4, "http://tnhen.bplaced.net/psp-updatelist.txt");
						_sw(mod->text_addr + 0x32C4, mod->text_addr + 0x34D0);
						ClearCaches();
					}
				}
			}
		}
		pspSdkSetK1(k1);
	}

	return modid;
}

int sceUsbStartPatched(const char *driverName, int size, void *args)
{
	int k1 = pspSdkSetK1(0);
	if(strcmp(driverName, "USBStor_Driver") == 0)
	{
		if(config.usbdevice > 0 && config.usbdevice < 6)
		{
			pspUsbDeviceInit();
			if(pspUsbDeviceSetDevice(config.usbdevice - 1, !config.notprotectflash0, 0) < 0) pspUsbDeviceFinishDevice();
		}
	}

	pspSdkSetK1(k1);
	return sceUsbStart620(driverName, size, args);
}

int sceUsbStopPatched(const char *driverName, int size, void *args)
{
	int res = sceUsbStop620(driverName, size, args);
	int k1 = pspSdkSetK1(0);

	if(strcmp(driverName, "USBStor_Driver") == 0)
	{
		if(config.usbdevice > 0 && config.usbdevice < 6)
		{
			pspUsbDeviceFinishDevice();
		}
	}

	pspSdkSetK1(k1);
	return res;
}

void OnVshLoad()
{
	sctrlSEGetConfig(&config);

	if(config.vshcpuspeed != 0) firsttime = sceKernelGetSystemTimeLow();

	SceModule2 *mod = sceKernelFindModuleByName620("sceVshBridge_Driver");
	u32 text_addr = mod->text_addr;

	MAKE_CALL(text_addr + 0x25C, sceCtrlReadBufferPositivePatched);
	PatchSyscall(FindProc("sceController_Service", "sceCtrl", 0x1F803938), sceCtrlReadBufferPositivePatched); 

	MAKE_CALL(text_addr + 0x1564, sceKernelLoadModuleVSHPatched);

	PatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0xAE5DE6AF), sceUsbStartPatched);
	PatchSyscall(FindProc("sceUSB_Driver", "sceUsb", 0xC2464FA0), sceUsbStopPatched);

	ClearCaches();
}