/* v2 check, silent: a streamed voice never started by the program, a pause, and a resume. */
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
HRESULT WINAPI DirectSoundCreate8(const void*,void*,void*);
static const GUID cls={0xfac23f48,0x31f5,0x45a8,{0xb4,0x9b,0x52,0x25,0xd6,0x14,0x01,0xaa}};
static const GUID iid={0x8bcf1f58,0x9fe7,0x4583,{0x8a,0xc6,0xe2,0xad,0xc4,0x65,0xc8,0xbb}};
#define VT(o) (*(void ***)(o))
#define START(v) ((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(v)[17])(v,0,0)
#define STOP(v) ((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(v)[18])(v,0,0)
static short pcm[48000*4];static void*x,*m,*ds;
static void*mk(unsigned flags){void*v=0;unsigned b[9]={0};WAVEFORMATEX f={1,1,48000,96000,2,16,0};
 ((HRESULT(WINAPI*)(void*,void**,void*,UINT32,float,void*,void*,void*))VT(x)[8])(x,&v,&f,0,2.0f,0,0,0);
 b[0]=flags;b[1]=sizeof pcm;memcpy(&b[2],&(void*){pcm},4);((HRESULT(WINAPI*)(void*,void*,void*))VT(v)[19])(v,b,0);return v;}
static unsigned played(void*v){unsigned char s[16];unsigned long long p;((void(WINAPI*)(void*,void*))VT(v)[23])(v,s);memcpy(&p,s+8,8);return (unsigned)p;}
int main(void){void*st,*pr,*a,*b,*c;int i;CoInitializeEx(0,0);DirectSoundCreate8(0,&ds,0);
 if(FAILED(CoCreateInstance(&cls,0,1,&iid,&x)))return 1;((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(x)[5])(x,0,1);
 if(FAILED(((HRESULT(WINAPI*)(void*,void**,UINT32,UINT32,UINT32,UINT32,void*))VT(x)[10])(x,&m,0,0,0,0,0)))return 2;
 st=mk(0);pr=mk(0x40);a=mk(0x40);b=mk(0x40);c=mk(0x40);START(a);START(b);START(c);
 for(i=0;i<6;i++){Sleep(100);printf("%4d ms streamed-unstarted %6u  loaded-unstarted %u\n",(i+1)*100,played(st),played(pr));}
 STOP(a);STOP(b);STOP(c);Sleep(100);{unsigned p=played(st);Sleep(300);printf("paused: streamed moved %u samples in 300 ms\n",played(st)-p);}
 START(a);START(b);START(c);START(st);{unsigned p=played(st);Sleep(300);printf("resumed: streamed moved %u samples in 300 ms\n",played(st)-p);}
 return 0;}
