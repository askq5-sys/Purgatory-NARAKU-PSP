"""Read main.c the way the compiler sees it.

main.c includes its runtime modules (runtime/*.inc) in order. Tests that
search or extract runtime code use main_source() to get that code as one
text, with each module's own includes expanded as well.
"""
from pathlib import Path
import re

_INCLUDE = re.compile(r'^#include "(runtime/)?([A-Za-z0-9_]+\.inc)"$', re.M)
_LOCAL = re.compile(r'^#include "([A-Za-z0-9_]+\.(?:h|inc))"$', re.M)
_HEADER = re.compile(r'\A/\* [^\n]*\n \* Part of main\.c: included there in order, not a standalone unit\. \*/\n\n')


def _expand(text, runtime):
    def repl(match):
        path = runtime / match.group(2)
        if not path.exists():
            return match.group(0)
        body = _HEADER.sub('', path.read_text())
        # Modules include their neighbours by bare name; restore the
        # runtime/ prefix main.c itself uses.
        body = _LOCAL.sub(
            lambda m: f'#include "runtime/{m.group(1)}"'
            if (runtime / m.group(1)).exists() else m.group(0), body)
        return _expand(body, runtime).rstrip('\n')
    return _INCLUDE.sub(repl, text)


def main_source(root):
    root = Path(root)
    return _expand((root / 'main.c').read_text(), root / 'runtime')
