/* World at War audio logger. A stand-in dsound.dll: it forwards every DirectSound
 * export to the system's dsound.dll and, for the game, hooks CoCreateInstance so
 * XAudio2 2.0 objects get their start/stop/commit calls logged.
 * Single-player diagnosis only. It changes no files of the game and no other program.
 * Files, next to the DLL: wawaudio.log (output); wawaudio_fix.txt, if present when the
 * game starts, also commits queued operations right after a deferred Start. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define LOG_MAX (32u*1024*1024)
#define RING (1u<<20)
static char ring[RING], chunk[RING];
static unsigned ring_len, dropped, total;
static CRITICAL_SECTION lk;
static HANDLE logf_ = INVALID_HANDLE_VALUE;
static LARGE_INTEGER qf, q0;
static HMODULE self;
static int fix_mode;

static double now_ms(void)
{
    LARGE_INTEGER q;
    QueryPerformanceCounter(&q);
    return (q.QuadPart - q0.QuadPart) * 1000.0 / qf.QuadPart;
}

static void out(const char *f, ...)
{
    char b[512];
    int n, m;
    va_list v;

    n = _snprintf(b, sizeof b - 2, "%10.1f t%04lx ", now_ms(), GetCurrentThreadId());
    va_start(v, f);
    m = _vsnprintf(b + n, sizeof b - 2 - n, f, v);
    va_end(v);
    n = (m < 0 || n + m > (int)sizeof b - 2) ? (int)sizeof b - 2 : n + m;
    b[n++] = '\n';
    EnterCriticalSection(&lk);
    if (ring_len + n <= RING && total < LOG_MAX) {
        memcpy(ring + ring_len, b, n);
        ring_len += n;
        total += n;
    } else dropped++;
    LeaveCriticalSection(&lk);
}

static DWORD WINAPI writer(void *u)
{
    (void)u;
    for (;;) {
        unsigned n, d;
        DWORD w;
        Sleep(200);
        EnterCriticalSection(&lk);
        n = ring_len; memcpy(chunk, ring, n); ring_len = 0;
        d = dropped; dropped = 0;
        LeaveCriticalSection(&lk);
        if (n) WriteFile(logf_, chunk, n, &w, 0);
        if (d) {
            char b[64];
            int k = _snprintf(b, sizeof b, "-- %u lines dropped\n", d);
            WriteFile(logf_, b, k, &w, 0);
        }
        if (n || d) FlushFileBuffers(logf_);
    }
}

/* ---- shared-vtable patching: the table's address stays the same, only entries change,
 * so code that identifies an object by its vtable still works. ---- */
#define SLOTS 32
typedef struct { void **vt; void *orig[SLOTS]; } Tab;
static Tab tabs[32];
static volatile LONG ntabs;

static Tab *tab_of(void *obj)
{
    void **vt = *(void ***)obj;
    LONG i, n = ntabs;
    for (i = 0; i < n; i++) if (tabs[i].vt == vt) return &tabs[i];
    return 0;
}
#define ORIG(obj, s) (tab_of(obj)->orig[s])

typedef struct { int slot; void *fn; } Hook;

static int patch(void *obj, const Hook *h, int count, int slots, const char *what)
{
    void **vt;
    Tab *t;
    DWORD old;
    int i;

    if (!obj) return 0;
    EnterCriticalSection(&lk);
    if (tab_of(obj)) { LeaveCriticalSection(&lk); return 1; }
    vt = *(void ***)obj;
    if (ntabs >= 32 || !VirtualProtect(vt, slots * sizeof(void *), PAGE_READWRITE, &old)) {
        DWORD e = GetLastError();
        LeaveCriticalSection(&lk);
        out("PATCH FAILED %s vtable %p err %lu", what, vt, e);
        return 0;
    }
    t = &tabs[ntabs];
    t->vt = vt;
    for (i = 0; i < slots; i++) t->orig[i] = vt[i];
    InterlockedIncrement(&ntabs);   /* publish the originals before any entry changes */
    for (i = 0; i < count; i++) vt[h[i].slot] = h[i].fn;
    VirtualProtect(vt, slots * sizeof(void *), old, &old);
    LeaveCriticalSection(&lk);
    out("patched %s vtable %p", what, vt);
    return 1;
}

