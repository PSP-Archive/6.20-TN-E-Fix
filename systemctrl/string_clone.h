#ifndef __STRING_CLONE_H__
#define __STRING_CLONE_H__

char *strtok_r_clone(char *string, const char *seps, char **context);
char *strtok_clone(char *str, const char *seps);
void atob_clone(char *a0, int *a1);
size_t strspn_clone(const char *s, const char *accept);
size_t strcspn_clone(const char *s, const char *reject);

#endif