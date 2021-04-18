#ifndef __BLIT_H__
#define __BLIT_H__

#define CENTER(x) (pwidth - (((x >> 1) << 1) * 8)) / 2

extern int pwidth;

int blit_setup();
int blit_load_font();
void blit_set_color(int fg_col, int bg_col);
int blit_string(int sx, int sy, const char *msg);

#endif