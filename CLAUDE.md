# WorkoutLog2 — agent guide

## Comments

Write a comment ONLY when the code uses a trick or a solution that is not obvious
from reading it — a non-obvious algorithm or invariant, a workaround for a platform
quirk, or *why* an approach was chosen over an alternative. Do NOT write comments
that restate what the code plainly does, label sections, or narrate steps. Prefer
clear names over comments. When in doubt, leave it out.

## Build & verify

- One cargo workspace rooted at the repo root, three crates:
  `crates/workout-log-core` (pure domain), `crates/workout-log-mcp` (MCP server
  over it) and `crates/workout-log-app` (Dioxus UI over it).
- Core and MCP server need no system libraries, so they are the fast loop:
  `cargo test -p workout-log-core -p workout-log-mcp`,
  `cargo clippy -p workout-log-core -p workout-log-mcp --all-targets -- -D warnings`,
  `cargo fmt --all --check`.
- The app is Dioxus desktop, which is a WebKitGTK webview, so building it needs
  host libraries: `sudo apt-get install libgtk-3-dev libwebkit2gtk-4.1-dev
  libxdo-dev libayatana-appindicator3-dev librsvg2-dev`. Without them even
  `cargo check -p workout-log-app` fails in `gdk-sys`.
- Run it: `cd crates/workout-log-app && WORKOUTLOG_DATA=../../data dx serve --platform desktop`
  (`cargo install dioxus-cli` supplies `dx`).
- The web target needs no system libraries and is the cheap way to check the UI
  compiles: `cargo check -p workout-log-app --no-default-features --features web
  --target wasm32-unknown-unknown`.
- Styling is Tailwind v4. `dx serve` runs the watcher; by hand it is
  `cd crates/workout-log-app && npx @tailwindcss/cli -i tailwind.css -o assets/tailwind.css --minify`.
  The generated `assets/tailwind.css` is committed so a plain `cargo build`
  needs no Node.
- MCP server by hand: `cargo run -p workout-log-mcp -- --repo .`. It speaks MCP
  on stdio, so nothing in it may ever write to stdout — diagnostics go to stderr.
- Generate session stubs from a cycle template: `cargo run --bin wl_gen_cycle`
  (`--cycle`, `--repo`, `--out`, `--force`).
- Normalise session files after hand-editing them: `cargo run --bin wl_fmt`, or
  `--check` to fail without writing. `data/` is kept canonical, and
  `tests/canonical_round_trip.rs` pins the same property.

Buildable and verifiable on this machine: the core, the MCP server and the web
target. The desktop app is written and lints clean for wasm, but has never been
compiled here — the WebKitGTK headers above are not installed.

- Data files in `data/` are the source of truth (ADR-001), except in the browser,
  which holds a working copy and imports/exports (ADR-007).
- The core carries all domain logic and the UI stays pure presentation (ADR-004):
  the core has no filesystem, no UI and no protocol. `SessionStorage` is the seam,
  implemented once per platform. The two `src/bin/` tools may use `std::fs`; the
  library may not.
- `cycles.json` and `exercises.json` are hand-maintained and the app writes them
  back, so the models keep every key they do not themselves model:
  `#[serde(flatten)] extras`, plus `Option<Vec<_>>` wherever an empty list that
  was written by hand must survive a save. `tests/reference_files.rs` pins this —
  drop a key and a save would silently delete it from the user's file.
- The bundled fallback copies of `exercises.json` and `cycles.json` are
  `include_str!`ed from the repo root, so they cannot drift from the real files.
