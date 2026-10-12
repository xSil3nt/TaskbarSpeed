Startup fix: setup now uses a Windows Startup-folder shortcut, managed in **Task Manager > Startup apps**, and removes the old registry entry. The launcher waits longer for Explorer and retries temporary initialization failures. It exits after applying.

**Upgrading:** run **Undo.cmd** in the installed folder, sign out, then run the new **Setup.cmd**. Speed settings are kept.

Download **TaskbarSpeed-win11-24H2-x64.zip**, extract it, and double-click **Setup.cmd**. Turn on taskbar auto-hide in Windows first.

Setup installs in `%USERPROFILE%\TaskbarSpeed`, applies the tweak, and enables startup. Find **TaskbarSpeed** in Start to open settings and controls. You can delete the download and extracted folder afterward. No admin access or extra downloads needed.

Show and hide animations: **500% speed**. Wait before hiding: **250% speed** (normally 500 ms becomes 200 ms). Existing speed settings are preserved.

Requires matching x64 Windows 11 24H2 taskbar files from build 26100.9550. Different files are rejected. Windows updates may require a new build.

Run **Undo.cmd** in the installed folder to disable and remove startup.
