/* waw-dialog-fix: a stand-in dsound.dll for Call of Duty: World at War (single player) on Playden.
 *
 * Under Playden the game fills its streamed XAudio2 voices (dialog, some ambience) with audio but
 * does not start them until the player pauses and unpauses. This library starts such a voice
 * after DELAY_MS. It writes no files and changes no game files.
 *
 * - Every DirectSound export is passed to the system's dsound.dll.
 * - Only in CoDWaW.exe, the program's CoCreateInstance import is hooked to find XAudio2 2.0.
 *   In any other program (CoDWaWmp.exe, for one) nothing is hooked.
 * - Entries of XAudio2's own method tables are replaced in place; the tables keep their
 *   addresses, so code that tells objects apart by table still works.
 *
 * Only kernel32 is used: there is no C runtime. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

#ifndef DELAY_MS
#define DELAY_MS 250
#endif
#define END_OF_STREAM 0x40u     /* XAUDIO2_END_OF_STREAM: a sound that is loaded whole */
#define PAUSE_STOPS 3           /* a pause is this many Stops with nothing between them */

static CRITICAL_SECTION lk;
static HMODULE self;

/* ---- method tables ---- */
typedef struct { void **table; void *orig[26]; } Table;
static Table tables[8];
static volatile LONG table_count;

static Table *table_of(void *object)
{
    void **table = *(void ***)object;
    LONG i, n = table_count;
    for (i = 0; i < n; i++) if (tables[i].table == table) return &tables[i];
    return 0;
}
#define ORIG(type, object, slot) ((type)table_of(object)->orig[slot])

typedef struct { int slot; void *function; } Hook;

static void hook_table(void *object, const Hook *hooks, int count, int slots)
{
    void **table = *(void ***)object;
    DWORD old;
    int i;

    EnterCriticalSection(&lk);
    if (!table_of(object) && table_count < 8 && VirtualProtect(table, slots * sizeof *table, PAGE_READWRITE, &old)) {
        Table *t = &tables[table_count];
        t->table = table;
        for (i = 0; i < slots; i++) t->orig[i] = table[i];
        InterlockedIncrement(&table_count);     /* the originals are known before any entry changes */
        for (i = 0; i < count; i++) table[hooks[i].slot] = hooks[i].function;
        VirtualProtect(table, slots * sizeof *table, old, &old);
    }
    LeaveCriticalSection(&lk);
}

/* ---- voices the game has not started itself ----
 * A voice is listed from its creation until the game starts or destroys it, or shows that it is
 * not streamed. When the list is full a voice is left alone. */
enum { FRESH, WAITING, STARTED };       /* no data yet; streamed and not started; started here */
static struct { void *voice; int state; DWORD since; } list[256];
static int list_count;

static int list_find(void *voice)
{
    int i;
    for (i = 0; i < list_count; i++) if (list[i].voice == voice) return i;
    return -1;
}
static void list_drop(int i) { if (i >= 0) list[i] = list[--list_count]; }

/* ---- pause ----
 * Ending a sound is Stop, Flush, Destroy. A pause is a run of Stops; it lasts until one of those
 * voices is started again or all of them are destroyed, so a menu sound does not end it. */
static void *held[256];
static int held_count, paused;

static void held_drop(void *voice, int ended)
{
    int i;
    for (i = 0; i < held_count; i++) if (held[i] == voice) {
        held[i] = held[--held_count];
        if (paused && (!ended || !held_count)) paused = held_count = 0;
        break;
    }
}
static void run_broken(void) { if (!paused) held_count = 0; }

/* ---- source voice, XAudio2 2.0 layout ---- */
typedef HRESULT (WINAPI *Plain)(void *);
typedef void (WINAPI *Quiet)(void *);
typedef HRESULT (WINAPI *Switch)(void *, UINT32, UINT32);
typedef HRESULT (WINAPI *Submit)(void *, const UINT32 *, const void *);
typedef void (WINAPI *State)(void *, void *);
enum { DESTROY = 16, START, STOP, SUBMIT, FLUSH, GETSTATE = 23, VOICE_SLOTS = 26 };

static void WINAPI voice_destroy(void *voice)
{
    EnterCriticalSection(&lk);
    list_drop(list_find(voice));
    held_drop(voice, 1);
    run_broken();
    LeaveCriticalSection(&lk);
    ORIG(Quiet, voice, DESTROY)(voice);
}

