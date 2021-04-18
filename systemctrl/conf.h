#ifndef __CONF_H__
#define __CONF_H__

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

int sctrlSEGetConfig(TNConfig *config);

#endif