// tools/phreeqc2csv.cpp — pull the data out of a PHREEQC input file (C++23).
//
//   tools/phreeqc2csv model.pqi <outdir>
//
// Writes, into <outdir>:
//   solution.csv   block,key,value,unit     (SOLUTION blocks: temp, pH, pe, units, element lines)
//   phases.csv     name,reaction,log_k,delta_h,delta_h_unit,analytic   (PHASES blocks)
//   kinetics.csv   block,name,formula,m0,m,parms,steps,tol   (KINETICS blocks)
//   equilibrium.csv block,phase,si,amount                    (EQUILIBRIUM_PHASES blocks)
//   rates.txt      every RATES body verbatim, one per name (BASIC is not translated)
//   notes.txt      what was skipped and why
// Nothing is interpreted: this is a faithful extraction so a human (or csv2sio) can
// transcribe the constants into species.csv / reactions.csv with provenance intact.
#include "csv.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

using namespace dk;
namespace fs = std::filesystem;

static std::string upper(std::string s) { for (auto& c : s) c = toupper((unsigned char)c); return s; }
static std::string q(const std::string& s) {
    std::string o = "\""; for (char c : s) { if (c == '"') o += '"'; o += c; } return o + "\"";
}
static bool starts_block(const std::string& u) {
    static const char* kw[] = {"SOLUTION","PHASES","RATES","KINETICS","EQUILIBRIUM_PHASES","REACTION","SELECTED_OUTPUT","USER_PUNCH","END","TITLE","PRINT","KNOBS","SOLUTION_SPECIES","SOLUTION_MASTER_SPECIES","GAS_PHASE","EXCHANGE","SURFACE","TRANSPORT","REACTION_TEMPERATURE","INCREMENTAL_REACTIONS","USE","SAVE","MIX","DATABASE","CALCULATE_VALUES","USER_PRINT","USER_GRAPH","SOLID_SOLUTIONS","COPY","DUMP","INVERSE_MODELING","ADVECTION","RUN_CELLS"};
    for (auto k : kw) { std::string s = k; if (u.rfind(s, 0) == 0 && (u.size() == s.size() || !isalpha((unsigned char)u[s.size()]))) return true; }
    return false;
}

