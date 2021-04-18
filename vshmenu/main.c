#include <pspsdk.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <psppower.h>
#include <psputility_sysparam.h>
#include <systemctrl.h>
#include <kubridge.h>

#include "main.h"
#include "blit.h"
#include "clock.h"
#include "en_vshmenu.h"

PSP_MODULE_INFO("TNVshMenu", 0, 1, 0);

#define MENU_MAX 8
#define LANGUAGE_MAX (sizeof(en_vshmenu) / sizeof(char *))

char string[LANGUAGE_MAX][64];

int exit_mode; //0x00002474
int menu_mode; //0x00002478
int mode; //0x0000247C

u32 cur_buttons; //0x00002480
u32 buttons_on; //0x00002484

SceUID vshmenu_thid; //0x00002488

SceCtrlData pad; //0x0000248C

TNConfig config; //0x0000249C

int changed = 0; //0x0000259C
int menu_sel = 0; //0x000025A0

char *item_str[MENU_MAX]; //0x000025A4
int item_fcolor[MENU_MAX]; //0x000025C4

TNConfig *new_config; //0x000025E4

int ReadLine(SceUID fd, char *str)
{
	u8 ch = 0;
	int n = 0;

	while(1)
	{
		if(sceIoRead(fd, &ch, 1) != 1)
			return n;

		if(ch < 0x20)
		{
			if(n != 0)
				return n;
		}
		else
		{
			*str++ = ch;
			n++;
		}
	}
}

void GetLanguage()
{
	static char *language[] = { "ja", "en", "fr", "es", "de", "it", "nl", "pt", "ru", "ko", "ch1", "ch2" };

	int id;
	sceUtilityGetSystemParamInt(PSP_SYSTEMPARAM_ID_INT_LANGUAGE, &id);

	char file[64];
	sce_paf_private_sprintf(file, "ms0:/seplugins/%s_vshmenu.txt", language[id]);

	SceUID fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	if(fd < 0)
	{
		sce_paf_private_sprintf(file, "ef0:/seplugins/%s_vshmenu.txt", language[id]);
		fd = sceIoOpen(file, PSP_O_RDONLY, 0);
	}

	int i, j;
	if(fd >= 0)
	{
		for(i = 0; i < LANGUAGE_MAX; i++) ReadLine(fd, string[i]);
		sceIoClose(fd);
	}
	else
	{
		for(i = 0; i < LANGUAGE_MAX; i++) sce_paf_private_strcpy(string[i], en_vshmenu[i]);
	}

	int longest_len = 0;

	for(i = 0; i < 3; i++)
	{
		int len = sce_paf_private_strlen(string[i]);
		if(len > longest_len) longest_len = len;
	}

	for(i = 0; i < 3; i++) for(j = sce_paf_private_strlen(string[i]); j < longest_len; j++) string[i][j] = ' ';
}

//0x00000D88
int menu_ctrl(u32 buttons, u32 button_on)
{
	int direction = 0;

	button_on &= 0x0081F3F9;

	if(button_on & PSP_CTRL_DOWN) direction++;
	if(button_on & PSP_CTRL_UP) direction--;

	do
	{
		menu_sel = limit(menu_sel + direction, 0, MENU_MAX - 1);
	} while(string[menu_sel] == NULL);

	direction = -2;
	if(button_on & PSP_CTRL_LEFT) direction = -1;
	if(button_on & PSP_CTRL_CROSS) direction = 0;
	if(button_on & PSP_CTRL_RIGHT) direction = 1;

	if(button_on & PSP_CTRL_SELECT || button_on & PSP_CTRL_HOME)
	{
		direction = 0;
		menu_sel = MENU_MAX - 1;
	}

	if(direction > -2)
	{
		if(changed == 0 && direction != 0) changed = 1;

		if(menu_sel < MENU_MAX)
		{
			switch(menu_sel)
			{
				case 0:
					if(direction)
					{
						int cpu_num = limit(GetCPUSpeed(new_config->vshcpuspeed) + direction, 0, 8);
						new_config->vshcpuspeed = cpu_list[cpu_num];
						new_config->vshbusspeed = bus_list[cpu_num];
					}
					break;

				case 1:
					if(direction)
					{
						int cpu_num = limit(GetCPUSpeed(new_config->gamecpuspeed) + direction, 0, 8);
						new_config->gamecpuspeed = cpu_list[cpu_num];
						new_config->gamebusspeed = bus_list[cpu_num];
					}
					break;

				case 2:
					if(direction) new_config->usbdevice = limit(new_config->usbdevice + direction, 0, 6 - 1);
					break;

	   			case 3:
					return -1;

				case 4:
		   			return -2;

	   			case 5:
					return -3;

				case 6:
		   			return -4;

				case 7:
	   				return direction == 0 ? direction : 1;
			}
		}
	}

	return 1;
}

