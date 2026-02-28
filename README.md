![build](https://github.com/celyanito/tmuf-dll/actions/workflows/build-win32.yml/badge.svg)
# tmuf-dll

Injected DLL (Debug x86 / Win32) for TrackMania Forever / TMUF.

## Build (Visual Studio)
1. Open `TmMyMod.sln`
2. Select `Debug | Win32`
3. Build

Output: `Debug/TmMyMod.dll`

## Build (command line)
```powershell
msbuild TmMyMod.sln /m /p:Configuration=Debug /p:Platform=Win32