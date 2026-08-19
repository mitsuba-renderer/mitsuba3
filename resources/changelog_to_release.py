#!/usr/bin/env python3
"""Turn a section of docs/release_notes.rst into GitHub release notes.

Extracts the entry for a given version and rewrites the RST markup as
Markdown, which is what the GitHub release page expects::

    python3 resources/changelog_to_release.py 3.9.1

The converted text is printed to stdout. Pass ``--create`` to hand it
straight to ``gh release create`` for the matching ``vX.Y.Z`` tag, and
``--draft`` to leave that release unpublished for review.

Sphinx roles turn into links pointing at the documentation built for this
very version, so the notes keep working once later releases move the
``latest`` alias forward. References to Dr.Jit symbols instead point at the
Dr.Jit release that this version depends on, as recorded in pyproject.toml.

Paths are resolved relative to the repository root (the parent of this
script's directory), so the script can be invoked from anywhere.

This started out as Dr.Jit's resources/changelog_to_release.py and was
adapted to the conventions used by Mitsuba's release notes.
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = REPO_ROOT / "docs/release_notes.rst"
DOCS = REPO_ROOT / "docs"
SRC = REPO_ROOT / "src"

RTD = "https://mitsuba.readthedocs.io/en"
DRJIT_RTD = "https://drjit.readthedocs.io/en"

# Plugin categories, mirroring the section list in docs/generate_plugin_doc.py.
# Each one is both a directory under src/ and a generated plugins_*.rst page.
SECTIONS = ("shapes", "bsdfs", "media", "phase", "emitters", "sensors",
            "textures", "spectra", "integrators", "samplers", "films",
            "rfilters", "volumes")

# Section heading, e.g. "Mitsuba 3.9.1" over a row of dashes. Unlike Dr.Jit,
# the date lives on its own line below, as "*August 7, 2026*".
HEADING_RE = re.compile(r"^Mitsuba (\d+\.\d+\.\d+)\s*$")
UNDERLINE_RE = re.compile(r"^-{3,}\s*$")
DATE_RE = re.compile(r"^\*(.+)\*\s*$")

# Sphinx label definition, e.g. ".. _emitter-envmap:".
LABEL_RE = re.compile(r"^\.\. _([\w.-]+):\s*$", re.MULTILINE)

# Python domain role with an optional explicit target:
#   :py:func:`dr.median() <drjit.median>`   or   :py:class:`mitsuba.Shape`
PY_ROLE_RE = re.compile(r":py:(?:func|class|attr|meth|mod):`([^`<]+?)(?:\s*<([^>]+)>)?`")

# C++ domain role. The C++ API is not part of the Python reference, so these
# become plain code spans.
CPP_ROLE_RE = re.compile(r":cpp:[a-z]+:`([^`<]+?)(?:\s*<([^>]+)>)?`")

# Cross-reference to a labelled section, e.g. :ref:`envmap <emitter-envmap>`.
REF_ROLE_RE = re.compile(r":ref:`([^`<]+?)(?:\s*<([^>]+)>)?`")

# Citation, e.g. :cite:`Schuessler2017Microfacet`. Sphinx generates the anchor
# from a running counter, which cannot be reproduced here, so link the
# bibliography page as a whole.
CITE_ROLE_RE = re.compile(r":cite:`([^`]+)`")

# RST hyperlink: `TEXT <URL>`__. TEXT may wrap across lines.
LINK_RE = re.compile(r"`([^`<]+?)\s*<([^>]+)>`__", re.DOTALL)

# Inline literal: ``code``. Must run after the roles, which also use backticks.
LITERAL_RE = re.compile(r"``([^`]+)``")

CODE_BLOCK_RE = re.compile(r"^(\s*)\.\. code-block:: *(\w*)\s*$")

# Start of a list item, capturing the indentation and the bullet marker.
BULLET_RE = re.compile(r"^(\s*)([-*]\s+)(.*)$")

# Dr.Jit pin in pyproject.toml, e.g. 'drjit==1.5.0' or 'drjit==1.6.0.dev1'.
DRJIT_PIN_RE = re.compile(r"""drjit==(\d+\.\d+\.\d+(?:\.[a-zA-Z]+\d*)?)""")


def extract(text: str, version: str) -> tuple[str, str]:
    """Return the (body, date) of the release notes entry for ``version``."""
    lines = text.splitlines()
    start = date = None

    for i, line in enumerate(lines):
        m = HEADING_RE.match(line)
        if not m or i + 1 >= len(lines) or not UNDERLINE_RE.match(lines[i + 1]):
            continue

        if start is None and m.group(1) == version:
            # Skip the heading and its underline, then the date line if present
            start = i + 2
            if start < len(lines):
                d = DATE_RE.match(lines[start])
                if d:
                    date, start = d.group(1), start + 1
        elif start is not None:
            return "\n".join(lines[start:i]).strip("\n"), date or "unknown date"

    if start is None:
        raise SystemExit(f"error: no release notes entry for version {version}")

    return "\n".join(lines[start:]).strip("\n"), date or "unknown date"


def drjit_version(version: str) -> str:
    """Dr.Jit release that ``version`` depends on, for documentation links."""
    try:
        pyproject = subprocess.run(
            ["git", "-C", str(REPO_ROOT), "show", f"v{version}:pyproject.toml"],
            capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        path = REPO_ROOT / "pyproject.toml"
        pyproject = path.read_text() if path.is_file() else ""

    m = DRJIT_PIN_RE.search(pyproject)
    return f"v{m.group(1)}" if m else "stable"


def label_map() -> dict[str, str]:
    """Map each Sphinx label to the page that defines it, without its suffix."""
    labels: dict[str, str] = {}

    # Plugin labels live in the C++ sources and end up on a generated page
    for section in SECTIONS:
        for path in sorted((SRC / section).rglob("*.cpp")):
            for m in LABEL_RE.finditer(path.read_text(errors="replace")):
                labels.setdefault(m.group(1), f"src/generated/plugins_{section}")

    # Everything else is defined by a documentation page directly
    for path in sorted(DOCS.rglob("*.rst")):
        if "generated" in path.relative_to(DOCS).parts:
            continue
        page = path.relative_to(DOCS).with_suffix("").as_posix()
        for m in LABEL_RE.finditer(path.read_text(errors="replace")):
            labels.setdefault(m.group(1), page)

    return labels


def convert(body: str, version: str, labels: dict[str, str]) -> str:
    base = f"{RTD}/v{version}"
    drjit_base = f"{DRJIT_RTD}/{drjit_version(version)}"

    def py_role(m: re.Match) -> str:
        text, target = m.group(1), m.group(2) or m.group(1)

        # Resolve the two shorthands used throughout the notes
        if target.startswith("dr."):
            target = "drjit." + target[3:]
        elif target.startswith("mi."):
            target = "mitsuba." + target[3:]

        if target.startswith("drjit."):
            return f"[`{text}`]({drjit_base}/reference.html#{target})"
        if not target.startswith("mitsuba."):
            target = f"mitsuba.{target}"
        return f"[`{text}`]({base}/src/api_reference.html#{target})"

    def cpp_role(m: re.Match) -> str:
        return f"`{m.group(1)}`"

    def ref_role(m: re.Match) -> str:
        text, label = m.group(1), m.group(2)
        if label is None:
            return text
        page = labels.get(label)
        if page is None:
            return text
        return f"[{text}]({base}/{page}.html#{label})"

    def cite_role(m: re.Match) -> str:
        return f"[{m.group(1)}]({base}/zz_bibliography.html)"

    body = PY_ROLE_RE.sub(py_role, body)
    body = CPP_ROLE_RE.sub(cpp_role, body)
    body = REF_ROLE_RE.sub(ref_role, body)
    body = CITE_ROLE_RE.sub(cite_role, body)
    body = LINK_RE.sub(lambda m: f"[{re.sub(r'\s+', ' ', m.group(1)).strip()}]({m.group(2).strip()})", body)
    body = LITERAL_RE.sub(lambda m: f"`{m.group(1)}`", body)

    return unwrap(fence_code_blocks(body))


def unwrap(body: str) -> str:
    """Join wrapped lines so that each paragraph occupies a single line.

    GitHub renders release notes with hard line breaks, so the wrapping of
    the release notes would otherwise survive into the published notes.
    """
    out: list[str] = []
    prefix: str | None = None
    parts: list[str] = []
    fenced = False

    def flush() -> None:
        nonlocal prefix, parts
        if prefix is not None:
            out.append(prefix + " ".join(parts))
        prefix, parts = None, []

    for line in body.splitlines():
        stripped = line.strip()

        if stripped.startswith("```"):
            flush()
            fenced = not fenced
            out.append(line)
            continue

        if fenced:
            out.append(line)
            continue

        if not stripped:
            flush()
            out.append("")
            continue

        m = BULLET_RE.match(line)
        if m:
            flush()
            prefix, parts = m.group(1) + m.group(2), [m.group(3).strip()]
        elif prefix is None:
            prefix, parts = line[:len(line) - len(line.lstrip())], [stripped]
        else:
            parts.append(stripped)

    flush()

    return "\n".join(out).strip("\n")


def fence_code_blocks(body: str) -> str:
    """Replace ``.. code-block:: lang`` directives with Markdown fences."""
    out: list[str] = []
    lines = body.splitlines()
    i = 0

    while i < len(lines):
        m = CODE_BLOCK_RE.match(lines[i])
        if not m:
            out.append(lines[i])
            i += 1
            continue

        indent, lang = m.group(1), m.group(2)
        i += 1
        while i < len(lines) and not lines[i].strip():
            i += 1

        block: list[str] = []
        while i < len(lines):
            line = lines[i]
            if line.strip() and not line.startswith(indent + " "):
                break
            block.append(line)
            i += 1

        while block and not block[-1].strip():
            block.pop()

        # Strip the directive's extra indentation, keep the outer level so the
        # fence stays inside its list item.
        strip = min((len(b) - len(b.lstrip()) for b in block if b.strip()),
                    default=len(indent))
        out.append(f"{indent}```{lang}")
        out.extend(indent + b[strip:] if b.strip() else "" for b in block)
        out.append(f"{indent}```")

        if i < len(lines) and lines[i].strip():
            out.append("")

    return "\n".join(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", help="release to convert, e.g. 3.9.1")
    parser.add_argument("--create", action="store_true",
                        help="create the GitHub release via 'gh'")
    parser.add_argument("--draft", action="store_true",
                        help="with --create, leave the release unpublished")
    args = parser.parse_args()

    if not CHANGELOG.is_file():
        print(f"error: {CHANGELOG} not found", file=sys.stderr)
        return 2

    body, date = extract(CHANGELOG.read_text(), args.version)
    notes = convert(body, args.version, label_map())

    if not args.create:
        print(notes)
        return 0

    tag = f"v{args.version}"
    cmd = ["gh", "release", "create", tag, "--verify-tag",
           "--title", tag, "--notes-file", "-"]
    if args.draft:
        cmd.append("--draft")

    result = subprocess.run(cmd, input=notes, text=True)
    if result.returncode:
        return result.returncode

    print(f"created release {tag} ({date})", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
