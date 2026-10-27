#include "../source/ui.h"
#include <SDL2/SDL.h>
#include <stdlib.h>
int main(int argc,char **argv){
 if(!ui_init())return 1;
 int state=argc>1?atoi(argv[1]):2;
 ui_draw(state?1:0,state?9000:0,0,0,0,state,86,0,"21:48","PC   192.168.1.100",0);
 SDL_Window *w=SDL_GetWindowFromID(1);SDL_Renderer *r=SDL_GetRenderer(w);
 SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
 SDL_RenderReadPixels(r,0,SDL_PIXELFORMAT_RGBA32,s->pixels,s->pitch);
 SDL_SaveBMP(s,argc>2?argv[2]:"preview.bmp");SDL_FreeSurface(s);ui_close();return 0;
}
