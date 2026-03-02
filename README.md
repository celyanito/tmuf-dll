![build](https://github.com/celyanito/tmuf-dll/actions/workflows/build-win32.yml/badge.svg)

# tmuf-dll

Injected DLL (**Win32 / x86**) for TrackMania Forever (TMNF/TMUF).

## Features

- D3D9 hook (MinHook)
  - Robust install via **dummy device fallback** (works even if injected late)
  - Hooks **EndScene** + **Reset**
- ImGui overlay
  - Toggle: **F1** To enable the FPS Counter imGui window

  - Full mouse/keyboard support via **WndProc hook**
- Memory reading
  - Waits for valid `CTrackMania*` (no timeout)
  - Reads Profile/Login and sets console title
- Debug dump
  - Open ESC menu, press **F8** to dump `GameMenu->Frames` pointers

## Build (Visual Studio)

1. Open `TmMyMod.sln`
2. Select `Debug | Win32`
3. Build

Output: `Debug/TmMyMod.dll`

## Build (command line)

```powershell
msbuild TmMyMod.sln /m /p:Configuration=Debug /p:Platform=Win32