int main(int argc, char** argv) try {
    if (argc < 3) { std::cerr << "usage: phreeqc2csv model.pqi <outdir>\n"; return 2; }
    std::ifstream in(argv[1]); if (!in) throw std::runtime_error(std::string("cannot open ") + argv[1]);
    fs::path out = argv[2]; fs::create_directories(out);
    std::ofstream sol(out / "solution.csv"), ph(out / "phases.csv"), kin(out / "kinetics.csv"), eqp(out / "equilibrium.csv"), rates(out / "rates.txt"), notes(out / "notes.txt");
    sol << "block,key,value,unit\n"; ph << "name,reaction,log_k,delta_h,delta_h_unit,analytic\n";
    kin << "block,name,formula,m0,m,parms,steps,tol\n"; eqp << "block,phase,si,amount\n";

    std::string line, block, blockName; int nsol = 0, nph = 0, nkin = 0, neq = 0, nrates = 0;
    // phases state
    std::string pname, preact, plogk, pdh, pdhu, panal;
    auto flush_phase = [&]() { if (!pname.empty()) { ph << q(pname) << "," << q(preact) << "," << plogk << "," << pdh << "," << pdhu << "," << q(panal) << "\n"; ++nph; } pname.clear(); preact.clear(); plogk.clear(); pdh.clear(); pdhu.clear(); panal.clear(); };
    // kinetics state
    std::string kname, kformula, km0, km, kparms, ksteps, ktol;
    auto flush_kin = [&]() { if (!kname.empty()) { kin << q(blockName) << "," << q(kname) << "," << q(kformula) << "," << km0 << "," << km << "," << q(kparms) << "," << q(ksteps) << "," << ktol << "\n"; ++nkin; } kname.clear(); kformula.clear(); km0.clear(); km.clear(); kparms.clear(); ksteps.clear(); ktol.clear(); };
    std::string rname;

    while (std::getline(in, line)) {
        std::string raw = line;
        auto hash = raw.find('#'); if (hash != std::string::npos) raw = raw.substr(0, hash);
        std::string s = trim(raw); if (s.empty()) continue;
        std::string u = upper(s);
        if (starts_block(u)) {
            if (block == "PHASES") flush_phase();
            if (block == "KINETICS") flush_kin();
            std::stringstream ss(s); ss >> block; block = upper(block);
            std::getline(ss, blockName); blockName = trim(blockName);
            if (block == "RATES") { rname.clear(); }
            continue;
        }
        if (block == "SOLUTION") {
            std::stringstream ss(s); std::string key, val, unit; ss >> key >> val; std::getline(ss, unit); unit = trim(unit);
            if (key.size() && key[0] == '-') key = key.substr(1);
            sol << q(blockName) << "," << q(key) << "," << q(val) << "," << q(unit) << "\n"; ++nsol;
        } else if (block == "PHASES") {
            std::string first; { std::stringstream fs0(s); fs0 >> first; first = upper(first); if (!first.empty() && first[0] == '-') first = first.substr(1); }
            static const char* popts[] = {"LOG_K","LOGK","DELTA_H","DELTAH","ANALYTIC","ANALYTICAL_EXPRESSION","VM","T_C","P_C","OMEGA","NO_CHECK","ADD_LOGK","ADD_CONSTANT","GAMMA","DW","MILLERO","NO_"};
            bool isopt = false; for (auto o2 : popts) if (first.rfind(o2, 0) == 0) { isopt = true; break; }
            if (isopt) {
                std::stringstream ss(s); std::string key; ss >> key; key = upper(key[0] == '-' ? key.substr(1) : key);
                std::string rest; std::getline(ss, rest); rest = trim(rest);
                if (key.rfind("LOG_K", 0) == 0) plogk = rest;
                else if (key.rfind("DELTA_H", 0) == 0) { std::stringstream rs(rest); rs >> pdh; std::getline(rs, pdhu); pdhu = trim(pdhu); }
                else if (key.rfind("ANALYTIC", 0) == 0) panal = rest;
                else notes << "PHASES " << pname << ": skipped option " << s << "\n";
            } else if (s.find('=') != std::string::npos) { preact = s; }
            else { flush_phase(); pname = s; }
        } else if (block == "KINETICS") {
            if (s[0] == '-') {
                std::stringstream ss(s); std::string key; ss >> key; key = upper(key.substr(1));
                std::string rest; std::getline(ss, rest); rest = trim(rest);
                if (key.rfind("FORMULA", 0) == 0) kformula = rest;
                else if (key.rfind("M0", 0) == 0) km0 = rest;
                else if (key == "M") km = rest;
                else if (key.rfind("PARM", 0) == 0) kparms = rest;
                else if (key.rfind("STEP", 0) == 0) ksteps = rest;
                else if (key.rfind("TOL", 0) == 0) ktol = rest;
                else if (key.rfind("CVODE", 0) == 0 || key.rfind("RUNGE", 0) == 0 || key.rfind("BAD", 0) == 0) notes << "KINETICS " << kname << ": solver option kept out: " << s << "\n";
                else notes << "KINETICS " << kname << ": skipped option " << s << "\n";
            } else { flush_kin(); kname = s; }
        } else if (block == "EQUILIBRIUM_PHASES") {
            std::stringstream ss(s); std::string phase, si, amount; ss >> phase >> si >> amount;
            eqp << q(blockName) << "," << q(phase) << "," << si << "," << amount << "\n"; ++neq;
        } else if (block == "RATES") {
            if (upper(s).rfind("-START", 0) == 0) { rates << "=== RATES " << rname << " ===\n"; ++nrates; }
            else if (upper(s).rfind("-END", 0) == 0) { rates << "=== end " << rname << " ===\n\n"; }
            else if (s[0] != '-' && !isdigit((unsigned char)s[0])) { rname = s; }
            else rates << line << "\n";
        } else if (block == "END" || block.empty()) {
        } else {
            notes << block << " " << blockName << ": line not extracted: " << s << "\n";
        }
    }
    if (block == "PHASES") flush_phase();
    if (block == "KINETICS") flush_kin();
    std::cout << "solution rows " << nsol << ", phases " << nph << ", kinetics " << nkin << ", equilibrium phases " << neq << ", rate bodies " << nrates << " -> " << out.string() << "\n";
    std::cout << "rates.txt holds the BASIC bodies verbatim; transcribe each rate law into reactions.csv by hand, with its source.\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "phreeqc2csv: " << e.what() << "\n"; return 1; }
