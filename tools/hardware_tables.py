#!/usr/bin/env python3
"""Builds the launcher's hardware rating tables from the benchmark databases.

    python tools/hardware_tables.py gpus.json cpus.json

writes launcher/resources/hardware/gpus.tsv and cpus.tsv. The launcher uses them to suggest a
graphics preset on the first launch of the pack (launcher/minecraft/HardwareRating.cpp). Only
what the launcher needs is kept: the lookup key, the score and the number of samples (to pick
the most common of several close variants).

    gpus.tsv: match_key  g3d_mark  samples
    cpus.tsv: model_key  cpu_mark  single_thread  samples   (model_key is the name without "@ 1.60GHz")
"""
import json
import pathlib
import sys

OUT = pathlib.Path(__file__).resolve().parent.parent / "launcher" / "resources" / "hardware"


def table(entries, key, columns):
    best = {}
    for e in entries:
        k = (e.get(key) or "").strip()
        if not k or any(e.get(c) is None for c in columns):
            continue
        # a key can repeat (the same chip at different clocks): keep the most tested one
        if k not in best or (e.get("samples") or 0) > (best[k].get("samples") or 0):
            best[k] = e
    lines = []
    for k in sorted(best):
        e = best[k]
        lines.append("\t".join([k] + [str(int(e[c])) for c in columns] + [str(int(e.get("samples") or 0))]))
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    gpus = json.loads(pathlib.Path(sys.argv[1]).read_text(encoding="utf-8"))
    cpus = json.loads(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8"))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "gpus.tsv").write_text(table(gpus, "match_key", ["g3d_mark"]), encoding="utf-8", newline="\n")
    (OUT / "cpus.tsv").write_text(table(cpus, "model_key", ["cpu_mark", "single_thread"]), encoding="utf-8", newline="\n")
    print("gpus.tsv and cpus.tsv written to", OUT)


if __name__ == "__main__":
    main()
