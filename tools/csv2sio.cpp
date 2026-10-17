// tools/csv2sio.cpp — CSV model folder -> generated Sounio program (C++23).
//
//   tools/csv2sio <folder> [--out generated/<name>.sio]
//
// <folder>/species.csv   name,init,init_sigma,unit,note
// <folder>/reactions.csv id,equation,k,k_sigma,unit,source      equation: "A + 2 B -> C + D"
// <folder>/run.csv       key,value   (title, dt, steps, temperature_C, note...)
//
// The generated program calls chemistry::kinetics::simulate_general_epistemic
// (mass action, integer orders, <= 8 species, <= 8 reactions, RK4 fixed step)
// and prints every species as value ± 1-sigma, then CSV2SIO_OK.
#include "csv.hpp"
#include <cstdio>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <sstream>

using namespace dk;
namespace fs = std::filesystem;

struct Term { int coef; std::string name; };

static std::vector<Term> parse_side(const std::string& side) {
    std::vector<Term> out; std::stringstream ss(side); std::string tok;
    std::vector<std::string> toks;
    while (ss >> tok) if (tok != "+") toks.push_back(tok);
    for (size_t i = 0; i < toks.size(); ++i) {
        int coef = 1; std::string name = toks[i];
        bool allDigits = !name.empty();
        for (char c : name) if (!isdigit((unsigned char)c)) { allDigits = false; break; }
        if (allDigits) {
            if (i + 1 >= toks.size()) throw std::runtime_error("dangling coefficient in '" + side + "'");
            coef = std::stoi(name); name = toks[++i];
        } else {
            size_t j = 0; while (j < name.size() && isdigit((unsigned char)name[j])) ++j;
            if (j > 0 && j < name.size()) { coef = std::stoi(name.substr(0, j)); name = name.substr(j); }
        }
        out.push_back({coef, name});
    }
    return out;
}

static std::string lit(double v) {
    char b[64]; std::snprintf(b, sizeof b, "%.12g", v);
    std::string s = b;
    if (s.find_first_of(".eE") == std::string::npos) s += ".0";
    if (s.find('e') != std::string::npos && s.find('.') == std::string::npos) {
        // 1e-05 -> 1.0e-05 so the lexer sees a float
        auto e = s.find('e'); s = s.substr(0, e) + ".0" + s.substr(e);
    }
    if (!s.empty() && s[0] == '-') return "0.0 - " + s.substr(1);
    return s;
}


