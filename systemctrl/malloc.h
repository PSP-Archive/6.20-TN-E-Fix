#ifndef __MALLOC_H__
#define __MALLOC_H__

int mallocinit();
void *tn_malloc(SceUID partitionid, SceSize size, SceUID *memid);
void tn_free(SceUID memid);

#endif