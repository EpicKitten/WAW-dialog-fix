/* Does real XAudio2 2.0 call voice callbacks for a voice that has data but was never started? Silent. */
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
static const GUID cls={0xfac23f48,0x31f5,0x45a8,{0xb4,0x9b,0x52,0x25,0xd6,0x14,0x01,0xaa}};
static const GUID iid={0x8bcf1f58,0x9fe7,0x4583,{0x8a,0xc6,0xe2,0xad,0xc4,0x65,0xc8,0xbb}};
#define VT(o) (*(void ***)(o))
static volatile LONG n[7];
static void WINAPI p0(void*t){(void)t;InterlockedIncrement(&n[0]);}
static void WINAPI p1(void*t){(void)t;InterlockedIncrement(&n[1]);}
static void WINAPI p2(void*t){(void)t;InterlockedIncrement(&n[2]);}
static void WINAPI p3(void*t,void*c){(void)t;(void)c;InterlockedIncrement(&n[3]);}
static void WINAPI p4(void*t,void*c){(void)t;(void)c;InterlockedIncrement(&n[4]);}
static void WINAPI p5(void*t,void*c){(void)t;(void)c;InterlockedIncrement(&n[5]);}
static void WINAPI p6(void*t,void*c,HRESULT h){(void)t;(void)c;(void)h;InterlockedIncrement(&n[6]);}
static void *vt[7]={p0,p1,p2,p3,p4,p5,p6};static void *cb=vt;static short pcm[48000];
static void show(const char*w){printf("%-28s passStart %ld passEnd %ld streamEnd %ld bufStart %ld bufEnd %ld\n",w,n[0],n[1],n[2],n[3],n[4]);}
int main(void){void*x=0,*m=0,*v=0;unsigned b[9]={0};WAVEFORMATEX f={1,1,48000,96000,2,16,0};HRESULT hr;
 CoInitializeEx(0,0);hr=CoCreateInstance(&cls,0,1,&iid,&x);if(FAILED(hr))return 1;
 ((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(x)[5])(x,0,1);
 hr=((HRESULT(WINAPI*)(void*,void**,UINT32,UINT32,UINT32,UINT32,void*))VT(x)[10])(x,&m,0,0,0,0,0);if(FAILED(hr))return 2;
 hr=((HRESULT(WINAPI*)(void*,void**,void*,UINT32,float,void*,void*,void*))VT(x)[8])(x,&v,&f,0,2.0f,&cb,0,0);if(FAILED(hr))return 3;
 Sleep(400);show("created, no data, 400ms");
 b[1]=sizeof pcm;memcpy(&b[2],&(void*){pcm},4);((HRESULT(WINAPI*)(void*,void*,void*))VT(v)[19])(v,b,0);
 Sleep(500);show("data, not started, 500ms");
 ((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(v)[17])(v,0,0);Sleep(300);show("started, 300ms");
 ((HRESULT(WINAPI*)(void*,UINT32,UINT32))VT(v)[18])(v,0,0);Sleep(50);{LONG a=n[0];Sleep(300);printf("stopped again: %ld more passStart in 300ms\n",n[0]-a);}
 ((void(WINAPI*)(void*))VT(v)[16])(v);((void(WINAPI*)(void*))VT(m)[16])(m);return 0;}