int main(int argc, char** argv) try {
    if (argc < 2) { std::cerr << "usage: csv2sio <folder> [--out file.sio]\n"; return 2; }
    fs::path dir = argv[1]; std::string out;
    for (int i = 2; i + 1 < argc; ++i) if (std::string(argv[i]) == "--out") out = argv[i + 1];
    std::string name = dir.filename().string();
    if (name.empty()) name = dir.parent_path().filename().string();
    if (out.empty()) { fs::create_directories("generated"); out = "generated/" + name + ".sio"; }

    Table sp = read_csv((dir / "species.csv").string());
    Table rx = read_csv((dir / "reactions.csv").string());
    std::map<std::string, std::string> run;
    if (fs::exists(dir / "run.csv")) {
        Table rt = read_csv((dir / "run.csv").string());
        for (size_t r = 0; r < rt.rows.size(); ++r) run[rt.at(r, "key")] = rt.at(r, "value");
    }
    auto runv = [&](const std::string& k, const std::string& d) { auto it = run.find(k); return it == run.end() ? d : it->second; };

    const size_t NSP = sp.rows.size(), NRX = rx.rows.size();
    if (NSP == 0 || NSP > 8) throw std::runtime_error("species.csv must have 1..8 rows (engine limit), has " + std::to_string(NSP));
    if (NRX == 0 || NRX > 8) throw std::runtime_error("reactions.csv must have 1..8 rows (engine limit), has " + std::to_string(NRX));
    std::map<std::string, int> idx;
    for (size_t i = 0; i < NSP; ++i) {
        const std::string& n = sp.at(i, "name");
        if (idx.count(n)) throw std::runtime_error("duplicate species " + n);
        idx[n] = int(i);
    }
    // nu[species][reaction]
    std::vector<std::vector<int>> nu(NSP, std::vector<int>(NRX, 0));
    std::vector<std::string> eq(NRX);
    for (size_t r = 0; r < NRX; ++r) {
        std::string e = rx.at(r, "equation"); eq[r] = e;
        size_t arrow = e.find("->"); if (arrow == std::string::npos) arrow = e.find("=>");
        if (arrow == std::string::npos) throw std::runtime_error("reaction " + rx.at(r, "id") + ": no '->' in '" + e + "'");
        auto L = parse_side(e.substr(0, arrow)), R = parse_side(e.substr(arrow + 2));
        for (auto& t : L) { if (!idx.count(t.name)) throw std::runtime_error("reaction " + rx.at(r, "id") + ": unknown species '" + t.name + "' (add it to species.csv)"); nu[idx[t.name]][r] -= t.coef; }
        for (auto& t : R) { if (!idx.count(t.name)) throw std::runtime_error("reaction " + rx.at(r, "id") + ": unknown species '" + t.name + "' (add it to species.csv)"); nu[idx[t.name]][r] += t.coef; }
        int order = 0; for (auto& t : L) order += t.coef;
        if (order > 4) throw std::runtime_error("reaction " + rx.at(r, "id") + ": total order " + std::to_string(order) + " > 4 (engine fast-path bound)");
    }

    double dt = num(runv("dt", "0.01"), "dt");
    long steps = std::lround(num(runv("steps", "1000"), "steps"));
    std::string title = runv("title", name);

    std::ostringstream o;
    o << "// " << out << " — GENERATED by tools/csv2sio from " << dir.string() << "\n";
    o << "// Edit the CSV files and regenerate; do not edit this file.\n";
    o << "// Engine: chemistry::kinetics::simulate_general_epistemic (mass action, integer orders,\n";
    o << "// <= 8 species / 8 reactions, RK4 dt=" << dt << " x " << steps << " steps; 1-sigma by per-step delta method).\n\n";
    o << "use chemistry::kinetics::*\n\n";
    o << R"SIO(// --- byte-stable number printer (the stdlib import shadows the builtin print_f64) ---
fn dk_pd(d: i64) with IO {
    if d == 0 { print("0") }
    if d == 1 { print("1") }
    if d == 2 { print("2") }
    if d == 3 { print("3") }
    if d == 4 { print("4") }
    if d == 5 { print("5") }
    if d == 6 { print("6") }
    if d == 7 { print("7") }
    if d == 8 { print("8") }
    if d == 9 { print("9") }
}
fn dk_floor_i64(x: f64) -> i64 with Mut {
    var n = x as i64
    if (n as f64) > x { n = n - 1 }
    return n
}
// prints x as d.dddddde[+-]dd ; NaN/inf refused with a marker
fn dk_pf(x: f64) with IO, Mut, Div {
    var v = x
    if !(v == v) { print("NaN"); return }
    if v < 0.0 { print("-"); v = 0.0 - v }
    if v > 1.0e300 { print("INF"); return }
    if v == 0.0 { print("0.000000e+00"); return }
    var e: i64 = 0
    while v >= 10.0 { v = v / 10.0; e = e + 1 }
    while v < 1.0 { v = v * 10.0; e = e - 1 }
    var n = dk_floor_i64(v * 1000000.0 + 0.5)
    if n >= 10000000 { n = n / 10; e = e + 1 }
    dk_pd(n / 1000000)
    print(".")
    var fp = n % 1000000
    var fdiv: i64 = 100000
    while fdiv > 0 { dk_pd((fp / fdiv) % 10); fdiv = fdiv / 10 }
    print("e")
    if e < 0 { print("-"); e = 0 - e } else { print("+") }
    if e >= 100 { dk_pd(e / 100) }
    dk_pd((e / 10) % 10)
    dk_pd(e % 10)
}

)SIO";
    o << "fn main() -> i32 with IO, Mut, Div, Panic {\n";
    o << "    println(\"=== " << title << " ===\")\n";
    for (auto& [k, v] : run) if (k != "title" && k != "dt" && k != "steps") o << "    println(\"    " << k << ": " << v << "\")\n";
    o << "    println(\"    species " << NSP << ", reactions " << NRX << ", dt " << dt << ", steps " << steps << "\")\n";
    for (size_t r = 0; r < NRX; ++r)
        o << "    println(\"    " << rx.at(r, "id") << "  " << eq[r] << "   k = " << rx.at(r, "k") << " ± " << rx.at(r, "k_sigma") << " " << rx.at(r, "unit") << "  [" << rx.at(r, "source") << "]\")\n";
    o << "    println(\"\")\n";
    auto arr = [&](const std::string& nm, auto get) {
        o << "    let " << nm << ": [f64; 8] = [";
        for (size_t i = 0; i < 8; ++i) o << (i ? ", " : "") << lit(get(i));
        o << "]\n";
    };
    arr("iv",  [&](size_t i) { return i < NSP ? num(sp.at(i, "init"), "init of " + sp.at(i, "name")) : 0.0; });
    arr("isv", [&](size_t i) { return i < NSP ? num(sp.at(i, "init_sigma"), "init_sigma of " + sp.at(i, "name")) : 0.0; });
    arr("ks",  [&](size_t i) { return i < NRX ? num(rx.at(i, "k"), "k of " + rx.at(i, "id")) : 0.0; });
    arr("uks", [&](size_t i) { return i < NRX ? num(rx.at(i, "k_sigma"), "k_sigma of " + rx.at(i, "id")) : 0.0; });
    o << "    var nu = matnm_new(" << NSP << " as i64, " << NRX << " as i64)\n";
    for (size_t s = 0; s < NSP; ++s) for (size_t r = 0; r < NRX; ++r) if (nu[s][r] != 0)
        o << "    nu = matnm_set(nu, " << s << " as i64, " << r << " as i64, " << lit(nu[s][r]) << ")   // " << sp.at(s, "name") << " in " << rx.at(r, "id") << "\n";
    o << "    let (y, u) = simulate_general_epistemic(&iv, &isv, &ks, &uks, nu, " << NSP << " as i64, " << NRX << " as i64, " << lit(dt) << ", " << steps << " as i64)\n";
    o << "    println(\"  final state after " << lit(dt * steps) << " (dt units), value ± 1 sigma:\")\n";
    for (size_t s = 0; s < NSP; ++s) {
        o << "    print(\"    " << sp.at(s, "name") << " = \"); dk_pf(y[" << s << "]); print(\" ± \"); dk_pf(u[" << s << "]); println(\" " << sp.at(s, "unit") << "\")\n";
    }
    o << "    println(\"\")\n    println(\"  initial:\")\n";
    for (size_t s = 0; s < NSP; ++s)
        o << "    print(\"    " << sp.at(s, "name") << " = \"); dk_pf(iv[" << s << "]); print(\" ± \"); dk_pf(isv[" << s << "]); println(\" " << sp.at(s, "unit") << "\")\n";
    // trajectory: re-run from the initial state with k*steps/snapshots steps and print one CSV
    // line per snapshot (the engine returns final state only; each result is printed before
    // the next call, which the lean_single engine requires).
    long snaps = std::lround(num(runv("snapshots", "10"), "snapshots"));
    if (snaps > 0) {
        o << "    println(\"\")\n    print(\"TRAJ,t\")\n";
        for (size_t s = 0; s < NSP; ++s) o << "    print(\"," << sp.at(s, "name") << "\")\n";
        o << "    println(\"\")\n";
        o << "    print(\"TRAJ,0\")\n";
        for (size_t s = 0; s < NSP; ++s) o << "    print(\",\"); dk_pf(iv[" << s << "])\n";
        o << "    println(\"\")\n";
        o << "    var k: i64 = 1\n    while k <= " << snaps << " {\n";
        o << "        let n = (" << steps << " * k) / " << snaps << "\n";
        o << "        let (yk, uk) = simulate_general_epistemic(&iv, &isv, &ks, &uks, nu, " << NSP << " as i64, " << NRX << " as i64, " << lit(dt) << ", n)\n";
        o << "        print(\"TRAJ,\"); dk_pf(" << lit(dt) << " * (n as f64))\n";
        for (size_t s = 0; s < NSP; ++s) o << "        print(\",\"); dk_pf(yk[" << s << "])\n";
        o << "        println(\"\")\n        k = k + 1\n    }\n";
    }
    o << "    println(\"\")\n    println(\"CSV2SIO_OK\")\n    0\n}\n";

    std::ofstream f(out); f << o.str();
    std::cout << "wrote " << out << "  (" << NSP << " species, " << NRX << " reactions, dt " << dt << " x " << steps << ")\n";
    return 0;
} catch (const std::exception& e) {
    std::cerr << "csv2sio: " << e.what() << "\n"; return 1;
}
