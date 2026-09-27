# 05 — Architecture

## System shape

One domain core, several surfaces:

```
   macOS   Linux   Windows   iOS   Android   Web        an MCP client
      └───────┴────────┴──────┴───────┴───────┘                  │
                         │                              workout_log_mcp
            Dioxus UI (pure presentation)               (tools over stdio)
                         │                                       │
                         └───────────────┬───────────────────────┘
                                         │
      workout-log-core — analytics, parsing, validation
                 (pure Rust: no I/O, no UI, no protocol)
                         │
              SessionStorage (one per platform)
                         │
           JSON session files  ←  source of truth
                         │
           Folder sync (iCloud Drive / git / Dropbox)
```

The web surface is the exception: it has no folder, so it holds a working copy and
imports/exports instead. See ADR-007.

---

## ADR-001 — Files are the source of truth

**Decision.** One JSON file per session on disk. No database.

**Why.** Human-readable, greppable, git-diffable, zero dependencies, outlives any
framework choice. At my volume (a few hundred sessions per year), loading the
entire archive into memory is trivially fast — a database buys nothing and costs
a schema migration story.

**Consequence — this resolves the ownership question.** If the files are
canonical, nothing else can own the model: not SwiftData, not CloudKit, not the
UI. Every surface is a projection of the folder. That was the open tension in the
earlier database-centric design, and it disappears here.

**Trade-off accepted.** No transactions, no indices, no concurrent writers. All
acceptable: one user, one writer, append-mostly.

---

## ADR-002 — JSON, not XML

First-class in every language the project might plausibly use, less structural
noise, cleaner diffs. XML buys schema validation I can get from a JSON Schema
anyway.

The codecs are shaped by hand rather than taken as serde's defaults, as they
were in Dart and in Swift before that:
the on-disk shape is not what a generator emits by default — a flat tagged union
for blocks, sorted keys, omitted nulls, and integral doubles written without a
`.0`. Each of those is pinned by a test.

---

## ADR-003 — Sync is a folder concern, not a document concern

**Decision.** Session files carry no sync metadata. Syncing is a folder sync.

**Why.** iCloud Drive syncs a directory with no schema commitment, so the Apple
phase gets sync nearly free — *without* CloudKit becoming the data model. The
happy path (`NSPersistentCloudKitContainer`) would drag the model into SwiftData
and contradict ADR-001. A synced folder does not.

**Consequence.** Android/Linux later replace the sync mechanism (git, Dropbox,
WebDAV — anything that syncs a folder) without touching the format. This is the
main reason the cross-platform phase stays cheap.

---

## ADR-004 — No business logic in the UI

All parsing, validation, derivation and analytics live in the core. The UI layer
is pure presentation.

**Why.** It is the only thing that makes a port to another toolkit a re-skin
rather than a rewrite. If a single 1RM formula or rep-band rule leaks into a view,
that promise is broken quietly and I will not notice until the port.

**How it is enforced (2026-09-21).** The core is its own crate,
`crates/workout-log-core`, whose library depends on no UI toolkit, no protocol and
no filesystem — `SessionStorage` is a trait the platforms implement. Its only
`std::fs` use is in `src/bin/`, outside the library. The Swift package carried the
same intent as a comment on the target; a comment cannot fail a build.

**Verdict after two ports.** It held both times. Every domain rule — the notation
grammar, the flat block union, activation weighting, the colour ramp — moved
across as a mechanical translation, first to Dart and then to Rust. What had to be
rebuilt each time was the view layer, which is what the ADR promised.

---

## ADR-005 — Core language: start Swift, extract Rust when justified

**Decision.** Write the core in Swift for the macOS entry app. Extract it to Rust
(via `uniffi`) when a non-Apple surface actually arrives.

**Why.** The entry app is a JSON reader/writer with a form. A Rust core plus
`uniffi` bindings plus a build pipeline is real infrastructure that buys nothing
until there is a second, non-Apple platform to share with. ADR-001 already
protects portability — the *files* are the portable asset, not the code. A Rust
core can read the same folder later, on any platform, with no migration.

**Revisit when.** Android is next, or the analytics engine grows heavy enough that
sharing it across platforms is worth the binding layer.

**Counter-argument, honestly.** Building the core in Rust now means the Android
phase reuses it wholesale. It's a defensible call — and the one I'd make if the
Rust practice were itself a goal of the project. But it front-loads cost onto the
phase most likely to stall, so the format carries portability instead.

### Amendment, 2026-09-20 — the non-Apple surface arrived, and the core went to Dart

The trigger fired, and ADR-005's prediction was tested the hard way first. The
non-Apple surface arrived as a Dear ImGui/SDL3 UI for Linux and Windows, and
rather than binding the Swift core it was given a *second* domain core in C++
(`core/`, with `wl_verify`, `wl_map`, `wl_fmt` and `wl_cycle` beside it). That
is what ADR-005 was trying to avoid: not a binding layer, but the thing you get
when you skip one. Two cores, one set of rules, no mechanism keeping them
honest — `docs/ui-parity/` existed to track the drift by hand.

The app then moved to Flutter, which reaches macOS, iOS, Android, Linux, Windows
and the web from one tree. The stated response was to extract the core to Rust.
That is not what happened, and the reasoning that produced ADR-005 is why.

ADR-005's actual argument was never *"Swift specifically"* — it was that a binding
layer must earn its cost. Flutter did not change that arithmetic, it strengthened
it. A Rust core under Flutter means `uniffi` or FFI plus `flutter_rust_bridge`,
a cross-compilation pipeline for six targets, and a hard time on the web. Against
that: the entire domain was ~800 lines of pure functions and codecs with no
dependencies. Re-expressing it in Dart took one pass and left a single language in
the repository.

