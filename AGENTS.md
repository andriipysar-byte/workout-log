# AGENTS.md — C++ guidance for `core/` and `tools/`

The C++ coding and safety rules for this codebase. Read this **before** touching C++ code.
They are adapted from `../remoteguard/CERAMIC/AGENTS.md` — the CERAMIC-specific parts (network
transport, tokens/TLS, the security-issue ledger, the RPi4 gate, plugin skills) are dropped, and
the rest is restated in this project's terms.

Sections and rules are numbered so they can be referenced directly in review (e.g. "see 2.1").

Scope: the C++ core (`core/`), the CLI tools (`tools/`), and the later `capi/` and `ui/` trees.
The Swift tree under `app/` follows Swift conventions; only §1.1.11 (comments) and §5
(documentation cross-references) apply there.

## 1. Modern & safe C++ (the `core/` rules)

The C++ tree targets **C++20**, and `wl_core` must also compile under Apple clang for the macOS
app, so it stays inside a conservative subset: no `<format>`, no `<expected>`, no `<chrono>`
calendar types, no `std::regex`, no unguarded floating-point `std::from_chars`
(see `CMakeLists.txt`). Write code that reads like the existing core —
`core/src/store.cpp` and `core/src/json.cpp` are the reference patterns. Note that this
codebase is `snake_case` for functions, variables and members (trailing `_` on private data
members), `PascalCase` for types, and lowercase for namespaces. Match it.

### 1.1 Modern — do this

1.1.1. **Value semantics first; smart pointers where ownership is real.** The domain types
(`Session`, `Block`, `WorkSet`) are plain values that copy and compare — keep them that way.
Where a heap object is genuinely needed, use `std::unique_ptr` + `std::make_unique`, and reach for
the **pimpl idiom** (`struct Impl; std::unique_ptr<Impl> impl_;`) when a class needs a
compilation firewall — a header that would otherwise drag a dependency's headers into every
translation unit that includes it. That is the case for the SDL3/ImGui/lunasvg-facing types in
`ui/` and for anything in `capi/`, not for the domain models, which own nothing and should stay
transparent. No owning raw pointers, no manual `new`/`delete`, no `malloc`/`free`.

1.1.2. **RAII for every resource** (files, streams, SDL/lunasvg handles once the UI lands).
Cleanup in destructors; no manual teardown path that can be skipped on an early return or throw.
`store.cpp`'s atomic write is the shape to follow — a temporary that cleans up after itself, not
a half-written file on the error path.

1.1.3. **Report failure the way the neighbouring code does.** `std::optional<T>` for a "not found
/ unrecognised" answer the caller decides about (`kind_from_string`, `get_opt`); a thrown
`std::runtime_error` whose message *names the offending key or path* for a malformed file
(`json::decode_session`, `paths::resolve_repo_root`); a result struct that carries both when a
partial answer is the point (`SessionStore::LoadAllResult`). Never a sentinel value or an
out-param. `std::expected` is not available in this subset.

1.1.4. **Value-semantics + move.** Pass sinks by value and `std::move` into members
(`explicit SessionStore(std::filesystem::path folder) : folder_(std::move(folder))`).
Mark single-argument constructors `explicit`.

1.1.5. **`constexpr`** for constants, **`enum class`** for enumerations, **`std::span`** for
buffer views.

1.1.6. **Take read-only string inputs as `std::string_view`, not `const std::string&`.** A
parameter the function only reads should be `std::string_view` — it binds to `std::string`,
string literals and substrings without forcing a `std::string` construction/copy at the call
site, and the type states "I only read this." Reserve `const std::string&` for when you genuinely
need an existing `std::string` (e.g. to forward to an API that takes one); take by value +
`std::move` when you need to *own* it. Caveat: a `string_view` is **not** null-terminated and does
not own its buffer — never pass its `.data()` to a C API expecting a C-string, and never store a
view that outlives the string it points into.

```cpp
// Bad
std::optional<Pattern> pattern_from_string(const std::string& raw);

// Good
std::optional<Pattern> pattern_from_string(std::string_view raw);
```

1.1.7. **Every constructed object must have a valid, well-defined state.** Give members in-class
initializers with reasonable defaults so a default-constructed object is immediately usable and
never holds indeterminate values. Prefer `= default` / `= delete`. This is the primary rule: a
type that defaults to a valid state cannot be misused into an ill-defined one — including via
`emplace`, which default-constructs out of sight.

