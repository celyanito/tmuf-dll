#include <Windows.h>
#include <cstdint>

#include "../../include/common.h"
#include "../../include/ui/imgui_layer.h"
#include "../../include/discord_presence.h"
#include "../../include/tm_mapname.h"

#include "../../external/imgui/imgui.h"
#include "../../external/imgui/backends/imgui_impl_win32.h"
#include "../../external/imgui/backends/imgui_impl_dx9.h"

// Some setups/defines may hide the declaration from the backend header.
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace ui {

    static Config g_cfg{};
    static HWND g_hwnd = nullptr;
    static IDirect3DDevice9* g_dev = nullptr;
    static bool g_inited = false;
    static bool g_menu_open = false;

    bool Init(HWND__* hwnd_, IDirect3DDevice9* device, const Config& cfg)
    {
        if (g_inited) return true;
        if (!hwnd_ || !device) return false;

        g_hwnd = (HWND)hwnd_;
        g_dev = device;
        g_cfg = cfg;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX9_Init(g_dev);

        g_inited = true;
        return true;
    }

    void RenderFrame()
    {
        if (!g_inited) return;

        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Tick map name (throttled)
        static ULONGLONG lastTick = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastTick >= 200) {
            tmu::TickMapName();
            lastTick = now;
        }

        const std::string mapRaw = tmu::GetLastMapName();
        const std::string mapClean = tmu::StripTmFormatting(mapRaw);

        // Discord RPC update (ALWAYS, even if overlay is closed)
        if (!mapClean.empty() && mapClean != "<unknown>")
            discord_presence::SetMap(mapClean);
        else
            discord_presence::SetMenu();

        if (g_menu_open)
        {
            ImGui::Begin("TMUF Overlay", &g_menu_open);

            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

            ImGui::Separator();
            ImGui::Text("Map (raw):   %s", mapRaw.empty() ? "<unknown>" : mapRaw.c_str());
            ImGui::Text("Map (clean): %s", mapClean.empty() ? "<unknown>" : mapClean.c_str());

            ImGui::End();
        }

        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }

    // Used by your WndProc hook
    LRESULT HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
    }

    // Legacy signature used by some of your older hook code (kept for compatibility)
    bool OnWndProc(HWND__* hwnd_, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam)
    {
        if (!g_inited) return false;
        return HandleWndProc((HWND)hwnd_, (UINT)msg, (WPARAM)wparam, (LPARAM)lparam) != 0;
    }

    void OnResetBegin()
    {
        if (!g_inited) return;
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }

    void OnResetEnd()
    {
        if (!g_inited) return;
        ImGui_ImplDX9_CreateDeviceObjects();
    }

    void Shutdown()
    {
        if (!g_inited) return;

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        g_dev = nullptr;
        g_hwnd = nullptr;
        g_menu_open = false;
        g_inited = false;
    }

    bool IsInitialized() { return g_inited; }
    bool IsMenuOpen() { return g_menu_open; }
    void SetMenuOpen(bool open) { g_menu_open = open; }

} // namespace ui
