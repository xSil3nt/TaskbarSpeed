$ErrorActionPreference = 'Stop'
try {
    $destination = Join-Path $env:USERPROFILE 'TaskbarSpeed'
    $files = @('TaskbarSpeed.exe', 'TaskbarSpeedHook.dll', 'TaskbarSpeed.ini',
        'Setup.cmd', 'Install.ps1', 'Apply.cmd', 'Status.cmd', 'Undo.cmd',
        'EnableStartup.cmd', 'README.txt', 'LICENSE.txt', 'LICENSE-MINHOOK.txt')
    foreach ($name in $files) {
        if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot $name) -PathType Leaf)) {
            throw "Missing $name. Extract the whole ZIP before running Setup.cmd."
        }
    }
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    foreach ($name in $files) {
        $source = Join-Path $PSScriptRoot $name
        $target = Join-Path $destination $name
        if ($source -eq $target) { continue }
        if (Test-Path -LiteralPath $target) {
            if ($name -eq 'TaskbarSpeed.ini') { continue }
            if ((Get-FileHash -LiteralPath $source).Hash -eq (Get-FileHash -LiteralPath $target).Hash) { continue }
        }
        try { Copy-Item -LiteralPath $source -Destination $target -Force }
        catch { throw "Could not update $name. Run Undo.cmd in the installed folder, sign out, and run setup again. $($_.Exception.Message)" }
    }
    $launcher = Join-Path $destination 'TaskbarSpeed.exe'
    $apply = Start-Process -FilePath $launcher -ArgumentList '/apply /quiet /wait' -WindowStyle Hidden -PassThru -Wait
    if ($apply.ExitCode -ne 0) {
        $detail = Get-Content -LiteralPath (Join-Path $destination 'Status.txt') -Raw
        throw "Setup could not apply the tweak. $detail"
    }
    $startup = Start-Process -FilePath $launcher -ArgumentList '/startup /quiet' -WindowStyle Hidden -PassThru -Wait
    if ($startup.ExitCode -ne 0) { throw 'Could not enable startup. Run EnableStartup.cmd in the installed folder.' }
    $menu = [Environment]::GetFolderPath('Programs')
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut((Join-Path $menu 'TaskbarSpeed.lnk'))
    $shortcut.TargetPath = $destination
    $shortcut.Description = 'TaskbarSpeed settings and controls'
    $shortcut.Save()
    Write-Host "Installed in $destination"
    Write-Host 'Active now and starts at sign-in. Find TaskbarSpeed in Start to change settings or undo.'
    Write-Host 'You can delete the downloaded ZIP and its extracted folder.'
} catch {
    Write-Host $_.Exception.Message
    exit 1
}
