#include <pspsdk.h>
#include <pspkernel.h>
#include <psputility_sysparam.h>
#include <systemctrl.h>
#include <kubridge.h>

#include "main.h"
#include "utils.h"
#include "en_tnsettings.h"

PSP_MODULE_INFO("XmbControl", 0x0007, 1, 0);

typedef struct
{
	char *name;
	char *items[2];
	char *options[17];
	char *slimcolors;
	char *buttonassign;
	char *speed[9];
	char *usbdevice[6];
	char *regions[14];
} StringContainer;

StringContainer string;

#define N_STRINGS ((sizeof(string) / sizeof(char **)))

typedef struct
{
	int mode;
	int negative;
	char *item;
} GetItem;

GetItem GetItemes[] =
{
	{ 1, 0, "msg_cpuclockxmb" },
	{ 1, 0, "msg_cpuclockgame" },
	{ 2, 0, "msg_usbdevice" },
	{ 3, 0, "msg_fakeregion" },
	{ 0, 0, "msg_passwordcontrol" },
	{ 0, 1, "msg_osklimitincrease" },
	{ 0, 0, "msg_skipgameboot" },
	{ 0, 1, "msg_hidemacaddr" },
	{ 0, 1, "msg_tnnetworkupd" },
	{ 0, 0, "msg_hidepic0pic1" },
	{ 0, 1, "msg_spoofversion" },
	{ 0, 0, "msg_usbcharge_slimcolors" },
	{ 0, 0, "msg_fastscrollmusic" },
	{ 0, 1, "msg_usevshmenu" },
	{ 0, 0, "msg_hideheneboot" },
	{ 0, 0, "msg_fakeindexdat" },
	{ 0, 1, "msg_protectflash0" },
};

#define N_ITEMS (sizeof(GetItemes) / sizeof(GetItem))

char *button_options[] = { "O", "X" };

char *items[] =
{
	"msgtop_sysconf_configuration",
	"msgtop_sysconf_plugins",
};

enum PluginMode
{
	PluginModeVSH = 0,
	PluginModeGAME = 1,
	PluginModePOPS
};

char *plugin_texts_ms0[] =
{
	"ms0:/seplugins/vsh.txt",
	"ms0:/seplugins/game.txt",
	"ms0:/seplugins/pops.txt"
};

char *plugin_texts_ef0[] =
{
	"ef0:/seplugins/vsh.txt",
	"ef0:/seplugins/game.txt",
	"ef0:/seplugins/pops.txt"
};

#define MAX_PLUGINS_CONST 100

typedef struct
{
	char *path;
	char *plugin;
	int activated;
	int mode;
} Plugin;

Plugin *table = NULL;
int count = 0;

int (* AddVshItem)(void *a0, int topitem, SceVshItem *item);
SceSysconfItem *(*GetSysconfItem)(void *a0, void *a1);
int (* ExecuteAction)(int action, int action_arg);
int (* UnloadModule)(int skip);
int (* OnXmbPush)(void *arg0, void *arg1);
int (* OnXmbContextMenu)(void *arg0, void *arg1);

void (* LoadStartAuth)();
int (* auth_handler)(int a0);
void (* OnRetry)();

void (* AddSysconfItem)(u32 *option, SceSysconfItem **item);
void (* OnInitMenuPspConfig)();

u32 sysconf_unk, sysconf_option;

int is_tn_config = 0;
int unload = 0;

u32 backup[4];
int context_mode = 0;

char user_buffer[128];

STMOD_HANDLER previous;
TNConfig config;

int psp_model;

int startup = 1;

SceContextItem *context;
SceVshItem *new_item;
void *xmb_arg0, *xmb_arg1;

int cpu_list[] = { 0, 20, 75, 100, 133, 222, 266, 300, 333 };
int bus_list[] = { 0, 10, 37, 50, 66, 111, 133, 150, 166 };

#define N_CPUS (sizeof(cpu_list) / sizeof(int))
#define N_BUSS (sizeof(bus_list) / sizeof(int))

void ClearCaches()
{
	sceKernelDcacheWritebackAll();
	kuKernelIcacheInvalidateAll();
}

