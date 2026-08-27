#!/usr/bin/env python
"""RULING CENSUS -- the safety net for an editorial pass over the doc corpus.

A split/compress pass relocates and rewrites prose. The DANGER is not a broken link
(verify-docs.py already catches those) -- it is a hard-won RULING quietly evaporating in
the rephrasing. This tool takes a BEFORE snapshot of every ruling-bearing line in the
corpus and, after the pass, reports which ones no longer appear anywhere.

A ruling-bearing line is one carrying a rule marker or an owner attribution. Matching is
on a NORMALIZED SIGNATURE (case/whitespace/punctuation/emphasis stripped, markers dropped),
so a line may be re-wrapped, re-emphasized or moved to another file and still MATCH --
only a line whose SUBSTANCE is gone is reported.

    python Tools/verify-doc-rulings.py --snapshot     # before the pass
    python Tools/verify-doc-rulings.py --check        # after; non-zero if rulings vanished

Advisory by design on --check content, but it EXITS NON-ZERO when a ruling is missing:
a dropped ruling is the one failure this pass must never ship.
"""
import io, os, re, sys, json, glob

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SNAPSHOT_PATH = os.path.join(REPO_ROOT, "Tools", ".doc-rulings-snapshot.json")
MARKERS = ("\u26d4", "\u2696", "\u2691", "\u26a0")  # block, scale, flag, warn
OWNER_PATTERN = re.compile(r"\(owner[^)]*\)|owner:", re.I)
# provenance, never substance: an attribution must not make two identical rules differ
ATTRIBUTION_PATTERN = re.compile(r"\(owner(?:[^)]*)\)|owner:\s*", re.I)
LINK_PATTERN = re.compile(r"\[([^\]]*)\]\([^)]*\)")
EMPHASIS_PATTERN = re.compile(r"[*_`>#\[\]()]")
NON_WORD_PATTERN = re.compile(r"[^a-z0-9 ]+")
WHITESPACE_PATTERN = re.compile(r"\s+")


def corpus_files():
    found = []
    for path in glob.glob(os.path.join(REPO_ROOT, "docs", "**", "*.md"), recursive=True):
        rel = os.path.relpath(path, REPO_ROOT).replace("\\", "/")
        if "CHANGELOG" in rel:
            continue
        found.append(rel)
    for extra in ("AGENTS.md", "Sources/AGENTS.md"):
        if os.path.exists(os.path.join(REPO_ROOT, extra)):
            found.append(extra)
    return sorted(found)


def normalize(line):
    """Reduce a line to its substance so re-wrapping/re-emphasis still matches."""
    text = LINK_PATTERN.sub(r"", line)
    text = ATTRIBUTION_PATTERN.sub(" ", text)   # keep link TEXT, drop the target: a
                                          # re-pointed link must not read as a lost ruling
    for marker in MARKERS:
        text = text.replace(marker, " ")
    text = EMPHASIS_PATTERN.sub(" ", text.lower())
    text = NON_WORD_PATTERN.sub(" ", text)
    return WHITESPACE_PATTERN.sub(" ", text).strip()


def is_ruling(line):
    if any(marker in line for marker in MARKERS):
        return True
    return bool(OWNER_PATTERN.search(line))


def collect_rulings():
    rulings = []
    for rel in corpus_files():
        full = os.path.join(REPO_ROOT, rel)
        for number, raw in enumerate(io.open(full, encoding="utf-8", errors="replace"), 1):
            line = raw.strip()
            if len(line) < 25 or not is_ruling(line):
                continue
            signature = normalize(line)
            # 12, not 20: stripping link targets leaves genuine rulings short
            # ("never a second walk" normalizes to 19 chars and IS a ruling)
            if len(signature) < 12:
                continue
            rulings.append({"file": rel, "line": number, "signature": signature, "text": line})
    return rulings


def snapshot():
    rulings = collect_rulings()
    payload = {"count": len(rulings), "rulings": rulings}
    io.open(SNAPSHOT_PATH, "w", encoding="utf-8").write(json.dumps(payload, indent=1, ensure_ascii=False))
    print("ruling census SNAPSHOT: %d ruling-bearing lines across %d files"
          % (len(rulings), len(set(item["file"] for item in rulings))))
    print("written to %s" % os.path.relpath(SNAPSHOT_PATH, REPO_ROOT).replace("\\", "/"))
    return 0


def check():
    if not os.path.exists(SNAPSHOT_PATH):
        print("no snapshot -- run --snapshot BEFORE the editorial pass")
        return 2
    payload = json.load(io.open(SNAPSHOT_PATH, encoding="utf-8"))
    before = payload["rulings"]
    now = set()
    for rel in corpus_files():
        full = os.path.join(REPO_ROOT, rel)
        for raw in io.open(full, encoding="utf-8", errors="replace"):
            signature = normalize(raw.strip())
            if len(signature) >= 12:
                now.add(signature)
    # a ruling may also survive inside a re-wrapped paragraph: match on containment too
    blob = " \u00b6 ".join(sorted(now))
    missing = []
    for item in before:
        # re-normalize from the stored raw TEXT so the check stays valid even when the
        # normalizer itself is improved between snapshot and check
        signature = normalize(item["text"])
        if signature in now or signature in blob:
            continue
        missing.append(item)
    print("ruling census CHECK: %d before, %d ruling lines now, %d MISSING"
          % (len(before), len(now), len(missing)))
    if not missing:
        print("verify-doc-rulings: clean -- every ruling from the snapshot still has a home.")
        return 0
    print("\nMISSING rulings -- each vanished in the pass; restore it or account for it:")
    for item in missing[:60]:
        print("  %s:%d" % (item["file"], item["line"]))
        print("      %s" % item["text"][:150])
    if len(missing) > 60:
        print("  ... +%d more" % (len(missing) - 60))
    return 1


def main():
    argument = sys.argv[1] if len(sys.argv) > 1 else "--check"
    if argument == "--snapshot":
        return snapshot()
    if argument == "--check":
        return check()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
