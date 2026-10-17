// tools/parity.cpp — compare two trajectory CSVs column by column (C++23).
//
//   tools/parity ours.csv theirs.csv [--tol 1e-3] [--time t]
//
// Both files: header "t,<species>,..." (the time column is the first one, any name).
// Columns are matched by header name; rows of `theirs` are linearly interpolated at
// each `t` of `ours`. Prints, per shared species: n points, max |rel dev|, RMS rel dev,
// and where the max occurs. Exit 1 if any max |rel dev| > tol. Values with |theirs| <
// 1e-300 are compared absolutely and flagged.
#include "csv.hpp"
#include <cmath>
#include <cstdio>
#include <iostream>

using namespace dk;

struct Traj { std::vector<std::string> names; std::vector<double> t; std::vector<std::vector<double>> cols; };

static Traj load(const std::string& p) {
    Table tb = read_csv(p); Traj tr; tr.names = tb.header;
    tr.cols.assign(tb.header.size(), {});
    for (auto& r : tb.rows) {
        if (r.size() < tb.header.size()) throw std::runtime_error(p + ": short row");
        tr.t.push_back(num(r[0], "t"));
        for (size_t c = 0; c < tb.header.size(); ++c) tr.cols[c].push_back(num(r[c], tb.header[c]));
    }
    for (size_t i = 1; i < tr.t.size(); ++i) if (tr.t[i] < tr.t[i - 1]) throw std::runtime_error(p + ": time not increasing at row " + std::to_string(i));
    return tr;
}

static double interp(const std::vector<double>& t, const std::vector<double>& y, double x, bool& inside) {
    inside = !(x < t.front() || x > t.back());
    if (x <= t.front()) return y.front(); if (x >= t.back()) return y.back();
    size_t hi = std::lower_bound(t.begin(), t.end(), x) - t.begin(); size_t lo = hi - 1;
    double w = (x - t[lo]) / (t[hi] - t[lo]); return y[lo] + w * (y[hi] - y[lo]);
}

int main(int argc, char** argv) try {
    if (argc < 3) { std::cerr << "usage: parity ours.csv theirs.csv [--tol 1e-3]\n"; return 2; }
    double tol = 1e-3;
    for (int i = 3; i + 1 < argc; ++i) if (std::string(argv[i]) == "--tol") tol = std::stod(argv[i + 1]);
    Traj a = load(argv[1]), b = load(argv[2]);
    std::printf("%-16s %6s %14s %14s %12s %s\n", "species", "n", "max|rel|", "rms rel", "at t", "note");
    bool fail = false; int shared = 0;
    for (size_t ca = 1; ca < a.names.size(); ++ca) {
        int cb = -1; for (size_t j = 1; j < b.names.size(); ++j) if (b.names[j] == a.names[ca]) cb = int(j);
        if (cb < 0) { std::printf("%-16s %6s %14s %14s %12s %s\n", a.names[ca].c_str(), "-", "-", "-", "-", "not in theirs"); continue; }
        ++shared; double mx = 0, at = 0, ss = 0; size_t n = 0; bool absflag = false, outside = false;
        for (size_t i = 0; i < a.t.size(); ++i) {
            bool in; double y = interp(b.t, b.cols[cb], a.t[i], in); if (!in) { outside = true; continue; }
            double x = a.cols[ca][i]; double d;
            if (std::fabs(y) < 1e-300) { d = std::fabs(x - y); absflag = true; } else d = std::fabs((x - y) / y);
            if (d > mx) { mx = d; at = a.t[i]; }
            ss += d * d; ++n;
        }
        double rms = n ? std::sqrt(ss / n) : 0;
        std::string note; if (absflag) note += "abs-compared where theirs=0; "; if (outside) note += "some t outside theirs; "; if (mx > tol) { note += "ABOVE TOL"; fail = true; }
        std::printf("%-16s %6zu %14.3e %14.3e %12.6g %s\n", a.names[ca].c_str(), n, mx, rms, at, note.c_str());
    }
    if (!shared) { std::cerr << "no shared species columns between the two files\n"; return 2; }
    std::printf("tolerance %.3g: %s\n", tol, fail ? "FAIL" : "PASS");
    return fail ? 1 : 0;
} catch (const std::exception& e) { std::cerr << "parity: " << e.what() << "\n"; return 1; }