For **aggregates** (plain structs with no member initializers — the domain models here, and
third-party types you don't control), that guarantee doesn't exist, so initialize them explicitly
at the point of declaration:

- Prefer a single **designated-initializer** expression with named members (`.field = value`),
  never default-construct and then assign field-by-field.
- When the values genuinely aren't known up front (members are filled in conditionally
  afterwards), at least **brace-initialize with `{}`** (`WorkSet s{};`), never bare `WorkSet s;`.
  Bare default-construction of an aggregate leaves every trivial member (ints, doubles, pointers,
  bools, enums) holding indeterminate garbage; if a later branch skips a field, you read an
  uninitialized value — UB that ASan/UBSan may or may not catch. The trailing `{}`
  value-initializes the whole aggregate, so a skipped field is a defined `0`/`nullptr`/empty
  optional.

```cpp
struct Cell { int day; int index; };

// Wrong -- default-construct then assign field-by-field:
Cell first;
first.day = 0;
first.index = 1;

// Right -- one expression, members named, order checked by the compiler:
Cell first {
    .day = 0,
    .index = 1,
};

struct Point { int x; int y; int z; };

// Wrong -- bare default-init: if the `if` is not taken, `a1.z` is garbage.
Point a1;
a1.x = 2;
a1.y = 3;
if (cond) {
    a1.z = 2;
}

// Right -- `{}` value-inits first, so a skipped `a1.z` is a defined 0.
Point a1{};
a1.x = 2;
a1.y = 3;
if (cond) {
    a1.z = 2;
}
```

(C++20 designated initializers must follow declaration order; members may be omitted and are then
value-initialized.)

1.1.8. **Namespaces.** All namespaces reside under `workoutlog`, with a nested namespace per
subsystem where one already exists (`workoutlog::json`, `workoutlog::paths`). Prefer the
`namespace workoutlog::json { ... }` concatenated form. Use **anonymous namespaces** for
file-local helpers — the reader/writer helpers in `json.cpp` are the model. Close every namespace
with a comment naming it.

```cpp
namespace workoutlog::json {
...
} // namespace workoutlog::json
```

1.1.9. **No `using namespace` for project namespaces** — it drags whole namespaces into scope,
hides where names come from, and invites collisions. Qualify explicitly
(`workoutlog::json::decode_session`) or add a narrow local alias (`using nl = nlohmann::json;`,
as `json.cpp` does). Never `using namespace` at file/global scope, and never in a header.

1.1.10. **Use self-documenting, non-ambiguous names that carry the *what*** (What is it? What is
it doing?); add a comment only when the code needs extra description. Name constants, variables
and functions for their intent so the code reads as its own explanation — `rep_volume` not `rv`,
`resolve_repo_root()` not `find()`, a named `constexpr` not a bare magic number. Prefer
extracting a well-named helper or local over adding a comment to explain a dense expression.

1.1.11. **Comment sparingly — only where the solution is non-obvious or controversial.** This
restates the rule in `CLAUDE.md`, which applies to the whole repository. Default to
self-documenting code and let it carry the *what*. Add a comment only when the code can't speak
for itself: a workaround for a toolchain quirk, a non-local invariant, a subtle ordering/lifetime
constraint, a deliberate deviation from the obvious approach, or a decision recorded in
`docs/05-architecture.md` — i.e. answer the *why*, especially "why this and not the obvious
thing." Don't narrate what the code plainly states (`// increment i`), restate a function's own
name/signature, or label sections; that's noise that rots out of sync. Examples worth keeping
from this repo: "Files are the source of truth (ADR-001)", "A corrupted file costs one session,
never the archive", "Named WorkSet (not Set) ... to avoid clashing with the standard library". If
you change code, delete or fix any now-stale or now-obvious comment next to it.

**Before calling any change done, reread every comment you just wrote and delete the ones that
don't clear the bar above.** It is easy to over-comment while a design is still taking shape —
a comment that felt load-bearing mid-task often turns out, once the code is finished, to just
restate the line below it (a section label, a comment naming a type or repeating a function's own
signature, a note that duplicates something already said better elsewhere in the same file). Do
this pass as its own step, separate from writing the code, and judge each comment against
§1.1.11's test — a workaround, a non-local invariant, a subtle ordering/lifetime constraint, a
deliberate deviation, or a recorded decision — not against whether it seemed useful at the time
you wrote it.

1.1.12. **No ad-hoc printing from the core.** `core/` computes and returns data; the CLI tools
(`tools/`) and the UI do the printing. A `std::cout` or `printf` inside a core function makes it
unusable from the macOS app and untestable from `wl_verify`. Errors leave the core as a thrown
`std::runtime_error` or a recorded `LoadFailure`, not as a message on a stream.

