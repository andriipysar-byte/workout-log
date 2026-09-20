# 05 — Architecture

## System shape

One domain core, several surfaces:

```
   macOS   Linux   Windows   iOS   Android   Web
      └───────┴────────┴──────┴───────┴───────┘
                         │
              Flutter UI (pure presentation)
                         │
      workout_log_core — analytics, parsing, validation
                    (pure Dart: no Flutter, no dart:io)
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

The codecs are hand-written rather than generated, in Dart as they were in Swift:
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

**How it is enforced (2026-09-20).** The core is its own package,
`flutter/packages/workout_log_core`, which depends on neither `flutter` nor
`dart:io`. Nothing platform-specific can compile there, so the boundary is a build
error rather than a convention. `test/purity_test.dart` asserts it directly. The
Swift package carried the same intent as a comment on the target; a comment cannot
fail a build.

**Verdict after the Flutter port.** It held. Every domain rule — the notation
grammar, the flat block union, activation weighting, the colour ramp — moved
across as a mechanical translation. What had to be rebuilt was the view layer,
which is what the ADR promised.

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

**Revisit when.** A surface arrives that Flutter does not reach, or the analytics
engine grows heavy enough that a shared native core beats a Dart one.

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
learns which one it is talking to, which is also what keeps `dart:io` out of it
(ADR-004).

**Trade-off accepted.** On the web the archive is not durable across a reload
unless the user exports. That is stated in the UI rather than hidden: the status
bar says *working copy*. The alternative — silently persisting to IndexedDB and
letting a browser cache eviction look like data loss — is worse.
