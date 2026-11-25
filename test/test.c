/* Silent check of the fix against the real XAudio2 2.0. Run it as CoDWaW.exe (the fix should act)
 * and as CoDWaWmp.exe (it should not), next to dsound.dll. Exit code 0 means every check passed. */
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

HRESULT WINAPI DirectSoundCreate8(const void *, void *, void *);
static const GUID cls = {0xfac23f48, 0x31f5, 0x45a8, {0xb4, 0x9b, 0x52, 0x25, 0xd6, 0x14, 0x01, 0xaa}};
static const GUID iid = {0x8bcf1f58, 0x9fe7, 0x4583, {0x8a, 0xc6, 0xe2, 0xad, 0xc4, 0x65, 0xc8, 0xbb}};
#define VT(o) (*(void ***)(o))
static short pcm[48000 * 8];
static void *x, *m;
static int failed, acts;

static void start(void *v) { ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(v)[17])(v, 0, 0); }
static void stop(void *v) { ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(v)[18])(v, 0, 0); }
static void end(void *v)
{
    stop(v);
    ((HRESULT (WINAPI *)(void *))VT(v)[20])(v);
    ((void (WINAPI *)(void *))VT(v)[16])(v);
}
static void *make(unsigned flags)
{
    void *v = 0, *data = pcm;
    unsigned b[9] = {0};
    WAVEFORMATEX f = {1, 1, 48000, 96000, 2, 16, 0};
    ((HRESULT (WINAPI *)(void *, void **, void *, UINT32, float, void *, void *, void *))VT(x)[8])(x, &v, &f, 0, 2.0f, 0, 0, 0);
    if (!v) { puts("no source voice"); ExitProcess(3); }
    b[0] = flags; b[1] = sizeof pcm; memcpy(&b[2], &data, 4);
    ((HRESULT (WINAPI *)(void *, void *, void *))VT(v)[19])(v, b, 0);
    return v;
}
static unsigned played(void *v)
{
    unsigned char s[16];
    unsigned long long p;
    ((void (WINAPI *)(void *, void *))VT(v)[23])(v, s);
    memcpy(&p, s + 8, 8);
    return (unsigned)p;
}
/* the game polls its voices while it runs */
static unsigned moved(void *v, int ms)
{
    unsigned p = played(v);
    for (; ms > 0; ms -= 20) { Sleep(20); played(v); }
    return played(v) - p;
}
static void check(const char *what, int ok) { printf("%-4s %s\n", ok ? "ok" : "FAIL", what); failed += !ok; }

int main(void)
{
    char path[MAX_PATH];
    void *ds = 0, *line, *line2, *loaded, *a, *b, *c, *click;
    int i, n = GetModuleFileNameA(0, path, MAX_PATH);

    acts = n > 10 && !lstrcmpiA(path + n - 10, "CoDWaW.exe");
    printf("the fix should %s\n", acts ? "act" : "stay out");
    CoInitializeEx(0, 0);
    check("DirectSound is passed through", SUCCEEDED(DirectSoundCreate8(0, &ds, 0)) && ds);
    if (FAILED(CoCreateInstance(&cls, 0, 1, &iid, &x))) { puts("XAudio2 2.0 is not installed"); return 2; }
    ((HRESULT (WINAPI *)(void *, UINT32, UINT32))VT(x)[5])(x, 0, 1);
    if (FAILED(((HRESULT (WINAPI *)(void *, void **, UINT32, UINT32, UINT32, UINT32, void *))VT(x)[10])(x, &m, 0, 0, 0, 0, 0))) return 2;

    line = make(0); loaded = make(0x40); a = make(0x40); b = make(0x40); c = make(0x40);
    start(a); start(b); start(c);
    check("a streamed voice does not start at once", moved(line, 100) == 0);
    moved(line, 350);
    check("a streamed voice starts after the delay (or not at all, when out)", (moved(line, 200) > 0) == acts);
    check("a loaded voice is never started for the game", moved(loaded, 400) == 0);

    stop(a); stop(b); stop(c);
    check("a pause holds it", (Sleep(50), moved(line, 300) == 0));
    click = make(0x40); start(click);
    line2 = make(0);
    check("a menu sound does not end the pause", moved(line, 300) == 0);
    check("no new voice is started while paused", moved(line2, 500) == 0);
    end(click);
    start(a); start(b); start(c); start(line); start(line2);
    check("a resume plays it again", moved(line, 200) > 0);

    stop(a); stop(b); stop(c);
    end(a); end(b); end(c); end(line); end(line2);
    line = make(0);
    moved(line, 450);
    check("after a pause that ends in a restart, new voices still start", (moved(line, 200) > 0) == acts);
    end(line);

    for (i = 0; i < 600; i++) end(make(i & 1 ? 0x40 : 0));
    line = make(0);
    moved(line, 450);
    check("after 600 short-lived voices, new voices still start", (moved(line, 200) > 0) == acts);
    end(line);
    printf("%d failed\n", failed);
    return failed != 0;
}
