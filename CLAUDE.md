## graphify

This project has a **knowledge graph** at `graphify-out/`, built from the first-party C++
source (engine, editor, Azure sample game, tests — 243 files → ~1,857 nodes, 190 communities).
It captures god nodes, community structure (Scene & Actors, Rendering 2D/3D, DirectX11/OpenGL
backends, Material System, Cascaded Shadows, Audio, Input, Runtime UI, Editor, …), and
cross-file call/type relationships.

**Scope is controlled by `.graphifyignore`** at the repo root. The vendored submodules under
`ExternalDependencies/` and `GlobalExternalDependencies/` are excluded — keep them excluded
(scanning them explodes the graph from ~1.8k to ~49k nodes).

### Using the graph (orient before grepping)
- For codebase questions, run `graphify query "<question>"` first whenever
  `graphify-out/graph.json` exists. It returns a scoped subgraph, usually much smaller than
  raw grep output or `GRAPH_REPORT.md`.
- Use `graphify path "<A>" "<B>"` for how two concepts relate, and
  `graphify explain "<concept>"` for a focused concept.
- Read `graphify-out/GRAPH_REPORT.md` only for broad architecture review, or when
  query/path/explain don't surface enough context.

### Keeping the graph current (auto-update)
- **Automatic:** a git `post-commit` hook re-extracts the code that changed and rebuilds
  `graph.json`, `graph.html`, and `GRAPH_REPORT.md` after every commit (AST-only, no API
  cost, runs detached in the background). A `post-checkout` hook does the same on branch
  switch. Both honor `.graphifyignore` and reuse the community names in
  `graphify-out/.graphify_labels.json`, so labels persist across rebuilds.
- **Mid-session, before committing:** after editing code, run `graphify update .` to refresh
  the graph immediately (respects `.graphifyignore`, code-only/no LLM).
- For changes to **docs/images** (not code), re-run `/graphify` from your AI assistant.

> `graphify-out/` is git-ignored — it is regenerated locally and holds machine-specific paths.
