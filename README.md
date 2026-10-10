# TaskbarSpeed

Faster Windows 11 auto-hide animations. Show and hide run at 500% speed. The wait before hiding runs at 250% speed: normally 500 ms becomes 200 ms. Other animations stay enabled.

## Set up

1. Turn on **Automatically hide the taskbar** in Windows taskbar settings.
2. Download the ZIP from [Releases](https://github.com/xSil3nt/TaskbarSpeed/releases/latest) and extract it somewhere you will keep it.
3. Double-click **Setup.cmd**. It applies the tweak and enables it at sign-in.

No Windhawk, Visual Studio, or installer needed. The launcher exits after applying; a small DLL stays inside Explorer.

**Compatibility:** this release targets x64 Windows 11 24H2, build 26100.9550. It checks the taskbar files and stops if they differ. Windows updates may require a new build.

## Change or remove

- Edit **TaskbarSpeed.ini**, then run **Apply.cmd**. `100` means normal speed; values up to `1000` are supported.
- Run **Status.cmd** to check settings and timing.
- If Explorer restarts, run **Apply.cmd** again.
- Run **Undo.cmd** to disable the tweak and remove startup. Sign out before deleting the folder.

## Source

[Build instructions](BUILDING.md). GPL-3.0-or-later. Uses [MinHook](https://github.com/TsudaKageyu/minhook), with animation hooks based on [m417z's mod](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-speed.wh.cpp) and taskbar timer IDs from [Bo0ii's mod](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-autohide-instant-show.wh.cpp).
