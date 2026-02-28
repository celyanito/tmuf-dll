#include "pch.h"
#include "safe_memory.h"
#include "game_structs.h"
#include "offsets.h"
#include "dump_thread.h"

// ================= THREAD =================
DWORD WINAPI WaitAndDumpThread(LPVOID)
{
    Sleep(2000);

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);

    // Wait for CTrackMania* pointer at base + O_CTRACKMANIA
    uintptr_t trackmania = 0;
    while (!trackmania)
    {
        // (same behavior as your code: direct deref)
        trackmania = *reinterpret_cast<uintptr_t*>(base + O_CTRACKMANIA);
        Sleep(50);
    }

    std::cout << "[+] CTrackMania* = 0x" << std::hex << trackmania << std::dec << "\n";

    // ================= PROFILE =================
    uintptr_t profile = 0;
    while (!profile)
    {
        profile = *reinterpret_cast<uintptr_t*>(trackmania + PROFILE_OFFSET);
        Sleep(200);
    }

    std::cout << "[+] Profile* = 0x" << std::hex << profile << std::dec << "\n";

    CFastString login = *reinterpret_cast<CFastString*>(profile + LOGIN_OFFSET);

    if (login.psz && login.size > 0 && login.size < 256)
    {
        std::string loginStr(login.psz, login.size);
        std::string newTitle = loginStr + "'s Console";
        SetConsoleTitleA(newTitle.c_str());

        std::cout << "[+] Login = " << loginStr << "\n";
        std::cout << "[+] Console title set\n";
    }
    else
    {
        std::cout << "[!] Login invalid\n";
    }

    // ================= MENU =================
    uintptr_t menuMgr = *reinterpret_cast<uintptr_t*>(trackmania + MENUMGR_OFFSET);
    std::cout << "[+] MenuManager* = 0x" << std::hex << menuMgr << std::dec << "\n";
    std::cout << "[*] Open ESC then press F8 to dump frames\n";

    // CMwId_GetName (BulbToys RVA)
    using CMwId_GetNameFn = CFastString * (__thiscall*)(CMwId*, CFastString*);
    auto CMwId_GetName = (CMwId_GetNameFn)(base + RVA_CMwId_GetName);
    (void)CMwId_GetName; // (unused for now, same as your current state)

    // ================= LOOP =================
    while (true)
    {
        if (GetAsyncKeyState(VK_F8) & 1)
        {
            uintptr_t gameMenu = 0;
            SafeRead(menuMgr + GAMEMENU_OFFSET, gameMenu);

            std::cout << "\n[F8] GameMenu = 0x" << std::hex << gameMenu << std::dec << "\n";
            if (!gameMenu)
            {
                std::cout << "[F8] Open ESC first\n";
                continue;
            }

            CFastBuffer<uintptr_t> frames{};
            if (!SafeRead(gameMenu + FRAMES_OFFSET, frames))
            {
                std::cout << "[F8] Cannot read frames\n";
                continue;
            }

            std::cout << "[F8] Frames.size=" << frames.size
                << " pElems=0x" << std::hex << (uintptr_t)frames.pElems
                << " cap=" << std::dec << frames.capacity << "\n";

            if (frames.size <= 0 || frames.size > 2000 || !IsPlausiblePtr((uintptr_t)frames.pElems))
            {
                std::cout << "[F8] Frames invalid\n";
                continue;
            }

            std::cout << "---- Frames pointers ----\n";
            for (int i = 0; i < frames.size; i++)
            {
                uintptr_t framePtr = 0;

                // x86 pointer size = 4, your original code used i*4 so we keep it
                if (!SafeRead((uintptr_t)frames.pElems + i * 4, framePtr))
                    continue;

                std::cout << "[Frame " << i << "] ptr=0x" << std::hex << framePtr << std::dec;

                uint32_t d0 = 0, d1 = 0, d2 = 0, d3 = 0;
                if (framePtr && IsReadableAddress(framePtr, 16))
                {
                    SafeRead(framePtr + 0x0, d0);
                    SafeRead(framePtr + 0x4, d1);
                    SafeRead(framePtr + 0x8, d2);
                    SafeRead(framePtr + 0xC, d3);

                    std::cout << " head: " << std::hex
                        << d0 << " " << d1 << " " << d2 << " " << d3
                        << std::dec;
                }

                std::cout << "\n";
            }

            std::cout << "-------------------------\n" << std::flush;
        }

        Sleep(20);
    }

    return 0;
}