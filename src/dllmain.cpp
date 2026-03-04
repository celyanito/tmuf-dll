#include "../include/dump_thread.h"
#include "../include/common.h"
#include "../include/ui/imgui_layer.h"
#include "../include/hooks/d3d9_hook.h"
#include "../include/discord_presence.h"
// ================= DLLMAIN =================
__declspec(dllexport) BOOL APIENTRY DllMain(
    HMODULE hModule,
    DWORD ul_reason_for_call,
    LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        if (!AllocConsole())
            break;

        freopen("CONOUT$", "w", stdout);
        freopen("CONIN$", "r", stdin);
        freopen("CONERR$", "w", stderr);

        HANDLE hThread = CreateThread(nullptr, 0, WaitAndDumpThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);

        break;
    }
    case DLL_PROCESS_DETACH:
        FreeConsole();
        discord_presence::Shutdown();
        break;
    }
    return TRUE;
}
