# TaskbarSpeed

Faster Windows 11 auto-hide animations. Show and hide run at 500% speed. The wait before hiding runs at 250% speed: normally 500 ms becomes 200 ms. Other animations stay enabled.

## Set up

1. Turn on **Automatically hide the taskbar** in Windows taskbar settings.
2. Download the ZIP from [Releases](https://github.com/xSil3nt/TaskbarSpeed/releases/latest) and extract it.
3. Double-click **Setup.cmd**. It installs in `%USERPROFILE%\TaskbarSpeed`, applies the tweak, and enables it at sign-in.

Find **TaskbarSpeed** in Start to open its folder. You can delete the downloaded ZIP and extracted folder after setup. No admin access needed. Setup preserves existing speed settings.

No Windhawk, Visual Studio, or extra downloads needed. Setup uses Windows PowerShell. The launcher exits after applying; a small DLL stays inside Explorer.

**Compatibility:** this release targets x64 Windows 11 24H2, build 26100.9550. It checks the taskbar files and stops if they differ. Windows updates may require a new build.

## Startup

Setup adds a shortcut to your Windows Startup folder. Enable or disable **TaskbarSpeed** in **Task Manager > Startup apps**. The launcher waits for Explorer, retries temporary startup failures, and exits.

To upgrade from an older release: run **Undo.cmd** in the installed folder, sign out, then run the new **Setup.cmd**.

## Change or remove

- Open **TaskbarSpeed** from Start, edit **TaskbarSpeed.ini**, then run **Apply.cmd**. `100` means normal speed; values up to `1000` are supported.
- Run **Status.cmd** to check settings and timing.
- If Explorer restarts, run **Apply.cmd** again.
- Run **Undo.cmd** to disable the tweak and remove startup. Sign out before deleting `%USERPROFILE%\TaskbarSpeed`. You can also delete the TaskbarSpeed shortcut from Start.

## Source

[Build instructions](BUILDING.md). GPL-3.0-or-later. Uses [MinHook](https://github.com/TsudaKageyu/minhook), with animation hooks based on [m417z's mod](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-auto-hide-speed.wh.cpp) and taskbar timer IDs from [Bo0ii's mod](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-autohide-instant-show.wh.cpp).
