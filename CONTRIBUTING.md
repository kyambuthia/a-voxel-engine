# Contributing

Scope: this repo is an independent C++17 voxel engine for PC (OpenGL 3.3
core) and Android (OpenGL ES 3.0), built from a clean-room behavioral spec
(see `docs/`). This file is the source of truth for commit hygiene.

## Commit discipline

Every commit is **one logical, independently meaningful change** that leaves
the tree buildable, with tests green where applicable.

### Message format

Conventional Commits, which this repo already uses:

```text
<type>(<scope>): <short imperative summary>

<body: what changed and why, one or two sentences.>
```

Types:

| Type | Use for |
| --- | --- |
| `feat:` | new capability (engine, world, game feature) |
| `fix:` | bug fix |
| `refactor:` | behavior-preserving restructuring |
| `docs:` | documentation only (including ADRs, CONTROLS, this file) |
| `test:` | tests only |
| `build:` | CMake / build system / toolchain changes |
| `chore:` | maintenance and non-code changes (submodules, ignores) |

Scope is optional but encouraged, e.g. `feat(world):`, `build(android):`,
`docs(adr):`.

### Splitting rules

- One logical change per commit. If the summary needs an "and", split it.
- Do not bundle unrelated work: a renderer rewrite must not share a commit
  with a new world-core module, and a build-system change must not ride along
  with engine features.
- New logic and its unit tests land in the **same** commit.
- A doc that decides or explains a change (an ADR, controls) goes in with that
  change, not in a catch-all docs commit.
- Keep every commit buildable; when in doubt, split finer and verify before
  committing.

### Workflow rules

- Commit locally in small increments; **push only when the user asks**.
- Never rewrite pushed history on `trunk`; fix forward with new commits.
- Never commit generated artifacts (`build/`, `android/app/build/`,
  `android/app/.cxx/`, `*.apk`) or credentials; `.gitignore` already covers
  these — keep it that way.
- Reference related ADRs/issues in the body when relevant.

### Suggested granularity for upcoming work

Each of the following should be its own commit (or a short, ordered series):

- `feat(world):` noise library (2D gradient noise + integer hash) + tests
- `feat(world):` deterministic terrain generator (pass pipeline) + tests
- `feat(world):` raycast break/place + edit journal + tests
- `feat(mesh):` exposed-face chunk mesher on the GL backend
- `feat(game):` first-person controller + HUD overlay
