#include <pspsdk.h>
#include <pspkernel.h>
#include <pspumd.h>
#include <systemctrl.h>
#include <pspusbdevice.h>
#include <string.h>

#include "main.h"

PspIoDrv *lflash;	//0x14BC
PspIoDrv *umd;		//0x14E8
PspIoDrv *msstor;	//0x14F4

char *UmdSectorPoolAddr; //0x149C
SceUID UmdSectorPoolId; //0x14A0

int filePointer; //0x14A4

u32 flashSize[4]; //0x14A8
u8 flash_unassigned[4]; //0x14C8

int (* msstorIoOpen) (PspIoDrvFileArg *arg, char *file, int flags, SceMode mode);	//0x14C4
int (* msstorIoClose) (PspIoDrvFileArg *arg);										//0x14D0
int (* msstorIoDevctl) (PspIoDrvFileArg *arg, const char *devname, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen); 	// 0x14CC
int (* msstorIoRead) (PspIoDrvFileArg *arg, char *data, int len);					//0x14D8
SceOff (* msstorIoLseek) (PspIoDrvFileArg *arg, SceOff ofs, int whence); 			//0x14DC
int (* msstorIoWrite) (PspIoDrvFileArg *arg, const char *data, int len);			//0x14E4
int (* msstorIoIoctl) (PspIoDrvFileArg *arg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);	// 0x14F0

int cur_device; // 0x14E0
int fileSize; // 0x14D4
int max_size;
int dat_14B8;
int cur_sector; // 0x14C0
int is_readonly; // 0x1498

int dat_13B4 = 3995648; //0x13B4

u8 dat_13B8[64] =
{
	0xEB, 0x00, 0x00, 0x4D, 0x41, 0x52, 0x43, 0x48, 0x20, 0x33, 0x33, 0x00, 0x02, 0x40, 0x08, 0x00,
	0x02, 0x00, 0x02, 0x00, 0x00, 0xF8, 0xF4, 0x00, 0x3F, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0xF8, 0x3C, 0x00, 0x00, 0x00, 0x29, 0x00, 0x00, 0x00, 0x00, 0x4E, 0x4F, 0x20, 0x4E, 0x41,
	0x4D, 0x45, 0x20, 0x20, 0x20, 0x20, 0x46, 0x41, 0x54, 0x31, 0x36, 0x20, 0x20, 0x20, 0x00, 0x00
};

u8 dat_13F8[64] =
{
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x08, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1D, 0x47, 0x0F, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x55, 0x4D, 0x44, 0x39, 0x36, 0x36, 0x30, 0x20, 0x49, 0x53, 0x4F, 0x21, 0x00, 0x00, 0x31, 0x1C,
	0x82, 0x36, 0x82, 0x36, 0x00, 0x00, 0x31, 0x1C, 0x82, 0x36, 0x02, 0x00, 0x33, 0x33, 0x33, 0x33
};