static HRESULT WINAPI voice_start(void *voice, UINT32 flags, UINT32 set)
{
    EnterCriticalSection(&lk);
    list_drop(list_find(voice));
    held_drop(voice, 0);
    run_broken();
    LeaveCriticalSection(&lk);
    return ORIG(Switch, voice, START)(voice, flags, set);
}

static HRESULT WINAPI voice_stop(void *voice, UINT32 flags, UINT32 set)
{
    HRESULT hr = ORIG(Switch, voice, STOP)(voice, flags, set);
    int i;

    EnterCriticalSection(&lk);
    list_drop(list_find(voice));
    for (i = 0; i < held_count && held[i] != voice; i++) ;
    if (i == held_count && held_count < 256) held[held_count++] = voice;
    if (!paused && held_count >= PAUSE_STOPS) {
        paused = 1;
        /* The game takes the voices started here for stopped ones: it does not stop them on a
         * pause, and starts them on a resume. The lock keeps them from being destroyed meanwhile. */
        for (i = 0; i < list_count; i++)
            if (list[i].state == STARTED) ORIG(Switch, list[i].voice, STOP)(list[i].voice, 0, 0);
    }
    LeaveCriticalSection(&lk);
    return hr;
}

static HRESULT WINAPI voice_submit(void *voice, const UINT32 *buffer, const void *wma)
{
    HRESULT hr = ORIG(Submit, voice, SUBMIT)(voice, buffer, wma);
    int i;

    EnterCriticalSection(&lk);
    i = list_find(voice);
    if (i >= 0 && list[i].state == FRESH) {
        if (!buffer || (buffer[0] & END_OF_STREAM)) list_drop(i);
        else { list[i].state = WAITING; list[i].since = GetTickCount(); }
    }
    LeaveCriticalSection(&lk);
    return hr;
}

static HRESULT WINAPI voice_flush(void *voice)
{
    EnterCriticalSection(&lk);
    run_broken();
    LeaveCriticalSection(&lk);
    return ORIG(Plain, voice, FLUSH)(voice);
}

/* The game asks each voice for its state as it plays, which is where a waiting one is started:
 * the voice is certainly alive here. */
static void WINAPI voice_state(void *voice, void *state)
{
    int i, start = 0;

    ORIG(State, voice, GETSTATE)(voice, state);
    if (!state || !((UINT32 *)state)[1]) return;        /* BuffersQueued */
    EnterCriticalSection(&lk);
    i = list_find(voice);
    if (!paused && i >= 0 && list[i].state == WAITING && GetTickCount() - list[i].since >= DELAY_MS) {
        list[i].state = STARTED;
        start = 1;
    }
    LeaveCriticalSection(&lk);
    if (start) ORIG(Switch, voice, START)(voice, 0, 0);
}

static const Hook voice_hooks[] = {
    {DESTROY, voice_destroy}, {START, voice_start}, {STOP, voice_stop},
    {SUBMIT, voice_submit}, {FLUSH, voice_flush}, {GETSTATE, voice_state},
};

/* ---- IXAudio2, XAudio2 2.0 layout ---- */
typedef HRESULT (WINAPI *Create)(void *, void **, const void *, UINT32, float, void *, const void *, const void *);
enum { CREATESOURCE = 8, ENGINE_SLOTS = 16 };

static HRESULT WINAPI engine_create(void *engine, void **voice, const void *format, UINT32 flags, float ratio,
                                    void *callback, const void *sends, const void *effects)
{
    HRESULT hr = ORIG(Create, engine, CREATESOURCE)(engine, voice, format, flags, ratio, callback, sends, effects);
    if (SUCCEEDED(hr) && voice && *voice) {
        hook_table(*voice, voice_hooks, 6, VOICE_SLOTS);
        EnterCriticalSection(&lk);
        if (table_of(*voice) && list_count < 256) {
            list_drop(list_find(*voice));
            list[list_count].voice = *voice;
            list[list_count++].state = FRESH;
        }
        LeaveCriticalSection(&lk);
    }
    return hr;
}
static const Hook engine_hooks[] = {{CREATESOURCE, engine_create}};

/* ---- finding XAudio2 ---- */
typedef HRESULT (WINAPI *CoCreate)(REFCLSID, IUnknown *, DWORD, REFIID, void **);
static CoCreate real_create;

