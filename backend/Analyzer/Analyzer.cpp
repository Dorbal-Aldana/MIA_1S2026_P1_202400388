#include "Analyzer.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace Analyzer {

static void replaceAll(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.length(), to);
        pos += to.length();
    }
}

static std::string normalizePdfQuotes(std::string s) {
    replaceAll(s, u8"“", "\"");
    replaceAll(s, u8"”", "\"");
    replaceAll(s, u8"‘", "'");
    replaceAll(s, u8"’", "'");
    return s;
}

static std::string trim(std::string str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    std::string result = str.substr(first, last - first + 1);
    while (!result.empty() && result.back() == '\r') result.pop_back();
    return result;
}

static void stripCarriageReturns(std::string& s) {
    for (size_t i = 0; i < s.length();)
        if (s[i] == '\r') s.erase(i, 1);
        else ++i;
}

std::string ParsedCommand::getParam(const std::string& key, const std::string& defaultValue) const {
    auto it = params.find(key);
    return (it != params.end()) ? it->second : defaultValue;
}

ParsedCommand parseLine(const std::string& input) {
    ParsedCommand cmd;
    std::string line = trim(normalizePdfQuotes(input));
    stripCarriageReturns(line);
    line = trim(line);

    if (line.empty() || line[0] == '#') {
        cmd.name = "comment";
        return cmd;
    }

    std::istringstream iss(line);
    std::string command;
    if (!(iss >> command)) {
        cmd.name = "comment";
        return cmd;
    }
    std::transform(command.begin(), command.end(), command.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Misma idea que CLASE7: -param="valor" o -param=valor
    // (El '-' del PDF a veces llega como guion Unicode.)
    static const std::regex re(R"((?:^|\s)[-\u2010-\u2015\u2212](\w+)=("[^"]+"|\S+))");
    std::sregex_iterator it(line.begin(), line.end(), re);
    std::sregex_iterator end;

    for (; it != end; ++it) {
        std::string key = it->str(1);
        std::string value = it->str(2);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);
        std::transform(key.begin(), key.end(), key.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        cmd.params[key] = value;
    }

    // Flags sin = : mkdir -p -path=... / mkfile -r -path=...
    static const std::regex flagRe(R"((?:^|\s)[-\u2010-\u2015\u2212]([pPrR])(?=\s|$))");
    for (std::sregex_iterator fit(line.begin(), line.end(), flagRe), fend; fit != fend; ++fit) {
        std::string fk = fit->str(1);
        std::transform(fk.begin(), fk.end(), fk.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        cmd.params[fk] = "1";
    }

    cmd.name = std::move(command);
    return cmd;
}

}  // namespace Analyzer
