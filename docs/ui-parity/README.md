# UI parity docs

Local, file-based substitute for a Figma library: one place that lists every
screen/feature once and tracks whether both native UIs actually implement it.

There are two UIs over the same domain core (ADR-004, `docs/05-architecture.md`):

- **SwiftUI** — `app/Sources/WorkoutLogApp/` — macOS, the original.
- **ImGui (C++)** — `ui/` — Linux/Windows port, built screen-by-screen against
  the SwiftUI source (see the "ported from ... in Views.swift"-style comments
  at the top of each `ui/*.hpp`).

[MATRIX.md](MATRIX.md) is the parity table: every screen/feature, its file on
each side, a status, and any intentional divergence.

## Keeping it in sync

There's no tooling here — update the matrix by hand:

- **Changed or added a feature in one UI?** Update that row's status, then
  either port it to the other UI or add a row-level note explaining why not
  yet (e.g. "not ported — low priority").
- **Found a difference that's intentional** (a platform constraint, a
  deliberate improvement over the original)? Record it as a divergence, not a
  gap — don't let it get "fixed" into an unwanted regression later. The `ui/`
  source already comments most of these inline (grep for "divergence" or
  "faithful" in `ui/*.cpp`); the matrix just makes them visible without
  reading C++.
- **Porting a screen?** Read the SwiftUI file(s) in the matrix row first, then
  the current ImGui file if one exists — the ImGui side is written to mirror
  SwiftUI structure 1:1 where the toolkits allow it, so that's the fastest way
  to find what's missing.

Status values used in the matrix:

- **Done** — feature parity, differences are cosmetic or documented as
  intentional divergences.
- **Partial** — implemented but missing sub-features; see notes.
- **Missing** — not implemented on that side at all.
