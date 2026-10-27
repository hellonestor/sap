#include "ui.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static SDL_Window *window;
static SDL_Renderer *r,*display;
static SDL_Surface *canvas;
static SDL_Texture *frame,*background;
static TTF_Font *fonts[7];
static const int sizes[]={18,23,30,40,82,15,31};
#define WHITE 0xeff1edff
#define MUTED 0x94999bff
#define LIME 0xc4f53fff
#define BG 0x171b1cff
#define INK 0x171b1cff
static void color(unsigned c){SDL_SetRenderDrawColor(r,c>>24,c>>16,c>>8,c);}
static void box(int x,int y,int w,int h,unsigned c){color(c);SDL_Rect q={x,y,w,h};SDL_RenderFillRect(r,&q);}
static void line(int x,int y,int xx,int yy,int width,unsigned c){thickLineRGBA(r,x,y,xx,yy,width,c>>24,c>>16,c>>8,c);}
typedef struct {int radius,width;unsigned color;SDL_Texture *texture;} Ring;
static Ring rings[32];static int ring_count;
static void ring(int x,int y,int rad,int width,unsigned c){
 Ring *item=NULL;
 for(int i=0;i<ring_count;i++)if(rings[i].radius==rad&&rings[i].width==width&&rings[i].color==c){item=&rings[i];break;}
 if(!item){
  if(ring_count==32)return;
  item=&rings[ring_count++];item->radius=rad;item->width=width;item->color=c;
  int n=2*rad+4;SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,n,n,32,SDL_PIXELFORMAT_RGBA32);
  if(!s)return;
  for(int yy=0;yy<n;yy++)for(int xx=0;xx<n;xx++){
   float dx=xx-(rad+1.5f),dy=yy-(rad+1.5f),distance=sqrtf(dx*dx+dy*dy);
   float outer=fminf(1.f,fmaxf(0.f,rad+.5f-distance));
   float inner=width>=rad?1.f:fminf(1.f,fmaxf(0.f,distance-(rad-width)+.5f));
   ((Uint32*)((Uint8*)s->pixels+yy*s->pitch))[xx]=SDL_MapRGBA(s->format,c>>24,c>>16,c>>8,(Uint8)(255*outer*inner));
  }
  item->texture=SDL_CreateTextureFromSurface(r,s);SDL_FreeSurface(s);
  SDL_SetTextureBlendMode(item->texture,SDL_BLENDMODE_BLEND);
 }
 SDL_Rect dst={x-rad-2,y-rad-2,rad*2+4,rad*2+4};SDL_RenderCopy(r,item->texture,NULL,&dst);
}
static void circle(int x,int y,int radius,unsigned c,bool fill){ring(x,y,radius,fill?radius:1,c);}
static void roundbox(int x,int y,int w,int h,int rad,unsigned c,bool fill){if(fill)roundedBoxRGBA(r,x,y,x+w,y+h,rad,c>>24,c>>16,c>>8,c);else roundedRectangleRGBA(r,x,y,x+w,y+h,rad,c>>24,c>>16,c>>8,c);}
typedef struct {char text[128];int font;unsigned color;SDL_Texture *texture;int w,h;} Label;
static Label labels[40];static int slot;
static void text(int x,int y,int font,unsigned c,const char *s,bool centered){
 Label *p=&labels[slot++];
 if(!p->texture||strcmp(p->text,s)||p->font!=font||p->color!=c){
  if(p->texture)SDL_DestroyTexture(p->texture);
  SDL_Color col={c>>24,c>>16,c>>8,c};SDL_Surface *surf=TTF_RenderUTF8_Blended(fonts[font],s,col);
  p->texture=NULL;if(!surf)return;
  p->texture=SDL_CreateTextureFromSurface(r,surf);p->w=surf->w;p->h=surf->h;SDL_FreeSurface(surf);
  snprintf(p->text,sizeof(p->text),"%s",s);p->font=font;p->color=c;
 }
 SDL_Rect dst={centered?x-p->w/2:x,y,p->w,p->h};SDL_RenderCopy(r,p->texture,NULL,&dst);
}
static bool held(uint64_t b,int bit){return !!(b&(1ULL<<bit));}
static void button(int x,int y,const char *name,bool down){
 ring(x,y,24,down?4:3,down?LIME:MUTED);
 text(x,y-22,2,down?WHITE:MUTED,name,true);
}
static void stick(int x,int y,int ax,int ay,bool down){
 bool moving=down||abs(ax)>3500||abs(ay)>3500;
 ring(x,y,47,moving?5:4,moving?LIME:MUTED);
 ring(x+ax*12/32768,y-ay*12/32768,27,3,down?LIME:MUTED);
}
static void usb_icon(int x,int y,unsigned c){
 line(x,y+17,x,y-14,3,c);filledTrigonRGBA(r,x,y-22,x-5,y-13,x+5,y-13,c>>24,c>>16,c>>8,c);
 line(x,y+6,x-10,y-3,3,c);line(x-10,y-3,x-10,y-10,3,c);circle(x-10,y-12,3,c,true);
 line(x,y,x+11,y-9,3,c);line(x+11,y-9,x+11,y-16,3,c);box(x+8,y-19,6,6,c);circle(x,y+17,4,c,true);
}
static void wifi_icon(int x,int y,unsigned c){
 for(int rad=13;rad<=29;rad+=8){
  int px=0,py=0;
  for(int deg=220;deg<=320;deg+=4){float a=deg*3.14159265f/180.f;int xx=x+cosf(a)*rad,yy=y+sinf(a)*rad;if(deg>220)line(px,py,xx,yy,3,c);px=xx;py=yy;}
 }
 circle(x,y-2,3,c,true);
}
bool ui_init(void){
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)<0||TTF_Init()<0)return false;
 SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,"linear");
 window=SDL_CreateWindow("SwitchAsPad",0,0,1280,720,0);if(!window)return false;
 display=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED);
 if(!display)display=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
 if(!display)return false;
 // All UI primitives and type render into one fixed 1280x720 canvas.
 // The finished frame is uniformly fitted to the display, never stretched per axis.
 canvas=SDL_CreateRGBSurfaceWithFormat(0,1280,720,32,SDL_PIXELFORMAT_RGBA32);
 if(!canvas)return false;
 r=SDL_CreateSoftwareRenderer(canvas);if(!r)return false;
 frame=SDL_CreateTexture(display,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STREAMING,1280,720);
 if(!frame)return false;