static void *engine;
typedef HRESULT (WINAPI *F1)(void *);
typedef void (WINAPI *V1)(void *);
typedef HRESULT (WINAPI *F2)(void *, UINT32);
typedef HRESULT (WINAPI *F3)(void *, UINT32, UINT32);
typedef HRESULT (WINAPI *FSub)(void *, const UINT32 *, const void *);
typedef void (WINAPI *FState)(void *, void *);
typedef void (WINAPI *VP)(void *, void *);
typedef void (WINAPI *VPE)(void *, void *, HRESULT);

/* voice callbacks: PassStart0 PassEnd1 StreamEnd2 BufferStart3 BufferEnd4 LoopEnd5 Error6 */
static void WINAPI cb_stream_end(void *t) { out("cb %p StreamEnd", t); ((V1)ORIG(t, 2))(t); }
static void WINAPI cb_buf_start(void *t, void *c) { out("cb %p BufferStart ctx %p", t, c); ((VP)ORIG(t, 3))(t, c); }
static void WINAPI cb_buf_end(void *t, void *c) { out("cb %p BufferEnd ctx %p", t, c); ((VP)ORIG(t, 4))(t, c); }
static void WINAPI cb_error(void *t, void *c, HRESULT e) { out("cb %p VOICE ERROR ctx %p hr %08lx", t, c, e); ((VPE)ORIG(t, 6))(t, c, e); }
static const Hook cb_hooks[] = {{2, cb_stream_end}, {3, cb_buf_start}, {4, cb_buf_end}, {6, cb_error}};

/* source voice, XAudio2 2.0 layout:
 * DestroyVoice16 Start17 Stop18 Submit19 Flush20 Discontinuity21 ExitLoop22 GetState23 */
static void WINAPI v_destroy(void *t) { out("voice %p Destroy", t); ((V1)ORIG(t, 16))(t); }

static HRESULT WINAPI v_start(void *t, UINT32 f, UINT32 op)
{
    HRESULT hr = ((F3)ORIG(t, 17))(t, f, op);
    out("voice %p Start flags %u opset %u%s hr %08lx", t, f, op, op ? " DEFERRED" : "", hr);
    if (op && fix_mode && engine) {
        HRESULT c = ((F2)ORIG(engine, 13))(engine, 0);
        out("fix: CommitChanges(ALL) hr %08lx", c);
    }
    return hr;
}

static HRESULT WINAPI v_stop(void *t, UINT32 f, UINT32 op)
{
    HRESULT hr = ((F3)ORIG(t, 18))(t, f, op);
    out("voice %p Stop flags %u opset %u%s hr %08lx", t, f, op, op ? " DEFERRED" : "", hr);
    return hr;
}

static HRESULT WINAPI v_submit(void *t, const UINT32 *b, const void *w)
{
    UINT32 fl = b ? b[0] : 0, by = b ? b[1] : 0;
    HRESULT hr = ((FSub)ORIG(t, 19))(t, b, w);
    out("voice %p Submit bytes %u flags %u wma %d hr %08lx", t, by, fl, w != 0, hr);
    return hr;
}

static HRESULT WINAPI v_flush(void *t)
{
    HRESULT hr = ((F1)ORIG(t, 20))(t);
    out("voice %p Flush hr %08lx", t, hr);
    return hr;
}

/* GetState is only watched when the game calls it, so no voice is read after it is destroyed */
static struct { void *v; UINT64 played; double since, said; } st[512];

static void WINAPI v_state(void *t, void *s)
{
    UINT32 queued;
    UINT64 played;
    double n;
    unsigned i = ((UINT_PTR)t >> 4) % 512;

    ((FState)ORIG(t, 23))(t, s);
    if (!s) return;
    memcpy(&queued, (char *)s + 4, 4);
    memcpy(&played, (char *)s + 8, 8);
    n = now_ms();
    EnterCriticalSection(&lk);
    if (st[i].v != t || st[i].played != played) {
        st[i].v = t; st[i].played = played; st[i].since = n; st[i].said = 0;
        LeaveCriticalSection(&lk);
        return;
    }
    if (!queued || n - st[i].since < 500 || n - st[i].said < 1000) { LeaveCriticalSection(&lk); return; }
    st[i].said = n;
    n -= st[i].since;
    LeaveCriticalSection(&lk);
    out("voice %p NOT ADVANCING %.0f ms, %u buffers queued, played %lu", t, n, queued, (unsigned long)played);
}
static const Hook v_hooks[] = {{16, v_destroy}, {17, v_start}, {18, v_stop}, {19, v_submit}, {20, v_flush}, {23, v_state}};

