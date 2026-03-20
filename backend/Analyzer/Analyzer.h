#pragma once

#include <cctype>
#include <map>
#include <string>

namespace Analyzer {

/// Resultado del análisis de una línea (estilo CLASE7: comando + mapa de parámetros).
struct ParsedCommand {
    std::string name;
    std::map<std::string, std::string> params;

    bool hasParam(const std::string& key) const {
        return params.find(key) != params.end();
    }
    /// Flag sin valor: -p, -r (calificación / lab)
    bool hasFlag(char c) const {
        std::string k(1, static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        auto it = params.find(k);
        return it != params.end() && (it->second == "1" || it->second.empty());
    }
    std::string getParam(const std::string& key, const std::string& defaultValue = "") const;
    bool isComment() const { return name == "comment"; }
};

/// Parsea una línea: nombre en minúsculas y parámetros `-clave=valor` vía regex (como CLASE7).
ParsedCommand parseLine(const std::string& input);

}  // namespace Analyzer
