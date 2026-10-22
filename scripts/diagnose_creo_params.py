#!/usr/bin/env python3
"""Dump string candidates near AUTHORS/TAGS/... for a Creo .prt file.

Run on Windows or Linux:
  python scripts/diagnose_creo_params.py path\\to\\detailstest.prt.5
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


PARAMS = [b"AUTHORS", b"TAGS", b"COMMENTS", b"TITLE", b"SUBJECT"]


def title_case(name: bytes) -> str:
    s = name.decode("ascii")
    return s[:1] + s[1:].lower()


def main() -> int:
    if len(sys.argv) < 2:
        print("Usage: diagnose_creo_params.py <creo-file>")
        return 2
    path = Path(sys.argv[1])
    data = path.read_bytes()
    print(f"file={path} size={len(data)}")
    print(f"ugc={data[:5]!r}")

    for needle in (b"BRO", b"Product Line", b"Descrip line 1", b"Descrip line 2"):
        positions = []
        start = 0
        while True:
            j = data.find(needle, start)
            if j < 0:
                break
            positions.append(j)
            start = j + 1
        print(f"literal {needle!r} count={len(positions)} offsets={positions[:10]}")

    for name in PARAMS:
        print(f"\n==== {name.decode()} (title-case would be {title_case(name)!r}) ====")
        start = 0
        hit = 0
        while True:
            j = data.find(name + b"\x00", start)
            if j < 0:
                break
            hit += 1
            window = data[j : j + 160]
            strings = [m.decode("ascii", "replace") for m in re.findall(rb"[\x20-\x7e]{2,}", window)]
            print(f"  hit@{j}: {strings}")
            print("   hex:", window[:48].hex(" "))
            start = j + 1
        print(f"  hits={hit}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
