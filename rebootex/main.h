#ifndef __MAIN_H__
#define __MAIN_H__

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);
#define MAKE_DUMMY_FUNCTION1(a) _sw(0x03E00008, a); _sw(0x24020001, a + 4);

enum BootLoadFlags
{
	BOOTLOAD_VSH = 1,
	BOOTLOAD_GAME = 2,
	BOOTLOAD_UPDATER = 4,
	BOOTLOAD_POPS = 8,
	BOOTLOAD_UNK = 32,
	BOOTLOAD_UMDEMU = 64, /* for original NP9660 */
	BOOTLOAD_MLNAPP = 128,
};

typedef struct
{
	u32 signature;
	u32 devkit;
	u32 unknown[2];
	u32 modestart;
	int nmodes;
	u32 unknown2[2];
	u32 modulestart;
	int nmodules;
	u32 unknown3[2];
	u32 modnamestart;
	u32 modnameend;
	u32 unknown4[2];
}  __attribute__((packed)) BtcnfHeader;

typedef struct
{
	u16 maxsearch;
	u16 searchstart;
	int mode1;
	int mode2;
	int reserved[5];
} __attribute__((packed)) ModeEntry;

typedef struct
{
	u32 stroffset;
	int reserved;
	u16 flags;
	u8 loadmode;
	u8 signcheck;
	int reserved2;
	u8 hash[0x10];
} __attribute__((packed)) ModuleEntry;

#endif