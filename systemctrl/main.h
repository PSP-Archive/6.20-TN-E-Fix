#ifndef __MAIN_H__
#define __MAIN_H__

#include <psploadexec_kernel.h>
#include <pspctrl.h>

#define log(...) { char msg[256]; sprintf(msg,__VA_ARGS__); logmsg(msg); }

#define PatchSyscall sctrlHENPatchSyscall
#define FindDriver sctrlHENFindDriver

#define FW_TO_FIRMWARE(f) ((((f >> 8) & 0xF) << 24) | (((f >> 4) & 0xF) << 16) | ((f & 0xF) << 8) | 0x10)
#define FIRMWARE_TO_FW(f) ((((f >> 24) & 0xF) << 8) | (((f >> 16) & 0xF) << 4) | ((f >> 8) & 0xF))

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);
#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);
#define REDIRECT_FUNCTION(a, f) _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a); _sw(0, a + 4);
#define MAKE_DUMMY_FUNCTION0(a) _sw(0x03E00008, a); _sw(0x00001021, a + 4);
#define MAKE_DUMMY_FUNCTION1(a) _sw(0x03E00008, a); _sw(0x24020001, a + 4);

#define PSP_INIT_KEYCONFIG_UPDATER 0x00000110

typedef struct
{
	u32 e_magic;
	u8 e_class;
	u8 e_data;
	u8 e_idver;
	u8 e_pad[9];
	u16 e_type;
	u16 e_machine;
	u32 e_version;
	u32 e_entry;
	u32 e_phoff;
	u32 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
} __attribute__((packed)) Elf32_Ehdr;

typedef struct
{
	u32 sh_name;
	u32 sh_type;
	u32 sh_flags;
	u32 sh_addr;
	u32 sh_offset;
	u32 sh_size;
	u32 sh_link;
	u32 sh_info;
	u32 sh_addralign;
	u32 sh_entsize;
} __attribute__((packed)) Elf32_Shdr;

extern int psp_model;
extern char *rebootex;
extern int size_rebootex;

SceModule2 *sceKernelFindModuleByName620(const char *modname);
SceModule2 *sceKernelFindModuleByAddress620(u32 addr);
SceModule2 *sceKernelFindModuleByUID620(SceUID modid);
int sceKernelCheckExecFile620(u32 *buf, int *check);
int sceKernelProbeExecutableObject620(void *buf, int *check);

SceUID sceKernelLoadModule620(const char *path, int flags, SceKernelLMOption *option);
SceUID sceKernelLoadModuleWithApitype2620(int apitype, const char *path, int flags, SceKernelLMOption *option);
SceUID sceKernelLoadModuleVSH620(const char *path, int flags, SceKernelLMOption *option);
SceUID sceKernelLoadModuleBuffer620(SceSize bufsize, void *buf, int flags, SceKernelLMOption *option);
int sceKernelStartModule620(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
int sceKernelStopModule620(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option);
int sceKernelUnloadModule620(SceUID modid);

int sceKernelExitVSHVSH620(struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHMs1620(const char *file, struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHMs2620(const char *file, struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHMs3620(const char *file, struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHMs4620(const char *file, struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHDisc620(const char *file, struct SceKernelLoadExecVSHParam *param);
int sceKernelLoadExecVSHDiscUpdater620(const char *file, struct SceKernelLoadExecVSHParam *param);

SceUID sceKernelCreateHeap620(SceUID partitionid, SceSize size, int unk, const char *name);
void *sceKernelAllocHeapMemory620(SceUID heapid, SceSize size);
int sceKernelFreeHeapMemory620(SceUID heapid, void *block);
SceSize sceKernelHeapTotalFreeSize620(SceUID heapid);
SceUID sceKernelAllocPartitionMemory620(SceUID partitionid, const char *name, int type, SceSize size, void *addr);
void *sceKernelGetBlockHeadAddr620(SceUID blockid);
int sceKernelFreePartitionMemory620(SceUID blockid);
int sceKernelSetDdrMemoryProtection620(void *addr, int size, int prot);
int sceKernelDevkitVersion620();
int sceKernelGetModel620();
int sceKernelGetSystemStatus620();

int scePowerGetCpuClockFrequency620();
int sceCtrlReadBufferPositive620(SceCtrlData *pad_data, int count);

int sceUsbStart620(const char* driverName, int size, void *args);
int sceUsbStop620(const char* driverName, int size, void *args);

int sceUmdWaitDriveStatCB620(int stat, unsigned int timeout);
int sceUmdWaitDriveStat620(int stat);
int sceUmdCheckMedium620(int unk);
int sceUmdActivate620(int unit, const char *drive);

void ClearCaches();
void PatchMesgLed();

#endif