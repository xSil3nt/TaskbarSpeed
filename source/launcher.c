/* Standalone TaskbarSpeed launcher. GPL-3.0-or-later. */
#define UNICODE
#define _UNICODE
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include "protocol.h"
#include "config.h"

static wchar_t message[2048], exePath[MAX_PATH], dllPath[MAX_PATH], iniPath[MAX_PATH], logPath[MAX_PATH];
static BOOL quiet;
static void failure(const wchar_t *what,DWORD error) { swprintf(message,2048,L"%ls (error %lu).\nSee README.txt for recovery instructions.",what,error); }
static BOOL samePath(const wchar_t *a,const wchar_t *b) { return _wcsicmp(a,b)==0; }
static BOOL module(DWORD pid,const wchar_t *name,MODULEENTRY32W *entry) {
    HANDLE snap=INVALID_HANDLE_VALUE;
    for (int i=0;i<10;i++) { snap=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid); if (snap!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH) break; }
    if (snap==INVALID_HANDLE_VALUE) return FALSE;
    MODULEENTRY32W m={0}; m.dwSize=sizeof(m);
    BOOL found=FALSE;
    if (Module32FirstW(snap,&m)) do { if (_wcsicmp(m.szModule,name)==0) { *entry=m; found=TRUE; break; } } while(Module32NextW(snap,&m));
    CloseHandle(snap); return found;
}
static void *remoteFunction(DWORD pid,void *function) {
    HMODULE ownModule;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)function,&ownModule)) return NULL;
    wchar_t path[MAX_PATH]; if (!GetModuleFileNameW(ownModule,path,MAX_PATH)) return NULL;
    wchar_t *name=wcsrchr(path,L'\\'); name=name?name+1:path;
    MODULEENTRY32W remote; if (!module(pid,name,&remote)) return NULL;
    return remote.modBaseAddr+((BYTE*)function-(BYTE*)ownModule);
}
static BOOL runThread(HANDLE process,void *function,void *arg,DWORD *result) {
    HANDLE thread=CreateRemoteThread(process,NULL,0,(LPTHREAD_START_ROUTINE)function,arg,0,NULL);
    if (!thread) { failure(L"Could not start Explorer control thread",GetLastError()); return FALSE; }
    DWORD wait=WaitForSingleObject(thread,30000);
    BOOL ok=wait==WAIT_OBJECT_0 && GetExitCodeThread(thread,result);
    CloseHandle(thread);
    if (!ok) failure(L"Explorer control did not finish; its input buffer was retained to avoid freeing memory still in use",wait);
    return ok;
}
static BOOL startup(BOOL add) {
    wchar_t folder[MAX_PATH], path[MAX_PATH];
    HRESULT hr=SHGetFolderPathW(NULL,CSIDL_STARTUP|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,folder);
    if (FAILED(hr)) { failure(L"Could not find your Startup folder",(DWORD)hr); return FALSE; }
    if (wcslen(folder)+wcslen(L"\\TaskbarSpeed.lnk")>=MAX_PATH) { failure(L"Startup path is too long",ERROR_FILENAME_EXCED_RANGE); return FALSE; }
    swprintf(path,MAX_PATH,L"%ls\\TaskbarSpeed.lnk",folder);
    if (add) {
        hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
        if (FAILED(hr)) { failure(L"Could not initialize shortcut creation",(DWORD)hr); return FALSE; }
        IShellLinkW *link=NULL; IPersistFile *file=NULL;
        hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
        if (SUCCEEDED(hr)) hr=IShellLinkW_SetPath(link,exePath);
        if (SUCCEEDED(hr)) hr=IShellLinkW_SetArguments(link,L"/apply /quiet /wait");
        if (SUCCEEDED(hr)) hr=IShellLinkW_SetDescription(link,L"Apply TaskbarSpeed at sign-in");
        if (SUCCEEDED(hr)) hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&file);
        if (SUCCEEDED(hr)) hr=IPersistFile_Save(file,path,TRUE);
        if (file) IPersistFile_Release(file);
        if (link) IShellLinkW_Release(link);
        CoUninitialize();
        if (FAILED(hr)) { failure(L"Could not save the startup shortcut",(DWORD)hr); return FALSE; }
        /* Explicit setup/enable resets a previous Task Manager disable. Apply does not. */
        HKEY approval;
        if (RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\StartupApproved\\StartupFolder",0,KEY_SET_VALUE,&approval)==ERROR_SUCCESS) {
            RegDeleteValueW(approval,L"TaskbarSpeed.lnk"); RegCloseKey(approval);
        }
    } else if (!DeleteFileW(path) && GetLastError()!=ERROR_FILE_NOT_FOUND) {
        failure(L"Could not remove the startup shortcut",GetLastError()); return FALSE;
    }
    /* Migrate the previous release without registering two login launches. */
    HKEY key;
    LONG result=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&key);
    if (result==ERROR_SUCCESS) {
        result=RegDeleteValueW(key,L"TaskbarSpeed"); RegCloseKey(key);
        if (result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS;
    } else if (result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS;
    if (result!=ERROR_SUCCESS) { failure(L"Could not remove the old registry startup entry",result); return FALSE; }
    swprintf(message,2048,add?L"TaskbarSpeed will apply at sign-in and then exit. Manage it in Task Manager > Startup apps.":L"TaskbarSpeed startup entry removed."); return TRUE;
}
static int control(DWORD action) {
    HWND taskbar=FindWindowW(L"Shell_TrayWnd",NULL);
    DWORD pid=0; GetWindowThreadProcessId(taskbar,&pid);
    if (!taskbar||!pid) { failure(L"Windows taskbar is not available",GetLastError()); return 1; }
    DWORD rights=PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ;
    if (action!=TBS_STATUS) rights|=PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE;
    /* Status queries use the exported control function, so require the same permissions once loaded. */
    MODULEENTRY32W plugin;
    BOOL loaded=module(pid,L"TaskbarSpeedHook.dll",&plugin);
    if (loaded) rights|=PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE;
    HANDLE process=OpenProcess(rights,FALSE,pid);
    if (!process) { failure(L"Cannot access your Explorer process",GetLastError()); return 1; }
    wchar_t processPath[MAX_PATH],expected[MAX_PATH]; DWORD pathSize=MAX_PATH;
    GetWindowsDirectoryW(expected,MAX_PATH); wcscat(expected,L"\\explorer.exe");
    if (!QueryFullProcessImageNameW(process,0,processPath,&pathSize)||!samePath(processPath,expected)) { failure(L"Taskbar owner is not the Windows Explorer executable",0); CloseHandle(process); return 1; }
    if (loaded&&!samePath(plugin.szExePath,dllPath)) { failure(L"A TaskbarSpeed DLL from another folder is already loaded; use that folder's Undo.cmd first",0); CloseHandle(process); return 1; }
    if (!loaded && action!=TBS_APPLY) { swprintf(message,2048,L"TaskbarSpeed is not loaded. Explorer uses its normal taskbar speed."); CloseHandle(process); return 0; }
    if (!loaded) {
        SIZE_T bytes=(wcslen(dllPath)+1)*sizeof(wchar_t),written=0;
        void *remote=VirtualAllocEx(process,NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if (!remote||!WriteProcessMemory(process,remote,dllPath,bytes,&written)||written!=bytes) { failure(L"Could not copy DLL path into Explorer",GetLastError()); if (remote) VirtualFreeEx(process,remote,0,MEM_RELEASE); CloseHandle(process); return 1; }
        void *loader=remoteFunction(pid,(void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"));
        DWORD result=0;
        if (!loader||!runThread(process,loader,remote,&result)) { CloseHandle(process); return 1; }
        VirtualFreeEx(process,remote,0,MEM_RELEASE);
        if (!module(pid,L"TaskbarSpeedHook.dll",&plugin)||!samePath(plugin.szExePath,dllPath)) { failure(L"Explorer did not load the taskbar DLL",result); CloseHandle(process); return 1; }
    }
    HMODULE local=LoadLibraryExW(dllPath,NULL,DONT_RESOLVE_DLL_REFERENCES);
    if (!local) { failure(L"Could not read DLL exports",GetLastError()); CloseHandle(process); return 1; }
    void *exported=(void*)GetProcAddress(local,"TaskbarSpeedControl");
    if (!exported) { failure(L"DLL control export is missing",GetLastError()); FreeLibrary(local); CloseHandle(process); return 1; }
    void *function=plugin.modBaseAddr+((BYTE*)exported-(BYTE*)local);
    FreeLibrary(local);
    Control c={0}; c.magic=TBS_MAGIC; c.action=action;
    c.showPercent=GetPrivateProfileIntW(L"Speed",L"ShowPercent",500,iniPath);
    c.hidePercent=GetPrivateProfileIntW(L"Speed",L"HidePercent",500,iniPath);
    c.hideDelayPercent=GetPrivateProfileIntW(L"Speed",L"HideDelayPercent",250,iniPath);
    SIZE_T written=0;
    void *remote=VirtualAllocEx(process,NULL,sizeof(c),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if (!remote||!WriteProcessMemory(process,remote,&c,sizeof(c),&written)||written!=sizeof(c)) { failure(L"Could not copy taskbar settings",GetLastError()); if (remote) VirtualFreeEx(process,remote,0,MEM_RELEASE); CloseHandle(process); return 1; }
    DWORD result=0;
    if (!runThread(process,function,remote,&result)) { CloseHandle(process); return 1; }
    BOOL read=ReadProcessMemory(process,remote,&c,sizeof(c),&written)&&written==sizeof(c);
    VirtualFreeEx(process,remote,0,MEM_RELEASE); CloseHandle(process);
    if (!read) { failure(L"Could not read the taskbar hook result",GetLastError()); return 1; }
    if (c.result||result) { DWORD error=c.result?c.result:result; failure(error==1002?L"Taskbar animation code was not recognized; nothing was hooked":L"Taskbar hook refused the operation",error); return error==1002?2:1; }
    swprintf(message,2048,L"TaskbarSpeed %ls. Launcher exits now.\nShow: %lu%% speed; hide: %lu%% speed.\nExplorer PID: %lu\nHook calls: legacy=%lu, show=%lu, hide=%lu\nModern show duration: %.2f -> %.2f ms\nModern hide duration: %.2f -> %.2f ms\nLegacy show duration: %lu -> %lu ms; last call %.2f ms\nLegacy hide duration: %lu -> %lu ms; last call %.2f ms\n\nIf measured values are zero, hover the taskbar and run Status.cmd again.",c.enabled?L"enabled":L"disabled",c.showPercent,c.hidePercent,pid,c.legacyCalls,c.expandCalls,c.collapseCalls,c.originalShow/10000.0,c.currentShow/10000.0,c.originalHide/10000.0,c.currentHide/10000.0,c.legacyShowOriginalMs,c.legacyShowTargetMs,c.legacyShowElapsedUs/1000.0,c.legacyHideOriginalMs,c.legacyHideTargetMs,c.legacyHideElapsedUs/1000.0);
    wchar_t delayMessage[200];
    swprintf(delayMessage,200,L"\nHide delay: %lu%% speed; timer %lu -> %lu ms (%lu calls).",c.hideDelayPercent,c.originalHideDelayMs,c.currentHideDelayMs,c.hideTimerCalls);
    wcsncat(message,delayMessage,2047-wcslen(message));
    return 0;
}
static BOOL waitForTaskbar(void) {
    DWORD previousPid=0, stable=0;
    for (int i=0;i<120;i++) {
        HWND window=FindWindowW(L"Shell_TrayWnd",NULL);
        DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        MODULEENTRY32W taskbar,view;
        BOOL ready=pid && module(pid,MODULES[0].name,&taskbar) && module(pid,MODULES[1].name,&view);
        stable=ready && pid==previousPid ? stable+1 : 0;
        if (stable>=2) return TRUE;
        previousPid=pid;
        Sleep(500);
    }
    failure(L"Taskbar did not finish starting; run Apply.cmd once it is visible",WAIT_TIMEOUT);
    return FALSE;
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,LPWSTR command,int show) {
    (void)instance;(void)previous;(void)show;
    quiet=wcsstr(command,L"/quiet")!=NULL;
    GetModuleFileNameW(NULL,exePath,MAX_PATH);
    wchar_t directory[MAX_PATH]; wcscpy(directory,exePath); wchar_t *slash=wcsrchr(directory,L'\\'); if (!slash) return 1; slash[1]=0;
    swprintf(dllPath,MAX_PATH,L"%lsTaskbarSpeedHook.dll",directory);
    swprintf(iniPath,MAX_PATH,L"%lsTaskbarSpeed.ini",directory);
    swprintf(logPath,MAX_PATH,L"%lsStatus.txt",directory);
    HANDLE mutex=CreateMutexW(NULL,FALSE,L"Local\\TaskbarSpeedLauncher");
    if (!mutex) return 1;
    DWORD wait=WaitForSingleObject(mutex,1000);
    if (wait!=WAIT_OBJECT_0&&wait!=WAIT_ABANDONED) { CloseHandle(mutex); return 1; }
    int result;
    if (wcsstr(command,L"/remove-startup")) result=startup(FALSE)?0:1;
    else if (wcsstr(command,L"/startup")) result=startup(TRUE)?0:1;
    else {
        DWORD action=wcsstr(command,L"/disable")?TBS_DISABLE:wcsstr(command,L"/status")?TBS_STATUS:TBS_APPLY;
        if (action==TBS_APPLY && wcsstr(command,L"/wait") && !waitForTaskbar()) result=1;
        else {
            result=control(action);
            /* Explorer can replace its shell process during sign-in. Retry transient
               initialization failures, but stop immediately on incompatible files. */
            if (action==TBS_APPLY && wcsstr(command,L"/wait")) {
                for (int attempt=0; result==1 && attempt<30; attempt++) {
                    Sleep(2000); result=control(action);
                }
            }
        }
    }
    FILE *log=_wfopen(logPath,L"wb");
    if (log) { char utf8[8192]; int bytes=WideCharToMultiByte(CP_UTF8,0,message,-1,utf8,sizeof(utf8),NULL,NULL); if (bytes>0) fwrite(utf8,1,bytes-1,log); fclose(log); }
    ReleaseMutex(mutex); CloseHandle(mutex);
    if (!quiet) MessageBoxW(NULL,message,L"TaskbarSpeed",MB_OK|(result?MB_ICONWARNING:MB_ICONINFORMATION));
    return result;
}