static HRESULT WINAPI my_create(REFCLSID id, IUnknown *outer, DWORD context, REFIID iid, void **object)
{
    /* CLSID_XAudio2 2.0, {fac23f48-31f5-45a8-b49b-5225d61401aa}, as four little-endian words */
    static const DWORD xaudio2_0[4] = {0xfac23f48, 0x45a831f5, 0x25529bb4, 0xaa0114d6};
    HRESULT hr = real_create(id, outer, context, iid, object);
    const DWORD *d = (const DWORD *)id;

    if (SUCCEEDED(hr) && id && object && *object && d[0] == xaudio2_0[0] && d[1] == xaudio2_0[1] &&
        d[2] == xaudio2_0[2] && d[3] == xaudio2_0[3])
        hook_table(*object, engine_hooks, 1, ENGINE_SLOTS);
    return hr;
}

static void hook_program(void)
{
    BYTE *base = (BYTE *)GetModuleHandleA(0);
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *d =
        (IMAGE_IMPORT_DESCRIPTOR *)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    DWORD old;

    if ((BYTE *)d == base) return;
    for (; d->Name; d++) {
        void **entry = (void **)(base + d->FirstThunk);
        DWORD *name = (DWORD *)(base + d->OriginalFirstThunk);
        if (lstrcmpiA((char *)base + d->Name, "ole32.dll") || !d->OriginalFirstThunk) continue;
        for (; *entry && *name; entry++, name++) {
            if ((*name & 0x80000000u) || lstrcmpA((char *)base + *name + 2, "CoCreateInstance")) continue;
            if (!VirtualProtect(entry, sizeof *entry, PAGE_READWRITE, &old)) return;
            real_create = (CoCreate)*entry;
            *entry = (void *)my_create;
            VirtualProtect(entry, sizeof *entry, old, &old);
            return;
        }
    }
}

/* ---- DirectSound, loaded on first use and never from DllMain ---- */
static void *system_function(const char *name)
{
    static HMODULE library;
    if (!library) {
        char path[MAX_PATH];
        UINT n = GetSystemDirectoryA(path, MAX_PATH - 12);
        HMODULE h;
        lstrcpyA(path + n, "\\dsound.dll");
        h = LoadLibraryA(path);
        if (!h || h == self) return 0;
        library = h;
    }
    return (void *)GetProcAddress(library, name);
}

#define FORWARD(name, fail, parameters, arguments) \
    HRESULT WINAPI name parameters { \
        HRESULT (WINAPI *f) parameters = (HRESULT (WINAPI *) parameters)system_function(#name); \
        return f ? f arguments : (HRESULT)fail; }
#define NO_DRIVER 0x88780078
FORWARD(DirectSoundCreate, NO_DRIVER, (void *a, void *b, void *c), (a, b, c))
FORWARD(DirectSoundCreate8, NO_DRIVER, (void *a, void *b, void *c), (a, b, c))
FORWARD(DirectSoundCaptureCreate, NO_DRIVER, (void *a, void *b, void *c), (a, b, c))
FORWARD(DirectSoundCaptureCreate8, NO_DRIVER, (void *a, void *b, void *c), (a, b, c))
FORWARD(DirectSoundEnumerateA, NO_DRIVER, (void *a, void *b), (a, b))
FORWARD(DirectSoundEnumerateW, NO_DRIVER, (void *a, void *b), (a, b))
FORWARD(DirectSoundCaptureEnumerateA, NO_DRIVER, (void *a, void *b), (a, b))
FORWARD(DirectSoundCaptureEnumerateW, NO_DRIVER, (void *a, void *b), (a, b))
FORWARD(GetDeviceID, NO_DRIVER, (void *a, void *b), (a, b))
FORWARD(DirectSoundFullDuplexCreate, NO_DRIVER,
        (void *a, void *b, void *c, void *d, void *e, DWORD l, void *g, void *h, void *i, void *j),
        (a, b, c, d, e, l, g, h, i, j))
FORWARD(DllGetClassObject, CLASS_E_CLASSNOTAVAILABLE, (REFCLSID a, REFIID b, void **c), (a, b, c))
HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, void *reserved)
{
    char path[MAX_PATH], *name = path, *p;

    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    self = module;
    DisableThreadLibraryCalls(module);
    InitializeCriticalSection(&lk);
    if (!GetModuleFileNameA(0, path, MAX_PATH)) return TRUE;
    path[MAX_PATH - 1] = 0;
    for (p = path; *p; p++) if (*p == '\\' || *p == '/') name = p + 1;
    if (!lstrcmpiA(name, "CoDWaW.exe")) hook_program();
    return TRUE;
}
