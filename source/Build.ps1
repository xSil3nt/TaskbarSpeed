param([Parameter(Mandatory=$true)][string]$ZigPath)
$ErrorActionPreference = 'Stop'
$taskRoot = Split-Path $PSScriptRoot -Parent
$taskVendor = Join-Path $PSScriptRoot 'vendor\minhook'
$taskDllOutput = Join-Path $taskRoot 'TaskbarSpeedHook.dll'
$taskExeOutput = Join-Path $taskRoot 'TaskbarSpeed.exe'
& $ZigPath cc -target x86_64-windows-gnu -std=c11 -Os -Wall -Wextra -shared -s -I "$taskVendor\include" -I $PSScriptRoot (Join-Path $PSScriptRoot 'hook.c') "$taskVendor\src\buffer.c" "$taskVendor\src\hook.c" "$taskVendor\src\trampoline.c" "$taskVendor\src\hde\hde64.c" -luser32 -o $taskDllOutput
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed. It cannot be overwritten while loaded in Explorer.' }
& $ZigPath cc -target x86_64-windows-gnu -std=c11 -Os -Wall -Wextra -municode '-Wl,--subsystem,windows' -s (Join-Path $PSScriptRoot 'launcher.c') -lbcrypt -ladvapi32 -luser32 -lshell32 -lole32 -luuid -o $taskExeOutput
if ($LASTEXITCODE -ne 0) { throw 'Launcher build failed.' }
