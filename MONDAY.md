# Segunda-feira no Demokritos — checklist

Máquina: o workspace Linux x86-64 (`sounio-workspace-ssh`), com `/workspace/sounio` ao lado de `/workspace/sounio-demokritos`.
Eles mandam arquivos; a gente roda e devolve `runs/<carimbo>_<tag>/` (saída limpa + `receipt.json` + `trajectory.csv`).

## Antes de sair de casa
- [ ] `bin/doctor` → tudo `[ok]` (hello roda nos dois engines)
- [ ] `make` → três ferramentas compiladas
- [ ] `tools/csv2sio hydrogen/examples/brine_calcite_toy && bin/run generated/brine_calcite_toy.sio` → `CSV2SIO_OK`
- [ ] `tools/csv2sio hpc/examples/consecutive_abc && bin/run generated/consecutive_abc.sio` → `CSV2SIO_OK`
- [ ] `git -C ../sounio status` limpo; `demos/intro/demo_uncertainty.sio` commitado

## Hidrogênio (Manos)
1. Receber: `.pqi/.dat` do PHREEQC, tabela de constantes (CSV/Excel), saídas COMSOL (CSV), e — se existirem — C_a, C_d, E_a, E_d, σ, Y das eqs. 9–11 do Gkanas 2020.
2. `tools/phreeqc2csv modelo.pqi hydrogen/intake/<nome>` → ler `rates.txt` com ele e transcrever cada lei de taxa em `reactions.csv` (fonte em cada linha).
3. `tools/csv2sio hydrogen/intake/<nome>` → `bin/run generated/<nome>.sio --tag <nome>`.
4. Paridade: exportar a trajetória deles como `t,<espécies>` e `tools/parity runs/<run>/trajectory.csv deles.csv --tol 1e-3`.
5. Se o modelo passa de 8 espécies/8 reações, ou tem especiação de equilíbrio: anotar e usar `demos/hydrogen/uhs_brine_calcite.sio` como base (slots S1–S4 no cabeçalho).
6. Compressor: `demos/hydrogen/mh7_coupled_ceiling.sio`, `examples/hydrogen/mhhc_batch_margins.sio` já rodam com a Tabela 3; com as constantes das eqs. 9–11, `mh7_kinetic_band.sio` perde a "declared box".

## HPC / modelagem
- Mesmo funil (`hpc/`): CSV → `.sio` → `bin/run`. Se vierem mecanismos grandes (Cantera `.yaml`), o caminho é `stdlib/chemistry/gri30_*.sio` e `benchmarks/chemistry/` — não o csv2sio.
- Arrhenius com incerteza: `chemistry::kinetics::arrhenius`.

## IA / grafos de conhecimento
- `ai/README.md`: OBO/OWL em `ai/intake/`, closure EL+ pelos comandos de `demos/demokritos` (`scripts/talk.sh --list`).

## Limites para dizer antes que perguntem
- Linux x86-64 apenas (macOS ARM64: PR #2978 mergeado em main, binário não empacotado).
- Variância de 1ª ordem não atravessa chamadas de função do usuário (KL-11; PR #2977).
- Engine geral: 8×8, ordens inteiras, banda por passo (ver E1–E4 no cabeçalho do uhs_brine_calcite.sio).
- Sem claims de desempenho.
