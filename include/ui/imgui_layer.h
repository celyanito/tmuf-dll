#pragma once
#include <cstdint>   // <-- indispensable pour std::uintptr_t / std::intptr_t

struct IDirect3DDevice9;
struct HWND__;

namespace ui {
    struct Config { int toggle_vk = 0x70; }; // F1

    bool Init(HWND__* hwnd, IDirect3DDevice9* device, const Config& cfg = {});
    void RenderFrame();
    void OnResetBegin();
    void OnResetEnd();
    void Shutdown();

    bool IsInitialized();
    bool IsMenuOpen();
    void SetMenuOpen(bool open);

    bool OnWndProc(HWND__* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
    LRESULT HandleWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
}