### 1.2 Safe — required, especially at the file boundary

1.2.1. **Validate everything crossing a boundary before use.** Every JSON file under `data/`,
`exercises.json`, `cycles.json`, every CLI argument and environment variable is untrusted input —
"I wrote that file by hand" is exactly why it can be malformed. Go through the checked accessors
in `json.cpp` (`require`, `require_as`, `get_opt`, `get_opt_enum`) rather than
`j["k"].get<T>()`, so a missing key or a wrong type fails with a message naming the key instead
of throwing something opaque or reading a default. Never assume a field exists, its type, or its
size.

1.2.2. **Check length and bounds on anything derived from a file.** No unbounded allocation sized
from input; cap loop and array sizes that come from outside. Reject oversized or nonsensical
input rather than coercing it.

1.2.3. **Avoid unsafe C string APIs** (`strcpy`/`strcat`/`sprintf`/`gets`). Use `std::string`,
`std::ostringstream`, and checked container access (`.at()` or an explicit bounds check) — never
unchecked `operator[]` on an index derived from file contents. Prefer `std::copy` with
`std::span` over `memcpy`. Where the conservative subset forces a C API (`std::snprintf` for hex
colour formatting in `muscle_map_svg.cpp`), keep the buffer sized by `sizeof` and say why the C
call is there.

1.2.4. **Wrap external-boundary calls in `try`/`catch`** (JSON parse, file I/O, `std::from_chars`
misuse) and fail closed. A parse failure must degrade to a recorded failure or a clear error, not
an escaped exception from a batch loop — `SessionStore::load_all` is the pattern. An intentionally
empty `catch` needs a comment saying so; `.clang-tidy` recognises the word "Intentionally" and
`bugprone-empty-catch` will flag anything else.

1.2.5. **Thread discipline.** If work could stall the UI's event loop, move it off that thread;
guard shared mutable state with mutexes or atomics. Nothing in the core is threaded today — keep
it that way unless the task requires otherwise, and run the TSan preset (§4.3) if it does.

