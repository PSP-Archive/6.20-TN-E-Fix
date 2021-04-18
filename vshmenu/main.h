#ifndef __MAIN_H__
#define __MAIN_H__

typedef struct
{
	int magic; /* 0x47434E54 */
	int vshcpuspeed;
	int vshbusspeed;
	int gamecpuspeed;
	int gamebusspeed;
	int usbdevice;
	int fakeregion;
	int passwordcontrol;
	int notincreaseosklimit;
	int skipgameboot;
	int nothidemacaddress;
	int notusetnnetupd;
	int hidepic0pic1;
	int notspoofversion;
	int usbcharge_slimcolors;
	int fastscrollmusic;
	int novshmenu;
	int hideheneboot;
	int fakeindexdat;
	int notprotectflash0;
} TNConfig;

int sce_paf_private_strlen(char *);
int sce_paf_private_sprintf(char *, const char *, ...);
void *sce_paf_private_memcpy(void *, void *, int);
char *sce_paf_private_strcpy(char *, const char *);

int scePowerRequestColdReset(int unk);

int sctrlSEGetConfig(TNConfig *config);
int sctrlSESetConfig(TNConfig *config);

int vctrlVSHExitVshMenu(TNConfig *config);
int vctrlVSHRegisterVshMenu(int (* ctrl)(SceCtrlData *, int));

#endif