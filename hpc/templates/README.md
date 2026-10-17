# hpc — any kinetic / ODE network as CSV

Same three files as `hydrogen/templates/` (`species.csv`, `reactions.csv`, `run.csv`), same tool:

```sh
tools/csv2sio hpc/examples/consecutive_abc
bin/run generated/consecutive_abc.sio --tag consecutive_abc
```

Limits: 8 species, 8 reactions, mass action, integer orders, fixed-step RK4, first-order band.
For a full mechanism (tens of species) use `stdlib/chemistry/gri30_full.sio` / `gri30_h2.sio`
directly: `examples/chemistry/h2_ignition_uq_demo.sio` is the worked example (Cantera parity in
`benchmarks/chemistry/`). Arrhenius k(T) with uncertainty: `chemistry::kinetics::arrhenius(A, uA, Ea, uEa, T, uT)`.

**Stiff or fast kinetics (combustion-scale k, k·c·dt not small):** the general engine propagates the
band per step with a Jacobian that carries no dt (see E1–E4 in the header of
`demos/hydrogen/uhs_brine_calcite.sio`), so the variance overflows and the band prints `NaN` while the
values stay exact. That is the engine saying "not this way": use the dedicated mechanism modules
(`gri30_h2.sio`, `gri30_full.sio`) whose band is step-size invariant.
