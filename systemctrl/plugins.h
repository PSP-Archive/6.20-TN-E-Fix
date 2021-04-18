#ifndef __PLUGINS_H__
#define __PLUGINS_H__

extern u32 apitype_addr;
extern u32 filename_addr;
extern u32 keyconfig_addr;

int PatchInit(int (* module_bootstart)(SceSize, void *), void *argp);

#endif