u8 dat_1438[96] = 
{
	0x02, 0x00, 0x08, 0x00, 0x08, 0x00, 0x07, 0x9F, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x00, 0x21, 0x21, 0x00, 0x00, 0x20, 0x01, 0x08, 0x00, 0x02, 0x00, 0x02, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
	0x00, 0x00, 0x00, 0x01, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//100%
//0x00000000
int lflashIoOpen(PspIoDrvFileArg *arg, char *file, int flags, SceMode mode)
{
	switch(cur_device)
	{
		case PSP_USBDEVICE_FLASH0:
		{
			file = "0,0";
			break;
		}
		case PSP_USBDEVICE_FLASH1:
		{
			file = "0,1";
			break;
		}
		case PSP_USBDEVICE_FLASH2:
		{
			file = "0,2";
			break;
		}
		case PSP_USBDEVICE_FLASH3:
		{
			file = "0,3";
			break;
		}
		default:
			return 0x800200D2;
	}

	return lflash->funcs->IoOpen(arg, file, flags, mode);
}

//100%
//0x00000088
int msstorIoWritePatch(PspIoDrvFileArg *arg, const char *data, int len)
{
	if(is_readonly) return -1;
	return lflash->funcs->IoWrite(arg, data, len);
}

//100%
//0x000000B8
int msstorIoDevctlPatch(PspIoDrvFileArg *arg, const char *devname, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
	if(cmd == 0x02125801)
	{
		u8 *data = (u8 *)outdata;
		data[0] = 1;
		data[1] = 0;
		data[2] = 0;
		data[3] = 1;
		data[4] = 0;
		return 0;
	}

	return 0x80010086;
}

//done
//0x000009C4
int msstorIoIoctlPatch(PspIoDrvFileArg *arg, unsigned int cmd, void *indata, int inlen, u32 *outdata, int outlen)
{
	if(cmd == 0x02125008)
	{
		if(!is_readonly) *outdata = 1;
		else *outdata = 0;
	}
	else if(cmd == 0x02125803)
	{
		memcpy(outdata, dat_1438, sizeof(dat_1438));
	}
	else if(cmd == 0x02125006)
	{
		*outdata = flashSize[cur_device];
	}
	else if(cmd == 0x0211D032)
	{
		msstorIoWritePatch(arg, (char *)outdata, outlen);
	}

	return 0;
}

//100%
//0x000000F0
int umdIoOpen(PspIoDrvFileArg *arg, char *file, int flags, SceMode mode)
{
	cur_sector = -1;
	filePointer = 0;
	return umd->funcs->IoOpen(arg, file, flags, 1);
}

//95%
//0x0000011C
int GetUMDFreeSize(int size, u16 *data)
{
	int t0;

	if(!size)
	{
		data[0] = -8;
		data[1] = -1;
		t0 = 2;
	}
	else
	{
		size = (size * 256) - 2;
		t0 = 0;
	}

	if(size < max_size)
	{
		u16 size_plus_3 = size + 3;
		u16 *data_plus_t0 = data + t0;

		int i = 0;

		while(1)
		{
			i++;
			data_plus_t0++;

			if(t0 < 256)
			{
				if(i != (max_size - size))
				{
					data_plus_t0[-1] = size_plus_3++;
					t0++;
					continue;
				}
				else
				{
					data[t0] = -1;
				}
			}

			break;
		}
	}

	return 0;
}

//95%
//0x000001B0
int umdReadSector(PspIoDrvFileArg *arg, int sector)
{	
	if(sector == cur_sector) return 0;

	if(cur_sector != (sector - 1))
	{
		if(umd->funcs->IoLseek(arg, sector, 0) < 0) return -1;
	}

	int read = umd->funcs->IoRead(arg, UmdSectorPoolAddr, 1);
	if(read >= 0) cur_sector = sector;

	return read;
}

//100%
//0x00000254
SceOff umdIoLseek(PspIoDrvFileArg *arg, SceOff ofs, int whence)
{
	if(ofs & 0x1FF) return 0x80010016;

	ofs = (int)ofs / 512;

	if(whence == PSP_SEEK_SET) filePointer = ofs;
	else if(whence == PSP_SEEK_CUR) filePointer += ofs;
	else if(whence == PSP_SEEK_END) filePointer = dat_13B4 + ofs;
	else return 0x80010016;

	return filePointer * 512;
}

//80%
//0x00000678
int umdIoRead(PspIoDrvFileArg *arg, char *data, int len)
{
	if(!data || len == 0 || len & 0x1FF) return 0x80010016;

	memset(data, 0, len);

	int fp_min_210 = filePointer - 0x210;
	int n_sectors = len / 512;
	int remaining = 0;

	if(filePointer < 0x210 || fp_min_210 > fileSize)
	{
		int i;
		for(i = 0; i < n_sectors; filePointer++, i++, data += 512)
		{
			u32 size_1 = filePointer - 8;
			u32 size_2 = filePointer - 0xFC;

			if(filePointer == 0)
			{
				memcpy(data, dat_13B8, sizeof(dat_13B8));
				data[0x1FE] = 85;
				data[0x1FF] = -86;
			}
			else if(size_1 < 0xF4 || size_2 < 0xF4)
			{
				if(GetUMDFreeSize(size_1, (u16 *)data)) return i * 512;
			}
			else if(filePointer == 0x1F0)
			{
				memcpy(data, dat_13F8, sizeof(dat_13F8));
			}
		}

		return i * 512;
	}
	else
	{	
		if((fp_min_210 + n_sectors) >= dat_13B4) remaining = dat_13B4 - fp_min_210;
		else remaining = n_sectors;

		int nextsector = fp_min_210 / 4;
		int unk = fp_min_210 & 0x3;

		if(unk)
		{
			if(remaining)
			{
				filePointer += n_sectors;
				return n_sectors * 512;
			}

			umdReadSector(arg, nextsector);

			int read = remaining;
			if(read > (4 - unk)) read = (4 - unk);

			memcpy(data, UmdSectorPoolAddr + unk * 512, read * 512);

			data += (read * 512);
			remaining -= read;
			nextsector++;
		}

		if(remaining < 0)
		{
			filePointer += n_sectors;
			return n_sectors * 512;
		}

		umd->funcs->IoLseek(arg, nextsector, 0);

		int remaining_div_4 = (remaining / 4);
		if(remaining_div_4 != 0)
		{
			if(umd->funcs->IoRead(arg, data, remaining_div_4) < 0) return -1;

			data += remaining_div_4 * 2048;
			nextsector = nextsector + remaining_div_4;
			remaining -= remaining_div_4 * 4;
		}

		if(remaining > 0)
		{
			umdReadSector(arg, nextsector);
			memcpy(data, UmdSectorPoolAddr + unk * 512, remaining * 512);
		}

		filePointer += n_sectors;
	}

	return n_sectors * 512;
}

//0x00000AA0
int pspUsbDeviceSetDevice(u32 device, int ronly, int unassign_mask)
{
	int k1 = pspSdkSetK1(0);

	int intr = 0;

	if(device >= 5)
	{
		pspSdkSetK1(k1);
		return 0x80010016;
	}

	cur_device = device;
	memset(flash_unassigned, 0, sizeof(flash_unassigned));
	UmdSectorPoolAddr = NULL;

	int res = 0;

	if(device == PSP_USBDEVICE_UMD9660)
	{
		if(!sceUmdCheckMedium620(0))
		{
			if(sceUmdWaitDriveStatCB620(PSP_UMD_PRESENT, 2000000) < 0)
			{
				pspSdkSetK1(k1);
				return 0x80210003;
			}
		}

		sceUmdActivate620(1, "disc0:");
		sceUmdWaitDriveStat620(PSP_UMD_READY);

		int ret = sceIoDevctl("umd0:", 0x01F20002, 0, 0, &fileSize, sizeof(fileSize));
		if(ret < 0)
		{
			pspSdkSetK1(k1);
			return ret;
		}

		char *buffer = 0;

		/* Get UMD name */
		if(sceIoDevctl("umd0:", 0x01E28035, 0, 0, (void *)&buffer, sizeof(char *)) >= 0)
		{
			if(memcmp(buffer + 1, "CD001", 5) == 0)
			{
				int umd_id_len = 0;

				char *umd_idendifier = buffer + 0x373;

				/* Search end */
				char *p = strchr(umd_idendifier, '|');
				if(p)
				{
					umd_id_len = (int)(p - buffer) - 0x373;
					if(umd_id_len >= 12) umd_id_len = 11;
				}

				memcpy(dat_13B8 + 0x2B, umd_idendifier, umd_id_len);
				memcpy(dat_13F8, buffer + 0x373, umd_id_len);
			}
		}

		/* maybe calculate size of umd? */
		fileSize *= 4;
		max_size = fileSize / 64;

		if((fileSize & 0x3F) != 0) max_size++;

		UmdSectorPoolId = sceKernelCreateFpl("UmdSector", 1, 0, 2048, 1, 0);
		if(UmdSectorPoolId < 0)
		{
			pspSdkSetK1(k1);
			return UmdSectorPoolId;
		}

		sceKernelAllocateFpl(UmdSectorPoolId, (void *)&UmdSectorPoolAddr, 0);

		is_readonly = 1;
		dat_14B8 = dat_13B4;
		*(u32 *)(dat_13F8 + 60) = fileSize * 512;

		intr = sceKernelCpuSuspendIntr();

		PspIoDrvFuncs *msstor_funcs = msstor->funcs;

		msstor_funcs->IoLseek = (void *)umdIoLseek;
		msstor_funcs->IoOpen = (void *)umdIoOpen;
		msstor_funcs->IoClose = (void *)umd->funcs->IoClose;
		msstor_funcs->IoRead = (void *)umdIoRead;
	}
	else
	{
		is_readonly = ronly;

		if(unassign_mask & UNASSIGN_MASK_FLASH0)
		{
			res = sceIoUnassign("flash0:");
			if(res >= 0) flash_unassigned[PSP_USBDEVICE_FLASH0] = 1;
			else if(res != 0x80020321)
			{
				pspSdkSetK1(k1);
				return res;
			}
		}
		
		if(unassign_mask & UNASSIGN_MASK_FLASH1)
		{
			res = sceIoUnassign("flash1:");
			if(res >= 0) flash_unassigned[PSP_USBDEVICE_FLASH1] = 1;
			else if(res != 0x80020321)
			{
				pspSdkSetK1(k1);
				return res;
			}
		}
		
		if(unassign_mask & UNASSIGN_MASK_FLASH2)
		{
			res = sceIoUnassign("flash2:");
			if(res >= 0) flash_unassigned[PSP_USBDEVICE_FLASH2] = 1;
			else if(res != 0x80020321)
			{
				pspSdkSetK1(k1);
				return res;
			}
		}
		
		if(unassign_mask & UNASSIGN_MASK_FLASH3)
		{
			res = sceIoUnassign("flash3:");
			if(res >= 0) flash_unassigned[PSP_USBDEVICE_FLASH3] = 1;
			else if(res != 0x80020321)
			{
				pspSdkSetK1(k1);
				return res;
			}
		}

		intr = sceKernelCpuSuspendIntr();

		PspIoDrvFuncs *lflash_funcs = lflash->funcs;
		PspIoDrvFuncs *msstor_funcs = msstor->funcs;

		msstor_funcs->IoLseek = (void *)lflash_funcs->IoLseek;
		msstor_funcs->IoOpen = (void *)lflashIoOpen;
		msstor_funcs->IoClose = (void *)lflash_funcs->IoClose;
		msstor_funcs->IoRead = (void *)lflash_funcs->IoRead;
	}

	PspIoDrvFuncs *funcs = msstor->funcs;

	funcs->IoDevctl = (void *)msstorIoDevctlPatch;
	funcs->IoWrite = (void *)msstorIoWritePatch;
	funcs->IoIoctl = (void *)msstorIoIoctlPatch;

	sceKernelCpuResumeIntr(intr);

	pspSdkSetK1(k1);
	return res;
}

//100%
//0x000004AC
int pspUsbDeviceFinishDevice(void)
{
	int k1 = pspSdkSetK1(0);
	int intr = sceKernelCpuSuspendIntr();

	PspIoDrvFuncs *funcs = msstor->funcs;

	funcs->IoIoctl = (void *)msstorIoIoctl;
	funcs->IoOpen = (void *)msstorIoOpen;
	funcs->IoClose = (void *)msstorIoClose;
	funcs->IoDevctl = (void *)msstorIoDevctl;
	funcs->IoRead = (void *)msstorIoRead;
	funcs->IoLseek = (void *)msstorIoLseek;
	funcs->IoWrite = (void *)msstorIoWrite;

	sceKernelCpuResumeIntr(intr);

	if(UmdSectorPoolAddr)
	{
		sceKernelFreeFpl(UmdSectorPoolId, UmdSectorPoolAddr);
		sceKernelDeleteFpl(UmdSectorPoolId);
		UmdSectorPoolAddr = NULL;
	}

	int ret = 0;

	if(flash_unassigned[PSP_USBDEVICE_FLASH0])
	{
		ret = sceIoAssign("flash0:", "lflash0:0,0", "flashfat0:", IOASSIGN_RDONLY, NULL, 0);
		if(ret < 0)
		{
			pspSdkSetK1(k1);
			return ret;
		}
	}

	if(flash_unassigned[PSP_USBDEVICE_FLASH1])
	{
		ret = sceIoAssign("flash1:", "lflash0:0,1", "flashfat1:", IOASSIGN_RDWR, NULL, 0);
		if(ret < 0)
		{
			pspSdkSetK1(k1);
			return ret;
		}
	}

	if(flash_unassigned[PSP_USBDEVICE_FLASH2])
	{
		ret = sceIoAssign("flash2:", "lflash0:0,2", "flashfat2:", IOASSIGN_RDWR, NULL, 0);
		if(ret < 0)
		{
			pspSdkSetK1(k1);
			return ret;
		}
	}

	if(flash_unassigned[PSP_USBDEVICE_FLASH3])
	{
		ret = sceIoAssign("flash3:", "lflash0:0,3", "flashfat3:", IOASSIGN_RDWR, NULL, 0);
	}

	pspSdkSetK1(k1);
	return ret;
}

//100%
//0x000002E8
int pspUsbDeviceInit()
{
	char string[12];
	strcpy(string, "lflash0:0,0");

	lflash = FindDriver("lflash");
	umd = FindDriver("umd");
	msstor = FindDriver("msstor");

	if(!lflash || !msstor) return -1;

	SceUID fpl = sceKernelCreateFpl("", 1, 0, 512, 1, NULL);
	if(fpl < 0) return fpl;

	u32 *buffer = NULL;
	sceKernelAllocateFpl(fpl, (void *)&buffer, 0);

	/* Find out size of flash0-flash3 */
	u32 *ptr = flashSize;

	u8 ch;
	for(ch = '0'; ch < '4'; ch++, ptr++)
	{
		string[10] = ch;

		SceUID fd = sceIoOpen(string, PSP_O_RDONLY, 0);
		if(fd >= 0)
		{
			if(sceIoIoctl(fd, 0x0003D001, NULL, 0, buffer, 68) >= 0) *ptr = buffer[0x14/4];
			sceIoClose(fd);
		}
	}

	PspIoDrvFuncs *funcs = msstor->funcs;

	msstorIoOpen = (void *)funcs->IoOpen;
	msstorIoClose = (void *)funcs->IoClose;
	msstorIoDevctl = (void *)funcs->IoDevctl;
	msstorIoRead = (void *)funcs->IoRead;
	msstorIoLseek = (void *)funcs->IoLseek;
	msstorIoWrite = (void *)funcs->IoWrite;
	msstorIoIoctl = (void *)funcs->IoIoctl;

	sceKernelFreeFpl(fpl, buffer);
	sceKernelDeleteFpl(fpl);

	return 0;
}