__attribute__((noinline)) int GetCPUSpeed(int cpu)
{
	int i;
	for(i = 0; i < N_CPUS; i++) if(cpu_list[i] == cpu) return i;
	return 0;
}

__attribute__((noinline)) int GetBUSSpeed(int bus)
{
	int i;
	for(i = 0; i < N_BUSS; i++) if(bus_list[i] == bus) return i;
	return 0;
}

int readPlugins(int mode, int cur_n, Plugin *plugin_table)
{
	char temp_path[64];
	int res = 0, i = cur_n;
	
	SceUID fd = sceIoOpen(plugin_texts_ms0[mode], PSP_O_RDONLY, 0);
	if(fd < 0)
	{
		fd = sceIoOpen(plugin_texts_ef0[mode], PSP_O_RDONLY, 0);
		if(fd < 0) return i;
	}

	char *buffer = (char *)sce_paf_private_malloc(1024);
	int size = sceIoRead(fd, buffer, 1024);
	char *p = buffer;

	do
	{
		sce_paf_private_memset(temp_path, 0, sizeof(temp_path));

		res = GetPlugin(p, size, temp_path, &plugin_table[i].activated);
		if(res > 0)
		{
			if(plugin_table[i].path) sce_paf_private_free(plugin_table[i].path);
			plugin_table[i].path = (char *)sce_paf_private_malloc(sce_paf_private_strlen(temp_path) + 1);
			sce_paf_private_strcpy(plugin_table[i].path, temp_path);

			if(plugin_table[i].plugin) sce_paf_private_free(plugin_table[i].plugin);
			table[i].plugin = (char *)sce_paf_private_malloc(64);
			sce_paf_private_sprintf(table[i].plugin, "plugin_%08X", (u32)i);

			plugin_table[i].mode = mode;

			size -= res;
			p += res;
			i++;
		}
	} while(res > 0);

	sce_paf_private_free(buffer);
	sceIoClose(fd);

	return i;
}

