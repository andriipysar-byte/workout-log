# WorkoutLog2 — agent guide

## Comments

Write a comment ONLY when the code uses a trick or a solution that is not obvious
from reading it — a non-obvious algorithm or invariant, a workaround for a platform
quirk, or *why* an approach was chosen over an alternative. Do NOT write comments
that restate what the code plainly does, label sections, or narrate steps. Prefer
clear names over comments. When in doubt, leave it out.

## Build & verify

- Command Line Tools only (no full Xcode): `swift test` / XCTest do not run here.
  Build with `cd app && swift build`; verify with `swift run wl-verify`.
- Run the app: `WORKOUTLOG_DATA=../data swift run WorkoutLogApp`.
- Data files in `data/` are the source of truth (ADR-001); the core carries all
  domain logic and the SwiftUI layer stays pure presentation (ADR-004).

### C++ core (Linux/Windows port in progress)

The domain core is being ported to C++20 under `core/` so Linux/Windows can share it
(the Swift core above is still what macOS runs today — the two aren't wired together
yet). No Swift toolchain is required for this half:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/bin/wl_verify                 # acceptance gate; must print ALL CHECKS PASSED
./build/bin/wl_map <session.json> [set_count|rep_volume|tonnage] [out.svg]
./build/bin/wl_fmt [--check] <path.json...>   # canonical JSON writer, data/ already reformatted
```
