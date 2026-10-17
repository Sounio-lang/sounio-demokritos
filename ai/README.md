# ai — ontologies and knowledge graphs

What runs today (from `Sounio-lang/sounio`, see `demos/demokritos/README.md` steps 8a–8g):

- EL+ role-aware closure of Gene Ontology go-plus: 38,245 classes, 92 roles, 2,135,207 role edges
  (`bash scripts/talk.sh --list` for the exact commands; the Lean 4 proof of the reasoner is in `formal/lean4/`).
- Cell Ontology, UBERON, GO roots: 1–5 min each.

Bring:

1. your ontology as OBO or OWL (EL profile: subclass axioms, role chains, existential restrictions);
2. the question you want closed (subsumption between two named classes, or the full classification);
3. for calibrated bands on a model: the model's predictions and the held-out outcomes as CSV
   (`id,prediction,sigma,outcome`), and the funnel's `species/reactions` CSVs if it is a kinetic model.

Put the files in `ai/intake/<name>/` (gitignored) and we run the closure on the workspace.
