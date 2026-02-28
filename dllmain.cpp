#include "pch.h"
#include "dump_thread.h"

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

        std::cout << "Hello World!\n";

        HANDLE hThread = CreateThread(nullptr, 0, WaitAndDumpThread, nullptr, 0, nullptr);
        if (hThread) CloseHandle(hThread);

        break;
    }
    case DLL_PROCESS_DETACH:
        FreeConsole();
        break;
    }
    return TRUE;
}