#include <pspsdk.h>
#include <pspkernel.h>
#include <psputilsforkernel.h>
#include <systemctrl.h>
#include <string.h>

#include "nid_table.h"
#include "custom_png.h"

#include "main.h"
#include "string_clone.h"
#include "sysmodpatches.h"
#include "malloc.h"
#include "plugins.h"
#include "conf.h"

//#define DEBUG
//#define REGISTRATION_BYPASS

PSP_MODULE_INFO("SystemControl", 0x3007, 2, 5);

int psp_model;
char *rebootex;
int size_rebootex;

int (* new_DecryptPBP)(u32 *tag, u8 *keys, u32 code, u32 *buf, int size, int *retSize, int m, void *unk0, int unk1, int unk2, int unk3, int unk4); //0x0000A780
int (* new_DecryptPRX)(u32 *buf, int size, int *retSize, int m); //0x0000A784

int set_cpuspeed = 0;

STMOD_HANDLER module_handler; //0x0000A78C
int (* PrologueModule)(void *modmgr_param, SceModule2 *mod);

int (* ProbeExec1)(void *buf, u32 *check); //0x0000A79C
int (* PartitionCheck)(u32 *st0, u32 *check); //0x0000A7A0
int (* ProbeExec2)(u8 *buf, u32 *check); //0x0000A7A4

int (* VerifySigncheck)(u32 *buf, int size, int m); //0x0000A7A8
int (* DecryptPRX)(u32 *buf, int size, int *retSize, int m); //0x0000A7AC
int (* memlmd_keyseed)(int unk, u32 addr); //0x0000A7B0
int (* DecryptPBP)(u32 *tag, u8 *keys, u32 code, u32 *buf, int size, int *retSize, int m, void *unk0, int unk1, int unk2, int unk3, int unk4); //0x0000A7B4

int (* GetFunction)(void *lib, u32 nid, int unk, int unk2);

int (* sceResmgrDecryptIndex)(void *buf, int size, int *retSize);

int (* scePopsManExitVSHKernel)(u32 error);
int (* SetKeys)(char *filename, void *keys, int unk);
int (* scePopsSetKeys)(int size, void *keys, void *a2);

int use_custom_png = 1;
int patch_pops = 0;

int reinsert_ms = 0; //0x0000B150
SceUID evid = 0; //0x0000B178

SceUID intextuid;
SceUID outtextuid;

#ifdef DEBUG

void logmsg(char *msg)
{
	SceUID fd = sceIoOpen("ms0:/log.txt", PSP_O_WRONLY | PSP_O_CREAT, 0777);
	if(fd > 0)
	{
		sceIoLseek(fd, 0, PSP_SEEK_END);
		sceIoWrite(fd, msg, strlen(msg));
		sceIoClose(fd);
	}
}

#endif

//0x0000005C
int IsStaticElf(void *buf)
{
	Elf32_Ehdr *header = (Elf32_Ehdr *)buf;
	if(header->e_magic == 0x464C457F && header->e_type == 2) return 1;
	return 0;
}

//0x00000094
int PatchExec2(void *buf, int *check)
{
	int index = check[0x4C/4];
	if(index < 0) index += 3;

	u32 addr = (u32)(buf + index);
	if(addr >= 0x88400000 && addr <= 0x88800000) return 0;

	check[0x58/4] = ((u32 *)buf)[index / 4] & 0xFFFF;
	return ((u32 *)buf)[index / 4];
}

//0x000000E8
int PatchExec1(void *buf, int *check)
{
	if(((u32 *)buf)[0] != 0x464C457F) return -1;

	if(check[8/4] >= 0x120)
	{
		if(check[8/4] != 0x120 && check[8/4] != 0x141 && check[8/4] != 0x142 && check[8/4] != 0x143 && check[8/4] != 0x140) return -1;

		if(check[0x10/4] == 0)
		{
			if(check[0x44/4] != 0)
			{
				check[0x48/4] = 1;
				return 0;
			}
			return -1;
		}

		check[0x48/4] = 1;
		check[0x44/4] = 1;
		PatchExec2(buf, check);

		return 0;
	}
	else if(check[8/4] >= 0x52) return -1;

	if(check[0x44/4] != 0)
	{
		check[0x48/4] = 1;
		return 0;
	}

	return -2;
}

//0x000001B0
int ProbeExec1Patched(void *buf, u32 *check)
{
	int res = ProbeExec1(buf, check);

	if(((u32 *)buf)[0] != 0x464C457F) return res;

	u16 *modinfo = ((u16 *)buf) + (check[0x4C/4] / 2);

	u16 realattr = *modinfo;
	u16 attr = realattr & 0x1E00;

	if(attr != 0)
	{
		u16 attr2 = ((u16 *)check)[0x58/2];
		attr2 &= 0x1E00;

		if(attr2 != attr) ((u16 *)check)[0x58/2] = realattr;
	}

	if(check[0x48/4] == 0) check[0x48/4] = 1;

	return res;
}

int sceKernelProbeExecutableObjectPatched(void *buf, int *check)
{
	int res = sceKernelProbeExecutableObject620(buf, check);

	if(((u32 *)buf)[0] == 0x464C457F)
	{
		if(check[8/4] >= 0x52)
		{
			if(check[0x20/4] == -1);
			{
				if(IsStaticElf(buf))
				{
					check[0x20/4] = 3;
					return 0;
				}
			}
		}
	}

	return res;
}

