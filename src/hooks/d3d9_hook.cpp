#include "../../include/common.h"
#include "../../include/hooks/d3d9_hook.h"
#include "../../include/ui/imgui_layer.h"
#include <d3d9.h>

// MinHook
#include "../../external/minhook/include/MinHook.h"  // si tu as mis le include dir, sinon mets un chemin relatif vers MinHook.h

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

        static bool g_installed = false;

        static void* g_pDirect3DCreate9 = nullptr; // adresse réelle exportée
        static Direct3DCreate9_t g_origDirect3DCreate9 = nullptr;
        static CreateDevice_t    g_origCreateDevice = nullptr;
        static EndScene_t        g_origEndScene = nullptr;

        static IDirect3DDevice9* g_lastDevice = nullptr;

        // vtable indices
        static constexpr int VTABLE_INDEX_IDirect3D9_CreateDevice = 16; // IDirect3D9::CreateDevice
        static constexpr int VTABLE_INDEX_IDirect3DDevice9_EndScene = 42; // IDirect3DDevice9::EndScene

        static HRESULT __stdcall hkEndScene(IDirect3DDevice9* device)
        {
            g_lastDevice = device;

            static bool inited = false;
            static HWND hwnd = nullptr;

            if (!inited)
            {
                D3DDEVICE_CREATION_PARAMETERS cp{};
                if (SUCCEEDED(device->GetCreationParameters(&cp)) && cp.hFocusWindow)
                {
                    hwnd = cp.hFocusWindow;

                    // init UI une seule fois
                    ui::Init(reinterpret_cast<HWND__*>(hwnd)); // ou ui::Init((HWND__*)hwnd) selon ton header
                    inited = true;

                    std::cout << "[ui] Init with HWND=" << hwnd << "\n";
                }
            }

            // rendu UI par frame (même si pour l'instant c'est vide)
            if (inited)
            {
                ui::RenderFrame();
            }

            return g_origEndScene(device);
        }

        // Hook sur IDirect3D9::CreateDevice (on récupère le vrai device du jeu)
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

                // Hook EndScene sur la vtable du vrai device (1 seule fois)
                if (!g_origEndScene)
                {
                    void** vtbl = *reinterpret_cast<void***>(dev);
                    void* pEndScene = vtbl[VTABLE_INDEX_IDirect3DDevice9_EndScene];

                    std::cout << "[d3d9] Hooking EndScene: orig=" << pEndScene
                        << " new=" << (void*)&hkEndScene << "\n";

                    // Hook via MinHook sur l'adresse de la fonction (pas sur le slot vtable)
                    if (MH_CreateHook(pEndScene, &hkEndScene, reinterpret_cast<void**>(&g_origEndScene)) == MH_OK &&
                        MH_EnableHook(pEndScene) == MH_OK)
                    {
                        std::cout << "[d3d9] EndScene hook enabled\n";
                    }
                    else
                    {
                        std::cout << "[d3d9] EndScene hook FAILED\n";
                    }
                }
            }

            return hr;
        }

        // Hook de Direct3DCreate9 : on récupère l'instance IDirect3D9 et on hook sa méthode CreateDevice
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

        bool Install()
        {
            if (g_installed)
                return true;

            std::cout << "[d3d9] Install (MinHook)\n";

            if (MH_Initialize() != MH_OK)
            {
                std::cout << "[d3d9] MH_Initialize failed\n";
                return false;
            }

            HMODULE hD3D9 = GetModuleHandleA("d3d9.dll");
            if (!hD3D9)
                hD3D9 = LoadLibraryA("d3d9.dll");

            if (!hD3D9)
            {
                std::cout << "[d3d9] LoadLibrary d3d9.dll failed\n";
                return false;
            }

            g_pDirect3DCreate9 = (void*)GetProcAddress(hD3D9, "Direct3DCreate9");
            if (!g_pDirect3DCreate9)
            {
                std::cout << "[d3d9] GetProcAddress Direct3DCreate9 failed\n";
                return false;
            }

            if (MH_CreateHook(
                g_pDirect3DCreate9,
                (void*)&hkDirect3DCreate9,
                (void**)&g_origDirect3DCreate9) != MH_OK)
            {
                std::cout << "[d3d9] MH_CreateHook Direct3DCreate9 failed\n";
                return false;
            }

            auto st = MH_EnableHook(g_pDirect3DCreate9);
            if (st != MH_OK)
            {
                std::cout << "[d3d9] MH_EnableHook Direct3DCreate9 failed, code=" << (int)st << "\n";
                return false;
            }

            std::cout << "[d3d9] Direct3DCreate9 hook enabled\n";

            g_installed = true;
            return true;
        }

        void Uninstall()
        {
            if (!g_installed) return;

            std::cout << "[d3d9] Uninstall (MinHook)\n";

            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();

            g_installed = false;
            g_origCreateDevice = nullptr;
            g_origEndScene = nullptr;
            g_lastDevice = nullptr;
        }

        bool IsInstalled() { return g_installed; }
        IDirect3DDevice9* GetLastDevice() { return g_lastDevice; }
    }
}
