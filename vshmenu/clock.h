#ifndef __CLOCK_H__
#define __CLOCK_H__

extern int cpu_list[]; //0x00002284
extern int bus_list[]; //0x000022A8

int limit(int val, int min, int max);
int GetCPUSpeed(int cpu);
int GetBUSSpeed(int bus);

#endif