/* IXAudio2 2.0 layout: CreateSourceVoice8 StartEngine11 StopEngine12 CommitChanges13 */
typedef HRESULT (WINAPI *FCreate)(void *, void **, const WAVEFORMATEX *, UINT32, float, void *, const void *, const void *);

static HRESULT WINAPI e_create(void *t, void **pp, const WAVEFORMATEX *w, UINT32 fl, float r, void *cb, const void *s, const void *c)
{
    HRESULT hr;
    if (cb) patch(cb, cb_hooks, 4, 7, "voice callback");
    hr = ((FCreate)ORIG(t, 8))(t, pp, w, fl, r, cb, s, c);
    if (SUCCEEDED(hr) && pp && *pp) {
        patch(*pp, v_hooks, 6, 26, "source voice");
        out("voice %p Create fmt %04x ch %u rate %lu bits %u flags %u cb %p", *pp, w ? w->wFormatTag : 0,
            w ? w->nChannels : 0, w ? w->nSamplesPerSec : 0, w ? w->wBitsPerSample : 0, fl, cb);
    } else out("CreateSourceVoice FAILED fmt %04x ch %u hr %08lx", w ? w->wFormatTag : 0, w ? w->nChannels : 0, hr);
    return hr;
}

static HRESULT WINAPI e_start(void *t)
{
    HRESULT hr;
    out("engine StartEngine");
    hr = ((F1)ORIG(t, 11))(t);
    out("engine StartEngine done hr %08lx", hr);
    return hr;
}
static void WINAPI e_stop(void *t) { out("engine StopEngine"); ((V1)ORIG(t, 12))(t); out("engine StopEngine done"); }
static HRESULT WINAPI e_commit(void *t, UINT32 op)
{
    HRESULT hr = ((F2)ORIG(t, 13))(t, op);
    out("engine CommitChanges opset %u hr %08lx", op, hr);
    return hr;
}
static const Hook e_hooks[] = {{8, e_create}, {11, e_start}, {12, e_stop}, {13, e_commit}};

static const GUID clsid_xa20 = {0xfac23f48, 0x31f5, 0x45a8, {0xb4, 0x9b, 0x52, 0x25, 0xd6, 0x14, 0x01, 0xaa}};
typedef HRESULT (WINAPI *FCo)(REFCLSID, IUnknown *, DWORD, REFIID, void **);
static FCo real_co;

static HRESULT WINAPI my_co(REFCLSID c, IUnknown *o, DWORD x, REFIID i, void **pp)
{
    HRESULT hr = real_co(c, o, x, i, pp);
    if (c && !memcmp(c, &clsid_xa20, sizeof(GUID))) {
        out("CoCreateInstance XAudio2 2.0 hr %08lx obj %p", hr, pp ? *pp : 0);
        if (SUCCEEDED(hr) && pp && *pp && patch(*pp, e_hooks, 4, 16, "XAudio2 engine")) engine = *pp;
    }
    return hr;
}

