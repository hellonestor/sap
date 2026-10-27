#pragma once
#include <stdint.h>
#include <stdbool.h>
bool ui_init(void);
void ui_draw(uint64_t buttons,int lx,int ly,int rx,int ry,int transport,int battery,bool powered,const char *clock,const char *ip,float exit_progress);
void ui_close(void);
