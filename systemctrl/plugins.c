#include <pspsdk.h>
#include <pspkernel.h>
#include <string.h>

#include "main.h"
#include "sysmodpatches.h"
#include "conf.h"

u32 apitype_addr;
u32 filename_addr;
u32 keyconfig_addr;

u32 unk_B154[0x24/4];

int plugins_started = 0; //0x0000AA80

//0x00006754
int sctrlKernelSetInitFileName(char *filename)
{
	int k1 = pspSdkSetK1(0);
	*(u32 *)filename_addr = (u32)filename;
	pspSdkSetK1(k1);
	return 0;
}

//0x00006794
int sctrlKernelSetInitKeyConfig(int key)
{
	int k1 = pspSdkSetK1(0);
	int prev = sceKernelInitKeyConfig();
	*(u32 *)keyconfig_addr = key;
	pspSdkSetK1(k1);
	return prev;
}

//0x000067F0
int sctrlKernelSetInitApitype(int apitype)
{
	int k1 = pspSdkSetK1(0);
	int prev = sceKernelInitApitype();
	*(u32 *)apitype_addr = apitype;
	pspSdkSetK1(k1);
	return prev;
}

//0x0000687C
void trim(char *str)
{
	int i;
	for(i = strlen(str) - 1; i >= 0; i--)
	{
		if(str[i] == 0x20 || str[i] == '\t') str[i] = 0;
		else break;
	}
}

//0x000068D4
int GetPlugin(char *buf, int size, char *str, int *activated)
{
	char ch = 0;
	int n = 0, i = 0;
	char *s = str;

	while(1)
	{
		if(i >= size) break;

		ch = buf[i];

		if(ch < 0x20 && ch != '\t')
		{
			if(n != 0)
			{
				i++;
				break;
			}
		}
		else
		{
			*str++ = ch;
			n++;
		}

		i++;
	}

	trim(s);

	*activated = 0;

	if(i > 0)
	{
		char *p = strpbrk(s, " \t");
		if(p)
		{
			char *q = p + 1;
			while(*q < 0) q++;
			if(strcmp(q, "1") == 0) *activated = 1;
			*p = 0;
		}   
	}

	return i;
}

//0x000069B4
int LoadStartModule(char *file)
{
	SceUID mod = sceKernelLoadModule620(file, 0, NULL);
	if(mod < 0) return mod;

	if(psp_model == 4)
	{
		if(strncmp(file, "ef0:/", 5) == 0)
		{
			SceModule2 *module = sceKernelFindModuleByUID620(mod);

			int i, j;
			for(i = 0; i < module->nsegment; i++)
			{
				for(j = 0; j < module->segmentsize[i]; j += 4)
				{
					u32 text_addr = module->segmentaddr[i] + j;
					if(strncmp((char *)text_addr, "ms0", 3) == 0) memcpy((char *)text_addr, "ef0", 3);
					else if(strncmp((char *)text_addr, "fatms", 5) == 0) memcpy((char *)text_addr, "fatef", 5);
				}
			}

			ClearCaches();
		}
	}

	return sceKernelStartModule620(mod, strlen(file) + 1, file, NULL, NULL);
}

//0x00006A24
int sceKernelStartModulePatched(SceUID modid, SceSize argsize, void *argp, int *status, SceKernelSMOption *option)
{
	SceUID fd = -1;
	SceUID fpl = -1;
	int res = 0;
	char *buffer;
	char plugin[64];

	SceModule2 *mod = sceKernelFindModuleByUID620(modid);
	if(mod && !plugins_started)
	{
		if(sceKernelFindModuleByName620("sceUtility_Driver"))
		{
			plugins_started = 1;
			sctrlSEGetConfig(&config);

			int keyconfig = sceKernelInitKeyConfig();
			if(keyconfig == PSP_INIT_KEYCONFIG_VSH)
			{
				if(sceKernelFindModuleByName620("scePspNpDrm_Driver"))
				{
					fd = sceIoOpen("ms0:/seplugins/vsh.txt", PSP_O_RDONLY, 0);
					if(fd < 0) fd = sceIoOpen("ef0:/seplugins/vsh.txt", PSP_O_RDONLY, 0);
				}
			}
			else if(keyconfig == PSP_INIT_KEYCONFIG_GAME)
			{
				fd = sceIoOpen("ms0:/seplugins/game.txt", PSP_O_RDONLY, 0);
				if(fd < 0) fd = sceIoOpen("ef0:/seplugins/game.txt", PSP_O_RDONLY, 0);
			}
			else if(keyconfig == PSP_INIT_KEYCONFIG_POPS)
			{
				fd = sceIoOpen("ms0:/seplugins/pops.txt", PSP_O_RDONLY, 0);
				if(fd < 0) fd = sceIoOpen("ef0:/seplugins/pops.txt", PSP_O_RDONLY, 0);
			}

			if(fd >= 0)
			{
				fpl = sceKernelCreateFpl("", PSP_MEMORY_PARTITION_KERNEL, 0, 1024, 1, NULL);
				if(fpl >= 0)
				{
					sceKernelAllocateFpl(fpl, (void *)&buffer, NULL);
					int size = sceIoRead(fd, buffer, 1024);
					char *p = buffer;

					do
					{
						int activated = 0;
						memset(plugin, 0, sizeof(plugin));

						res = GetPlugin(p, size, plugin, &activated);

						if(res > 0)
						{
							if(activated) LoadStartModule(plugin);
							size -= res;
							p += res;
						}
					} while(res > 0);

					sceIoClose(fd);
				}
			}
		}
	}

	if(buffer)
	{
		sceKernelFreeFpl(fpl, buffer);
		sceKernelDeleteFpl(fpl);
	}

	return sceKernelStartModule620(modid, argsize, argp, status, option);
}

//0x00006EE4
int PatchInit(int (* module_bootstart)(SceSize, void *), void *argp)
{
	u32 text_addr = ((u32)module_bootstart) - 0x1A4C;

	memset(unk_B154, 0, sizeof(unk_B154));

	MAKE_JUMP(text_addr + 0x1CC4, sceKernelStartModulePatched);

	unk_B154[1] = (u32)argp;

	ClearCaches();

	return module_bootstart(4, argp);
}