//0x00000AE8
void menu_setup(TNConfig *conf)
{
	new_config = conf;

	int i;
	for(i = 0; i < MENU_MAX; i++) item_str[i] = 0;

	static char clock_buf_xmb[64]; //0x000025F0
	static char clock_buf_game[64]; //0x000025F8
	static char usbdevice_buf[64]; //0x00002600

	if(GetCPUSpeed(new_config->vshcpuspeed) && GetBUSSpeed(new_config->vshbusspeed)) sce_paf_private_sprintf(clock_buf_xmb, "%d/%d", new_config->vshcpuspeed, new_config->vshbusspeed);
	else sce_paf_private_sprintf(clock_buf_xmb, string[8]);
	item_str[0] = clock_buf_xmb;

	if(GetCPUSpeed(new_config->gamecpuspeed) && GetBUSSpeed(new_config->gamebusspeed)) sce_paf_private_sprintf(clock_buf_game, "%d/%d", new_config->gamecpuspeed, new_config->gamebusspeed);
	else sce_paf_private_sprintf(clock_buf_game, string[8]);
	item_str[1] = clock_buf_game;

	if(new_config->usbdevice > 0 && new_config->usbdevice < 5) sce_paf_private_sprintf(usbdevice_buf, "Flash %d", new_config->usbdevice - 1);
	else if(new_config->usbdevice == 0) sce_paf_private_sprintf(usbdevice_buf, string[9]);
	else if(new_config->usbdevice == 5) sce_paf_private_sprintf(usbdevice_buf, string[10]);
	item_str[2] = usbdevice_buf;
}

//0x00000948
int menu_draw()
{
	if(blit_setup() < 0) return -1;

	blit_set_color(0x00FFFFFF, 0x8000FF00);
	blit_string(CENTER(11), 6 * 8, "TN VSH MENU");

	int i;
	for(i = 0; i < MENU_MAX; i++)
	{
		u32 bc = i == menu_sel ? 0x00FF8080 : 0xC00000FF;

		blit_set_color(0x00FFFFFF, bc);

		if(string[i])
		{
			int y = (i + 8) * 8;

			int len = sce_paf_private_strlen(string[i]);

			int center = len;
			if(item_str[i]) center += 1 + sce_paf_private_strlen(string[8]);
			center = CENTER(center);

			blit_string(center, y, string[i]);

			if(item_str[i])
			{
				blit_set_color(0x00FFFFFF, bc);
				blit_string(center + (len + 1) * 8, y, item_str[i]);
			}
		}
	}

	blit_set_color(0x00FFFFFF, 0x00000000);

	return 0;
}

//0x00000430
void ctrl_handler(SceCtrlData *pad_data, int count)
{
	sce_paf_private_memcpy(&pad, pad_data, sizeof(SceCtrlData));

	buttons_on = ~cur_buttons & pad.Buttons;
	cur_buttons = pad.Buttons;

	int i;
	for(i = 0; i < count; i++) pad_data[i].Buttons &= ~0x0081F3F9;
}

static void buttons_func()
{
	switch(mode)
	{
		case 0:
			if(cur_buttons && (cur_buttons & PSP_CTRL_SELECT) == 0) mode = 1;
			break;

		case 1:
			if(menu_mode == 0) menu_mode = 1;
			else
			{
				int res = menu_ctrl(cur_buttons, buttons_on);
				if(res == 0) mode = 2;
				else if(res == -1) mode = 3;
				else if(res == -2) mode = 4;
				else if(res == -3) mode = 5;
				else if(res == -4) mode = 6;
			}
			break;

		case 2:
			if((cur_buttons & 0x0083F3F9) == 0) exit_mode = 1;
			break;

		default:
			if(mode >= 3 && mode <= 6)
			{
				if((cur_buttons & 0x0083F3F9) == 0) exit_mode = mode - 1;
			}
	}
}

static void menu_exit()
{
	if(changed) sctrlSESetConfig(&config);

	switch(exit_mode)
	{
		case 2:
			scePowerRequestStandby();
			break;

		case 3:
			scePowerRequestSuspend();
			break;

		case 4:
			scePowerRequestColdReset(0);
			break;

		case 5:
			sctrlKernelExitVSH(0);
			break;
	}
}

//0x0000010C
int VshMenu_Thread()
{
	sceKernelChangeThreadPriority(0, 8);
	sctrlSEGetConfig(&config);
	vctrlVSHRegisterVshMenu(ctrl_handler);

	while(1)
	{
		sceDisplayWaitVblankStart();

		if(exit_mode) break;

		switch(menu_mode)
		{
			case 1:
				menu_setup(&config);
				menu_mode++;
				break;
			case 2:
				menu_draw();
				menu_setup(&config);
				break;
		}

		buttons_func();
	}

	menu_exit();

	vctrlVSHExitVSHMenu(&config);

	return sceKernelExitDeleteThread(0);
}

//0x00000058
int module_start()
{
	GetLanguage();
	blit_load_font();
	vshmenu_thid = sceKernelCreateThread("VshMenu_Thread", VshMenu_Thread, 0x10, 0x1000, 0, NULL);
	if(vshmenu_thid >= 0) sceKernelStartThread(vshmenu_thid, 0, NULL);
	return 0;
}

//0x00000000
int module_stop()
{
	exit_mode = 1;
	SceUInt timeout = 100000;
	if(sceKernelWaitThreadEnd(vshmenu_thid, &timeout) < 0) sceKernelTerminateDeleteThread(vshmenu_thid);
	return 0;
}