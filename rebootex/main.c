#include <pspsdk.h>
#include "main.h"
#include "systemctrl.h"
#include "xmbctrl.h"

int Main(void *, void *, int, int);

int _start(void *, void *, int, int) __attribute__((section(".text.start")));
int _start(void *reboot_param, void *exec_param, int api, int initial_rnd)
{
	return Main(reboot_param, exec_param, api, initial_rnd);
}

int (* sceReboot)(void *reboot_param, void *exec_param, int api, int initial_rnd) = (void *)0x88600000;
int (* ClearIcache)() = (void *)0x886001E4;
int (* ClearDcache)() = (void *)0x88600938;

int (* sceBootLfatOpen)(char *file);
int (* sceBootLfatRead)(void *buf, int size);
int (* sceBootLfatClose)();
int (* sceKernelCheckPspConfig)(void *buf, int size, int flag);

int (* DecryptExecutable)(void *buf, int size, int *retSize);
int (* VerifySigncheck)(u8 *buf, int size);

char *reboot_module_after;
void *reboot_buf;
int reboot_size;
int reboot_flags;

int hen_found;
int xmb_found;
int rtm_found;

__attribute__((noinline)) int _memcmp(char *m1, char *m2, int size)
{
	int i;
	for(i = 0; i < size; i++) if(m1[i] != m2[i]) return m2[i] - m1[i];
	return 0;
}

__attribute__((noinline)) void _memset(u8 *m1, int c, int size)
{
	int i;
	for(i = 0; i < size; i++) m1[i] = c;
}

__attribute__((noinline)) void _memcpy(u8 *m1, u8 *m2, int size)
{
	int i;
	for(i = 0; i < size; i++) m1[i] = m2[i];
}

__attribute__((noinline)) void _memmove(void *dst, void *src, int len)
{
	char *d = dst;
	char *s = src;

	if(s < d)
	{
		for(s += len, d += len; len; --len) *--d = *--s;
	}
	else if(s != d)
	{
		for(; len; --len) *d++ = *s++;
	}
}

__attribute__((noinline)) int _strlen(char *str)
{
	int i = 0;
	while(*str)
	{
		str++;
		i++;
	}
	return i;
}

void ClearCaches()
{
	ClearIcache();
	ClearDcache();
}

int sceBootLfatOpenPatched(char *file)
{
	if(_memcmp(file, "/hen.prx", 9) == 0)
	{
		hen_found = 1;
		return 0;
	}
	else if(_memcmp(file, "/xmb.prx", 9) == 0)
	{
		xmb_found = 1;
		return 0;
	}
	else if(_memcmp(file, "/rtm.prx", 9) == 0)
	{
		rtm_found = 1;
		return 0;
	}
	return sceBootLfatOpen(file);
}

int sceBootLfatReadPatched(void *buf, int size)
{
	if(hen_found)
	{
		_memcpy(buf, (void *)systemctrl, size_systemctrl);
		return size_systemctrl;
	}
	else if(xmb_found)
	{
		_memcpy(buf, (void *)xmbctrl, size_xmbctrl);
		return size_xmbctrl;
	}
	else if(rtm_found)
	{
		_memcpy(buf, (void *)reboot_buf, reboot_size);
		return reboot_size;
	}
	return sceBootLfatRead(buf, size);
}

int sceBootLfatClosePatched()
{
	if(hen_found)
	{
		hen_found = 0;
		return 0;
	}
	else if(xmb_found)
	{
		xmb_found = 0;
		return 0;
	}
	else if(rtm_found)
	{
		rtm_found = 0;
		return 0;
	}
	return sceBootLfatClose();
}

int InsertModule(BtcnfHeader *header, char *module_after, char *new_module, u16 flags)
{
	ModuleEntry *modules = (ModuleEntry *)((u32)header + header->modulestart);
	ModeEntry *modes = (ModeEntry *)((u32)header + header->modestart);

	char *modnamestart = (char *)((u32)header + header->modnamestart);
	char *modnameend = (char *)((u32)header + header->modnameend);

	/* Add new_module at end */
	int len = _strlen(new_module) + 1;
	_memcpy((void *)modnameend, (void *)new_module, len);
	header->modnameend += len;

	int i;
	for(i = 0; i < header->nmodules; i++)
	{
		if(_memcmp(modnamestart + modules[i].stroffset, module_after, _strlen(module_after) + 1) == 0)
		{
			/* Move module_after forward */
			_memmove((void *)(modules + i + 1), (void *)(modules + i), (header->nmodules - i) * sizeof(ModuleEntry) + len + modnameend - modnamestart);

			/* Add new_module */
			modules[i].stroffset = modnameend - modnamestart;
			modules[i].flags = flags;

			/* Fix header */
			header->nmodules++;
			header->modnamestart += sizeof(ModuleEntry);
			header->modnameend += sizeof(ModuleEntry);

			for(i = 0; i < header->nmodes; i++) modes[i].maxsearch++;
		}
	}

	return 0;
}