/* replace one import of the main program, matched by name (or by address when names are gone) */
static int hook_import(HMODULE m, const char *dll, const char *fn, void *to, void **was)
{
    BYTE *b = (BYTE *)m;
    IMAGE_NT_HEADERS *nt = (void *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    DWORD rva = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
    IMAGE_IMPORT_DESCRIPTOR *d;
    HMODULE lib = GetModuleHandleA(dll);
    void *want = lib ? (void *)GetProcAddress(lib, fn) : 0;

    if (!rva) return 0;
    for (d = (void *)(b + rva); d->Name; d++) {
        void **a = (void **)(b + d->FirstThunk);
        DWORD *n = d->OriginalFirstThunk ? (DWORD *)(b + d->OriginalFirstThunk) : 0;
        if (lstrcmpiA((char *)b + d->Name, dll)) continue;
        for (; *a; a++) {
            int hit = want && *a == want;
            DWORD old;
            if (n) {
                if (*n && !(*n & 0x80000000u) && !strcmp((char *)b + *n + 2, fn)) hit = 1;
                n++;
            }
            if (!hit) continue;
            if (!VirtualProtect(a, sizeof *a, PAGE_READWRITE, &old)) return 0;
            *was = *a;
            *a = to;
            VirtualProtect(a, sizeof *a, old, &old);
            return 1;
        }
    }
    return 0;
}

/* ---- DirectSound forwarding; the system library is loaded on first use, never in DllMain ---- */
static HMODULE sysds(void)
{
    static HMODULE h;
    if (!h) {
        char p[MAX_PATH];
        UINT n = GetSystemDirectoryA(p, MAX_PATH - 12);
        lstrcpyA(p + n, "\\dsound.dll");
        h = LoadLibraryA(p);
        if (h == self) h = 0;
        out("system dsound %s -> %p", p, h);
    }
    return h;
}
static void *sysfn(const char *n) { HMODULE h = sysds(); return h ? (void *)GetProcAddress(h, n) : 0; }

#define NODRV ((HRESULT)0x88780078)
#define FWD(name, args, call) HRESULT WINAPI name args { HRESULT (WINAPI *f) args = sysfn(#name); return f ? f call : NODRV; }
FWD(DirectSoundCreate, (const void *a, void *b, void *c), (a, b, c))
FWD(DirectSoundCreate8, (const void *a, void *b, void *c), (a, b, c))
FWD(DirectSoundCaptureCreate, (const void *a, void *b, void *c), (a, b, c))
FWD(DirectSoundCaptureCreate8, (const void *a, void *b, void *c), (a, b, c))
FWD(DirectSoundEnumerateA, (void *a, void *b), (a, b))
FWD(DirectSoundEnumerateW, (void *a, void *b), (a, b))
FWD(DirectSoundCaptureEnumerateA, (void *a, void *b), (a, b))
FWD(DirectSoundCaptureEnumerateW, (void *a, void *b), (a, b))
FWD(GetDeviceID, (const void *a, void *b), (a, b))
FWD(DirectSoundFullDuplexCreate, (void *a, void *b, void *c, void *d, void *e, DWORD l, void *g, void *h, void *i, void *j),
    (a, b, c, d, e, l, g, h, i, j))
HRESULT WINAPI DllGetClassObject(REFCLSID a, REFIID b, void **c)
{
    HRESULT (WINAPI *f)(REFCLSID, REFIID, void **) = sysfn("DllGetClassObject");
    return f ? f(a, b, c) : CLASS_E_CLASSNOTAVAILABLE;
}
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }

BOOL WINAPI DllMain(HINSTANCE h, DWORD why, void *r)
{
    char base[MAX_PATH], p[MAX_PATH + 32], exe[MAX_PATH], *s;

    (void)r;
    if (why != DLL_PROCESS_ATTACH) return TRUE;
    self = h;
    DisableThreadLibraryCalls(h);
    InitializeCriticalSection(&lk);
    QueryPerformanceFrequency(&qf);
    QueryPerformanceCounter(&q0);
    GetModuleFileNameA(h, base, MAX_PATH);
    if ((s = strrchr(base, '\\'))) *s = 0;
    _snprintf(p, sizeof p, "%s\\wawaudio_fix.txt", base);
    fix_mode = GetFileAttributesA(p) != INVALID_FILE_ATTRIBUTES;
    _snprintf(p, sizeof p, "%s\\wawaudio.log", base);
    logf_ = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    if (logf_ == INVALID_HANDLE_VALUE && GetEnvironmentVariableA("LOCALAPPDATA", base, MAX_PATH)) {
        _snprintf(p, sizeof p, "%s\\wawaudio.log", base);
        logf_ = CreateFileA(p, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
    }
    GetModuleFileNameA(0, exe, MAX_PATH);
    out("wawaudio 1, program %s, fix %s", exe, fix_mode ? "ON" : "off");
    if (hook_import(GetModuleHandleA(0), "ole32.dll", "CoCreateInstance", my_co, (void **)&real_co)) out("CoCreateInstance hooked");
    else out("CoCreateInstance NOT hooked: nothing more will be logged");
    if (logf_ != INVALID_HANDLE_VALUE) {
        HANDLE t = CreateThread(0, 0, writer, 0, 0, 0);
        if (t) CloseHandle(t);
    }
    return TRUE;
}