**What was given up.** The Swift and C++ cores were deleted outright rather than
kept as unverified references, along with both UIs, their CMake build, their CI
and the parity docs that tracked the drift between them. Keeping either would
have recreated the problem this amendment exists to record: implementations of
the same rules drifting apart in silence. The domain now lives in Dart only, and
`data/` remains the thing that outlives all three.

**Revisit when.** See the 2026-09-21 amendment below: the Rust core arrived.

### Amendment, 2026-09-21 — the Rust core arrived after all

**What happened.** The Dart core and the Flutter UI were replaced by
`crates/workout-log-core` and a Dioxus UI built from the Rust/UI component
registry. The original ADR said "extract Rust when justified"; what justified it
was not performance, which was never the problem, but that the reason to *avoid*
Rust had gone. In 2026-09-20 a Rust core meant a binding layer — `uniffi` or
`flutter_rust_bridge` — between the domain and the UI, and that layer was the cost
that kept losing the argument. With the UI itself in Rust there is no binding
layer: the app calls the core the way any crate calls any crate.

**What it cost.** One pass over roughly 3,000 lines of domain logic, ported
against the behaviour the Dart suite already pinned rather than against the Dart
source. The conformance targets were the real files: all nine sessions in `data/`
round-trip byte for byte through the new encoder, `exercises.json` and
`cycles.json` survive the models with every hand-written key intact, and
`wl_gen_cycle` regenerates the eight planned stubs byte-identically to the ones on
disk. A port that can reproduce the archive is a port that read the rules right.

**What it bought.** One language across the domain, the MCP server and the UI —
the same consolidation the Dart move was for, one layer deeper. The muscle map
also stopped being a special case: the core emits SVG, and a webview renders SVG
natively, where Flutter needed `flutter_svg` and a rendering test to guard against
CSS support drifting.

**What it cost honestly.** Dioxus desktop is a WebKitGTK webview, so "no Flutter"
is not "no large GUI runtime" — it is a different one, with system library
dependencies the Flutter build did not have. Rust/UI is also young and describes
itself as experimental, so the component layer is the part of this tree most
likely to churn. Neither is load-bearing for the domain, which is the point of
ADR-004.

**Revisit when.** A surface arrives that Dioxus does not reach, or the component
registry stops being maintained and the UI layer has to stand on Dioxus alone.

---

## ADR-006 — The importer tolerates history

The paper and CSV logs drifted across years (4-day → 8-day → 12-day cycles,
changing columns, changing notation). The importer targets *several* historical
shapes and reports what it cannot parse instead of guessing. Unparseable entries
are surfaced for manual review, never silently dropped.

---

## ADR-007 — The folder is canonical where a folder exists

**Decision.** ADR-001 holds wherever the platform has a real directory. Where it
does not, the app keeps a working copy and the user's folder stays canonical via
explicit import and export.

| Platform | Storage |
|---|---|
| macOS, Linux, Windows | the real directory: `$WORKOUTLOG_DATA`, else the last folder chosen, else `<cwd>/../data` |
| iOS, Android | the app's documents directory, synced per ADR-003 by whatever syncs that directory |
| Web | an in-memory working copy, filled by import and written back by download |

**Why.** A browser has no folder to point at. The File System Access API would give
one, but only on Chromium, and building the single source of truth on a
browser-specific API contradicts the reason ADR-001 exists. Mobile has a folder but
not *the* folder: the sandbox is the only directory an app may own.

**Consequence.** `SessionStorage` is an interface declared in the core with four
methods — list, read, write, delete — and implemented per platform. The core never
learns which one it is talking to, which is also what keeps the filesystem out of
it (ADR-004).

**Trade-off accepted.** On the web the archive is not durable across a reload
unless the user exports. That is stated in the UI rather than hidden: the status
bar says *working copy*. The alternative — silently persisting to IndexedDB and
letting a browser cache eviction look like data loss — is worse.

---

## ADR-008 — The MCP server is a surface, not a second core

**Decision.** `crates/workout-log-mcp` speaks MCP over stdio and calls
`workout-log-core` for everything. Argument parsing, the session folder and JSON
formatting live there; no domain rule does.

**Why.** This is ADR-004 with a language model as the UI, and the amendment to
ADR-005 is the reason to state it again rather than assume it. The cheap version
of this server is a script that opens the JSON files and computes tonnage itself
— and that is a second implementation of the rep-band rule, the cluster sum and
the notation grammar, drifting from the app's in silence. The expensive part of
the Swift/C++ episode was never the code, it was having two of them.

**Consequence.** Metrics that did not exist yet went into the core rather than
into the server: `SessionMetrics`, `ExerciseProgress`, `TrainingReport` and
`SessionValidator` are pure core code beside `MuscleActivation`, so the app can put
them on screen without anything moving. The server's own test suite exercises
the protocol and the file effects, not the arithmetic — that is tested where it
lives.

**Trade-off accepted.** `DirectoryStorage` is written twice, once in the app and
once here, because the core may not touch the filesystem and neither surface
may depend on the other. It is thirty lines of read/write/rename with no domain
rule in it; a third package to share it would cost more than it saves.

**Writes are guarded, not blocked.** A tool refuses a session that would not
parse back and reports warnings beside the ones it accepts, because a
half-written plan is a normal state of a file (ADR-006) and a model that cannot
save an unfinished session will invent values to finish it. Deleting takes an
explicit confirmation: the folder is the only copy (ADR-001).

**Consequence for the reference files.** The app's bundled fallback copies of
`exercises.json` and `cycles.json` are `include_str!`ed from the repo root, so a
catalogue edit through the server cannot leave a stale copy behind. The Dart tree
mirrored the files into an asset directory and needed a sync tool and a drift test
to keep the two honest; compiling the real file in deletes that whole class of bug.
