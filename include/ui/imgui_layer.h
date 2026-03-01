#pragma once
#include <cstdint>

// Forward decl Win32 sans Windows.h pour éviter d'en mettre partout
struct HWND__;

// Layer UI (ImGui plus tard)
namespace ui
{
    struct Config
    {
        // Toggle menu (par défaut Insert)
        int toggle_vk = 0x2D; // VK_INSERT
    };

    // Appelée une fois quand tu as un HWND valide (et plus tard un device D3D9)
    bool Init(HWND__* hwnd, const Config& cfg = {});

    // Appelée à chaque frame (quand tu auras hook Present/EndScene)
    void RenderFrame();

    // Libération
    void Shutdown();

    // État
    bool IsInitialized();
    bool IsMenuOpen();
    void SetMenuOpen(bool open);

    // Input (optionnel) : à appeler depuis un hook WndProc plus tard
    // Retourne true si l'UI consomme le message.
    bool OnWndProc(HWND__* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
}
