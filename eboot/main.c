#include <pspsdk.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <string.h>

#include "rebootex.h"

PSP_MODULE_INFO("TN Homebrew Enabler", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(0);

#define GET_V1(v1) asm volatile ("move %0, $v1\n" : "=r" (v1))
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

int sceUtilityAppletRegisterPowerCallback(int slot, SceUID cbid);
int sceKernelPowerLock(int a0, int a1);

int psp_model;

int (* decompress_kle)(void *outbuf, u32 outcapacity, void *inbuf, void *unk);

int decompress_kle_patched(void *outbuf, u32 outcapacity, void *inbuf, void *unk)
{
	int (* sceKernelGzipDecompress620)(u8 *dest, u32 destSize, const u8 *src, u32 unknown) = (void *)0x8800FA10;
	int size = sceKernelGzipDecompress620((void *)0x88FC0000, 0x10000, rebootex, 0);

	memset((void *)0x88FB0000, 0, 0x20);
	*(u32 *)0x88FB0000 = psp_model;
	*(u32 *)0x88FB0004 = size;

	return decompress_kle(outbuf, outcapacity, inbuf, unk);
}

#define LOADCORE_ADDR 0x88017100

int PatchLoadExec()
{
	SceModule2 *(*sceKernelFindModuleByName620)(const char *name) = (void *)LOADCORE_ADDR + 0x7A78;

	SceModule2 *mod = sceKernelFindModuleByName620("sceLoadExec");
	u32 text_addr = mod->text_addr;

	int (* sceKernelGetModel620)() = (void *)0x8800A1C4;
	psp_model = sceKernelGetModel620();

	static u32 ofs_go[] = { 0x2F28, 0x2F74 };
	static u32 ofs_other[] = { 0x2CD8, 0x2D24 };

	u32 *ofs = ofs_other;
	if(psp_model == 4) ofs = ofs_go;

	decompress_kle = (void *)text_addr;
	MAKE_CALL(text_addr + ofs[0], decompress_kle_patched);
	_sw(0x3C0188FC, text_addr + ofs[1]);

	/* Repair sysmem */
	_sw(0xACC24230, 0x8800CCB0);
	_sw(0x0A003322, 0x8800CCB4);
	_sw(0x00001021, 0x8800CCB8);
	_sw(0x3C058801, 0x8800CCBC);

	return 0;
}

void ClearCaches()
{
	/* Clear Icache */
	int i = 0;
	while(i < 0x100000) i++;

	/* Clear Dcache */
	sceKernelDcacheWritebackInvalidateAll();
}

int main()
{
	/* Patch sceKernelPowerLock */
	SceUID cbid = sceKernelCreateCallback("", NULL, NULL);

	sceUtilityAppletRegisterPowerCallback(0, cbid);
	sceUtilityAppletRegisterPowerCallback(0, cbid);

	u32 power_buf_addr;
	GET_V1(power_buf_addr);

	int slot = (0x8800CCB0 - power_buf_addr) >> 4;
	sceUtilityAppletRegisterPowerCallback(slot, cbid);

	sceKernelDeleteCallback(cbid);

	ClearCaches();

	/* Patch sceLoadExec from kmode */
	u32 var1 = (u32)((u32)PatchLoadExec | 0x80000000);
	u32 var2 = ((u32)&var1) - 0x10;
	sceKernelPowerLock(0, ((u32)&var2) - 0x4234);

	ClearCaches();

	sceKernelExitGame();

	return 0;
}