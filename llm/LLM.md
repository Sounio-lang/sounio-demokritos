# Running an open-weights LLM in Sounio — where it stands, measured

Target chosen on 2026-10-17: **Qwen2.5-0.5B or Llama-3.2-1B**, forward pass with logit parity
against the reference implementation, then generation. Everything below was measured on the
workspace (Linux x86-64, Madaros v0.80.0 default engine) with `bin/run`; the receipts are the
`runs/*probe_*` folders and the probe sources are in `llm/`.

## What already exists in the repository

- `stdlib/tensor`: `Tensor { data: *mut f64, shape, storage_owner }`, heap-allocated
  (`heap_alloc`/`heap_free`), rank ≤ 8, row-major; `tensor_matmul`, `transpose`, `permute`,
  `gelu`, `sum`, `mean`, `reshape`. f64 only.
- `stdlib/nn`: autograd (v1, v2), dense layers, optimizers, beam search, epistemic layers;
  the E2E training demo (loss 1.10e1 → 1.06e-5 in 300 iterations) runs on this.
- GPU: eleven emitted PTX kernels in `docs/gpu/` (octonion associator, O-SSM cells, `oct_matvec_dh16_tile`,
  `sed_batch_vjp_tile`, …) that ran on NVIDIA (DGX Spark GB10, associator R² = 1.0000), Metal kernels
  (`stdlib/gpu/sedenion_f3.metal`, `vendor/metal-softfloat`, PR #2978: 40 960 zero mismatches),
  `stdlib/gpu/tilelang.sio`. What does not exist is the *generic* `souc build --backend gpu` path
  from arbitrary source (TOUR.md §6); the targeted-kernel path is real.
- `stdlib/data`, `csv`, `io`: text I/O; no binary reader for safetensors/GGUF yet.

## Measured on 2026-10-17

| probe | result |
|---|---|
| `llm/probe_alloc.sio` — allocate, touch and free a 256 MiB, 2 GiB and 4 GiB `Tensor` | all three **ok**, 11 s total. Heap is not the blocker. |
| `llm/probe_matmul.sio` — `tensor_matmul` 1024³ | **`madaros: handles full`**: the stdlib matmul creates an aggregate per inner iteration; the handle table (4 194 304 objects, never reclaimed) dies before one 1024³ product finishes. |
| `llm/probe_matmul2.sio` — hand-written triple loop via `tensor_get`/`tensor_set`, 4 × 512³ (1.07 GFLOP) | 221 s → **≈ 5 MFLOP/s** |
| `llm/probe_matmul3.sio` — same loop on raw pointers (`read_f64`/`write_f64`), hoisted indices | 76 s incl. ≈ 10 s compile → **≈ 16 MFLOP/s** |

A naive C triple loop on this machine is in the 1–3 GFLOP/s range, so scalar Madaros code is
roughly **100–200× slower than naive C** on this kernel today, and the stdlib matmul cannot be used
for anything LLM-sized until its per-element aggregates are hoisted (same lesson as
`stdlib/chemistry/surface.sio`).

What that means for the target: Qwen2.5-0.5B is ≈ 1 GFLOP per token in the forward pass, so
on the CPU-scalar path a single token costs **about a minute**. Correct, receipted, and useless as a
demo. The path to a usable demo is the one that already runs: **PTX tiles**.

## The ladder

1. **Correctness first, CPU, small.** Safetensors reader (JSON header + raw tensors; bf16 → f32
   by bit shift), BPE tokenizer, RMSNorm / RoPE / GQA attention / SwiGLU on `Tensor`, KV cache.
   Criterion: logits of the first token equal to the HF reference to float tolerance, printed next
   to the command (the Cantera method). Model: Qwen2.5-0.5B. Cost: the handle discipline above in
   every kernel; no stdlib `tensor_matmul`.
2. **f32 `Tensor`.** Today the tensor is f64: double the memory and bandwidth of the reference.
   A `Tensor32` (or a dtype field) halves both; bf16 storage with f32 accumulate is the real target.
3. **`matmul_tile` in PTX**, in the family of `oct_matvec_dh16_tile`: one tiled f32 GEMM kernel
   covers ~90 % of a transformer's FLOPs; RMSNorm/softmax/RoPE are memory-bound and can stay scalar
   or get trivial kernels. This is where tokens/second comes from, and it reuses the path that
   already emitted and ran PTX.
4. **Quantization as a type.** A Q8/Q4 block weight is a value, a scale and a rounding error:
   `Knowledge<f32>` with provenance "checkpoint X, scheme Y". Propagating the band through the
   forward pass yields logits with an uncertainty that decomposes by layer — the epistemic-inference
   angle no other runtime has. Do this after 1–3, on the small model.
5. **Codegen.** Independently of the LLM, the 16 MFLOP/s number says the native backend is leaving
   two orders of magnitude on the table in tight loops (bounds/handle overhead, no register
   allocation across the loop). Fixing that benefits every demo in this repo.

## Honest limits to say out loud

- No bf16/f16/int8/int4 types yet; the generic GPU flag is not finished; the stdlib matmul exhausts
  the handle table at LLM sizes; CPU scalar throughput is ≈ 16 MFLOP/s measured.
- 7B-class models at interactive speed depend on step 3 being done and on the GPU available
  (GB10 in the DARWIN cluster is the natural target).
