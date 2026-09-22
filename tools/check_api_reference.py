#!/usr/bin/env python3
"""Fails if a public function has no entry in the API reference.

Reads the function names declared in the public headers, reads the names the
reference page documents, and reports anything in the first set and not the
second. Keeps docs/user/api-reference.md from drifting as the API changes.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

HEADERS = [
    ROOT / "include" / "srnp" / "srnp_kernel.h",
    ROOT / "include" / "srnp" / "meta_pair_callback.hpp",
]

REFERENCE = ROOT / "docs" / "user" / "api-reference.md"

# Names deliberately left out of the reference, each with the reason why.
# Empty is the right state; an entry here is a documentation gap made visible.
SKIP: dict[str, str] = {}

# A free function declaration: a return type, then name(, at the start of a line.
DECLARATION = re.compile(
    r"^(?!\s)(?:[\w:<>,\s&*]+?)\b(\w+)\s*\([^;{]*\)\s*(?:const\s*)?;", re.MULTILINE
)

# Anything written as code in the page counts as documenting that name.
DOCUMENTED = re.compile(r"`[^`]*?\b(\w+)\s*\(")

# Not functions, or not part of the surface a user calls.
NOT_A_FUNCTION = {"if", "for", "while", "switch", "return", "sizeof", "throw"}


def declared_names() -> dict[str, str]:
    """Function name -> the header it came from."""
    names = {}
    for header in HEADERS:
        text = header.read_text()
        # Strip comments so a name inside one is not mistaken for a declaration.
        text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
        text = re.sub(r"//.*", "", text)
        for name in DECLARATION.findall(text):
            if name not in NOT_A_FUNCTION:
                names.setdefault(name, header.name)
    return names


def documented_names() -> set[str]:
    return set(DOCUMENTED.findall(REFERENCE.read_text()))


def main() -> int:
    declared = declared_names()
    if not declared:
        print(f"nothing parsed out of {[h.name for h in HEADERS]}, so this check "
              "is not testing anything", file=sys.stderr)
        return 1

    documented = documented_names()
    missing = sorted(
        name for name in declared if name not in documented and name not in SKIP
    )

    if missing:
        print(f"{REFERENCE.relative_to(ROOT)} is missing an entry for:", file=sys.stderr)
        for name in missing:
            print(f"  {name}()  declared in {declared[name]}", file=sys.stderr)
        print("\nDocument it, or add it to SKIP in this script with a reason.",
              file=sys.stderr)
        return 1

    print(f"all {len(declared)} public functions are documented")
    return 0


if __name__ == "__main__":
    sys.exit(main())
