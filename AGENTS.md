# Repository instructions (agents)

This repo is an independent C++17 voxel engine for PC (OpenGL 3.3 core) and
Android (OpenGL ES 3.0), built from a clean-room behavioral spec. Read
`docs/` (ADR-001, ADR-002, CLEAN_ROOM_IMPLEMENTATION_PLAN, CONTROLS) and
follow `CONTRIBUTING.md` before making changes.

Commit discipline — non-negotiable for automated agents:

- Small atomic commits: one logical change per commit, Conventional-Commits
  messages (`feat(world):`, `fix:`, `docs:`, `build:`, ...).
- Never bundle unrelated work into one commit.
- Tests ship in the same commit as the logic they cover.
- Never commit generated artifacts (`build/`, `android/app/build/`,
  `android/app/.cxx/`, `*.apk`) or credentials.
- Never rewrite pushed history on `trunk`.
- Do not commit or push unless the user explicitly asks; when asked to
  "commit", push only if they also say to.