//0x00000248
__attribute__((noinline)) char *GetStrTab(u8 *buf)
{
	Elf32_Ehdr *header = (Elf32_Ehdr *)buf;
	if(header->e_magic != 0x464C457F) return NULL;

	u8 *pData = buf + header->e_shoff;

	int i;
	for(i = 0; i < header->e_shnum; i++)
	{
		if(header->e_shstrndx == i)
		{
			Elf32_Shdr *section = (Elf32_Shdr *)pData;
			if(section->sh_type == 3) return (char *)buf + section->sh_offset;
		}
		pData += header->e_shentsize;
	}

	return NULL;
}

//0x000011F4
int ProbeExec2Patched(u8 *buf, u32 *check)
{
	int res = ProbeExec2(buf, check);

	if(((u32 *)buf)[0] != 0x464C457F) return res;

	if(IsStaticElf(buf)) if((check[8/4] - 0x140) < 0x5) check[8/4] = 0x120; //Fake apitype to avoid reject

	if(check[0x4C/4] == 0)
	{
		if(IsStaticElf(buf))
		{
			char *strtab = GetStrTab(buf);
			if(strtab)
			{
				Elf32_Ehdr *header = (Elf32_Ehdr *)buf;
				u8 *pData = buf + header->e_shoff;

				int i;
				for(i = 0; i < header->e_shnum; i++)
				{
					Elf32_Shdr *section = (Elf32_Shdr *)pData;

					if(strcmp(strtab + section->sh_name, ".rodata.sceModuleInfo") == 0)
					{
						check[0x4C/4] = section->sh_offset;
						check[0x58/4] = 0;
					}

					pData += header->e_shentsize;
				}
			}
		}
	}

	return res;
}

//0x0000137C
int PartitionCheckPatched(u32 *st0, u32 *check)
{
	static u32 buf[256/4]; //0x0000AA84

	SceUID fd = (SceUID)st0[0x18/4];
	u16 attributes;

	u32 pos = sceIoLseek(fd, 0, PSP_SEEK_CUR);
	if(pos < 0) return PartitionCheck(st0, check);

	sceIoLseek(fd, 0, PSP_SEEK_SET);
	if(sceIoRead(fd, buf, 256) < 256)
	{
		sceIoLseek(fd, pos, PSP_SEEK_SET);
		return PartitionCheck(st0, check);
	}

	if(buf[0] == 0x50425000)
	{
		sceIoLseek(fd, buf[8], PSP_SEEK_SET);
		sceIoRead(fd, buf, 0x14);

		if(buf[0] != 0x464C457F)
		{
			/* Encrypted module */
			sceIoLseek(fd, pos, PSP_SEEK_SET);
			return PartitionCheck(st0, check);
		}

		sceIoLseek(fd, buf[8] + check[0x4C/4], PSP_SEEK_SET);

		if(!IsStaticElf(buf)) check[0x10/4] = buf[9] - buf[8]; // Allow psar's in decrypted pbp's
	}
	else if(buf[0] == 0x464C457F)
	{
		sceIoLseek(fd, check[0x4C/4], PSP_SEEK_SET);
	}
	else /* encrypted module */
	{
		sceIoLseek(fd, pos, PSP_SEEK_SET);
		return PartitionCheck(st0, check);
	}

	sceIoRead(fd, &attributes, sizeof(attributes));

	if(IsStaticElf(buf)) check[0x44/4] = 0;
	else
	{
		if(attributes & 0x1000) check[0x44/4] = 1;
		else check[0x44/4] = 0;
	}

	sceIoLseek(fd, pos, PSP_SEEK_SET);
	return PartitionCheck(st0, check);
}

//0x000015A4
int PatchExec3(void *buf, int *check, int isPlain, int res)
{
	if(!isPlain) return res;

	if(check[8/4] >= 0x52)
	{
		if(check[0x20/4] == -1) if(IsStaticElf(buf)) check[0x20/4] = 3;
		return res;
	}

	if(!(PatchExec2(buf, check) & 0x0000FF00)) return res;

	check[0x44/4] = 1;
	return 0;
}

//0x00001644
int sceKernelCheckExecFilePatched(u32 *buf, int *check)
{
	int res = PatchExec1(buf, check);
	if(res == 0) return res;

	int isPlain = (((u32 *)buf)[0] == 0x464C457F);

	res = sceKernelCheckExecFile620(buf, check);

	return PatchExec3(buf, check, isPlain, res);
}

//0x000002E4
__attribute__((noinline)) u32 GetNewNID(NidTable *nid_table, u32 nid)
{
	int i;
	for(i = 0; i < nid_table->n_nid; i++) if(nid_table->nid[i].old_nid == nid) return nid_table->nid[i].new_nid;
	return 0;
}

//0x00000324
void SystemCtrlForKernel_6A5F76B5(int a0)
{
	//unk_A790 = a0;
}

//0x00000330
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER handler)
{
	STMOD_HANDLER prev = module_handler;
	module_handler = (STMOD_HANDLER)((u32)handler | 0x80000000);
	return prev;
}

//0x00000348
int VerifySigncheckPatched(u32 *buf, int size, int m)
{
	if(buf[0] == 0x5053507E)
	{
		int i;
		for(i = 0; i < 0x58; i++)
		{
			if(((u8 *)buf)[0xD4 + i])
			{
				if(buf[0xB8/4] && buf[0xBC/4])
				{
					return VerifySigncheck(buf, size, m);
				}
			}
		}
	}
	return 0;
}

//0x000003B4
void *SetNewDecryptPRX(int (* handler)(u32 *, int, int *, int))
{
	void *res = new_DecryptPRX;
	new_DecryptPRX = handler;
	return res;
}

//0x000003C4
void *SetNewDecryptPBP(int (* handler)(u32 *, u8 *, u32, u32 *, int, int *, int, void *, int, int, int, int))
{
	void *res = new_DecryptPBP;
	new_DecryptPBP = handler;
	return res;
}