#ifdef __SWITCH__
 const char *regular="romfs:/ui.otf",*bold="romfs:/bold.otf",*layout="romfs:/layout.bmp";
#else
 const char *regular="romfs/ui.otf",*bold="romfs/bold.otf",*layout="romfs/layout.bmp";
#endif
 for(int i=0;i<7;i++){fonts[i]=TTF_OpenFont(i==4||i==6?bold:regular,sizes[i]);if(!fonts[i])return false;}
 SDL_Surface *bg=SDL_LoadBMP(layout);if(!bg)return false;
 background=SDL_CreateTextureFromSurface(r,bg);SDL_FreeSurface(bg);
 return background!=NULL;
}
void ui_draw(uint64_t b,int lx,int ly,int rx,int ry,int transport,int battery,bool powered,const char *clock,const char *ip,float progress){
 SDL_Event ev;while(SDL_PollEvent(&ev)){}
 slot=0;SDL_RenderCopy(r,background,NULL,NULL);
 text(113,23,6,WHITE,"SwitchAsPad",false);
 text(1038,26,1,WHITE,clock,false);
 roundbox(1135,35,34,19,3,WHITE,false);roundbox(1136,36,32,17,2,WHITE,false);box(1171,41,3,7,WHITE);
 if(battery>=0)box(1140,40,25*battery/100,9,battery<20?0xffb067ff:WHITE);
 char bat[24];if(battery>=0)snprintf(bat,sizeof(bat),"%d%%",battery);else snprintf(bat,sizeof(bat),"--%%");
 text(1187,26,1,WHITE,bat,false);
 if(powered){Sint16 bx[]={1250,1244,1249,1246,1255,1250};Sint16 by[]={32,44,44,54,41,41};filledPolygonRGBA(r,bx,by,6,196,245,63,255);}
 text(132,180,4,WHITE,transport?"已连接":"待连接",false);circle(439,249,15,transport?LIME:MUTED,true);
 text(135,309,3,WHITE,transport==2?"USB 有线连接":transport==1?"Wi-Fi 无线连接":"等待电脑接入",false);
 text(136,379,2,MUTED,transport?"电脑端已就绪":"请运行电脑接收程序",false);
 // Segmented status: left USB and right Wi-Fi, both have the same dimensions.
 roundbox(134,465,430,69,26,0x252b2dff,true);
 if(transport){int x=transport==2?135:350;roundbox(x,466,213,67,25,LIME,true);box(transport==2?320:350,466,29,67,LIME);}
 roundbox(134,465,430,69,26,0x777d80ff,false);
 usb_icon(210,500,transport==2?INK:MUTED);
 text(245,477,2,transport==2?INK:MUTED,"USB",false);
 wifi_icon(424,510,transport==1?INK:MUTED);
 text(457,477,2,transport==1?INK:MUTED,"Wi-Fi",false);
 text(136,561,1,MUTED,ip,false);
 // All controller coordinates are taken from the approved 16:9 artwork.
 line(809,221,826,221,4,held(b,11)?LIME:MUTED);
 line(967,221,985,221,4,held(b,10)?LIME:MUTED);line(976,212,976,230,4,held(b,10)?LIME:MUTED);
 stick(764,316,lx,ly,held(b,4));stick(1026,480,rx,ry,held(b,5));
 button(1026,281,"X",held(b,2));button(1071,326,"A",held(b,0));
 button(1026,371,"B",held(b,1));button(981,326,"Y",held(b,3));
 if(held(b,13))roundbox(751,418,25,32,4,LIME,true);
 if(held(b,15))roundbox(751,478,25,32,4,LIME,true);
 if(held(b,12))roundbox(718,451,32,27,4,LIME,true);
 if(held(b,14))roundbox(777,451,32,27,4,LIME,true);
 text(54,658,0,MUTED,"双通道 · 自动切换",false);
 text(1024,658,0,MUTED,"+  −  长按 2 秒退出",false);
 if(progress>0)box(54,639,(int)(1172*progress),3,LIME);
 SDL_RenderPresent(r);
 SDL_UpdateTexture(frame,NULL,canvas->pixels,canvas->pitch);
 int ow,oh;SDL_GetRendererOutputSize(display,&ow,&oh);
 float scale=fminf(ow/1280.f,oh/720.f);int w=lroundf(1280*scale),h=lroundf(720*scale);
 SDL_Rect dst={(ow-w)/2,(oh-h)/2,w,h};
 SDL_SetRenderDrawColor(display,0,0,0,255);SDL_RenderClear(display);SDL_RenderCopy(display,frame,NULL,&dst);SDL_RenderPresent(display);
}
void ui_close(void){
 for(int i=0;i<40;i++)if(labels[i].texture)SDL_DestroyTexture(labels[i].texture);
 for(int i=0;i<ring_count;i++)if(rings[i].texture)SDL_DestroyTexture(rings[i].texture);
 for(int i=0;i<7;i++)if(fonts[i])TTF_CloseFont(fonts[i]);
 if(background)SDL_DestroyTexture(background);
 if(frame)SDL_DestroyTexture(frame);
 if(r)SDL_DestroyRenderer(r);
 if(canvas)SDL_FreeSurface(canvas);
 if(display)SDL_DestroyRenderer(display);
 if(window)SDL_DestroyWindow(window);
 TTF_Quit();SDL_Quit();
}
