#include "../../include/common.h"
#include "../../include/ui/imgui_layer.h"

namespace ui
{
    static Config g_cfg{};
    static HWND__* g_hwnd = nullptr;
    static bool g_inited = false;
    static bool g_menu_open = false;

    bool Init(HWND__* hwnd, const Config& cfg)
    {
        if (g_inited) return true;
        if (!hwnd) return false;

        g_hwnd = hwnd;
        g_cfg = cfg;

        // Plus tard ici :
        // - ImGui::CreateContext()
        // - ImGui_ImplWin32_Init(hwnd)
        // - ImGui_ImplDX9_Init(device)

        g_inited = true;
        return true;
    }

    void RenderFrame()
    {
        if (!g_inited) return;

        // Toggle (simple) — tu peux aussi le gérer ailleurs si tu préfères
        if (g_cfg.toggle_vk != 0 && (GetAsyncKeyState(g_cfg.toggle_vk) & 1))
            g_menu_open = !g_menu_open;

        // Plus tard ici :
        // - ImGui_ImplDX9_NewFrame()
        // - ImGui_ImplWin32_NewFrame()
        // - ImGui::NewFrame()
        // - if (g_menu_open) { ImGui::Begin(...); ...; ImGui::End(); }
        // - ImGui::EndFrame(); ImGui::Render();
        // - ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    void Shutdown()
    {
        if (!g_inited) return;

        // Plus tard ici :
        // - ImGui_ImplDX9_Shutdown()
        // - ImGui_ImplWin32_Shutdown()
        // - ImGui::DestroyContext()

        g_hwnd = nullptr;
        g_menu_open = false;
        g_inited = false;
    }

    bool IsInitialized() { return g_inited; }
    bool IsMenuOpen() { return g_menu_open; }
    void SetMenuOpen(bool open) { g_menu_open = open; }

    bool OnWndProc(HWND__* /*hwnd*/, unsigned /*msg*/, std::uintptr_t /*wparam*/, std::intptr_t /*lparam*/)
    {
        if (!g_inited) return false;

        // Plus tard :
        // return ImGui_ImplWin32_WndProcHandler((HWND)hwnd, msg, wparam, lparam) != 0;

        return false;
    }
}
