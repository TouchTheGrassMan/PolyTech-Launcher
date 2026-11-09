#!/usr/bin/env python3
"""Obsidium: refresh the translations built into the launcher.

The launcher no longer downloads translations: they live in launcher/resources/translations
as Qt resources. This script rebuilds that folder from the Prism Launcher translation sources
(https://github.com/PrismLauncher/Translations, one <lang>.ts per language):

    git clone --depth 1 https://github.com/PrismLauncher/Translations ../Translations
    python tools/update_translations.py ../Translations --lrelease C:/Qt/6.10.2/msvc2022_64/bin/lrelease.exe

It compiles every .ts into mmc_<lang>.qm, writes index_v2.json (the list of languages with
how much of each is translated) and translations.qrc.
"""

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "launcher" / "resources" / "translations"


def stats(ts: Path):
    text = ts.read_text(encoding="utf-8")
    translated = untranslated = 0
    for message in re.findall(r"<message[\s>].*?</message>", text, re.S):
        tag = re.search(r"<translation([^>]*)>", message)
        attrs = tag.group(1) if tag else ""
        if "vanished" in attrs or "obsolete" in attrs:
            continue
        if "unfinished" in attrs:
            untranslated += 1
        else:
            translated += 1
    return translated, untranslated


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("sources", type=Path, help="folder with the <lang>.ts files")
    parser.add_argument("--lrelease", default=shutil.which("lrelease") or "lrelease")
    args = parser.parse_args()

    files = sorted(args.sources.glob("*.ts"))
    if not files:
        sys.exit(f"no .ts files in {args.sources}")

    if OUT.exists():
        for old in OUT.iterdir():
            old.unlink()
    OUT.mkdir(parents=True, exist_ok=True)

    languages = {}
    for ts in files:
        key = ts.stem
        qm = OUT / f"mmc_{key}.qm"
        subprocess.run([args.lrelease, "-silent", str(ts), "-qm", str(qm)], check=True)
        data = qm.read_bytes()
        translated, untranslated = stats(ts)
        languages[key] = {
            "file": qm.name,
            "sha1": hashlib.sha1(data).hexdigest(),
            "size": len(data),
            "translated": translated,
            "untranslated": untranslated,
            "fuzzy": 0,
        }

    index = {"file_type": "MMC-TRANSLATION-INDEX", "version": 2, "languages": languages}
    (OUT / "index_v2.json").write_text(json.dumps(index, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")

    entries = "\n".join(f"        <file>{name}</file>" for name in ["index_v2.json"] + [v["file"] for v in languages.values()])
    (OUT / "translations.qrc").write_text(
        '<!DOCTYPE RCC>\n<RCC version="1.0">\n    <qresource prefix="/translations">\n' + entries + "\n    </qresource>\n</RCC>\n",
        encoding="utf-8",
    )
    print(f"{len(languages)} languages -> {OUT}")


if __name__ == "__main__":
    main()
