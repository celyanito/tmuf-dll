struct IDirect3DDevice9;
struct HWND__;

namespace ui {
    struct Config { int toggle_vk = 0x70; }; // F1 Key

    bool Init(HWND__* hwnd, IDirect3DDevice9* device, const Config& cfg = {});
    void RenderFrame();            // appelle NewFrame + ton UI + Render
    void OnResetBegin();           // InvalidateDeviceObjects
    void OnResetEnd();             // CreateDeviceObjects
    void Shutdown();

    bool IsInitialized();
    bool IsMenuOpen();
    void SetMenuOpen(bool open);

    bool OnWndProc(HWND__* hwnd, unsigned msg, std::uintptr_t wparam, std::intptr_t lparam);
}
