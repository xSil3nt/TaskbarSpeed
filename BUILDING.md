# Build

The release is built on GitHub Actions with Zig 0.13.0. The workflow verifies the compiler's SHA-256 checksum, cross-compiles for Windows x64, and packages the files from this repository.

To build locally on Windows, download the portable Zig 0.13.0 compiler and run:

```powershell
powershell -NoProfile -File source\Build.ps1 -ZigPath C:\path\to\zig.exe
```

This rebuilds for the Windows files specified in `source/config.h`. It does **not** add support for other Windows versions.

The configuration contains module hashes, timestamps, sizes, symbol addresses, and function signatures. The legacy animation hook also uses two fields verified in this build's `TrayUI::SlideWindow` implementation. Supporting a new Windows build requires resolving its Microsoft symbols, checking the function layout, and updating those guards. Keep the checks in place.

Only the legacy animation path has been exercised on the original PC. The optional modern WinRT animation hooks are included but have not been verified on other machines. No system files are changed on disk.