1.2.6. **Build clean under `-Wall -Wextra`** (fix, don't suppress; see `cmake/Warnings.cmake`).
Run ASan/UBSan on `wl_verify` before claiming a core change is done.

## 2. Additional C++ rules (with samples)

These sharpen a few of the idioms above; follow them alongside the rules in section 1.

2.1. **Test an `optional` explicitly with `.has_value()` — avoid the implicit `bool` conversion.**
`if (band)` only tells you the optional is *engaged*; it says nothing about whether the contained
value is meaningful. The implicit conversion reads like a validity check and misleads. Spell out
what you mean.

```cpp
// Bad -- implicit bool conversion; "is it set" reads as "is it valid":
if (!opt_value) { ... }

// Good -- explicit and unambiguous:
if (!opt_value.has_value()) { ... }
```

2.2. **Never unwrap an `optional` without first proving it holds a value.** Before
`opt_value.value()` (or `*opt_value`), either guard with `if (opt_value.has_value())` or wrap the
access in `try`/`catch` for `std::bad_optional_access`. A blind `.value()` on an empty optional
throws — and in a codebase where nearly every model field is optional, that is the easiest bug to
write here.

```cpp
// Bad -- throws std::bad_optional_access if empty:
const auto reps = set.reps.value();

// Good -- guard first:
if (set.reps.has_value()) {
    const auto reps = set.reps.value();
    ...
}
```

2.3. **Make every local `const` unless it must change.** For function-local assignments, default
to `const auto` / `const auto&`; drop `const` only where the variable is genuinely reassigned. A
non-`const` local that never changes misleads the reader and blocks the compiler's checks.

```cpp
// Bad -- mutable but never reassigned:
auto  session = store.load(path);
auto& blocks  = session.blocks;

// Good -- const because they don't change:
const auto  session = store.load(path);
const auto& blocks  = session.blocks;
```

2.4. **Choose `switch` or `if`/`else` on the merits — neither is automatic.** Reach for `switch`
when you need to handle *every* enumerator and want the compiler to flag a missing case (with
`-Wall -Wextra`, and no `default:` to defeat it) — the `to_string` overloads over `Kind`,
`Pattern`, `Modality` are exactly that case, and the exhaustiveness warning is what catches a
newly added enumerator. Prefer `if`/`else` when you only care about one or two enumerators, or
when the branches carry conditional logic that a `switch` would make harder to read. When you do
use `switch`, mind its hazards: a missing `break` falls through, and a branch that declares
variables needs its own braces to give them a well-defined scope.

```cpp
// Good -- exhaustive handling, no `default:` so the compiler flags a new enumerator:
switch (band) {
    case RepBand::heavy: {
        ...
        break;
    }
    case RepBand::base: {
        ...
        break;
    }
    case RepBand::volume: {
        ...
        break;
    }
}

// Also good -- only one case matters here; a switch would add noise:
if (kind == Kind::deload) {
    ...
}
```

2.5. **No magic paths, filenames or environment names scattered through the code.** Repo-root and
data-directory discovery belongs in `workoutlog::paths` (`$WORKOUTLOG_ROOT`, `$WORKOUTLOG_DATA`,
marker walk-up); a hard-coded absolute path or a second copy of that search is the bug ADR-era
`#filePath` walk-ups already caused once. Take the location from the resolver, an argument, or
the environment — and validate it like any other external input.

```cpp
// Bad -- baked-in path, works only on the machine that wrote it:
const std::filesystem::path data{"/Users/me/Developer/WorkoutLog2/data"};

// Good -- resolved once, in one place:
const auto root = workoutlog::paths::resolve_repo_root();
const auto data = root / "data";
```

2.6. **Reach for `static_cast` only when it is genuinely required — never to silence a
narrowing.** A cast that truncates is a bug waiting to happen: `double` → `int` discards the
fraction, a wide integer → a narrow one keeps only the low bits, and a negative `int` →
`std::size_t` becomes an enormous positive index. Prefer a design that doesn't need the cast
(right type end-to-end). Where a conversion is truly needed, **establish the value is in range
first**, then cast, and say in a comment why it fits — `json.cpp`'s `num()` is the pattern
(`std::isfinite`, integrality and the 2^53 bound checked *before* the cast), and
`calendar.cpp`'s `kDays.at(static_cast<size_t>(month - 1))` is the minimum: a bounds-checked
accessor carrying the cast, with the comment saying the index is caller-supplied.

Don't hand-roll the same bounds check at every call site. This tree has no
checked-conversion helper yet, so the third site that needs one is the signal to extract it
(`workoutlog::narrow<To>(v)` throwing on loss, `try_narrow<To>(v)` returning
`std::optional<To>` where an exception must not escape) rather than to copy the check again —
that is the shape CERAMIC's `common/utils/numeric_utils.h` settled on. Until then, keep the
guard adjacent to the cast; a bare cast on a value derived from a file is a defect, not a style
issue.

```cpp
// Bad -- silent truncation, no check:
const std::int64_t reps = static_cast<std::int64_t>(total);

// Bad -- a negative `offset` wraps to an enormous size_t; the cast hides it:
const auto count = static_cast<size_t>(offset);

// Good -- range established first, cast justified in a comment:
if (std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 9007199254740992.0)
    return nl(static_cast<std::int64_t>(v));   // integral and within int64 range
```

Two carve-outs, so nobody "fixes" them: the `static_cast<unsigned char>` before a `<cctype>`
call (`std::isdigit`, `std::isspace`) in `notation.cpp` and `utf8.cpp` is **required** — passing a
negative `char` to those functions is undefined behaviour — and the byte-assembly casts in
`utf8.cpp`'s decoder are masked to their width by construction. Both are correct as written.

**No `reinterpret_cast` in this tree.** There are none today; keep it that way. If a
byte-oriented C or iostream API forces a raw-byte view, wrap it in one named helper
(`as_chars`/`as_const_chars`) rather than spreading the cast across call sites.

2.7. **Write only the code the task needs — no speculative scaffolding.** Add the function,
member, parameter or branch the current change actually requires; don't generate "just in case"
helpers, unused overloads, config knobs nothing reads, or abstraction layers with a single caller.
Dead and speculative code isn't free: it hides which paths are truly exercised, rots out of sync
with the code that matters, and misleads the next reader into thinking it's used. Follow YAGNI —
solve the problem in front of you; when a genuine second caller appears, add the generalization
then. Prefer deleting unused code over keeping it "for later"; if something must stay unused for a
staged milestone, say why in a comment. (A parameter you accept but never read, or an `enum` case
no `switch` handles, is a smell — remove it or wire it up.)

```cpp
// Bad -- speculative flags nothing passes, and an unused "future" path:
std::string encode_session(const Session& s, bool pretty = true, int indent = 2);

// Good -- exactly the surface the current callers use:
std::string encode_session(const Session& s);
```

## 3. Data-file integrity

The files under `data/` are the source of truth (ADR-001) and are the user's real, irreplaceable
training history. Treat them accordingly:

3.1. **Round-tripping must be lossless.** Decode → encode of any existing file must reproduce it
byte for byte. Unmodelled keys are not noise to drop — `wl_fmt`'s `canonicalize` exists precisely
so fields the typed model doesn't know about survive a rewrite.

3.2. **Writes are atomic.** Write to a temporary in the destination directory and rename over the
target, so an interrupted write can never truncate an existing session file.

3.3. **Never bulk-rewrite `data/` as a side effect of an unrelated change.** A reformat is its own
commit, run through `wl_fmt`, reviewed as a diff. If a change alters the writer's output, say so
explicitly and show what the diff on the corpus would be.

3.4. **A change to the model, the notation parser or the writer is not done until `wl_verify`
passes** (§4.1) — it is the acceptance gate over the real corpus, not a smoke test.

## 4. Verification

4.1. **Build and run the gate.** No Swift toolchain is needed for the C++ half:

```sh
cmake --preset gcc            # or: cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build --preset gcc -j
./builds/gcc/bin/wl_verify    # must print ALL CHECKS PASSED
ctest --preset gcc
```

Report the actual output. "It builds" is not "it passes".

4.2. **Before committing, run `clang-tidy`.** `cmake --build builds/gcc --target tidy` — checks
come from the top-level `.clang-tidy` (`clang-analyzer-*`, `bugprone-*`, `cppcoreguidelines-*`,
`performance-*`, `modernize-*`). Your changes must add **no new diagnostics**. A new finding in
code you added is yours to fix, not to silence: only exclude a check with evidence from an actual
`tidy` run, and record the reasoning in `.clang-tidy` the way the existing exclusions do. There is
no `.clang-format` config in this tree — match the surrounding file's style by hand.

4.3. **Run the sanitizer presets** for anything touching parsing, indexing, or the future UI/data
plane — `gcc-asan-ubsan` (heap/stack/UB/leaks) and `gcc-tsan` (data races):

```sh
cmake --preset gcc-asan-ubsan && cmake --build --preset gcc-asan-ubsan && ctest --preset gcc-asan-ubsan
cmake --preset gcc-tsan       && cmake --build --preset gcc-tsan       && ctest --preset gcc-tsan
```

They are separate build trees under `builds/` (their runtimes are mutually exclusive; `clang-*`
variants exist too). Offer this to the user even when you cannot run it yourself.

4.4. **Both cores must stay in step.** `core/` is a port of `app/Sources/WorkoutLogCore`; they
are not wired together yet, so a behaviour change on one side silently diverges from the other.
When you change one, either change the other or state plainly which side is now ahead. XCTest does
not run in this environment (Command Line Tools only) — say so rather than implying the Swift
tests passed.

4.5. **Verify the failure path, not just the happy path.** For a parser, a store or a resolver,
the round-trip passing proves the easy half. Raise your effort on anything touching the notation
parser, the decoder or the writer: feed it the malformed, the missing-key, the wrong-type and the
empty-optional case, and check it fails the way §1.1.3 says it should — a message naming the key,
a recorded `LoadFailure`, a `std::nullopt` — rather than throwing something opaque, reading a
default, or corrupting a file. `wl_verify` is where that assertion belongs.

4.6. **Never claim a check you didn't run.** If you can't run the macOS build, the Swift tests, or
a sanitizer preset, say which and flag it as unverified.

## 5. Documentation cross-references

5.1. **Name the thing, then the id.** The architecture decisions live in `docs/05-architecture.md`
as `ADR-00N`. A bare `ADR-004` is a dead end for a reader who doesn't have the file open — write
"no business logic in the UI (ADR-004 in `docs/05-architecture.md`)". An id is a pointer, not an
explanation: if the sentence stops making sense when the id is deleted, rewrite the sentence
rather than leaning harder on the citation.

5.2. **Never cite a document by line number.** `docs/05-architecture.md:120` is wrong after the
next edit; cite the section or the ADR id. `core/src/json.cpp:42` is right and expected — line
numbers are for source, section names are for documents.

5.3. **Don't invent ids.** Reference only ADRs that exist in `docs/05-architecture.md` and phases
that exist in `docs/06-roadmap.md`. If a decision isn't recorded anywhere, state the constraint in
plain terms instead of citing something a reader can't resolve. This already went wrong once:
`core/include/workoutlog/json.hpp`, `tools/wl_fmt/main.cpp` and `tools/wl_verify/main.cpp` all cite
an **ADR-007** that `docs/05-architecture.md` does not contain (it ends at ADR-006) — either write
that ADR or drop the citation, and don't add another like it.
