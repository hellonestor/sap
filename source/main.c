#include <switch.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "ui.h"
typedef struct __attribute__((packed)){char magic[4];uint32_t sequence;uint64_t buttons;int16_t lx,ly,rx,ry;} PadReport;
_Static_assert(sizeof(PadReport)==24,"report size");
static unsigned char usb_buffer[4096] __attribute__((aligned(4096)));
static u64 ms(void){return armTicksToNs(armGetSystemTick())/1000000;}
int main(void){
 Result rom=romfsInit();
 if(R_FAILED(rom)||!ui_init()){
  ui_close();consoleInit(NULL);printf("UI initialization failed. Please restart from hbmenu.\n");
  PadState errpad;padConfigureInput(1,HidNpadStyleSet_NpadStandard);padInitializeDefault(&errpad);
  while(appletMainLoop()){padUpdate(&errpad);
 if(padGetButtonsDown(&errpad)&HidNpadButton_Plus)break;
  consoleUpdate(NULL);}
  consoleExit(NULL);
 if(R_SUCCEEDED(rom))romfsExit();
  return 1;
 }
 padConfigureInput(1,HidNpadStyleSet_NpadStandard);PadState pad;padInitializeDefault(&pad);
 Result psm=psmInitialize();
 Result net=socketInitializeDefault();int sock=R_SUCCEEDED(net)?socket(AF_INET,SOCK_DGRAM,0):-1;
 int yes=1;
 if(sock>=0)setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
 struct sockaddr_in dest={.sin_family=AF_INET,.sin_port=htons(26780)};dest.sin_addr.s_addr=inet_addr("255.255.255.255");
 FILE *f=fopen("sdmc:/switch/switchaspad/pc.txt","r");
 if(f){char ip[64];
 if(fgets(ip,sizeof(ip),f)){ip[strcspn(ip,"\r\n ")]=0;inet_pton(AF_INET,ip,&dest.sin_addr);}fclose(f);}
 char address[80];snprintf(address,sizeof(address),"PC  %s",inet_ntoa(dest.sin_addr));
 UsbCommsInterfaceInfo info={0xff,0,0};Result usb=usbCommsInitializeEx(1,&info,0x057e,0x3001);
 bool pending=false;u32 urb=0;uint32_t seq=0;u64 exit_start=0,last_usb=0,last_wifi=0,next_draw=0,next_power=0;
 int battery=-1;bool powered=false;
 while(appletMainLoop()){
  u64 now=ms();padUpdate(&pad);u64 keys=padGetButtons(&pad);
  if((keys&(HidNpadButton_Plus|HidNpadButton_Minus))==(HidNpadButton_Plus|HidNpadButton_Minus)){
   if(!exit_start)exit_start=now;
 if(now-exit_start>=2000)break;
  }else exit_start=0;
  HidAnalogStickState l=padGetStickPos(&pad,0),r=padGetStickPos(&pad,1);
  PadReport report={{'S','P','A','D'},seq++,keys,l.x,l.y,r.x,r.y};
  if(sock>=0){
   sendto(sock,&report,sizeof(report),MSG_DONTWAIT,(struct sockaddr*)&dest,sizeof(dest));
   char ack[16];struct sockaddr_in from;socklen_t len=sizeof(from);
   for(int i=0;i<8;i++){
    int n=recvfrom(sock,ack,sizeof(ack),MSG_DONTWAIT,(struct sockaddr*)&from,&len);
    if(n<0)break;
    if(n==4&&!memcmp(ack,"SPAK",4)&&from.sin_port==htons(26780)&&
       (dest.sin_addr.s_addr==INADDR_BROADCAST||from.sin_addr.s_addr==dest.sin_addr.s_addr))last_wifi=now;
   }
  }
  if(R_SUCCEEDED(usb)){
   if(pending&&R_SUCCEEDED(eventWait(usbCommsGetWriteCompletionEvent(0),0))){
    u32 size=0;Result rc=usbCommsGetWriteResult(urb,&size,0);pending=false;
    if(R_SUCCEEDED(rc)&&size==sizeof(report))last_usb=now;
   }
   UsbState state;
   if(!pending&&R_SUCCEEDED(usbDsGetState(&state))&&state==UsbState_Configured){
    memcpy(usb_buffer,&report,sizeof(report));pending=R_SUCCEEDED(usbCommsWriteAsync(usb_buffer,sizeof(report),&urb,0));
   }
  }
  if(now>=next_power){
   next_power=now+2000;u32 value;PsmChargerType charger;
   battery=R_SUCCEEDED(psm)&&R_SUCCEEDED(psmGetBatteryChargePercentage(&value))?(int)value:-1;
   powered=R_SUCCEEDED(psm)&&R_SUCCEEDED(psmGetChargerType(&charger))&&charger!=PsmChargerType_Unconnected;
  }
  if(now>=next_draw){
   next_draw=now+33;time_t t=time(NULL);struct tm local;char clock[16]="--:--";
   if(localtime_r(&t,&local))strftime(clock,sizeof(clock),"%H:%M",&local);
   int transport=last_usb&&now-last_usb<500?2:last_wifi&&now-last_wifi<1000?1:0;
   ui_draw(keys,l.x,l.y,r.x,r.y,transport,battery,powered,clock,address,exit_start?(now-exit_start)/2000.f:0);
  }
  svcSleepThread(8000000);
 }
 if(R_SUCCEEDED(usb))usbCommsExit();
 if(sock>=0)close(sock);
 if(R_SUCCEEDED(net))socketExit();
 if(R_SUCCEEDED(psm))psmExit();
 ui_close();romfsExit();return 0;
}
