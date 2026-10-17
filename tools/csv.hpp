// tools/csv.hpp — tiny CSV reader shared by the intake tools (C++23).
#pragma once
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dk {

struct Table {
    std::vector<std::string> header;
    std::vector<std::vector<std::string>> rows;
    int col(std::string_view name) const {
        for (size_t i = 0; i < header.size(); ++i) if (header[i] == name) return int(i);
        return -1;
    }
    const std::string& at(size_t r, std::string_view name) const {
        int c = col(name);
        if (c < 0) throw std::runtime_error("missing column: " + std::string(name));
        static const std::string empty;
        return size_t(c) < rows[r].size() ? rows[r][c] : empty;
    }
};

inline std::string trim(std::string s) {
    auto ws = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    while (!s.empty() && ws(s.back())) s.pop_back();
    size_t i = 0; while (i < s.size() && ws(s[i])) ++i;
    return s.substr(i);
}

inline std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> out; std::string cur; bool q = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (q) {
            if (c == '"') { if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; } else q = false; }
            else cur += c;
        } else if (c == '"') q = true;
        else if (c == ',') { out.push_back(trim(cur)); cur.clear(); }
        else cur += c;
    }
    out.push_back(trim(cur));
    return out;
}

inline Table read_csv(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    Table t; std::string line; bool first = true;
    while (std::getline(in, line)) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#') continue;
        auto cells = split_csv_line(s);
        if (first) { t.header = cells; first = false; }
        else t.rows.push_back(cells);
    }
    if (first) throw std::runtime_error("empty csv: " + path);
    return t;
}

inline double num(const std::string& s, const std::string& what) {
    if (s.empty()) throw std::runtime_error("empty number for " + what);
    size_t n = 0; double v = std::stod(s, &n);
    if (n != s.size()) throw std::runtime_error("bad number '" + s + "' for " + what);
    return v;
}

} // namespace dk
