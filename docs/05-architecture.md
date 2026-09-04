# 05 — Architecture

## System shape

One domain core, several surfaces:

```
   Journal (iOS)          Analytics (macOS)        Entry (macOS)
        │                        │                      │
        └────────────────────────┴──────────────────────┘
                                 │
                    Domain core (analytics, parsing, validation)
                                 │
                    JSON session files  ←  source of truth
                                 │
                    Folder sync (iCloud Drive / git)
```

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

Native `Codable` in Swift and `serde` in Rust, less structural noise, cleaner
diffs. XML buys schema validation I can get from a JSON Schema anyway.

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

All parsing, validation, derivation and analytics live in the core. SwiftUI is
pure presentation.

**Why.** It is the only thing that makes the later Linux/Windows/Android port a
re-skin rather than a rewrite. If a single 1RM formula or rep-band rule leaks into
a SwiftUI view, that promise is broken quietly and I will not notice until the port.

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

---

## ADR-006 — The importer tolerates history

The paper and CSV logs drifted across years (4-day → 8-day → 12-day cycles,
changing columns, changing notation). The importer targets *several* historical
shapes and reports what it cannot parse instead of guessing. Unparseable entries
are surfaced for manual review, never silently dropped.

---

## ADR-007 — Canonical JSON formatting

**Decision.** All JSON files must be formatted canonically: sorted keys, 2-space
indent, integer-valued doubles rendered as ints, raw UTF-8 (never `\uXXXX`),
unescaped slashes, absent optionals omitted (never written as `null`), a
trailing newline, and untyped preservation of unmodeled fields via
`canonicalize`.

**Why.** ADR-001 means these files are diffed and read by humans. Format churn
from different writers destroys git history. Preserving unmodeled fields
ensures a typed reader doesn't destroy data it doesn't understand.
