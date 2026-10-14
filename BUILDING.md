# Build

The release is built on GitHub Actions with Zig 0.13.0. The workflow verifies the compiler's SHA-256 checksum, cross-compiles for Windows x64, and packages the files from this repository.

To build locally on Windows, download the portable Zig 0.13.0 compiler and run:

```powershell
powershell -NoProfile -File source\Build.ps1 -ZigPath C:\path\to\zig.exe
```

There is no Windows build, file-hash, timestamp, or fixed image-size gate. The legacy hook scans function starts from the loaded module's x64 unwind table and requires a unique prologue match with both known duration-field reads. Its fields are still implementation details and may change in future Windows updates.

`source/config.h` contains function hints and signatures. Modern hooks use fixed address hints with code checks; if unavailable, the legacy animation and hide-delay hooks can still apply. No symbol downloads or background scanning are needed at login.

Only the legacy animation path has been exercised on the original PC. The optional modern WinRT animation hooks are included but have not been verified on other machines. No system files are changed on disk.
