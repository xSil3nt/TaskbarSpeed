/* Taskbar-only speed hook. GPL-3.0-or-later.
 * Animation interception is adapted from m417z's Taskbar auto-hide speed mod:
 * https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-speed.wh.cpp
 * Copyright (c) m417z and contributors. Standalone implementation, 2026.
 * Taskbar timer IDs follow Bo0ii's taskbar-autohide-instant-show mod
 * in the same repository (GPL-3.0-or-later).
 */
#include <windows.h>
#include <stdint.h>
#include <string.h>
#include <wchar.h>
#include "MinHook.h"
#include "protocol.h"
#include "config.h"

static HMODULE self;
static volatile LONG enabled, initialized, showPercent=500, hidePercent=500, hideDelayPercent=250;
static volatile LONG legacyCalls, expandCalls, collapseCalls;
static LARGE_INTEGER frequency;
static DWORD legacyShowOriginalMs, legacyShowTargetMs, legacyHideOriginalMs, legacyHideTargetMs;
static LONGLONG legacyShowElapsedUs, legacyHideElapsedUs;
static volatile LONG hideTimerCalls, originalHideDelayMs, currentHideDelayMs;
static UINT_PTR (WINAPI *originalSetTimer)(HWND,UINT_PTR,UINT,TIMERPROC);
typedef void (WINAPI *SlideFn)(void*, HWND, const RECT*, HMONITOR, BOOL, BOOL);
typedef void** (WINAPI *AnimationFn)(void*, void*);
static SlideFn originalSlide;
static AnimationFn originalExpand, originalCollapse;

/* IKeyFrameAnimation ABI from Microsoft's Windows SDK. */
static const GUID keyframeIID={0x126e7f22,0x3ae9,0x4540,{0x9a,0x8a,0xde,0xae,0x8a,0x4a,0x4a,0x84}};
typedef struct { void **vtable; } Com;
typedef HRESULT (WINAPI *QueryFn)(Com*,const GUID*,void**);
typedef ULONG (WINAPI *ReleaseFn)(Com*);
typedef HRESULT (WINAPI *GetDurationFn)(Com*,LONGLONG*);
typedef HRESULT (WINAPI *SetDurationFn)(Com*,LONGLONG);
typedef struct { Com *animation; LONGLONG original, current; } Saved;
static Saved expand, collapse;
static SRWLOCK animationLock=SRWLOCK_INIT;
static SRWLOCK controlLock=SRWLOCK_INIT;