//0x000003D4
__attribute__((noinline)) void sctrlHENPatchSyscall(u32 addr, void *newaddr)
{
	void **ptr;
	asm("cfc0 %0, $12\n" : "=r" (ptr));

	int i;
	for(i = 0; i < 0x10000; i++) if(((u32 *)(*ptr + 0x10))[i] == addr) ((u32 *)(*ptr + 0x10))[i] = (u32)newaddr;
}

//0x00000404
int DecryptPBPPatched(u32 *tag, u8 *keys, u32 code, u32 *buf, int size, int *retSize, int m, void *unk0, int unk1, int unk2, int unk3, int unk4)
{
	if(new_DecryptPBP) if(new_DecryptPBP(tag, keys, code, buf, size, retSize, m, unk0, unk1, unk2, unk3, unk4) >= 0) return 0;

	if(tag && buf && retSize)
	{
		if(buf[0x130/4] == 0x28796DAA || buf[0x130/4] == 0x7316308C || buf[0x130/4] == 0x3EAD0AEE || buf[0x130/4] == 0x8555ABF2)
		{
			if(((u8 *)buf)[0x150] == 0x1F && ((u8 *)buf)[0x151] == 0x8B)
			{
				int gzip_size = buf[0xB0/4];
				*retSize = gzip_size;
				memmove(buf, buf + 0x150/4, gzip_size);
				return 0;
			}
		}
	}

	int res = DecryptPBP(tag, keys, code, buf, size, retSize, m, unk0, unk1, unk2, unk3, unk4);
	if(res < 0) if(VerifySigncheckPatched(buf, size, m) >= 0) return DecryptPBP(tag, keys, code, buf, size, retSize, m, unk0, unk1, unk2, unk3, unk4);

	return res;
}

//0x00000600
int DecryptPRXPatched(u32 *buf, int size, int *retSize, int m)
{
	if(new_DecryptPRX) if(new_DecryptPRX(buf, size, retSize, m) >= 0) return 0;

	if(buf && retSize)
	{
		if(buf[0x130/4] == 0xC6BA41D3 || buf[0x130/4] == 0x55668D96)
		{
			if(((u8 *)buf)[0x150] == 0x1F && ((u8 *)buf)[0x151] == 0x8B)
			{
				int gzip_size = buf[0xB0/4];
				*retSize = gzip_size;
				memmove(buf, buf + 0x150/4, gzip_size);
				return 0;
			}
		}
	}

	int res = DecryptPRX(buf, size, retSize, m);
	if(res < 0)
	{
		if(VerifySigncheckPatched(buf, size, m) >= 0)
		{
			memlmd_keyseed(0, 0xBFC00200);
			return DecryptPRX(buf, size, retSize, m);
		}
	}

	return res;
}

int sceResmgrDecryptIndexPatched(void *buf, int size, int *retSize)
{
	int res = sceResmgrDecryptIndex(buf, size, retSize);
	int k1 = pspSdkSetK1(0);

	if(config.fakeindexdat && strstr(buf, "release:6.20") != 0)
	{
		SceUID fd = sceIoOpen("ms0:/seplugins/version.txt", PSP_O_RDONLY, 0);
		if(fd < 0) fd = sceIoOpen("ef0:/seplugins/version.txt", PSP_O_RDONLY, 0);
		if(fd >= 0)
		{
			*retSize = sceIoRead(fd, buf, size);
			sceIoClose(fd);
		}
	}

	pspSdkSetK1(k1);
	return res;
}

//0x00000760
NidTable *GetLibraryByName(const char *libname)
{
	if(libname)
	{
		int i = 0;
		do
		{
			if(strcmp(libname, nid_table_620[i].libname) == 0) return nid_table_620 + i;
			i++;
		}
		while(i < NIDS_N);
	}
	return 0;
}

int PrologueModulePatched(void *modmgr_param, SceModule2 *mod)
{
	int res = PrologueModule(modmgr_param, mod);
	if(res >= 0 && module_handler) module_handler(mod);
	return res;
}

//0x000008D4
u32 sctrlHENFindFunction(const char *szMod, const char *szLib, u32 nid)
{
	struct SceLibraryEntryTable *entry;

	SceModule2 *pMod = sceKernelFindModuleByName620(szMod);
	if(!pMod)
	{
		pMod = sceKernelFindModuleByAddress620((SceUID)szMod);
		if(!pMod) return 0;
	}

	NidTable *nid_table = GetLibraryByName(szLib);
	if(nid_table)
	{
		u32 new_nid = GetNewNID(nid_table, nid);
		if(new_nid) nid = new_nid;
	}

	void *entTab = pMod->ent_top;
	int entLen = pMod->ent_size;

	int i = 0;
	while(i < entLen)
	{
		entry = (struct SceLibraryEntryTable *)(entTab + i);

		int total = entry->stubcount + entry->vstubcount;
		u32 *vars = entry->entrytable;

		if(entry->libname && strcmp(entry->libname, szLib) == 0)
		{
			if(entry->stubcount > 0)
			{
				int count;
				for(count = 0; count < entry->stubcount; count++) if(vars[count] == nid) return vars[count + total];
			}
		}
		else
		{
			if(entry->vstubcount > 0)
			{
				int count;
				for(count = 0; count < entry->vstubcount; count++)
				{
					if(vars[count + entry->stubcount] == nid) return vars[count + entry->stubcount + total];
				}
			}
		}

		i += (entry->len * 4);
	}

	return 0;
}

//0x000009F8
void ClearCaches()
{
	sceKernelIcacheInvalidateAll();
	sceKernelDcacheWritebackInvalidateAll();
}

