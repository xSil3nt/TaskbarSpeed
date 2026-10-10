TaskbarSpeed

1. Turn on "Automatically hide the taskbar" in Windows taskbar settings.
2. Keep this extracted folder somewhere permanent.
3. Double-click Setup.cmd. It applies the tweak and enables it at sign-in.

Show and hide animations: 500% speed.
Wait before hiding: 250% speed (normally 500 ms becomes 200 ms).
Other Windows animations and the wait before showing are unchanged.

Edit TaskbarSpeed.ini and run Apply.cmd to change the speeds.
Run Status.cmd to check settings and timing.
If Explorer restarts, run Apply.cmd again.
Run Undo.cmd to disable and remove startup. Sign out before deleting the folder.

Requires matching x64 Windows 11 24H2 taskbar files from build 26100.9550.
If the files differ, it stops. Windows updates may require a new build.

The launcher exits after applying. A small DLL stays inside Explorer.
No Windhawk, Visual Studio, installer, or runtime downloads needed.

Source and license: https://github.com/xSil3nt/TaskbarSpeed
GPL-3.0-or-later. MinHook's BSD license is included in LICENSE-MINHOOK.txt.
