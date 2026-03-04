#pragma once
#include <string>

namespace tmu {
    // Renvoie la dernière valeur connue (UTF-8). Jamais de crash.
    std::string GetLastMapName();
    std::string StripTmFormatting(const std::string& s);
    // Met à jour (à appeler dans un thread ou tick). Retourne true si changé.
    bool TickMapName();
}