int GetFunctionPatched(void *lib, u32 nid, void *stub, int count)
{
	char *libname = (char *)((u32 *)lib)[0x44/4];
	u32 stubtable = ((u32 *)stub)[0x18/4];
	u32 original_stub = ((u32 *)stub)[0x24/4];
	int is_user_mode = ((u32 *)stub)[0x34/4];
	u32 stub_addr = stubtable + (count * 8);

	/* Kernel Module */
	if(!is_user_mode)
	{
		u32 module_sdk_version = FindProc((char *)original_stub, NULL, 0x11B97506);
		if(module_sdk_version && FIRMWARE_TO_FW(_lw(module_sdk_version)) == 0x620)
		{
			/* Sony module */
		}
		else
		{
			/* Resolve missing NIDs */
			if(strcmp(libname, "SysclibForKernel") == 0)
			{
				if(nid == 0x909C228B)
				{
					REDIRECT_FUNCTION(stub_addr, 0x88002E80);
					return -1;
				}
				else if(nid == 0x18FE80DB)
				{
					REDIRECT_FUNCTION(stub_addr, 0x88002EBC);
					return -1;
				}
				else if(nid == 0x1AB53A58)
				{
					REDIRECT_FUNCTION(stub_addr, strtok_r_clone);
					return -1;
				}
				else if(nid == 0x87F8D2DA)
				{
					REDIRECT_FUNCTION(stub_addr, strtok_clone);
					return -1;
				}
				else if(nid == 0x1D83F344)
				{
					REDIRECT_FUNCTION(stub_addr, atob_clone);
					return -1;
				}
				else if(nid == 0x62AE052F)
				{
					REDIRECT_FUNCTION(stub_addr, strspn_clone);
					return -1;
				}
				else if(nid == 0x89B79CB1)
				{
					REDIRECT_FUNCTION(stub_addr, strcspn_clone);
					return -1;
				}
			}
			else if(strcmp(libname, "sceSyscon_driver") == 0)
			{
				if(nid == 0xC8439C57)
				{
					SceModule2 *mod = sceKernelFindModuleByName620("sceSYSCON_Driver");
					u32 text_addr = mod->text_addr;

					REDIRECT_FUNCTION(stub_addr, text_addr + 0x2C64);
					return -1;
				}
			}
			else if(strcmp(libname, "sceSysreg_driver") == 0)
			{
				SceModule2 *mod = sceKernelFindModuleByName620("sceLowIO_Driver");
				u32 text_addr = mod->text_addr;

				if(nid == 0x0436B60F)
				{
					REDIRECT_FUNCTION(stub_addr, text_addr + 0xAF0);
					return -1;
				}
				else if(nid == 0x58F47EFD)
				{
					REDIRECT_FUNCTION(stub_addr, text_addr + 0xAFC);
					return -1;
				}
				else if(nid == 0x7A5D2D15)
				{
					REDIRECT_FUNCTION(stub_addr, text_addr + 0xB58);
					return -1;
				}
				else if(nid == 0x25B0AC52)
				{
					REDIRECT_FUNCTION(stub_addr, text_addr + 0xB64);
					return -1;
				}
			}
			else if(strcmp(libname, "scePower_driver") == 0)
			{
				if(nid == 0x737486F2)
				{
					u32 scePowerSetClockFrequency_k = FindPowerFunction(nid);
					if(scePowerSetClockFrequency_k)
					{
						REDIRECT_FUNCTION(stub_addr, scePowerSetClockFrequency_k);
						return -1;
					}
				}
			}

			/* Resolve old NIDs */
			NidTable *nid_table = GetLibraryByName(libname);
			if(nid_table)
			{
				u32 new_nid = GetNewNID(nid_table, nid);
				if(new_nid) nid = new_nid;
			}
		}
	}

	int res = GetFunction(lib, nid, -1, 0);

	if(res < 0)
	{
		#ifdef DEBUG
		log("Missing: %s - 0x%08X\n", libname, nid);
		#endif

		_sw(0x0000054C, stub_addr);
		_sw(0x00000000, stub_addr + 4);
		return -1;
	}

	return res;
}

//0x00000C28
void PatchMesgLed() 
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceMesgLed");
	u32 text_addr = mod->text_addr;

	static u32 ofs_phat[] = { 0x1E3C, 0x3808, 0x3BC4, 0x1ECC };
	static u32 ofs_slim[] = { 0x1ECC, 0x3D10, 0x415C, 0x1F5C };
	static u32 ofs_brite[] = { 0x1F5C, 0x41F0, 0x4684, 0x1FEC };
	static u32 ofs_brite_v2[] = { 0x1F5C, 0x41F0, 0x4684, 0x1FEC };
	static u32 ofs_go[] = { 0x1FEC, 0x4674, 0x4B50, 0x207C };

	u32 *ofs = NULL;
	if(psp_model == 0) ofs = ofs_phat;
	else if(psp_model == 1) ofs = ofs_slim;
	else if(psp_model == 2) ofs = ofs_brite;
	else if(psp_model == 3) ofs = ofs_brite_v2;
	else if(psp_model == 4) ofs = ofs_go;

	DecryptPBP = (void *)text_addr + 0xE0;

	MAKE_CALL(text_addr + 0x1908, DecryptPBPPatched);
	MAKE_CALL(text_addr + ofs[0], DecryptPBPPatched);
	MAKE_CALL(text_addr + ofs[1], DecryptPBPPatched);
	MAKE_CALL(text_addr + ofs[2], DecryptPBPPatched);
	MAKE_CALL(text_addr + ofs[3], DecryptPBPPatched);

	sceResmgrDecryptIndex = (void *)FindProc("sceMesgLed", "sceResmgr", 0x9DC14891);
	PatchSyscall((u32)sceResmgrDecryptIndex, sceResmgrDecryptIndexPatched);
}

