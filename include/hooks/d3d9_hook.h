#pragma once
#include <cstdint>

struct IDirect3DDevice9;

namespace hooks
{
    namespace d3d9
    {
        // Installe le hook EndScene.
        // Retourne true si OK.
        bool Install();

        // Retire le hook (restaure le vtable ptr original).
        void Uninstall();

        bool IsInstalled();

        // (Optionnel) te donne le dernier device vu
        IDirect3DDevice9* GetLastDevice();
    }
}
