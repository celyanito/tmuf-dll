// src/hooks/d3d9_hook.cpp
#include "../../include/common.h"
#include "../../include/hooks/d3d9_hook.h"
#include "../../include/ui/imgui_layer.h"
#include <d3d9.h>
#include "../../include/discord_presence.h"
#include "../../external/imgui/imgui.h"
// MinHook
#include "../../external/minhook/include/MinHook.h"
#include "../../external/imgui/backends/imgui_impl_win32.h"
namespace hooks
{
    namespace d3d9
    {
        using Direct3DCreate9_t = IDirect3D9 * (WINAPI*)(UINT);
        using CreateDevice_t = HRESULT(WINAPI*)(
            IDirect3D9*,
            UINT,
            D3DDEVTYPE,
            HWND,
            DWORD,
            D3DPRESENT_PARAMETERS*,
            IDirect3DDevice9**
            );
        using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);
        using Reset_t = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);

        static void* g_pDirect3DCreate9 = nullptr;
        static Direct3DCreate9_t g_origDirect3DCreate9 = nullptr;
        static CreateDevice_t    g_origCreateDevice = nullptr;
        static EndScene_t        g_origEndScene = nullptr;
        static Reset_t           g_origReset = nullptr;

        static constexpr int VTABLE_INDEX_IDirect3D9_CreateDevice = 16;
        static constexpr int VTABLE_INDEX_IDirect3DDevice9_Reset = 16;
        static constexpr int VTABLE_INDEX_IDirect3DDevice9_EndScene = 42;

        static bool              g_installed = false;
        static HWND              g_hwnd = nullptr;
        static WNDPROC           g_oldWndProc = nullptr;
        static IDirect3DDevice9* g_lastDevice = nullptr;

        // Anti double-toggle: empêche WndProc + fallback de toggler sur la même pression
        static DWORD g_lastToggleTick = 0;
        static bool CanToggleNow()
        {
            DWORD now = GetTickCount();
            if (now - g_lastToggleTick < 200) // 200ms = bloque auto-repeat + double trigger
                return false;
            g_lastToggleTick = now;
            return true;
        }

        static void PollToggleFallback(HWND hwnd)
        {
            // Fallback sur F1 (même touche que WndProc), mais gated par CanToggleNow()
            const bool down = (GetAsyncKeyState(VK_F1) & 0x8000) != 0;
            static bool prev = false;

            const bool canToggle = hwnd && (GetForegroundWindow() == hwnd);
            if (canToggle && down && !prev) {
                if (CanToggleNow())
                    ui::SetMenuOpen(!ui::IsMenuOpen());
            }

            prev = down;
        }

        // ------------------------------------------------------------
        // WndProc hook: toggle F1 + forward input to ImGui layer
        // ------------------------------------------------------------
        LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            // Toggle d'abord (ne dépend pas d'ImGui)
            if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && wParam == VK_F1) {
                if (CanToggleNow())
                    ui::SetMenuOpen(!ui::IsMenuOpen());
                return 0;
            }

            // Puis ImGui
            if (ui::IsMenuOpen()) {
                // Laisse ImGui “voir” les events
                ui::HandleWndProc(hWnd, msg, wParam, lParam);

                // Bloque seulement si ImGui veut capturer l’input
                ImGuiIO& io = ImGui::GetIO();

                const bool isMouseMsg =
                    (msg == WM_MOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
                        msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP || msg == WM_MOUSEWHEEL ||
                        msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP || msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP);

                const bool isKeyMsg =
                    (msg == WM_KEYDOWN || msg == WM_KEYUP || msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP ||
                        msg == WM_CHAR);

                if ((isMouseMsg && io.WantCaptureMouse) || (isKeyMsg && io.WantCaptureKeyboard)) {
                    return 1; // ImGui garde l’input
                }
            }

            return CallWindowProc(g_oldWndProc, hWnd, msg, wParam, lParam);
        }

        // ------------------------------------------------------------
        // Reset hook (important)
        // ------------------------------------------------------------
        static HRESULT __stdcall hkReset(IDirect3DDevice9* device, D3DPRESENT_PARAMETERS* pp)
        {
            ui::OnResetBegin();
            HRESULT hr = g_origReset(device, pp);
            if (SUCCEEDED(hr)) ui::OnResetEnd();
            return hr;
        }

        // ------------------------------------------------------------
        // EndScene hook: init ImGui + install WndProc once + render
        // ------------------------------------------------------------
        static HRESULT __stdcall hkEndScene(IDirect3DDevice9* device)
        {
            g_lastDevice = device;

            D3DDEVICE_CREATION_PARAMETERS cp{};
            if (SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow)
            {
                // Si HWND a changé → on rehook + on rebind ImGui
                if (g_hwnd != cp.hFocusWindow)
                {
                    // Restore old wndproc on old window
                    if (g_hwnd && g_oldWndProc) {
                        SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
                        g_oldWndProc = nullptr;
                    }

                    const bool wasOpen = ui::IsMenuOpen();

                    // Re-init ImGui sur le nouveau HWND
                    if (ui::IsInitialized())
                        ui::Shutdown();

                    g_hwnd = cp.hFocusWindow;
                    ui::Init((HWND__*)g_hwnd, device);
                    ui::SetMenuOpen(wasOpen);

                    // Hook wndproc on new window
                    g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
                    std::cout << "[wndproc] re-hooked (HWND changed)\n";
                }
                else
                {
                    // Init “première fois” si pas encore fait
                    if (!ui::IsInitialized())
                    {
                        g_hwnd = cp.hFocusWindow; // IMPORTANT

                        ui::Init((HWND__*)g_hwnd, device);

                        if (!g_oldWndProc) {
                            g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
                            std::cout << "[wndproc] hooked\n";
                        }
                        std::cout << "[ui] Init with HWND=" << g_hwnd << "\n";
                    }
                }
            }

            if (ui::IsInitialized())
            {
                // (optionnel) ton fallback toggle, si tu l’as gardé
                PollToggleFallback(g_hwnd);

                // ---- Discord init (1x) + callbacks throttlés ----
                static bool discordInitDone = false;
                if (!discordInitDone) {
                    discord_presence::Init();
                    discordInitDone = true;
                }

                static ULONGLONG lastDiscordTick = 0;
                ULONGLONG now = GetTickCount64();
                if (now - lastDiscordTick >= 100) { // 10x/sec largement suffisant
                    discord_presence::Tick();
                    lastDiscordTick = now;
                }
                // -------------------------------------------------

                ui::RenderFrame();
            }

            return g_origEndScene(device);
        }


        // ------------------------------------------------------------
        // CreateDevice hook (si injection tôt)
        // ------------------------------------------------------------
        static HRESULT WINAPI hkCreateDevice(
            IDirect3D9* self,
            UINT Adapter,
            D3DDEVTYPE DeviceType,
            HWND hFocusWindow,
            DWORD BehaviorFlags,
            D3DPRESENT_PARAMETERS* pPresentationParameters,
            IDirect3DDevice9** ppReturnedDeviceInterface)
        {
            HRESULT hr = g_origCreateDevice(self, Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                pPresentationParameters, ppReturnedDeviceInterface);

            if (SUCCEEDED(hr) && ppReturnedDeviceInterface && *ppReturnedDeviceInterface)
            {
                IDirect3DDevice9* dev = *ppReturnedDeviceInterface;
                g_lastDevice = dev;

                std::cout << "[d3d9] CreateDevice -> got REAL device: " << dev << "\n";

                void** vtbl = *reinterpret_cast<void***>(dev);

                // EndScene
                if (!g_origEndScene) {
                    void* pEndScene = vtbl[VTABLE_INDEX_IDirect3DDevice9_EndScene];
                    if (MH_CreateHook(pEndScene, &hkEndScene, reinterpret_cast<void**>(&g_origEndScene)) == MH_OK &&
                        MH_EnableHook(pEndScene) == MH_OK) {
                        std::cout << "[d3d9] EndScene hook enabled\n";
                    }
                    else {
                        std::cout << "[d3d9] EndScene hook FAILED\n";
                    }
                }

                // Reset
                if (!g_origReset) {
                    void* pReset = vtbl[VTABLE_INDEX_IDirect3DDevice9_Reset];
                    if (MH_CreateHook(pReset, &hkReset, reinterpret_cast<void**>(&g_origReset)) == MH_OK &&
                        MH_EnableHook(pReset) == MH_OK) {
                        std::cout << "[d3d9] Reset hook enabled\n";
                    }
                    else {
                        std::cout << "[d3d9] Reset hook FAILED\n";
                    }
                }
            }

            return hr;
        }

        static IDirect3D9* WINAPI hkDirect3DCreate9(UINT SDKVersion)
        {
            IDirect3D9* d3d9 = g_origDirect3DCreate9(SDKVersion);
            std::cout << "[d3d9] Direct3DCreate9 -> " << d3d9 << "\n";

            if (d3d9 && !g_origCreateDevice)
            {
                void** vtbl = *reinterpret_cast<void***>(d3d9);
                void* pCreateDevice = vtbl[VTABLE_INDEX_IDirect3D9_CreateDevice];

                std::cout << "[d3d9] Hooking IDirect3D9::CreateDevice: orig=" << pCreateDevice
                    << " new=" << (void*)&hkCreateDevice << "\n";

                if (MH_CreateHook(pCreateDevice, &hkCreateDevice, reinterpret_cast<void**>(&g_origCreateDevice)) == MH_OK &&
                    MH_EnableHook(pCreateDevice) == MH_OK)
                {
                    std::cout << "[d3d9] CreateDevice hook enabled\n";
                }
                else
                {
                    std::cout << "[d3d9] CreateDevice hook FAILED\n";
                }
            }

            return d3d9;
        }

        // ------------------------------------------------------------
        // Dummy-device hooks (si injection tardive)
        // ------------------------------------------------------------
        static bool InstallDeviceHooksViaDummy()
        {
            IDirect3D9* d3d9 = Direct3DCreate9(D3D_SDK_VERSION);
            if (!d3d9) {
                std::cout << "[d3d9] dummy Direct3DCreate9 failed\n";
                return false;
            }

            const char* cls = "tmuf_dummy_d3d9";

            WNDCLASSEXA wc{};
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = DefWindowProcA;
            wc.hInstance = GetModuleHandleA(nullptr);
            wc.lpszClassName = cls;

            // Register (ignore "already exists" if called twice)
            if (!RegisterClassExA(&wc)) {
                DWORD err = GetLastError();
                if (err != ERROR_CLASS_ALREADY_EXISTS) {
                    std::cout << "[d3d9] RegisterClassExA failed err=" << err << "\n";
                    d3d9->Release();
                    return false;
                }
            }

            HWND hwnd = CreateWindowExA(
                0, cls, "dummy",
                WS_OVERLAPPEDWINDOW,
                0, 0, 100, 100,
                nullptr, nullptr, wc.hInstance, nullptr
            );

            if (!hwnd) {
                std::cout << "[d3d9] CreateWindowExA failed err=" << GetLastError() << "\n";
                UnregisterClassA(cls, wc.hInstance);
                d3d9->Release();
                return false;
            }

            D3DPRESENT_PARAMETERS pp{};
            pp.Windowed = TRUE;
            pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
            pp.hDeviceWindow = hwnd;

            IDirect3DDevice9* dev = nullptr;
            HRESULT hr = d3d9->CreateDevice(
                D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                &pp, &dev
            );

            if (FAILED(hr) || !dev) {
                std::cout << "[d3d9] dummy CreateDevice failed hr=0x" << std::hex << hr << std::dec << "\n";
                DestroyWindow(hwnd);
                UnregisterClassA(cls, wc.hInstance);
                d3d9->Release();
                return false;
            }

            void** vtbl = *reinterpret_cast<void***>(dev);
            void* pEndScene = vtbl[VTABLE_INDEX_IDirect3DDevice9_EndScene];
            void* pReset = vtbl[VTABLE_INDEX_IDirect3DDevice9_Reset];

            std::cout << "[d3d9] dummy vtbl EndScene=" << pEndScene << " Reset=" << pReset << "\n";

            if (!g_origEndScene) {
                if (MH_CreateHook(pEndScene, &hkEndScene, reinterpret_cast<void**>(&g_origEndScene)) == MH_OK)
                    MH_EnableHook(pEndScene);
            }
            if (!g_origReset) {
                if (MH_CreateHook(pReset, &hkReset, reinterpret_cast<void**>(&g_origReset)) == MH_OK)
                    MH_EnableHook(pReset);
            }

            dev->Release();
            DestroyWindow(hwnd);
            UnregisterClassA(cls, wc.hInstance);
            d3d9->Release();
            return true;
        }

        bool Install()
        {
            if (g_installed)
                return true;

            std::cout << "[d3d9] Install (MinHook)\n";

            if (MH_Initialize() != MH_OK) {
                std::cout << "[d3d9] MH_Initialize failed\n";
                return false;
            }

            HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
            if (!hD3D9) hD3D9 = LoadLibraryA("d3d9.dll");
            if (!hD3D9) {
                std::cout << "[d3d9] LoadLibrary d3d9.dll failed\n";
                return false;
            }

            g_pDirect3DCreate9 = (void*)GetProcAddress(hD3D9, "Direct3DCreate9");
            if (!g_pDirect3DCreate9) {
                std::cout << "[d3d9] GetProcAddress Direct3DCreate9 failed\n";
                return false;
            }

            if (MH_CreateHook(g_pDirect3DCreate9, (void*)&hkDirect3DCreate9, (void**)&g_origDirect3DCreate9) != MH_OK) {
                std::cout << "[d3d9] MH_CreateHook Direct3DCreate9 failed\n";
                return false;
            }

            auto st = MH_EnableHook(g_pDirect3DCreate9);
            if (st != MH_OK) {
                std::cout << "[d3d9] MH_EnableHook Direct3DCreate9 failed, code=" << (int)st << "\n";
                return false;
            }

            std::cout << "[d3d9] Direct3DCreate9 hook enabled\n";

            std::cout << "[d3d9] Installing dummy device hooks...\n";
            if (InstallDeviceHooksViaDummy())
                std::cout << "[d3d9] Dummy device hooks OK\n";
            else
                std::cout << "[d3d9] Dummy device hooks FAILED\n";

            g_installed = true;
            return true;
        }

        void Uninstall()
        {
            if (!g_installed) return;

            std::cout << "[d3d9] Uninstall (MinHook)\n";

            // Restore WndProc
            if (g_hwnd && g_oldWndProc) {
                SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)g_oldWndProc);
                g_oldWndProc = nullptr;
                g_hwnd = nullptr;
                std::cout << "[wndproc] restored\n";
            }

            ui::Shutdown();

            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();

            g_installed = false;
            g_origCreateDevice = nullptr;
            g_origEndScene = nullptr;
            g_origReset = nullptr;
            g_lastDevice = nullptr;
        }

        bool IsInstalled() { return g_installed; }
        IDirect3DDevice9* GetLastDevice() { return g_lastDevice; }
    }
}