//0x00000C8C
void PatchMemlmd()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceMemlmd");
	u32 text_addr = mod->text_addr;

	static u32 ofs_phat[] = { 0xF10, 0x1158, 0x10D8, 0x112C, 0xE10, 0xE74 };
	static u32 ofs_other[] = { 0xFA8, 0x11F0, 0x1170, 0x11C4, 0xEA8, 0xF0C };

	u32 *ofs = ofs_other;
	if(psp_model == 0) ofs = ofs_phat;

	VerifySigncheck = (void *)text_addr + ofs[0];
	DecryptPRX = (void *)text_addr + 0x134;
	memlmd_keyseed = (void *)text_addr + ofs[1];

	MAKE_CALL(text_addr + ofs[2], VerifySigncheckPatched);
	MAKE_CALL(text_addr + ofs[3], VerifySigncheckPatched);
	MAKE_CALL(text_addr + ofs[4], DecryptPRXPatched);
	MAKE_CALL(text_addr + ofs[5], DecryptPRXPatched);
}

int sceIoAssignPatched(const char *dev1, const char *dev2, const char *dev3, int mode, void* unk1, long unk2)
{
	int k1 = pspSdkSetK1(0);

	if(sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_UPDATER)
	{
		if(mode == IOASSIGN_RDWR && strcmp(dev3, "flashfat0:") == 0)
		{
			if(!config.notprotectflash0)
			{
				pspSdkSetK1(k1);
				return -1;
			}
		}
	}

	pspSdkSetK1(k1);
	return sceIoAssign(dev1, dev2, dev3, mode, unk1, unk2);
}

int sceIoDreadPatched(SceUID fd, SceIoDirent *dir)
{
	int k1 = pspSdkSetK1(0);
	int res = sceIoDread(fd, dir);

	if(res > 0)
	{
		if(config.hideheneboot && strcmp(dir->d_name, "TN_HEN") == 0) strcpy(dir->d_name, "."); //Hide TN EBOOT
	}

	pspSdkSetK1(k1);
	return res;
}

int sceIoMkdirPatched(const char *dir, SceMode mode)
{
	int k1 = pspSdkSetK1(0);

	if(strcmp(dir, "ms0:/PSP/GAME") == 0) sceIoMkdir("ms0:/seplugins", mode);
	else if(strcmp(dir, "ef0:/PSP/GAME") == 0) sceIoMkdir("ef0:/seplugins", mode);

	pspSdkSetK1(k1);
	return sceIoMkdir(dir, mode);
}

void PatchIoFileMgr()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceIOFileManager");
	u32 text_addr = mod->text_addr;

	PatchSyscall(text_addr + 0x1AAC, sceIoAssignPatched);

	if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_VSH)
	{
		PatchSyscall(text_addr + 0x15B8, sceIoDreadPatched);
		PatchSyscall(text_addr + 0x4260, sceIoMkdirPatched);
	}
}

void PatchThreadMan()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceThreadManager");
	u32 text_addr = mod->text_addr;

	/* Remove breakpoint */
	_sw(0, text_addr + 0x175AC);
}

//0x00000D18
void PatchInterruptMgr()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceInterruptManager");
	u32 text_addr = mod->text_addr;

	/* Allow execution of syscalls in kernel mode */
	_sw(0x408F7000, text_addr + 0xE94);
	_sw(0, text_addr + 0xE98);

	/* Remove 0xBC000004/8 protection */
	_sw(0, text_addr + 0xDE8);
	_sw(0, text_addr + 0xDEC);
}

//0x00000D48
void PatchModuleMgr()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceModuleManager");
	u32 text_addr = mod->text_addr;

	/* Avoid 0x80010148 error on PSPgo */
	if(psp_model == 4) MAKE_CALL(text_addr + 0x7C3C, sceKernelProbeExecutableObjectPatched);

	MAKE_JUMP(text_addr + 0x8854, sceKernelCheckExecFilePatched);

	PartitionCheck = (void *)text_addr + 0x7FC0;
	PrologueModule = (void *)text_addr + 0x8114;

	apitype_addr = text_addr + 0x9990;
	filename_addr = text_addr + 0x9994;
	keyconfig_addr = text_addr + 0x99EC;

	_sw(0, text_addr + 0x760); //sceKernelLoadModule (User)
	_sw(0x24020000, text_addr + 0x7C0); //sceKernelLoadModule (User)

	_sw(0, text_addr + 0x30B0); //sceKernelLoadModuleVSH
	_sw(0, text_addr + 0x310C); //sceKernelLoadModuleVSH
	_sw(0x10000009, text_addr + 0x3138); //sceKernelLoadModuleVSH

	_sw(0, text_addr + 0x3444); //sceKernelLoadModule (Kernel)
	_sw(0, text_addr + 0x349C); //sceKernelLoadModule (Kernel)
	_sw(0x10000010, text_addr + 0x34C8); //sceKernelLoadModule (Kernel)

	MAKE_CALL(text_addr + 0x64FC, PartitionCheckPatched);
	MAKE_CALL(text_addr + 0x6878, PartitionCheckPatched);

	MAKE_CALL(text_addr + 0x7028, PrologueModulePatched);
}

