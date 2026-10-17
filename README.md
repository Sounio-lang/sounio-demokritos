# sounio-demokritos

The intake funnel for running **your** data through [Sounio](https://github.com/Sounio-lang/sounio):
put a few CSV files in a folder, run one command, get a result with its uncertainty band
and a receipt (which binary, which inputs, which command, how long).

Three commands:

```sh
bin/doctor                         # is the compiler here, does hello run, which engines work
tools/csv2sio hydrogen/examples/brine_calcite_toy  # CSVs -> generated/brine_calcite_toy.sio
bin/run generated/brine_calcite_toy.sio --tag brine_calcite_toy   # run + receipt in runs/
```

Everything the run printed, plus `receipt.json`, lands in `runs/<timestamp>_<tag>/`.

## What goes in the folder

A model is three CSV files. Templates with comments are in `hydrogen/templates/`.

| file | columns | meaning |
|---|---|---|
| `species.csv` | `name,init,init_sigma,unit,note` | one row per species; initial amount and its 1σ |
| `reactions.csv` | `id,equation,k,k_sigma,unit,source` | mass-action reactions, `A + 2 B -> C`; rate constant and its 1σ |
| `run.csv` | `key,value` | `title`, `dt`, `steps`, `temperature_C`, free `note` rows |

Limits of the general epistemic engine used here (`chemistry::kinetics::simulate_general_epistemic`):
at most **8 species** and **8 reactions**, mass action with integer orders, fixed-step RK4.
Values are propagated exactly; the 1σ band is first-order (GUM, delta method, per step).
Bigger mechanisms (GRI-Mech size) use the dedicated modules in `stdlib/chemistry/gri30_*.sio`
and are not generated from CSV.

## Bringing PHREEQC input

```sh
tools/phreeqc2csv path/to/model.pqi hydrogen/intake/<name>
```

writes `solution.csv`, `phases.csv` (name, reaction, log_k, delta_h), `kinetics.csv`
(name, m0, parms, steps) and `rates.txt` (the BASIC rate bodies, verbatim). The rate
bodies are **not** translated automatically: a PHREEQC rate law is a program, not a constant.
Transcribe each into `reactions.csv` as mass-action steps, or keep it as a named slot.

## Comparing trajectories

```sh
tools/parity runs/<run>/trajectory.csv path/to/comsol_or_phreeqc_output.csv
```

Both files: header `t,<species>,…`; the tool aligns on time, prints max and RMS relative
deviation per species, and exits non-zero above a threshold (`--tol 1e-3`).

## Where things live

```
bin/            run, doctor
tools/          csv2sio, phreeqc2csv, parity (C++23, `make`)
hydrogen/       templates, examples, intake/ (your data, gitignored)
hpc/            same funnel for any kinetic / ODE network
ai/             ontology and knowledge-graph intake (EL+ closure)
llm/            LLM.md: running an open-weights model in Sounio, measured probes
generated/      .sio files written by csv2sio (regenerate, do not edit)
runs/           receipts (gitignored)
```

## Requirements

Linux x86-64, a checkout of `Sounio-lang/sounio` next to this repo (or `SOUNIO_ROOT` set),
`g++ ≥ 13` for the tools. `bin/doctor` tells you what is missing.
