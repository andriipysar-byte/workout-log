#!/usr/bin/env python3
"""Generate dated session-stub JSON files from a cycle definition in cycles.json.

Each session in the cycle is a template of planned blocks with no weights. This
expands that template onto the real Tue/Thu/Sun calendar (from `start_date`) and
writes one schema-valid stub per session into data/, ready to fill in at the gym.

Usage:
    python3 scripts/generate_cycle.py [--cycle hybrid-8] [--out data] [--force]

The planning-only helper fields `role` and `sets_reps` are consumed here and do
not appear in the emitted session files (which follow session.schema.json).
"""
from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from datetime import date, timedelta
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
WEEKDAY_ABBR = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]


def canonicalize(paths: list[Path]) -> None:
    """Best-effort pass through wl_fmt (core/src/json.cpp's writer) so freshly
    generated stubs already match the on-disk key order and don't need a separate
    reformat pass later. No-op if the C++ tools haven't been built yet."""
    if not paths:
        return

    candidates = [
        REPO / "builds" / "gcc" / "bin" / "wl_fmt",
        REPO / "build" / "bin" / "wl_fmt",
        REPO / "builds" / "clang" / "bin" / "wl_fmt",
    ]

    wl_fmt = None
    for cand in candidates:
        if cand.exists():
            wl_fmt = str(cand)
            break

    if not wl_fmt:
        wl_fmt = shutil.which("wl_fmt")

    if not wl_fmt:
        return

    subprocess.run([wl_fmt, *(str(p) for p in paths)], check=True)


def training_dates(start: date, training_days: list[str], count: int) -> list[date]:
    """The first `count` dates on/after `start` whose weekday is a training day."""
    out: list[date] = []
    d = start
    while len(out) < count:
        if WEEKDAY_ABBR[d.weekday()] in training_days:
            out.append(d)
        d += timedelta(days=1)
    return out


def block_from_template(tpl: dict) -> dict:
    """Translate a planning block template into a schema-valid session block."""
    t = tpl["type"]
    if t == "cardio":
        block = {"type": "cardio", "machine": tpl.get("machine", "")}
        if "duration_min" in tpl:
            block["duration_min"] = tpl["duration_min"]
        return block
    if t == "strength":
        block: dict = {"type": "strength", "exercise": tpl["exercise"]}
        block["sets"] = [{"reps": r} for r in tpl.get("sets_reps", [])]
        if tpl.get("notes"):
            block["notes"] = tpl["notes"]
        return block
    if t == "metcon":
        block = {"type": "metcon"}
        if "format" in tpl:
            block["format"] = tpl["format"]
        if "scheme" in tpl:
            block["scheme"] = tpl["scheme"]
        block["exercises"] = tpl.get("exercises", [])
        if tpl.get("notes"):
            block["notes"] = tpl["notes"]
        return block
    if t == "cooldown":
        return {"type": "cooldown"}
    raise ValueError(f"unknown block type: {t!r}")


def session_from_template(sess: dict, when: date) -> dict:
    out: dict = {
        "date": when.isoformat(),
        "cycle_day": sess["cycle_day"],
        "kind": "training",
    }
    if sess.get("session_notes"):
        out["notes"] = sess["session_notes"]
    out["blocks"] = [block_from_template(b) for b in sess["blocks"]]
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycle", default="hybrid-8", help="cycle id in cycles.json")
    ap.add_argument("--out", default="data", help="output dir (relative to repo)")
    ap.add_argument("--force", action="store_true", help="overwrite existing files")
    args = ap.parse_args()

    doc = json.loads((REPO / "cycles.json").read_text(encoding="utf-8"))
    cycle = next((c for c in doc["cycles"] if c["id"] == args.cycle), None)
    if cycle is None:
        raise SystemExit(f"cycle {args.cycle!r} not found in cycles.json")

    start = date.fromisoformat(cycle["start_date"])
    sessions = cycle["sessions"]
    dates = training_dates(start, cycle["training_days"], len(sessions))

    out_dir = REPO / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    written: list[Path] = []
    for sess, when in zip(sessions, dates):
        expected = sess.get("weekday")
        actual = WEEKDAY_ABBR[when.weekday()]
        if expected and expected != actual:
            raise SystemExit(
                f"{sess['cycle_day']}: calendar says {actual} but template says {expected}"
            )
        obj = session_from_template(sess, when)
        path = out_dir / f"{when.isoformat()}_{sess['cycle_day']}.json"
        if path.exists() and not args.force:
            print(f"skip (exists): {path.name}  — pass --force to overwrite")
            continue
        path.write_text(
            json.dumps(obj, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )
        written.append(path)
        print(f"wrote {path.name}")

    canonicalize(written)


if __name__ == "__main__":
    main()
