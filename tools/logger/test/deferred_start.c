/* Silent XAudio2 2.0 exercise: a deferred Start that is never committed, then StopEngine. */
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
HRESULT WINAPI DirectSoundCreate8(const void *, void *, void *);
static const GUID cls = {0xfac23f48, 0x31f5, 0x45a8, {0xb4, 0x9b, 0x52, 0x25, 0xd6, 0x14, 0x01, 0xaa}};
static const GUID iid = {0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb}};
#define VT(o) (*(void ***)(o))
static void WINAPI c0(void *t) { (void)t; }
static void WINAPI c1(void *t, void *c) { (void)t; (void)c; }
static void WINAPI c2(void *t, void *c, HRESULT h) { (void)t; (void)c; (void)h; }
static void *cbvt[7] = {c0, c0, c0, c1, c1, c1, c2};
static void *cb = cbvt;
static short pcm[48000 * 2];
static void state(void *v, const char *w)
{
    unsigned char s[16]; unsigned q; unsigned long long p;
    ((void (WINAPI *)(void *, void *))VT(v)[23])(v, s);
    memcpy(&q, s + 4, 4); memcpy(&p, s + 8, 8);
    printf("%-34s queued %u played %lu\n", w, q, (unsigned long)p);
}
int main(void)
{
    void *x = 0, *m = 0, *v = 0, *ds = 0; HRESULT hr; unsigned b[9] = {0}; int i;
    WAVEFORMATEX f = {1, 2, 48000, 192000, 4, 16, 0};
    CoInitializeEx(0, COINIT_MULTITHREADED);
    hr = DirectSoundCreate8(0, &ds, 0); printf("DirectSoundCreate8 %08lx\n", hr);
    hr = CoCreateInstance(&cls, 0, 1, &iid, &x); printf("XAudio2 2.0 %08lx\n", hr);
    if (FAILED(hr)) return 1;
    hr = ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(x)[5])(x, 0, 1); printf("Initialize %08lx\n", hr);
    hr = ((HRESULT (WINAPI *)(void *, void **, UINT32, UINT32, UINT32, UINT32, void *))VT(x)[10])(x, &m, 0, 0, 0, 0, 0); printf("Mastering %08lx\n", hr);
    if (FAILED(hr)) return 2;
    hr = ((HRESULT (WINAPI *)(void *, void **, void *, UINT32, float, void *, void *, void *))VT(x)[8])(x, &v, &f, 0, 2.0f, &cb, 0, 0); printf("Source %08lx\n", hr);
    if (FAILED(hr)) return 3;
    b[1] = sizeof pcm; memcpy(&b[2], &(void *){pcm}, 4);
    hr = ((HRESULT (WINAPI *)(void *, void *, void *))VT(v)[19])(v, b, 0); printf("Submit %08lx\n", hr);
    hr = ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(v)[17])(v, 0, 5); printf("Start opset 5 %08lx\n", hr);
    for (i = 0; i < 12; i++) { Sleep(100); state(v, "after deferred Start"); }
    ((void (WINAPI *)(void *))VT(x)[12])(x); puts("StopEngine");
    hr = ((HRESULT (WINAPI *)(void *))VT(x)[11])(x); printf("StartEngine %08lx\n", hr);
    Sleep(400); state(v, "after StopEngine/StartEngine");
    ((HRESULT (WINAPI *)(void *, UINT32))VT(x)[13])(x, 5); Sleep(400); state(v, "after CommitChanges(5)");
    ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(v)[18])(v, 0, 0);
    ((HRESULT (WINAPI *)(void *))VT(v)[20])(v);
    ((void (WINAPI *)(void *))VT(v)[16])(v);
    ((void (WINAPI *)(void *))VT(m)[16])(m);
    Sleep(500);
    puts("done");
    return 0;
}