static void release(Com *p) { ((ReleaseFn)p->vtable[2])(p); }
static HRESULT durationGet(Com *p,LONGLONG *v) { return ((GetDurationFn)p->vtable[8])(p,v); }
static HRESULT durationSet(Com *p,LONGLONG v) { return ((SetDurationFn)p->vtable[9])(p,v); }
static void updateSaved(Saved *saved,int percent) {
    if (!saved->animation) return;
    LONGLONG value=enabled ? saved->original*100/percent : saved->original;
    if (value<10000) value=10000; /* WinRT's minimum duration is 1 ms. */
    if (SUCCEEDED(durationSet(saved->animation,value))) saved->current=value;
}
static void changeAnimation(void *raw,Saved *saved,int percent) {
    if (!raw || !InterlockedCompareExchange(&enabled,0,0)) return;
    AcquireSRWLockExclusive(&animationLock);
    Com *animation=NULL;
    Com *unknown=(Com*)raw;
    if (SUCCEEDED(((QueryFn)unknown->vtable[0])(unknown,&keyframeIID,(void**)&animation))) {
        if (animation!=saved->animation) {
            LONGLONG value;
            if (SUCCEEDED(durationGet(animation,&value))) {
                if (saved->animation) { durationSet(saved->animation,saved->original); release(saved->animation); }
                saved->animation=animation;
                saved->original=value;
                animation=NULL; /* Keep the QueryInterface reference until Explorer exits. */
            }
        }
        if (animation) release(animation);
        updateSaved(saved,percent);
    }
    ReleaseSRWLockExclusive(&animationLock);
}
static void** WINAPI expandHook(void *object,void *result) {
    void **ret=originalExpand(object,result);
    InterlockedIncrement(&expandCalls);
    if (ret) changeAnimation(ret[0],&expand,showPercent);
    return ret;
}
static void** WINAPI collapseHook(void *object,void *result) {
    void **ret=originalCollapse(object,result);
    InterlockedIncrement(&collapseCalls);
    if (ret) changeAnimation(ret[0],&collapse,hidePercent);
    return ret;
}
static void WINAPI slideHook(void *object,HWND window,const RECT *rect,HMONITOR monitor,BOOL show,BOOL animate) {
    if (!InterlockedCompareExchange(&enabled,0,0)) { originalSlide(object,window,rect,monitor,show,animate); return; }
    /* Exact-build offsets verified in TrayUI::SlideWindow's machine code.
       Show reads this+0x16c; hide reads this+0x168. This avoids hooking
       timing APIs throughout Explorer and preserves the other animations. */
    volatile LONG *duration=(volatile LONG*)((BYTE*)object+(show?0x16c:0x168));
    LONG normal=InterlockedCompareExchange(duration,0,0);
    LONG adjusted=normal;
    if (normal>0 && animate) { adjusted=(LONG)((LONGLONG)normal*100/(show?showPercent:hidePercent)); if (adjusted<1) adjusted=1; }
    InterlockedCompareExchange(duration,adjusted,normal);
    LARGE_INTEGER start,end; QueryPerformanceCounter(&start);
    InterlockedIncrement(&legacyCalls);
    originalSlide(object,window,rect,monitor,show,animate);
    QueryPerformanceCounter(&end);
    InterlockedCompareExchange(duration,normal,adjusted);
    if (animate&&normal>0) {
        LONGLONG elapsed=(end.QuadPart-start.QuadPart)*1000000/frequency.QuadPart;
        if (show) { legacyShowOriginalMs=(DWORD)normal; legacyShowTargetMs=(DWORD)adjusted; InterlockedExchange64(&legacyShowElapsedUs,elapsed); }
        else { legacyHideOriginalMs=(DWORD)normal; legacyHideTargetMs=(DWORD)adjusted; InterlockedExchange64(&legacyHideElapsedUs,elapsed); }
    }
}
static UINT_PTR WINAPI timerHook(HWND window,UINT_PTR id,UINT delay,TIMERPROC callback) {
    /* Timer 2 schedules hiding; timer 3 schedules showing and is unchanged. */
    if (window && id==2 && InterlockedCompareExchange(&enabled,0,0)) {
        wchar_t name[64];
        if (GetClassNameW(window,name,64)>0 &&
            (wcscmp(name,L"Shell_TrayWnd")==0 || wcscmp(name,L"Shell_SecondaryTrayWnd")==0)) {
            InterlockedExchange(&originalHideDelayMs,(LONG)delay);
            delay=(UINT)((ULONGLONG)delay*100/(DWORD)hideDelayPercent);
            if (delay<USER_TIMER_MINIMUM) delay=USER_TIMER_MINIMUM;
            InterlockedExchange(&currentHideDelayMs,(LONG)delay);
            InterlockedIncrement(&hideTimerCalls);
        }
    }
    return originalSetTimer(window,id,delay,callback);
}
static BOOL validModule(HMODULE module,int index) {
    if (!module) return FALSE;
    IMAGE_DOS_HEADER *dos=(IMAGE_DOS_HEADER*)module;
    if (dos->e_magic!=IMAGE_DOS_SIGNATURE) return FALSE;
    IMAGE_NT_HEADERS *nt=(IMAGE_NT_HEADERS*)((BYTE*)module+dos->e_lfanew);
    return nt->Signature==IMAGE_NT_SIGNATURE && nt->FileHeader.TimeDateStamp==MODULES[index].timestamp && nt->OptionalHeader.SizeOfImage==MODULES[index].imageSize;
}
static void *target(const HookSpec *spec,HMODULE *modules) {
    BYTE *address=(BYTE*)modules[spec->module]+spec->rva;
    if (spec->rva+16>MODULES[spec->module].imageSize || memcmp(address,spec->signature,16)!=0) return NULL;
    return address;
}
static DWORD initialize(void) {
    HMODULE modules[2]={GetModuleHandleW(MODULES[0].name),GetModuleHandleW(MODULES[1].name)};
    if (!validModule(modules[0],0)||!validModule(modules[1],1)) return 1001;
    void *slide=target(&HOOK_SLIDE,modules),*show=target(&HOOK_EXPAND,modules),*hide=target(&HOOK_COLLAPSE,modules);
    if (!slide||!show||!hide) return 1002;
    QueryPerformanceFrequency(&frequency);
    MH_STATUS status=MH_Initialize();
    if (status!=MH_OK) return 1100+(DWORD)status;
    void *timer=(void*)GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetTimer");
    if (!timer) { MH_Uninitialize(); return 1003; }
    void *addresses[4]={slide,show,hide,timer};
    void *hooks[4]={(void*)slideHook,(void*)expandHook,(void*)collapseHook,(void*)timerHook};
    void **originals[4]={(void**)&originalSlide,(void**)&originalExpand,(void**)&originalCollapse,(void**)&originalSetTimer};
    for (int i=0;i<4;i++) {
        status=MH_CreateHook(addresses[i],hooks[i],originals[i]);
        if (status!=MH_OK) { MH_Uninitialize(); return 1200+(DWORD)status; }
    }
    /* Never unload callable hook code while another Explorer thread may use it. */
    HMODULE pinned;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,(LPCWSTR)&initialize,&pinned)) { MH_Uninitialize(); return 1004; }
    InterlockedExchange(&initialized,1);
    return 0;
}
__declspec(dllexport) DWORD WINAPI TaskbarSpeedControl(void *arg) {
    Control *control=(Control*)arg;
    if (!control || control->magic!=TBS_MAGIC) return 1;
    AcquireSRWLockExclusive(&controlLock);
    DWORD error=0;
    if (control->action==TBS_APPLY) {
        if (control->showPercent<100||control->showPercent>1000||control->hidePercent<100||control->hidePercent>1000||control->hideDelayPercent<100||control->hideDelayPercent>1000) error=1005;
        else {
            if (!initialized) error=initialize();
            if (!error) {
                InterlockedExchange(&showPercent,(LONG)control->showPercent);
                InterlockedExchange(&hidePercent,(LONG)control->hidePercent);
                InterlockedExchange(&hideDelayPercent,(LONG)control->hideDelayPercent);
                InterlockedExchange(&enabled,1);
                MH_STATUS status=MH_EnableHook(MH_ALL_HOOKS);
                if (status!=MH_OK && status!=MH_ERROR_ENABLED) { InterlockedExchange(&enabled,0); error=1300+(DWORD)status; }
            }
        }
    } else if (control->action==TBS_DISABLE) {
        InterlockedExchange(&enabled,0);
        if (initialized) {
            MH_STATUS status=MH_DisableHook(MH_ALL_HOOKS);
            if (status!=MH_OK&&status!=MH_ERROR_DISABLED) error=1400+(DWORD)status;
        }
    } else if (control->action!=TBS_STATUS) error=1006;
    AcquireSRWLockExclusive(&animationLock);
    if (control->action!=TBS_STATUS && !error) { updateSaved(&expand,showPercent); updateSaved(&collapse,hidePercent); }
    control->originalShow=expand.original; control->currentShow=expand.current;
    control->originalHide=collapse.original; control->currentHide=collapse.current;
    ReleaseSRWLockExclusive(&animationLock);
    control->enabled=(DWORD)enabled; control->initialized=(DWORD)initialized;
    control->legacyCalls=(DWORD)legacyCalls; control->expandCalls=(DWORD)expandCalls; control->collapseCalls=(DWORD)collapseCalls;
    control->legacyShowOriginalMs=legacyShowOriginalMs; control->legacyShowTargetMs=legacyShowTargetMs;
    control->legacyHideOriginalMs=legacyHideOriginalMs; control->legacyHideTargetMs=legacyHideTargetMs;
    control->legacyShowElapsedUs=InterlockedCompareExchange64(&legacyShowElapsedUs,0,0);
    control->legacyHideElapsedUs=InterlockedCompareExchange64(&legacyHideElapsedUs,0,0);
    control->showPercent=(DWORD)showPercent; control->hidePercent=(DWORD)hidePercent;
    control->hideDelayPercent=(DWORD)hideDelayPercent;
    control->hideTimerCalls=(DWORD)hideTimerCalls;
    control->originalHideDelayMs=(DWORD)originalHideDelayMs; control->currentHideDelayMs=(DWORD)currentHideDelayMs;
    control->result=error;
    ReleaseSRWLockExclusive(&controlLock);
    return error;
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID reserved) {
    (void)reserved;
    if (reason==DLL_PROCESS_ATTACH) { self=instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}
