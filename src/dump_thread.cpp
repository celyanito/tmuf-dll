#include "../include/safe_memory.h"
#include "../include/game_structs.h"
#include "../include/offsets.h"
#include "../include/dump_thread.h"
#include "../include/common.h"
#include "../include/hooks/d3d9_hook.h"

static bool GetModuleSize(uintptr_t base, uint32_t& outSizeOfImage)
{
    outSizeOfImage = 0;

    IMAGE_DOS_HEADER dos{};
    if (!SafeRead(base, dos)) return false;
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return false;

    uintptr_t ntAddr = base + (uintptr_t)dos.e_lfanew;
    IMAGE_NT_HEADERS32 nt{};
    if (!SafeRead(ntAddr, nt)) return false;
    if (nt.Signature != IMAGE_NT_SIGNATURE) return false;

    outSizeOfImage = nt.OptionalHeader.SizeOfImage;
    return outSizeOfImage != 0;
}

static bool IsPtrInModule(uintptr_t p, uintptr_t base, uint32_t sizeOfImage)
{
    if (!sizeOfImage) return false;
    return p >= base && p < (base + (uintptr_t)sizeOfImage);
}

// Heuristique : un objet C++ a une vtable en [0], et vtable[0] pointe souvent dans .text du module
static bool LooksLikeCppObject(uintptr_t obj, uintptr_t base, uint32_t sizeOfImage)
{
    if (!IsPlausiblePtr(obj) || !IsReadableAddress(obj, 4)) return false;

    uintptr_t vtbl = 0;
    if (!SafeRead(obj, vtbl)) return false;
    if (!IsPlausiblePtr(vtbl) || !IsReadableAddress(vtbl, 4)) return false;

    uintptr_t fn0 = 0;
    if (!SafeRead(vtbl, fn0)) return false;

    return IsPtrInModule(fn0, base, sizeOfImage);
}

static uintptr_t TryReadPtr(uintptr_t addr)
{
    uintptr_t v = 0;
    SafeRead(addr, v);
    return v;
}

static bool LooksLikeFastString(uintptr_t addr)
{
    CFastString s{};
    if (!SafeRead(addr, s)) return false;

    if (s.size <= 0 || s.size >= 256) return false;
    if (!IsPlausiblePtr((uintptr_t)s.psz)) return false;
    if (!IsReadableAddress((uintptr_t)s.psz, (size_t)s.size)) return false;

    // Optionnel : si tu veux être strict
    // if (!IsPrintableAscii(s.psz, s.size)) return false;

    return true;
}

static bool CandidateHasProfileAndLogin(uintptr_t trackmania)
{
    uintptr_t profile = 0;
    if (!SafeRead(trackmania + PROFILE_OFFSET, profile)) return false;
    if (!IsPlausiblePtr(profile) || !IsReadableAddress(profile, 4)) return false;

    return LooksLikeFastString(profile + LOGIN_OFFSET);
}

// ===== MODIF: on ne garde que candA =====
static uintptr_t FindCTrackMania(uintptr_t base, uintptr_t& outRawA)
{
    outRawA = 0;

    uint32_t sizeOfImage = 0;
    if (!GetModuleSize(base, sizeOfImage))
        return 0;

    outRawA = TryReadPtr(base + O_CTRACKMANIA_A);

    // On valide candA comme avant:
    // - objet C++ plausible
    // - profile/login valides
    if (LooksLikeCppObject(outRawA, base, sizeOfImage) && CandidateHasProfileAndLogin(outRawA))
        return outRawA;

    return 0;
}
static bool WaitForCTrackMania(uintptr_t base,
    uintptr_t& outTrackMania,
    uintptr_t& outRawA)
{
    outTrackMania = 0;
    outRawA = 0;

    std::cout << "[*] Waiting for valid CTrackMania*...\n";

    const int stepMs = 50;

    uintptr_t lastRawA = 0;
    bool printedStage = false;

    for (;;)
    {
        uintptr_t rawA = 0;
        uintptr_t tm = FindCTrackMania(base, rawA); // candA only

        // Log seulement quand candA apparaît / change
        if (rawA != 0 && rawA != lastRawA)
        {
            std::cout << "[*] candA(base+0x" << std::hex << O_CTRACKMANIA_A
                << ") -> 0x" << rawA << std::dec << "\n";
            lastRawA = rawA;

            if (!printedStage)
            {
                std::cout << "[*] candA found, waiting for Profile/Login...\n";
                printedStage = true;
            }
        }

        if (tm)
        {
            outTrackMania = tm;
            outRawA = rawA;
            return true;
        }

        Sleep(stepMs);
    }
}
DWORD WINAPI WaitAndDumpThread(LPVOID)
{
    Sleep(2000);

    std::cout << "[+] Installing D3D9 hook...\n";
    bool ok = hooks::d3d9::Install();
    std::cout << "[+] Install result: " << (ok ? "true" : "false") << "\n";

    uintptr_t base = (uintptr_t)GetModuleHandleW(nullptr);

    // Log exe path
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wcout << L"[*] EXE: " << exePath << L"\n";

    // Ancien test Nations/United (peut être faux avec ModLoader)
    unsigned char b = 0;
    if (SafeRead(base + 0x700B15, b))
    {
        if (b == 0xA3) std::cout << "[*] Detected byte: Nations (0xA3)\n";
        else if (b == 0xF7) std::cout << "[*] Detected byte: United (0xF7)\n";
        else std::cout << "[*] Detected byte: Unknown (0x" << std::hex << (int)b << std::dec << ")\n";
    }
    else
    {
        std::cout << "[*] Detection byte read failed\n";
    }

    // ===== Find CTrackMania* (candA only) =====
    uintptr_t trackmania = 0;
    uintptr_t rawA = 0;

    WaitForCTrackMania(base, trackmania, rawA); // plus de timeout
    std::cout << "[+] CTrackMania* = 0x" << std::hex << trackmania << std::dec << "\n";
    std::cout << "    candA(base+0x" << std::hex << O_CTRACKMANIA_A << ") -> 0x" << rawA << std::dec << "\n";
    // ================= PROFILE =================
    uintptr_t profile = 0;
    SafeRead(trackmania + PROFILE_OFFSET, profile);
    std::cout << "[+] Profile* = 0x" << std::hex << profile << std::dec << "\n";

    // ================= LOGIN =================
    CFastString login{};
    SafeRead(profile + LOGIN_OFFSET, login);

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
    uintptr_t menuMgr = 0;
    SafeRead(trackmania + MENUMGR_OFFSET, menuMgr);
    std::cout << "[+] MenuManager* = 0x" << std::hex << menuMgr << std::dec << "\n";
    std::cout << "[*] Open ESC then press F8 to dump frames\n";

    // CMwId_GetName (BulbToys RVA)
    using CMwId_GetNameFn = CFastString * (__thiscall*)(CMwId*, CFastString*);
    auto CMwId_GetName = (CMwId_GetNameFn)(base + RVA_CMwId_GetName);
    (void)CMwId_GetName;

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