//0x00000E48
void PatchLoadCore()
{
	SceModule2 *mod = sceKernelFindModuleByName620("sceLoaderCore");
	u32 text_addr = mod->text_addr;

	/* Patch calls and references to sceKernelCheckExecFile */
	_sw((u32)sceKernelCheckExecFilePatched, text_addr + 0x86B4);
	MAKE_CALL(text_addr + 0x1578, sceKernelCheckExecFilePatched);
	MAKE_CALL(text_addr + 0x15C8, sceKernelCheckExecFilePatched);
	MAKE_CALL(text_addr + 0x4A18, sceKernelCheckExecFilePatched);

	/* Patch relocation check in switch statement */
	_sw(_lw(text_addr + 0x8B58), text_addr + 0x8B74);

	/* Patch 2 functions called by sceKernelProbeExecutableObject */
	MAKE_CALL(text_addr + 0x46A4, ProbeExec1Patched);
	MAKE_CALL(text_addr + 0x4878, ProbeExec2Patched);

	/* Allow kernel modules to have syscall imports */
	_sw(0x3C090000, text_addr + 0x40A4);

	_sw(0, text_addr + 0x7E84);

	_sw(0, text_addr + 0x6880);
	_sw(0, text_addr + 0x6884);
	_sw(0, text_addr + 0x6990);
	_sw(0, text_addr + 0x6994);

	/* Patch to translate NIDs */
	MAKE_CALL(text_addr + 0x3DA0, GetFunctionPatched);
	_sw(0x02403821, text_addr + 0x3D9C); //move $a3, $s2
	_sw(0x02203021, text_addr + 0x3DA4); //move $a2, $s1
	_sw(0, text_addr + 0x3EE0);
	_sw(0, text_addr + 0x3EE8);

	/* Patch call to init module_bootstart */
	MAKE_CALL(text_addr + 0x1DB0, PatchInit);
	_sw(0x02E02021, text_addr + 0x1DB4); //move $a0, $s7

	ProbeExec1 = (void *)text_addr + 0x61D4;
	ProbeExec2 = (void *)text_addr + 0x60F0;
	GetFunction = (void *)text_addr + 0xF74;

	/* Restore original calls */
	u32 memlmd_2E208358 = FindProc("sceMemlmd", "memlmd", 0x2E208358);
	MAKE_CALL(text_addr + 0x6914, memlmd_2E208358);
	MAKE_CALL(text_addr + 0x6944, memlmd_2E208358);
	MAKE_CALL(text_addr + 0x69DC, memlmd_2E208358);

	u32 memlmd_CA560AA6 = FindProc("sceMemlmd", "memlmd", 0xCA560AA6);
	MAKE_CALL(text_addr + 0x41A4, memlmd_CA560AA6);
	MAKE_CALL(text_addr + 0x68F0, memlmd_CA560AA6);
}

SceUID sceIoOpenPatched(const char *file, int flags, SceMode mode)
{
	if(flags & 0x40000000) flags &= ~0x40000000;
	return sceIoOpen(file, flags, mode);
}

int sceIoIoctlPatched(SceUID fd, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	if(cmd == 0x04100001 || cmd == 0x04100002)
	{
		if(inlen == 4) sceIoLseek(fd, *(u32 *)indata, PSP_SEEK_SET);
		return 0;
	}

	return sceIoIoctl(fd, cmd, indata, inlen, outdata, outlen);
}

int sceIoReadPatched(SceUID fd, void *data, SceSize size)
{
	int res = sceIoRead(fd, data, size);
	if(res == size)
	{
		u32 magic = 0x474E5089;
		if(use_custom_png && size == size_custom_png)
		{
			if(memcmp(data, &magic, sizeof(magic)) == 0)
			{
				memcpy(data, custom_png, size_custom_png);
				return size_custom_png;
			}
		}
		else if(size == 4 && ((u32)data & 4))
		{
			if(((u32 *)data)[0] == 0x464C457F) ((u32 *)data)[0] = 0x5053507E;
			return res;
		}

		if(((u8 *)data)[0x41B] == 0x27 && ((u8 *)data)[0x41C] == 0x19)
		{
			if(((u8 *)data)[0x41D] == 0x22 && ((u8 *)data)[0x41E] == 0x41)
			{
				if(((u8 *)data)[0x41A] == ((u8 *)data)[0x41F])
				{
					((u8 *)data)[0x41B] = 0x55;
				}
			}
		}
	}

	return res;
}

int scePopsManExitVSHKernelPatched(u32 destSize, u8 *src, u8 *dest)
{
	int k1 = pspSdkSetK1(0);

	if(destSize < 0)
	{
		scePopsManExitVSHKernel(destSize);
		pspSdkSetK1(k1);
		return 0;
	}

	int size = sceKernelDeflateDecompress(dest, destSize, src, 0);
	pspSdkSetK1(k1);

	return (size ^ 0x9300 ? size : 0x92FF);
}

int ReadFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	if(fd < 0) return fd;
	int read = sceIoRead(fd, buf, size);
	sceIoClose(fd);
	return read;
}

int WriteFile(char *file, void *buf, int size)
{
	SceUID fd = sceIoOpen(file, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0) return fd;
	int written = sceIoWrite(fd, buf, size);
	sceIoClose(fd);
	return written;
}

int SetKeysPatched(char *filename, void *keys, int unk)
{
	char path[64];
	strcpy(path, filename);

	char *p = strrchr(path, '/');
	if(!p) return 0xCA000000;

	strcpy(p + 1, "KEYS.BIN");

	if(ReadFile(path, keys, 0x10) != 0x10)
	{
		SceUID fd = sceIoOpen(filename, PSP_O_RDONLY, 0);
		if(fd >= 0)
		{
			u32 header[10];
			sceIoRead(fd, header, sizeof(header));
			sceIoLseek(fd, header[8], PSP_SEEK_SET);
			sceIoRead(fd, header, sizeof(u32));
			sceIoClose(fd);

			if(header[0] == 0x464C457F)
			{
				memset(keys, 0, 0x10);
				scePopsSetKeys(0x10, keys, keys);
				return 0;
			}
		}

		int res = SetKeys(filename, keys, unk);
		if(res >= 0) WriteFile(path, keys, 0x10);
		return res;
	}

	scePopsSetKeys(0x10, keys, keys);
	return 0;
}

