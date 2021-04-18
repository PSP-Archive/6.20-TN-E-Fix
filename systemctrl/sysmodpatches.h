#ifndef __SYSMODPATCHES_H__
#define __SYSMODPATCHES_H__

#include "conf.h"

typedef struct
{
	u32 magic;
	u32 version;
	u32 keyofs;
	u32 valofs;
	u32 count;
} __attribute__((packed)) SFOHeader;

typedef struct
{
	u16 nameofs;
	u8 alignment;
	u8 type;
	u32 valsize;
	u32 totalsize;
	u32 dataofs;
} __attribute__((packed)) SFOEntry;

typedef struct _MemMgmtSubBlock
{
	struct _MemMgmtSubBlock *next;
	int pos; //bit 1 = useflag, >> 1 = pos
	int nblocks; //bit 1 = delflag, bit 2 = sizelock, >> 2 = num of blocks
} MemMgmtSubBlock;

typedef struct _MemMgmtBlock
{
	struct _MemMgmtBlock *next;
	MemMgmtSubBlock subblocks[0x14];
} MemMgmtBlock;

typedef struct _PartitionInfo
{
	struct _PartitionInfo *next; //0
	int addr; //4
	int size; //8
	int attr; //c
	struct _MemMgmtBlock *head;
	int nBlocksUsed; //14
	int unk_18;
} PartitionInfo;

extern u32 set_p2;
extern u32 set_p8;
extern TNConfig config;

u32 FindPowerFunction(u32 nid);
void SetSpeed(int cpu, int bus);

void OnImposeLoad();
void OnVshLoad();
void PatchWlan(u32 text_addr);
void PatchPower(u32 text_addr);
void PatchUmdCache(u32 text_addr);
void PatchUmdMan(u32 text_addr);
void PatchMediaSync(u32 text_addr);
void PatchSysconfPlugin(u32 text_addr);
void PatchMsVideoMainPlugin(u32 text_addr);
void PatchGamePlugin(u32 text_addr);
void PatchUpdatePlugin(u32 text_addr, u32 text_size);

#endif