#!/usr/bin/env python3
"""Normalize commit links in docs/release_notes.rst.

Rewrites RST commit references to a canonical form::

    `abc123 <https://github.com/org/repo/commit/<40-char SHA>>`__

- The link text is a 6-character short hash.
- The URL always contains the full 40-character SHA.
- Line-wrapped links (hash on one line, ``<URL>`` on the next) are joined.

Each commit is verified by running ``git rev-parse`` against a locally
cloned copy of the linked repository (Mitsuba itself and its submodules).
Unresolved commits are reported as broken. Backtick hashes that appear
without a URL are also flagged so the user can add links manually.

Paths are resolved relative to the repository root (the parent of this
script's directory), so the script can be invoked from anywhere::

    python3 docs/normalize_changelog_links.py
"""

from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CHANGELOG = REPO_ROOT / "docs/release_notes.rst"

# Map (GitHub org, repo) to a local clone path (relative to the repo root).
REPOS: dict[tuple[str, str], Path] = {
    ("mitsuba-renderer", "mitsuba3"):   REPO_ROOT,
    ("mitsuba-renderer", "drjit"):      REPO_ROOT / "ext/drjit",
    ("mitsuba-renderer", "drjit-core"): REPO_ROOT / "ext/drjit/ext/drjit-core",
    ("mitsuba-renderer", "nanothread"): REPO_ROOT / "ext/drjit/ext/drjit-core/ext/nanothread",
    ("wjakob", "nanobind"):             REPO_ROOT / "ext/nanobind",
}

# RST link: `TEXT <URL>`__. TEXT may contain whitespace / newlines (line wrap).
LINK_RE = re.compile(r"`([^`<]+?)\s*<([^>]+)>`__", re.DOTALL)

# Only `.../commit/<hash>` URLs are normalized. Pull-request and tag URLs are
# left alone.
COMMIT_URL_RE = re.compile(
    r"^https://github\.com/([\w.-]+)/([\w.-]+)/commit/([0-9a-f]+)/?$"
)

# Bare backtick hashes like `fdc7cae7` without a following URL. We only flag
# ones introduced by the word "commit"/"commits" to avoid false positives on
# arbitrary code snippets.
BARE_HASH_RE = re.compile(
    r"commits?\s+`([0-9a-f]{6,40})`(?!\s*<)", re.IGNORECASE
)

# Link text in this changelog is sometimes wrapped in square brackets, e.g.
# `[1a4bea2] <...>`__. Strip surrounding brackets before treating the text as a
# candidate hash.
HEX_RE = re.compile(r"^[0-9a-f]+$")
BRACKET_RE = re.compile(r"^\[(.*)\]$")


def git_resolve(repo_path: Path, rev: str) -> str | None:
    """Return the full 40-char SHA for ``rev`` in ``repo_path``, or None."""
    try:
        result = subprocess.run(
            ["git", "-C", str(repo_path), "rev-parse", "--verify",
             f"{rev}^{{commit}}"],
            check=True, capture_output=True, text=True,
        )
    except subprocess.CalledProcessError:
        return None
    sha = result.stdout.strip()
    return sha if len(sha) == 40 else None


def line_of(text: str, offset: int) -> int:
    return text.count("\n", 0, offset) + 1


def main() -> int:
    if not CHANGELOG.is_file():
        print(f"error: {CHANGELOG} not found (run from repo root)",
              file=sys.stderr)
        return 2

    for (org, repo), path in REPOS.items():
        if not (path / ".git").exists():
            print(f"error: no git repo at {path} (for {org}/{repo}); "
                  f"did you check out submodules?", file=sys.stderr)
            return 2

    text = CHANGELOG.read_text()

    broken: list[tuple[int, str, str]] = []  # (line, original, reason)

    def replace(m: re.Match) -> str:
        raw_text = m.group(1)
        url = m.group(2).strip()
        link_text = re.sub(r"\s+", "", raw_text)
        line = line_of(text, m.start())

        cm = COMMIT_URL_RE.match(url)
        if not cm:
            return m.group(0)  # not a commit URL — leave unchanged

        org, repo, url_hash = cm.group(1), cm.group(2), cm.group(3)
        repo_path = REPOS.get((org, repo))
        if repo_path is None:
            broken.append((line, m.group(0),
                           f"no local clone configured for {org}/{repo}"))
            return m.group(0)

        # Strip optional surrounding square brackets from the link text.
        bracket_m = BRACKET_RE.match(link_text)
        text_hash = bracket_m.group(1) if bracket_m else link_text

        candidates: list[str] = []
        if HEX_RE.match(text_hash) and len(text_hash) >= 4:
            candidates.append(text_hash)
        if url_hash not in candidates:
            candidates.append(url_hash)

        full_sha = None
        for cand in candidates:
            full_sha = git_resolve(repo_path, cand)
            if full_sha:
                break

        if full_sha is None:
            # Try the other configured repos to help diagnose mis-attribution.
            elsewhere: list[str] = []
            for (o2, r2), p2 in REPOS.items():
                if (o2, r2) == (org, repo):
                    continue
                for cand in candidates:
                    sha = git_resolve(p2, cand)
                    if sha:
                        elsewhere.append(f"{o2}/{r2}:{sha}")
                        break
            reason = (f"no commit matching text={text_hash!r} "
                      f"or url={url_hash!r} in {org}/{repo}")
            if elsewhere:
                reason += f" (found elsewhere: {', '.join(elsewhere)})"
            broken.append((line, m.group(0), reason))
            return m.group(0)

        short = full_sha[:6]
        new_url = f"https://github.com/{org}/{repo}/commit/{full_sha}"
        return f"`{short} <{new_url}>`__"

    new_text = LINK_RE.sub(replace, text)

    # Collect bare `<hash>` refs that have no URL.
    bare: list[tuple[int, str, str, list[tuple[str, str]]]] = []
    for m in BARE_HASH_RE.finditer(new_text):
        hash_ = m.group(1)
        line = line_of(new_text, m.start())
        matches: list[tuple[str, str]] = []
        for (org, repo), path in REPOS.items():
            sha = git_resolve(path, hash_)
            if sha:
                matches.append((f"{org}/{repo}", sha))
        bare.append((line, m.group(0), hash_, matches))

    changed = new_text != text
    if changed:
        CHANGELOG.write_text(new_text)

    print(f"{CHANGELOG}: {'updated' if changed else 'no changes'}")
    print()

    if broken:
        print(f"BROKEN OR UNRESOLVED COMMIT LINKS ({len(broken)}):")
        for line, orig, reason in broken:
            snippet = " ".join(orig.split())
            print(f"  line {line}: {snippet}")
            print(f"    -> {reason}")
        print()
    else:
        print("No broken commit links.\n")

    if bare:
        print(f"BARE COMMIT HASHES WITHOUT URL ({len(bare)}):")
        for line, snippet, hash_, matches in bare:
            print(f"  line {line}: {snippet!r}")
            if matches:
                for repo_name, sha in matches:
                    print(f"    found in {repo_name}: {sha}")
            else:
                print("    not found in any configured repo")
        print()
    else:
        print("No bare commit hashes found.\n")

    return 0 if not broken else 1


if __name__ == "__main__":
    sys.exit(main())
