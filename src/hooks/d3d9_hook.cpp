#include "../../include/common.h"
#include "../../include/hooks/d3d9_hook.h"
#include "../../include/ui/imgui_layer.h"
#include <d3d9.h>

// MinHook
#include "../../external/minhook/include/MinHook.h"

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

        static bool     g_installed = false;
        static HWND     g_hwnd = nullptr;
        static WNDPROC  g_oldWndProc = nullptr;
        static IDirect3DDevice9* g_lastDevice = nullptr;

        // ------------------------------------------------------------
        // WndProc hook: toggle F1 on KEYUP, forward input to ImGui
        // ------------------------------------------------------------
        static LRESULT CALLBACK WndProcHook(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
        {
            // Toggle on KEYUP -> 1 press = 1 toggle (pas besoin de maintenir)
            if (msg == WM_KEYUP && wParam == VK_F1) {
                ui::SetMenuOpen(!ui::IsMenuOpen());
                return 0;
            }

            // Forward to ImGui when menu open
            if (ui::IsInitialized() && ui::IsMenuOpen()) {
                if (ui::OnWndProc((HWND__*)hWnd, msg, (std::uintptr_t)wParam, (std::intptr_t)lParam))
                    return 1;

                // Block game input while menu open (optionnel mais recommandé)
                switch (msg) {
                case WM_MOUSEMOVE:
                case WM_LBUTTONDOWN: case WM_LBUTTONUP:
                case WM_RBUTTONDOWN: case WM_RBUTTONUP:
                case WM_MBUTTONDOWN: case WM_MBUTTONUP:
                case WM_MOUSEWHEEL:
                case WM_KEYDOWN: case WM_KEYUP:
                case WM_CHAR:
                    return 1;
                }
            }

            return g_oldWndProc ? CallWindowProc(g_oldWndProc, hWnd, msg, wParam, lParam)
                : DefWindowProc(hWnd, msg, wParam, lParam);
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

            if (!ui::IsInitialized())
            {
                D3DDEVICE_CREATION_PARAMETERS cp{};
                if (SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow)
                {
                    g_hwnd = cp.hFocusWindow;

                    ui::Init((HWND__*)g_hwnd, device);

                    // Install WndProc once
                    if (!g_oldWndProc) {
                        g_oldWndProc = (WNDPROC)SetWindowLongPtr(g_hwnd, GWLP_WNDPROC, (LONG_PTR)WndProcHook);
                        std::cout << "[wndproc] hooked\n";
                    }

                    std::cout << "[ui] Init with HWND=" << g_hwnd << "\n";
                }
            }

            if (ui::IsInitialized())
                ui::RenderFrame();

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