int sceKernelCheckPspConfigPatched(void *buf, int size, int flag)
{
	BtcnfHeader *header = (BtcnfHeader *)buf;
	int res = sceKernelCheckPspConfig(buf, size, flag);
	InsertModule(header, "/kd/init.prx", "/hen.prx", BOOTLOAD_VSH | BOOTLOAD_GAME | BOOTLOAD_UPDATER | BOOTLOAD_POPS | BOOTLOAD_UMDEMU | BOOTLOAD_MLNAPP);
	InsertModule(header, "/kd/me_wrapper.prx", "/xmb.prx", BOOTLOAD_VSH);
	if(reboot_module_after) InsertModule(header, reboot_module_after, "/rtm.prx", reboot_flags);
	return res;
}

int DecryptExecutablePatched(void *buf, int size, int *retSize)
{
	if(*(u32 *)(buf + 0x130) == 0xB301AEBA)
	{
		int gzip_size = *(u32 *)(buf + 0xB0);
		_memmove(buf, buf + 0x150, gzip_size);
		*retSize = gzip_size;
		return 0;
	}
	return DecryptExecutable(buf, size, retSize);
}

int VerifySigncheckPatched(u8 *buf, int size)
{
	int i;
	for(i = 0; i < 0x58; i++) if(buf[0xD4 + i]) return VerifySigncheck(buf, size);
	return 0;
}

int PatchLoadCore(int (* module_bootstart)(SceSize, void *), void *argp)
{
	u32 text_addr = ((u32)module_bootstart) - 0xBC4;

	MAKE_CALL(text_addr + 0x41A4, DecryptExecutablePatched);
	MAKE_CALL(text_addr + 0x68F0, DecryptExecutablePatched);

	MAKE_CALL(text_addr + 0x6914, VerifySigncheckPatched);
	MAKE_CALL(text_addr + 0x6944, VerifySigncheckPatched);
	MAKE_CALL(text_addr + 0x69DC, VerifySigncheckPatched);

	DecryptExecutable = (void *)text_addr + 0x8374;
	VerifySigncheck = (void *)text_addr + 0x835C;

	ClearCaches();

	return module_bootstart(8, argp);
}

int Main(void *reboot_param, void *exec_param, int api, int initial_rnd)
{
	static u32 ofs_phat[] =
	{
		0x82AC, 0x8420, 0x83C4, 0x565C, 0x26DC, 0x274C, 0x2778, 0x70F0,
		0x3798, 0x26D4, 0x2728, 0x2740, 0x5548, 0x7388, 0x2688, 0x2750
	};

	static u32 ofs_other[] =
	{
		0x8374, 0x84E8, 0x848C, 0x5724, 0x27A4, 0x2814, 0x2840, 0x71B8,
		0x3860, 0x279C, 0x27F0, 0x2808, 0x5610, 0x7450, 0x2750, 0x2818
	};

	u32 *ofs = ofs_other;
	if(*(u32 *)0x88FB0000 == 0) ofs = ofs_phat;

	sceBootLfatOpen = (void *)0x88600000 + ofs[0];
	sceBootLfatRead = (void *)0x88600000 + ofs[1];
	sceBootLfatClose = (void *)0x88600000 + ofs[2];
	sceKernelCheckPspConfig = (void *)0x88600000 + ofs[3];

	/* Patch calls to add own modules */
	MAKE_CALL(0x88600000 + ofs[4], sceBootLfatOpenPatched);
	MAKE_CALL(0x88600000 + ofs[5], sceBootLfatReadPatched);
	MAKE_CALL(0x88600000 + ofs[6], sceBootLfatClosePatched);
	MAKE_CALL(0x88600000 + ofs[7], sceKernelCheckPspConfigPatched);

	/* Dummy removeByDebugSection */
	MAKE_DUMMY_FUNCTION1(0x88600000 + ofs[8]);

	/* Patch sceBootLfatfsMount and lseek fail errors */
	_sw(0, 0x88600000 + ofs[9]);
	_sw(0, 0x88600000 + ofs[10]);
	_sw(0, 0x88600000 + ofs[11]);

	/* Patch call to LoadCore module_bootstart */
	_sw(0x02202021, 0x88600000 + ofs[12]);
	MAKE_CALL(0x88600000 + ofs[12] + 8, PatchLoadCore);

	/* Remove check of return code from the check of the hash in pspbtcnf */
	_sw(0, 0x88600000 + ofs[13]);

	/* Increase buffer size to allow big modules */
	_sh(0xF000, 0x88600000 + ofs[14]);
	_sh(0xF000, 0x88600000 + ofs[15]);

	reboot_module_after = (char *)(*(u32 *)0x88FB0010);
	reboot_buf = (void *)(*(u32 *)0x88FB0014);
	reboot_size = *(u32 *)0x88FB0018;
	reboot_flags = *(u32 *)0x88FB001C;

	hen_found = 0;
	xmb_found = 0;
	rtm_found = 0;

	ClearCaches();

	return sceReboot(reboot_param, exec_param, api, initial_rnd);
}