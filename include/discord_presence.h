#pragma once
#include <string>

namespace discord_presence
{
    bool Init();                 // safe: returns false if SDK missing
    void Shutdown();
    void Tick();                 // run callbacks (can be called each frame)
    void SetMap(const std::string& mapName); // updates only when changed
    void Clear();
    void SetMenu();
}