void writePlugins(int mode, Plugin *plugin_table, int count)
{	
	SceUID fd = sceIoOpen(plugin_texts_ms0[mode], PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
	if(fd < 0) fd = sceIoOpen(plugin_texts_ef0[mode], PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);

	if(fd >= 0)
	{
		int i;
		for(i = 0; i < count; i++)
		{
			if(plugin_table[i].mode == mode)
			{
				sceIoWrite(fd, plugin_table[i].path, sce_paf_private_strlen(plugin_table[i].path));
				sceIoWrite(fd, " ", sizeof(char));

				sceIoWrite(fd, (plugin_table[i].activated == 1 ? "1" : "0"), sizeof(char));

				if(i != (count - 1)) sceIoWrite(fd, "\r\n", 2 * sizeof(char));
			}
		}
		
		sceIoClose(fd);
	}
}

int readPluginTable()
{
	if(!table)
	{
		table = sce_paf_private_malloc(MAX_PLUGINS_CONST * sizeof(Plugin));
		sce_paf_private_memset(table, 0, MAX_PLUGINS_CONST * sizeof(Plugin));
	}

	count = readPlugins(PluginModeVSH, 0, table);
	count = readPlugins(PluginModeGAME, count, table);
	count = readPlugins(PluginModePOPS, count, table);

	if(count <= 0)
	{
		if(table)
		{
			sce_paf_private_free(table);
			table = NULL;
		}

		return 0;
	}

	return 1;
}

int LoadTextLanguage(int new_id)
{
	static char *language[] = { "ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "ch1", "ch2" };

	int id;
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &id);

	if(new_id >= 0)
	{
		if(new_id == id) return 0;
		id = new_id;
	}

	char file[64];
	sce_paf_private_sprintf(file, "ms0:/seplugins/%s_tnsettings.txt", language[id]);

	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	if(fd < 0)
	{
		sce_paf_private_sprintf(file, "ef0:/seplugins/%s_tnsettings.txt", language[id]);
		fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	}

	if(fd >= 0)
	{
		/* Skip UTF8 magic */
		u32 magic;
		sceIoRead(fd, &magic, sizeof(magic));
		sceIoLseek(fd, (magic & 0xFFFFFF) == 0xBFBBEF ? 3 : 0, PSP_SEEK_SET);
	}

	char line[128];

	int i;
	int j = 0;
	for(i = 0; i < N_STRINGS; i++)
	{
		sce_paf_private_memset(line, 0, sizeof(line));

		if(&((char **)&string)[i] >= &string.speed[1] && &((char **)&string)[i] <= &string.speed[8])
		{
			sce_paf_private_sprintf(line, "%d/%d", cpu_list[&((char **)&string)[i] - &*string.speed], bus_list[&((char **)&string)[i] - &*string.speed]);
		}
		else if(&((char **)&string)[i] >= &string.usbdevice[1] && &((char **)&string)[i] <= &string.usbdevice[4])
		{
			sce_paf_private_sprintf(line, "Flash %d", &((char **)&string)[i] - &*string.usbdevice - 1);
		}
		else
		{
			if(fd >= 0)
			{
				ReadLine(fd, line);
			}
			else
			{
				sce_paf_private_strcpy(line, en_tnsettings[j]);
				j++;
			}
		}

		/* Free buffer if already exist */
		if(((char **)&string)[i]) sce_paf_private_free(((char **)&string)[i]);

		/* Allocate buffer */
		((char **)&string)[i] = sce_paf_private_malloc(sce_paf_private_strlen(line) + 1);

		/* Copy to buffer */
		sce_paf_private_strcpy(((char **)&string)[i], line);
	}

	if(fd >= 0) sceIoClose(fd);

	if(psp_model == 0) sce_paf_private_strcpy(string.options[10], string.slimcolors);

	return 1;
}

int AddVshItemPatched(void *a0, int topitem, SceVshItem *item)
{
	if(sce_paf_private_strcmp(item->text, "msgtop_sysconf_console") == 0)
	{
		if(config.passwordcontrol) LoadStartAuth();
		else startup = 0;

		LoadTextLanguage(-1);

		new_item = (SceVshItem *)sce_paf_private_malloc(sizeof(SceVshItem));
		sce_paf_private_memcpy(new_item, item, sizeof(SceVshItem));

		new_item->id = 47; //information board id
		new_item->action_arg = sysconf_tnconfig_action_arg;
		new_item->play_sound = 0;
		sce_paf_private_strcpy(new_item->text, "msgtop_sysconf_tnconfig");

		context = (SceContextItem *)sce_paf_private_malloc((2 * sizeof(SceContextItem)) + 1);

		AddVshItem(a0, topitem, new_item);
	}

	return AddVshItem(a0, topitem, item);
}

int OnXmbPushPatched(void *arg0, void *arg1)
{
	xmb_arg0 = arg0;
	xmb_arg1 = arg1;
	return OnXmbPush(arg0, arg1);
}

int OnXmbContextMenuPatched(void *arg0, void *arg1)
{
	new_item->context = NULL;
	return OnXmbContextMenu(arg0, arg1);
}

int ExecuteActionPatched(int action, int action_arg)
{
	int old_is_tn_config = is_tn_config;

	if(action == sysconf_console_action)
	{
		if(action_arg == sysconf_tnconfig_action_arg)
		{
			sctrlSEGetConfig(&config);
			int n = readPluginTable() + 1;

			sce_paf_private_memset(context, 0, (2 * sizeof(SceContextItem)) + 1);

			int i;
			for(i = 0; i < n; i++)
			{
				sce_paf_private_strcpy(context[i].text, items[i]);
				context[i].play_sound = 1;
				context[i].action = 0x80000;
				context[i].action_arg = i + 1;
			}

			new_item->context = context;

			OnXmbContextMenu(xmb_arg0, xmb_arg1);
			return 0;
		}

		is_tn_config = 0;
	}
	else if(action == 0x80000)
	{
		is_tn_config = action_arg;
		action = sysconf_console_action;
		action_arg = sysconf_console_action_arg;
	}

	if(old_is_tn_config != is_tn_config)
	{
		/* Reset variables */
		sce_paf_private_memset(backup, 0, sizeof(backup));
		context_mode = 0;

		/* Unload sysconf */
		unload = 1;
	}

	return ExecuteAction(action, action_arg);
}

int UnloadModulePatched(int skip)
{
	if(unload)
	{
		skip = -1;
		unload = 0;
	}
	return UnloadModule(skip);
}

void AddSysconfContextItem(char *text, char *subtitle, char *regkey)
{
	SceSysconfItem *item = (SceSysconfItem *)sce_paf_private_malloc(sizeof(SceSysconfItem));

	item->id = 5;
	item->unk = (u32 *)sysconf_unk;
	item->regkey = regkey;
	item->text = text;
	item->subtitle = subtitle;
	item->page = "page_psp_config_umd_autoboot";

	((u32 *)sysconf_option)[2] = 1;

	AddSysconfItem((u32 *)sysconf_option, &item);
}

void OnInitMenuPspConfigPatched()
{
	if(is_tn_config == 1)
	{
		if(((u32 *)sysconf_option)[2] == 0)
		{
			int i;
			for(i = 0; i < N_ITEMS; i++)
			{
				AddSysconfContextItem(GetItemes[i].item, NULL, GetItemes[i].item);
			}

			AddSysconfContextItem("msg_buttonassign", NULL, "/CONFIG/SYSTEM/XMB/button_assign");
		}
	}
	else if(is_tn_config == 2)
	{
		if(((u32 *)sysconf_option)[2] == 0)
		{
			int i;
			for(i = 0; i < count; i++)
			{
				char *subtitle = NULL;
				switch(table[i].mode)
				{
					case PluginModeVSH:
						subtitle = "msg_pluginmode_vsh";
						break;

					case PluginModeGAME:
						subtitle = "msg_pluginmode_game";
						break;

					case PluginModePOPS:
						subtitle = "msg_pluginmode_pops";
						break;
				}

				AddSysconfContextItem(table[i].plugin, subtitle, table[i].plugin);
			}
		}
	}
	else
	{
		OnInitMenuPspConfig();
	}
}

SceSysconfItem *GetSysconfItemPatched(void *a0, void *a1)
{
	SceSysconfItem *item = GetSysconfItem(a0, a1);

	if(is_tn_config == 1)
	{
		int i;
		for(i = 0; i < N_ITEMS; i++)
		{
			if(sce_paf_private_strcmp(item->text, GetItemes[i].item) == 0)
			{
				context_mode = GetItemes[i].mode;
			}
		}

		if(sce_paf_private_strcmp(item->text, "msg_buttonassign") == 0)
		{
			context_mode = 4;
		}
	}

	return item;
}

wchar_t *scePafGetTextPatched(void *a0, char *name)
{
	if(name)
	{
		if(is_tn_config == 1)
		{
			int i;
			for(i = 0; i < N_ITEMS; i++)
			{
				if(sce_paf_private_strcmp(name, GetItemes[i].item) == 0)
				{
					utf8_to_unicode((wchar_t *)user_buffer, string.options[i]);
					return (wchar_t *)user_buffer;
				}
			}
		}
		else if(is_tn_config == 2)
		{
			if(sce_paf_private_strncmp(name, "plugin_", 7) == 0)
			{
				u32 i = sce_paf_private_strtoul(name + 7, NULL, 16);

				char file[64];
				sce_paf_private_strcpy(file, table[i].path);

				char *p = sce_paf_private_strrchr(table[i].path, '/');
				if(p)
				{
					char *p2 = sce_paf_private_strchr(p + 1, '.');
					if(p2)
					{
						int len = (int)(p2 - (p + 1));
						sce_paf_private_strncpy(file, p + 1, len);
						file[len] = '\0';
					}
				}

				utf8_to_unicode((wchar_t *)user_buffer, file);
				return (wchar_t *)user_buffer;
			}
		}

		if(sce_paf_private_strcmp(name, "msgtop_sysconf_tnconfig") == 0)
		{
			utf8_to_unicode((wchar_t *)user_buffer, string.name);
			return (wchar_t *)user_buffer;
		}
		else if(sce_paf_private_strcmp(name, "msgtop_sysconf_configuration") == 0)
		{
			utf8_to_unicode((wchar_t *)user_buffer, string.items[0]);
			return (wchar_t *)user_buffer;
		}
		else if(sce_paf_private_strcmp(name, "msgtop_sysconf_plugins") == 0)
		{
			utf8_to_unicode((wchar_t *)user_buffer, string.items[1]);
			return (wchar_t *)user_buffer;
		}
		else if(sce_paf_private_strcmp(name, "msg_buttonassign") == 0)
		{
			utf8_to_unicode((wchar_t *)user_buffer, string.buttonassign);
			return (wchar_t *)user_buffer;
		}
		else if(sce_paf_private_strcmp(name, "msg_pluginmode_vsh") == 0)
		{
			return L"[VSH]";
		}
		else if(sce_paf_private_strcmp(name, "msg_pluginmode_game") == 0)
		{
			return L"[GAME]";
		}
		else if(sce_paf_private_strcmp(name, "msg_pluginmode_pops") == 0)
		{
			return L"[POPS]";
		}
	}

	wchar_t *res = scePafGetText(a0, name);

	if(!config.notspoofversion && sce_paf_private_strcmp(name, "msgshare_version") == 0)
	{
		char tn_version[8];
		sce_paf_private_sprintf(tn_version, " TN-%c", 'A' + (sctrlHENGetVersion() & 0xF));
		utf8_to_unicode((wchar_t *)user_buffer, tn_version);

		int len = sce_paf_private_wcslen(res);

		static wchar_t version[64];
		sce_paf_private_memcpy(version, res, 2 * len);
		sce_paf_private_memcpy(version + len, user_buffer, 4 * sce_paf_private_wcslen((wchar_t *)user_buffer));

		return version;
	}

	return res;
}

int vshGetRegistryValuePatched(u32 *option, char *name, void *arg2, int size, int *value)
{
	if(name)
	{
		if(is_tn_config == 1)
		{
			int configs[] =
			{
				GetCPUSpeed(config.vshcpuspeed),
				GetCPUSpeed(config.gamecpuspeed),
				config.usbdevice,
				config.fakeregion,
				config.passwordcontrol,
				!config.notincreaseosklimit,
				config.skipgameboot,
				!config.nothidemacaddress,
				!config.notusetnnetupd,
				config.hidepic0pic1,
				!config.notspoofversion,
				config.usbcharge_slimcolors,
				config.fastscrollmusic,
				!config.novshmenu,
				config.hideheneboot,
				config.fakeindexdat,
				!config.notprotectflash0,
			};

			int i;
			for(i = 0; i < N_ITEMS; i++)
			{
				if(sce_paf_private_strcmp(name, GetItemes[i].item) == 0)
				{
					context_mode = GetItemes[i].mode;
					*value = configs[i];
					return 0;
				}
			}

			if(sce_paf_private_strcmp(name, "/CONFIG/SYSTEM/XMB/button_assign") == 0)
			{
				context_mode = 4;
			}
		}
		else if(is_tn_config == 2)
		{
			if(sce_paf_private_strncmp(name, "plugin_", 7) == 0)
			{
				u32 i = sce_paf_private_strtoul(name + 7, NULL, 16);
				*value = table[i].activated;
				return 0;
			}
		}
	}

	return vshGetRegistryValue(option, name, arg2, size, value);
}

int vshSetRegistryValuePatched(u32 *option, char *name, int size, int *value)
{
	if(name)
	{
		if(is_tn_config == 1)
		{
			static int *configs[] =
			{
				&config.vshcpuspeed,
				&config.gamecpuspeed,
				&config.usbdevice,
				&config.fakeregion,
				&config.passwordcontrol,
				&config.notincreaseosklimit,
				&config.skipgameboot,
				&config.nothidemacaddress,
				&config.notusetnnetupd,
				&config.hidepic0pic1,
				&config.notspoofversion,
				&config.usbcharge_slimcolors,
				&config.fastscrollmusic,
				&config.novshmenu,
				&config.hideheneboot,
				&config.fakeindexdat,
				&config.notprotectflash0,
			};

			int i;
			for(i = 0; i < N_ITEMS; i++)
			{
				if(sce_paf_private_strcmp(name, GetItemes[i].item) == 0)
				{
					if(sce_paf_private_strcmp(name, "msg_cpuclockxmb") == 0)
					{
						*configs[i] = cpu_list[*value];
						config.vshbusspeed = bus_list[*value];
					}
					else if(sce_paf_private_strcmp(name, "msg_cpuclockgame") == 0)
					{
						*configs[i] = cpu_list[*value];
						config.gamebusspeed = bus_list[*value];
					}
					else
					{
						*configs[i] = GetItemes[i].negative ? !(*value) : *value;
					}

					sctrlSESetConfig(&config);
					vctrlVSHExitVSHMenu(&config);
					return 0;
				}
			}
		}
		else if(is_tn_config == 2)
		{
			if(sce_paf_private_strncmp(name, "plugin_", 7) == 0)
			{
				u32 i = sce_paf_private_strtoul(name + 7, NULL, 16);
				table[i].activated = *value;
				writePlugins(table[i].mode, table, count);
				return 0;
			}
		}

		if(sce_paf_private_strcmp(name, "/CONFIG/SYSTEM/XMB/language") == 0)
		{
			LoadTextLanguage(*value);
		}
	}

	return vshSetRegistryValue(option, name, size, value);
}

void HijackContext(SceRcoEntry *src, char **options, int n)
{
	SceRcoEntry *plane = (SceRcoEntry *)((u32)src + src->first_child);
	SceRcoEntry *mlist = (SceRcoEntry *)((u32)plane + plane->first_child);
	u32 *mlist_param = (u32 *)((u32)mlist + mlist->param);

	/* Backup */
	if(backup[0] == 0 && backup[1] == 0 && backup[2] == 0 && backup[3] == 0)
	{
		backup[0] = mlist->first_child;
		backup[1] = mlist->child_count;
		backup[2] = mlist_param[16];
		backup[3] = mlist_param[18];
	}

	if(context_mode)
	{
		SceRcoEntry *base = (SceRcoEntry *)((u32)mlist + mlist->first_child);

		SceRcoEntry *item = (SceRcoEntry *)sce_paf_private_malloc(base->next_entry * n);
		u32 *item_param = (u32 *)((u32)item + base->param);

		mlist->first_child = (u32)item - (u32)mlist;
		mlist->child_count = n;
		mlist_param[16] = 13;
		mlist_param[18] = 6;

		int i;
		for(i = 0; i < n; i++)
		{
			sce_paf_private_memcpy(item, base, base->next_entry);

			item_param[0] = 0xDEAD;
			item_param[1] = (u32)options[i];

			if(i != 0) item->prev_entry = item->next_entry;
			if(i == n - 1) item->next_entry = 0;

			item = (SceRcoEntry *)((u32)item + base->next_entry);
			item_param = (u32 *)((u32)item + base->param);
		}
	}
	else
	{
		/* Restore */
		mlist->first_child = backup[0];
		mlist->child_count = backup[1];
		mlist_param[16] = backup[2];
		mlist_param[18] = backup[3];
	}

	sceKernelDcacheWritebackAll();
}

int PAF_Resource_GetPageNodeByID_Patched(void *resource, char *name, SceRcoEntry **child)
{
	int res = PAF_Resource_GetPageNodeByID(resource, name, child);

	if(name)
	{
		if(is_tn_config == 1)
		{
			if(sce_paf_private_strcmp(name, "page_psp_config_umd_autoboot") == 0)
			{
				switch(context_mode)
				{
					case 0:
						HijackContext(*child, NULL, 0);
						break;

					case 1:
						HijackContext(*child, string.speed, sizeof(string.speed) / sizeof(char *));
						break;

					case 2:
						HijackContext(*child, string.usbdevice, sizeof(string.usbdevice) / sizeof(char *));
						break;

					case 3:
						HijackContext(*child, string.regions, sizeof(string.regions) / sizeof(char *));
						break;

					case 4:
						HijackContext(*child, button_options, sizeof(button_options) / sizeof(char *));
						break;
				}
			}
		}
	}

	return res;
}

int PAF_Resource_ResolveRefWString_Patched(void *resource, u32 *data, int *a2, char **string, int *t0)
{
	if(data[0] == 0xDEAD)
	{
		utf8_to_unicode((wchar_t *)user_buffer, (char *)data[1]);
		*(wchar_t **)string = (wchar_t *)user_buffer;
		return 0;
	}

	return PAF_Resource_ResolveRefWString(resource, data, a2, string, t0);
}

int sceDisplaySetHoldModePatched(int status)
{
	if(config.skipgameboot) return 0;
	return sceDisplaySetHoldMode(status);
}

int auth_handler_new(int a0)
{
	startup = a0;
	return auth_handler(a0);
}

int OnInitAuthPatched(void *a0, int (* handler)(), void *a2, void *a3, int (* OnInitAuth)())
{
	return OnInitAuth(a0, startup ? auth_handler_new : handler, a2, a3);
}

int sceVshCommonGuiBottomDialogPatched(void *a0, void *a1, void *a2, int (* cancel_handler)(), void *t0, void *t1, int (* handler)(), void *t3)
{
	return sceVshCommonGuiBottomDialog(a0, a1, a2, startup ? OnRetry : cancel_handler, t0, t1, handler, t3);
}

void PatchVshMain(u32 text_addr)
{
	AddVshItem = (void *)text_addr + 0x21E18;
	ExecuteAction = (void *)text_addr + 0x16340;
	UnloadModule = (void *)text_addr + 0x16734;
	OnXmbContextMenu = (void *)text_addr + 0x15D38;
	OnXmbPush = (void *)text_addr + 0x16284;
	LoadStartAuth = (void *)text_addr + 0x5D10;
	auth_handler = (void *)text_addr + 0x19C40;

	/* Allow old SFOs */
	_sw(0, text_addr + 0x11A70);
	_sw(0, text_addr + 0x11A78);
	_sw(0, text_addr + 0x11D84);

	MAKE_CALL(text_addr + 0xC7A8, sceDisplaySetHoldModePatched);

	MAKE_CALL(text_addr + 0x206F8, AddVshItemPatched);
	MAKE_CALL(text_addr + 0x1631C, ExecuteActionPatched);
	MAKE_CALL(text_addr + 0x2FF8C, ExecuteActionPatched);
	MAKE_CALL(text_addr + 0x16514, UnloadModulePatched);

	_sw(0x8C48000C, text_addr + 0x5E44); //lw $t0, 12($v0)
	MAKE_CALL(text_addr + 0x5E48, OnInitAuthPatched);

	REDIRECT_FUNCTION(text_addr + 0x3ECB0, scePafGetTextPatched);

	_sw((u32)OnXmbPushPatched, text_addr + 0x51EBC);
	_sw((u32)OnXmbContextMenuPatched, text_addr + 0x51EC8);

	ClearCaches();
}

void PatchAuthPlugin(u32 text_addr)
{
	OnRetry = (void *)text_addr + 0x5C8;
	MAKE_CALL(text_addr + 0x13B4, sceVshCommonGuiBottomDialogPatched);
	ClearCaches();
}

void PatchSysconfPlugin(u32 text_addr)
{
	AddSysconfItem = (void *)text_addr + 0x27918;
	GetSysconfItem = (void *)text_addr + 0x22F54;
	OnInitMenuPspConfig = (void *)text_addr + 0x1C3A0;

	sysconf_unk = text_addr + 0x32608;
	sysconf_option = text_addr + 0x32ADC;

	_sh(0xFF, text_addr + 0x28B8);

	MAKE_CALL(text_addr + 0x16C8, vshGetRegistryValuePatched);
	MAKE_CALL(text_addr + 0x16EC, vshSetRegistryValuePatched);

	MAKE_CALL(text_addr + 0x2934, GetSysconfItemPatched);

	REDIRECT_FUNCTION(text_addr + 0x28DA4, PAF_Resource_GetPageNodeByID_Patched);
	REDIRECT_FUNCTION(text_addr + 0x28FFC, PAF_Resource_ResolveRefWString_Patched);
	REDIRECT_FUNCTION(text_addr + 0x2901C, scePafGetTextPatched);

	_sw((u32)OnInitMenuPspConfigPatched, text_addr + 0x2FA24);

	static wchar_t mac_info[] = L"00:00:00:00:00:00";

	if(!config.nothidemacaddress) sce_paf_private_memcpy((void *)text_addr + 0x2DB90, mac_info, sizeof(mac_info));

	if(psp_model == 0 && config.usbcharge_slimcolors)
	{
		_sw(_lw(text_addr + 0x7498), text_addr + 0x7494);
		_sw(0x24020001, text_addr + 0x7498);
	}

	ClearCaches();
}

void PatchGamePlugin(u32 text_addr)
{
	MAKE_DUMMY_FUNCTION0(text_addr + 0x1EB08); //For POPS exection
	MAKE_DUMMY_FUNCTION0(text_addr + 0x1EBD8); //For POPS resume
	MAKE_DUMMY_FUNCTION0(text_addr + 0x1F41C); //For homebrews

	if(config.hidepic0pic1)
	{
		_sw(0x00601021, text_addr + 0x1C098);
		_sw(0x00601021, text_addr + 0x1C0A4);
	}

	if(config.skipgameboot)
	{
		MAKE_CALL(text_addr + 0x17E5C, text_addr + 0x181BC);
		_sw(0x24040002, text_addr + 0x17E60); //li $a0, 2
	}

	ClearCaches();
}

void PatchUpdatePlugin(u32 text_addr, u32 text_size)
{
	/* Patch sceUpdateDownloadSetVersion */
	_sw(0x24040000 | (sctrlHENGetVersion() & 0xFFFF), text_addr + 0x81A4);

	/* Change path */
	u32 text_end = text_addr + text_size;
	for(; text_addr < text_end; text_addr++)
	{
	    if(sce_paf_private_strncmp((char *)text_addr, "/UPDATE", 7) == 0) sce_paf_private_strcpy((char *)text_addr, "/TN_HEN");
	}

	ClearCaches();
}

void PatchMsVideoMainPlugin(u32 text_addr)
{
	/* Patch resolution limit to (130560) pixels (480x272) */
	_sh(0xFE00, text_addr + 0x3AB2C);
	_sh(0xFE00, text_addr + 0x3ABB4);
	_sh(0xFE00, text_addr + 0x3D3AC);
	_sh(0xFE00, text_addr + 0x3D608);
	_sh(0xFE00, text_addr + 0x43B98);
	_sh(0xFE00, text_addr + 0x73A84);
	_sh(0xFE00, text_addr + 0x880A0);

	/* Patch bitrate limit (increase to 16384+2) */
	_sh(0x4003, text_addr + 0x3D324);
	_sh(0x4003, text_addr + 0x3D36C);
	_sh(0x4003, text_addr + 0x42C40);

	ClearCaches();
}

int OnModuleStart(SceModule2 *mod)
{
	char *modname = mod->modname;
	u32 text_addr = mod->text_addr;

	if(sce_paf_private_strcmp(modname, "vsh_module") == 0)
	{
		PatchVshMain(text_addr);
	}
	else if(sce_paf_private_strcmp(modname, "sceVshAuthPlugin_Module") == 0)
	{
		PatchAuthPlugin(text_addr);
	}
	else if(sce_paf_private_strcmp(modname, "sysconf_plugin_module") == 0)
	{
		PatchSysconfPlugin(text_addr);
	}
	else if(sce_paf_private_strcmp(modname, "game_plugin_module") == 0)
	{
		PatchGamePlugin(text_addr);
	}
	else if(!config.notusetnnetupd && sce_paf_private_strcmp(modname, "update_plugin_module") == 0)
	{
		PatchUpdatePlugin(text_addr, mod->text_size);
	}
	else if(sce_paf_private_strcmp(modname, "msvideo_main_plugin_module") == 0)
	{
		PatchMsVideoMainPlugin(text_addr);
	}

	if(!previous)
		return 0;

	return previous(mod);
}

int module_start(SceSize args, void *argp)
{
	psp_model = kuKernelGetModel();
	sctrlSEGetConfig(&config);
	previous = sctrlHENSetStartModuleHandler(OnModuleStart);
	return 0;
}