//0x0000684C
int ms_callback(SceSize args, void *argp)
{
	reinsert_ms = (u32)argp;
	sceKernelSetEventFlag(evid, 1);
	return 0;
}

//0x00000FF4
void OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	if(strcmp(modname, "sceLowIO_Driver") == 0)
	{
		if(mallocinit() < 0) while(1) _sw(0, 0);
	}
	else if(strcmp(modname, "sceUmdCache_driver") == 0)
	{
		PatchUmdCache(text_addr);
	}
	else if(strcmp(modname, "sceUmdMan_driver") == 0)
	{
		PatchUmdMan(text_addr);
	}
	else if(strcmp(modname, "sceIoFilemgrDNAS") == 0)
	{
		PatchIoFileMgr();

		int ms_status = 0;
		SceUID cbid;

		if(sceKernelFindModuleByName620("sceNp9660_driver")) reinsert_ms = 1;

		if(!reinsert_ms)
		{
			if(sceIoDevctl("mscmhc0:", 0x02025806, 0, 0, &ms_status, sizeof(ms_status)) >= 0)
			{
				if(ms_status == 1)
				{
					evid = sceKernelCreateEventFlag("", 0, 0, NULL);
					cbid = sceKernelCreateCallback("", (void *)ms_callback, 0);
					if(sceIoDevctl("fatms0:", 0x02415821, &cbid, sizeof(cbid), 0, 0) >= 0)
					{
						sceKernelWaitEventFlagCB(evid, 1, 17, 0, 0);
						sceIoDevctl("fatms0:", 0x02415822, &cbid, sizeof(cbid), 0, 0);
					}
					sceKernelDeleteCallback(cbid);
					sceKernelDeleteEventFlag(evid);
				}
			}

			ms_status = 0;
		}
	}
	else if(strcmp(modname, "sceMediaSync") == 0)
	{
		PatchMediaSync(text_addr);
	}
	else if(strcmp(modname, "sceImpose_Driver") == 0)
	{
		OnImposeLoad();
	}
	else if(strcmp(modname, "sceWlan_Driver") == 0)
	{
		PatchWlan(text_addr);
	}
	else if(strcmp(modname, "scePower_Service") == 0)
	{
		PatchPower(text_addr);
	}
	else if(strcmp(modname, "vsh_module") == 0)
	{
		OnVshLoad();
	}
	else if(!config.notincreaseosklimit && strcmp(modname, "sceHVUI_Module") == 0)
	{
		int osk_text_size = 1518 * 2;

		if(intextuid) tn_free(intextuid);
		if(outtextuid) tn_free(outtextuid);

		u16 *intext = (u16 *)tn_malloc(2, osk_text_size, &intextuid);
		u16 *outtext = (u16 *)tn_malloc(2, osk_text_size, &outtextuid);

		u32 intext_addr = text_addr + 0x6DBDC;
		u32 outtext_addr = text_addr + 0x6E3DC;

		_sw((u32)intext, intext_addr);
		_sw((u32)outtext, outtext_addr);

		/* outtextlimit -> outtextlength */
		_sb(0x42, text_addr + 0x27FA); //sw $v0, 48($s2)

		int i;
		for(i = 0; i < mod->text_size; i += 4)
		{
			u32 addr = text_addr + i;

			u32 addiu = (_lw(addr) & 0xFC00FFFF);
			if(addiu == (0x24000000 | (intext_addr & 0xFFFF)) || addiu == (0x24000000 | (outtext_addr & 0xFFFF)))
			{
				/* addiu -> lw */
				_sw(0x8C000000 | (_lw(addr) & 0x03FFFFFF), addr);
			}

			/* Increase size */
			if(i >= 0x21A10 && i <= 0x21D3C && i != 0x21B1C)
			{
				u32 li = (_lw(addr) & 0xFFE0FFFF);
				if(li == 0x240003FE) _sh(osk_text_size - 2, addr);
				else if(li == 0x24000400) _sh(osk_text_size, addr);

				u32 slti = (_lw(addr) & 0xFC00FFFF);
				if(slti == 0x280003FE) _sh(osk_text_size - 2, addr);
				else if(slti == 0x28000400) _sh(osk_text_size, addr);
			}
		}

		ClearCaches();
	}
	else if(strcmp(modname, "sceVshNpSignin_Module") == 0)
	{
		_sw(0x10000008, text_addr + 0x6C4C);
		_sw(0x3C041000, text_addr + 0x95F0);
		ClearCaches();
	}
	else if(strcmp(modname, "sceNpSignupPlugin_Module") == 0)
	{
		_sw(0x3C041000, text_addr + 0x331EC);
		ClearCaches();
	}
	else if(strcmp(modname, "scePops_Manager") == 0)
	{
		SceUID fd = sceIoOpen(sceKernelInitFileName(), PSP_O_RDONLY, 0);
		if(fd >= 0)
		{
			u32 header[10];
			sceIoRead(fd, header, sizeof(header));

			u32 psar_offset = header[9];
			u32 icon0_offset = header[3];

			sceIoLseek(fd, psar_offset, PSP_SEEK_SET);
			sceIoRead(fd, header, sizeof(header));

			if(memcmp((char *)header, "PSTITLE", 7) == 0) psar_offset += 0x200;
			else psar_offset += 0x400;

			sceIoLseek(fd, psar_offset, PSP_SEEK_SET);
			sceIoRead(fd, header, sizeof(header));

			if(header[0] != 0x44475000)
			{
				patch_pops = 1;

				sceIoLseek(fd, icon0_offset, PSP_SEEK_SET);
				sceIoRead(fd, header, sizeof(header));

				/* Check 80x80 PNG */
				if(header[0] == 0x474E5089 && header[1] == 0x0A1A0A0D && header[3] == 0x52444849 && header[4] == 0x50000000 && header[5] == header[4])
				{
					use_custom_png = 0;
				}

				int (* sceKernelSetCompiledSdkVersion390)(int sdkversion) = (void *)FindProc("sceSystemMemoryManager", "SysMemUserForUser", 0x315AD3A0);
				if(sceKernelSetCompiledSdkVersion390) sceKernelSetCompiledSdkVersion390(FW_TO_FIRMWARE(0x390));

				scePopsManExitVSHKernel = (void *)text_addr + 0x1FE4;
				PatchSyscall((u32)scePopsManExitVSHKernel, scePopsManExitVSHKernelPatched);

				REDIRECT_FUNCTION(text_addr + 0x3CD4, sceIoOpenPatched);
				REDIRECT_FUNCTION(text_addr + 0x3CE4, sceIoIoctlPatched);
				REDIRECT_FUNCTION(text_addr + 0x3CEC, sceIoReadPatched);

				/* Remove breakpoint */
				_sw(0, text_addr + 0x1EA8);

				_sw(0, text_addr + 0x564);
				_sw(0, text_addr + 0x126C);
				_sw(0, text_addr + 0x14B0);
				_sw(0, text_addr + 0x15B8);
				_sw(0, text_addr + 0x1780);
				_sw(0, text_addr + 0x1CCC);
			}

			sceIoClose(fd);

			scePopsSetKeys = (void *)text_addr + 0x124;
			SetKeys = (void *)text_addr + 0x26F8;
			MAKE_CALL(text_addr + 0xCB4, SetKeysPatched);
			MAKE_CALL(text_addr + 0x2F90, SetKeysPatched);

			/* Fix standby bug */
			_sw(0, text_addr + 0x2B4);

			ClearCaches();
		}
	}
	else if(patch_pops && strcmp(modname, "pops") == 0)
	{
		static u32 ofs_brite_v2[] = { 0xDEA8, 0xDE1C, 0x1667C, 0x29748, 0x3BCFC };
		static u32 ofs_go[] = { 0xE5C0, 0xE534, 0x16D64, 0x29FEC, 0x3DAE4 };
		static u32 ofs_other[] = { 0xDEA4, 0xDE18, 0x16690, 0x2971C, 0x3BCD0 };

		u32 *ofs = ofs_other;
		if(psp_model == 3) ofs = ofs_brite_v2;
		else if(psp_model == 4) ofs = ofs_go;

		/* Fix bug that jumps to home screen every second */
		_sw(0x5000FFDA, text_addr + ofs[0]);

		/* Patch call to scePopsManExitVSHKernel */
		_sw(_lw(text_addr + ofs[2]), text_addr + ofs[1]);

		_sw(0, text_addr + ofs[3]);

		if(use_custom_png) _sw(0x24050000 | size_custom_png, text_addr + ofs[4]);

		ClearCaches();
	}
	else if(strcmp(modname, "mccoy") == 0)
	{
		#ifdef REGISTRATION_BYPASS

		/* Action Replay registration patch */
		MAKE_DUMMY_FUNCTION1(text_addr + 0x1EACC);
		ClearCaches();

		#endif
	}
	else if(strcmp(modname, "scotty") == 0)
	{
		/* Make TiltFX and Action Replay compatible on 6.20 TN */
		_sw(0x344288FC, text_addr + 0x124);

		int i;
		for(i = 0; i < 0x6000; i += 4)
		{
			u32 scotty_addr = text_addr + i;
			if(_lw(scotty_addr) == 0x0060F809)
			{
				_sw(0x3C1988FC, scotty_addr + 0xC);
				i += 0x40;
			}
			else if(_lw(scotty_addr) == 0x03200008)
			{
				_sw(0x3C1988FC, scotty_addr - 0x28);
				break;
			}
		}

		ClearCaches();
	}
	else if(strcmp(modname, "VLF_Module") == 0)
	{
		u32 nids[] = { 0x2A245FE6, 0x7B08EAAB, 0x22050FC0, 0x158BE61A, 0xD495179F };

		int i;
		for(i = 0; i < (sizeof(nids) / sizeof(u32)); i++)
		{
			u32 vlf_proc = FindProc("VLF_Module", "VlfGui", nids[i]);
			if(vlf_proc)
			{
				MAKE_DUMMY_FUNCTION0(vlf_proc);
			}
		}

		ClearCaches();
	}

	if(set_cpuspeed == 0)
	{
		if(sceKernelGetSystemStatus620() == 0x20000)
		{
			if(sceKernelInitKeyConfig() == PSP_INIT_KEYCONFIG_GAME) SetSpeed(config.gamecpuspeed, config.gamebusspeed);
			set_cpuspeed = 1;
		}
	}
}

//0x000016C8
int module_start()
{
	psp_model = *(u32 *)0x88FB0000;

	PatchLoadCore();
	PatchModuleMgr();
	PatchMemlmd();
	PatchInterruptMgr();
	PatchThreadMan();

	ClearCaches();

	module_handler = (STMOD_HANDLER)((u32)OnModuleStart | 0x80000000);

	size_rebootex = *(u32 *)0x88FB0004;
	set_p2 = *(u32 *)0x88FB0008;
	set_p8 = *(u32 *)0x88FB000C;

	SceUID memid;
	rebootex = (char *)tn_malloc(1, size_rebootex, &memid);
	memcpy(rebootex, (void *)0x88FC0000, size_rebootex);

	ClearCaches();

	return 0;
}