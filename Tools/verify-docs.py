#!/usr/bin/env python
"""Verify the documentation's references into the repo still resolve -- links, anchors, symbols.

Two silent decay modes, one tool.

LINKS AND ANCHORS (hard failure). A doc corpus is portable to a wiki only if its
cross-references survive the move, and the ways they break are invisible: a relative path
whose depth is wrong, and an `#anchor` whose heading was renamed. Neither breaks a build nor
shows up in a diff -- the link simply stops going anywhere, and reads as authoritative until
someone clicks it. Anchors use GitHub's heading-slug algorithm.

SYMBOL CITATIONS (report only, never a failure). A spec's MODEL long outlives the code
pointers it cites, so docs drift into naming functions and headers that were renamed or
archived years of commits ago. Two verdicts, and the second is the dangerous one:
`MISSING` names something that exists nowhere, while `ARCHIVED-ONLY` survives in
`SourceArchive/` -- the doc is describing a world that has been deleted, which is precisely
the state that reads as current and gets built on.
This half only REPORTS, because a code span in prose is not unambiguously a symbol and a
checker that guessed would train people to ignore it. A human decides whether the citation
is repointed or the paragraph describing the dead world is deleted.

Usage:  python Tools/verify-docs.py
Exit code 1 when a link or anchor fails to resolve; the symbol census never fails the run.
"""

import os
import re
import sys
import unicodedata

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# The documentation corpus: the docs tree, the hosted catalogs, and the rule files at root.
SCAN_DIRECTORIES = ["docs", "indexes"]
SCAN_FILES = ["AGENTS.md", "CLAUDE.md", os.path.join("Sources", "AGENTS.md")]

LIVE_SOURCE_DIRS = [
    os.path.join(REPO_ROOT, "Sources"),
    os.path.join(REPO_ROOT, "Assets", "Python"),
    os.path.join(REPO_ROOT, "Tools"),
]
ARCHIVE_SOURCE_DIR = os.path.join(REPO_ROOT, "SourceArchive")

# Metasyntactic stand-ins a spec uses to describe a SHAPE rather than to cite a symbol.
# `SEVT_X` and `Class::m_field` name no real thing and never should.
PLACEHOLDER_QUALIFIERS = frozenset(["Class", "Type", "Foo", "Bar", "X", "Y", "N"])

# Docs whose PURPOSE is to name things that no longer exist. A dead symbol is correct here:
# the tombstone registry records killed approaches so they are not revived, and the parked
# backlog is carried as-is by an explicit ruling. Censusing them would report only noise.
SYMBOL_CENSUS_EXEMPT = (
    os.path.join("docs", "architecture", "superseded-ideas.md"),
    os.path.join("docs", "plans", "parked"),
)

# [text](target) -- target captured up to the closing paren, no nested parens supported.
LINK_PATTERN = re.compile(r"\[(?:[^\]]*)\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
HEADING_PATTERN = re.compile(r"^(#{1,6})\s+(.*?)\s*#*\s*$")
FENCE_PATTERN = re.compile(r"^\s*(```|~~~)")
CODE_SPAN_PATTERN = re.compile(r"`([^`\n]+)`")
# An explicit HTML anchor is a legitimate link target a heading slug cannot express --
# `<a id="x">` / `<a name="x">` -- so it counts alongside the headings.
EXPLICIT_ANCHOR_PATTERN = re.compile(r"<a\s+[^>]*?\b(?:id|name)\s*=\s*[\"']([^\"']+)[\"']", re.I)
IDENTIFIER_PATTERN = re.compile(r"[A-Za-z_][A-Za-z0-9_]*")

# Only shapes that are unambiguously identifiers in this codebase are censused.
ENGINE_CLASS_PATTERN = re.compile(r"^(Cv|Cy)[A-Z][A-Za-z0-9_]*$")
QUALIFIED_NAME_PATTERN = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)::([A-Za-z_][A-Za-z0-9_]*)$")
SOURCE_FILE_PATTERN = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\.(h|cpp|inl|py)$")
SPINE_EVENT_PATTERN = re.compile(r"^SEVT_[A-Z0-9_]+$")
SOURCE_EXTENSIONS = (".h", ".cpp", ".inl", ".def", ".bff", ".py")

# Links we deliberately do not resolve: external URLs and in-page protocol handlers.
EXTERNAL_PREFIXES = ("http://", "https://", "mailto:", "ftp://", "#!")


def slugify_heading(heading_text):
    """Reproduce GitHub's anchor slug for a heading's rendered text."""
    # Strip inline markdown that does not survive into the rendered heading text.
    text = re.sub(r"`([^`]*)`", r"\1", heading_text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    text = re.sub(r"[*_]{1,3}([^*_]+)[*_]{1,3}", r"\1", text)
    # GitHub trims the heading BEFORE discarding symbols, so a heading opening with an
    # emoji keeps the space behind it and slugs with a leading hyphen. Trimming after the
    # filter instead would swallow that hyphen and reject every such anchor as broken.
    text = text.lower().strip()

    kept_characters = []
    for character in text:
        if character in (" ", "-", "_"):
            kept_characters.append(character)
            continue
        category = unicodedata.category(character)
        # Letters and digits survive; punctuation, symbols and emoji do not.
        if category.startswith("L") or category.startswith("N") or category.startswith("M"):
            kept_characters.append(character)

    return "".join(kept_characters).replace(" ", "-")


def collect_markdown_files():
    """Every markdown file in the documentation corpus, as repo-relative paths."""
    collected = []
    for directory in SCAN_DIRECTORIES:
        absolute_directory = os.path.join(REPO_ROOT, directory)
        if not os.path.isdir(absolute_directory):
            continue
        for current_root, _directories, filenames in os.walk(absolute_directory):
            for filename in filenames:
                if filename.lower().endswith(".md"):
                    absolute_path = os.path.join(current_root, filename)
                    collected.append(os.path.relpath(absolute_path, REPO_ROOT))
    for relative_path in SCAN_FILES:
        if os.path.isfile(os.path.join(REPO_ROOT, relative_path)):
            collected.append(relative_path)
    return sorted(collected)


def read_file_text(relative_path):
    absolute_path = os.path.join(REPO_ROOT, relative_path)
    handle = open(absolute_path, "r", encoding="utf-8", errors="replace")
    try:
        return handle.read()
    finally:
        handle.close()


def anchors_in_file(relative_path, anchor_cache):
    """The set of heading anchors a file offers, with GitHub's duplicate disambiguation."""
    if relative_path in anchor_cache:
        return anchor_cache[relative_path]

    anchors = set()
    seen_counts = {}
    inside_fence = False

    for line in read_file_text(relative_path).splitlines():
        if FENCE_PATTERN.match(line):
            inside_fence = not inside_fence
            continue
        if inside_fence:
            continue
        anchors.update(EXPLICIT_ANCHOR_PATTERN.findall(line))
        match = HEADING_PATTERN.match(line)
        if not match:
            continue
        base_slug = slugify_heading(match.group(2))
        if not base_slug:
            continue
        occurrence = seen_counts.get(base_slug, 0)
        seen_counts[base_slug] = occurrence + 1
        anchors.add(base_slug if occurrence == 0 else "%s-%d" % (base_slug, occurrence))

    anchor_cache[relative_path] = anchors
    return anchors


def links_in_file(relative_path):
    """Every link target in a file, paired with the line it sits on, fenced code skipped."""
    found = []
    inside_fence = False
    for line_number, line in enumerate(read_file_text(relative_path).splitlines(), start=1):
        if FENCE_PATTERN.match(line):
            inside_fence = not inside_fence
            continue
        if inside_fence:
            continue
        for target in LINK_PATTERN.findall(line):
            found.append((line_number, target))
    return found


def build_symbol_index(source_directories):
    """Every identifier token and every filename appearing under the given source trees."""
    identifiers = set()
    filenames = set()
    for source_directory in source_directories:
        if not os.path.isdir(source_directory):
            continue
        for current_root, _directories, files in os.walk(source_directory):
            for filename in files:
                if not filename.lower().endswith(SOURCE_EXTENSIONS):
                    continue
                filenames.add(filename)
                handle = open(os.path.join(current_root, filename), "r",
                              encoding="utf-8", errors="replace")
                try:
                    identifiers.update(IDENTIFIER_PATTERN.findall(handle.read()))
                finally:
                    handle.close()
    return identifiers, filenames


def cited_symbols_in_file(relative_path):
    """Code spans that look like source symbols, paired with their line number."""
    cited = []
    inside_fence = False
    for line_number, line in enumerate(read_file_text(relative_path).splitlines(), start=1):
        if FENCE_PATTERN.match(line):
            inside_fence = not inside_fence
            continue
        if inside_fence:
            continue
        for span in CODE_SPAN_PATTERN.findall(line):
            span = re.sub(r"\(\s*\)$", "", span.strip()).strip()
            if not span or " " in span:
                continue
            if is_placeholder(span):
                continue
            if (ENGINE_CLASS_PATTERN.match(span)
                    or QUALIFIED_NAME_PATTERN.match(span)
                    or SOURCE_FILE_PATTERN.match(span)
                    or SPINE_EVENT_PATTERN.match(span)):
                cited.append((line_number, span))
    return cited


def is_placeholder(span):
    """True for a metasyntactic stand-in describing a shape rather than citing a symbol."""
    qualified_match = QUALIFIED_NAME_PATTERN.match(span)
    if qualified_match and qualified_match.group(1) in PLACEHOLDER_QUALIFIERS:
        return True
    if SPINE_EVENT_PATTERN.match(span) and len(span) - len("SEVT_") <= 1:
        return True
    return False


def classify_symbol(span, live_index, archive_index):
    """OK, ARCHIVED-ONLY (survives only in SourceArchive/), or MISSING (nowhere)."""
    live_identifiers, live_filenames = live_index
    archive_identifiers, archive_filenames = archive_index

    if SOURCE_FILE_PATTERN.match(span):
        if span in live_filenames:
            return "OK"
        return "ARCHIVED-ONLY" if span in archive_filenames else "MISSING"

    qualified_match = QUALIFIED_NAME_PATTERN.match(span)
    if qualified_match:
        parts = [qualified_match.group(1), qualified_match.group(2)]
        if all(part in live_identifiers for part in parts):
            return "OK"
        if all(part in archive_identifiers for part in parts):
            return "ARCHIVED-ONLY"
        return "MISSING"

    if span in live_identifiers:
        return "OK"
    return "ARCHIVED-ONLY" if span in archive_identifiers else "MISSING"


def run_symbol_census(markdown_files):
    """Report doc-cited symbols that no longer exist live. Never fails the run."""
    live_index = build_symbol_index(LIVE_SOURCE_DIRS)
    archive_index = build_symbol_index([ARCHIVE_SOURCE_DIR])

    dead_citations = []
    total_cited = 0

    for relative_path in markdown_files:
        if relative_path.startswith(SYMBOL_CENSUS_EXEMPT):
            continue
        for line_number, span in cited_symbols_in_file(relative_path):
            total_cited += 1
            verdict = classify_symbol(span, live_index, archive_index)
            if verdict != "OK":
                dead_citations.append((verdict, relative_path, line_number, span))

    print("")
    print("symbol citations checked: %d  (live index: %d identifiers)"
          % (total_cited, len(live_index[0])))

    if not dead_citations:
        print("symbol census: clean -- every cited symbol exists in the live tree.")
        return

    for wanted_verdict in ("ARCHIVED-ONLY", "MISSING"):
        entries = [entry for entry in dead_citations if entry[0] == wanted_verdict]
        if not entries:
            continue
        print("")
        if wanted_verdict == "ARCHIVED-ONLY":
            print("ARCHIVED-ONLY (%d) -- survives only in SourceArchive/; the doc describes a dead world:"
                  % len(entries))
        else:
            print("MISSING (%d) -- cited nowhere in the live tree:" % len(entries))
        for _verdict, relative_path, line_number, span in entries:
            print("  %s:%d  %s" % (relative_path, line_number, span))

    print("")
    print("symbol census is ADVISORY -- decide per entry whether to repoint the citation")
    print("or delete the passage describing the dead world. It never fails the run.")


def main():
    markdown_files = collect_markdown_files()
    anchor_cache = {}
    missing_targets = []
    missing_anchors = []
    total_links_checked = 0

    for relative_path in markdown_files:
        containing_directory = os.path.dirname(relative_path)

        for line_number, raw_target in links_in_file(relative_path):
            if raw_target.startswith(EXTERNAL_PREFIXES):
                continue

            target_path, _separator, anchor = raw_target.partition("#")
            target_path = target_path.strip()
            anchor = anchor.strip()

            if not target_path and not anchor:
                continue

            total_links_checked += 1

            if target_path:
                resolved = os.path.normpath(os.path.join(containing_directory, target_path))
                absolute_resolved = os.path.join(REPO_ROOT, resolved)
                if not os.path.exists(absolute_resolved):
                    missing_targets.append((relative_path, line_number, raw_target))
                    continue
                if not resolved.lower().endswith(".md"):
                    continue  # A directory or non-markdown file offers no headings to check.
            else:
                resolved = relative_path

            if anchor:
                if anchor not in anchors_in_file(resolved, anchor_cache):
                    missing_anchors.append((relative_path, line_number, raw_target))

    print("doc files scanned: %d" % len(markdown_files))
    print("intra-repo links checked: %d" % total_links_checked)

    if missing_targets:
        print("")
        print("BROKEN TARGET -- the linked file does not exist:")
        for source, line_number, target in missing_targets:
            print("  %s:%d  ->  %s" % (source, line_number, target))

    if missing_anchors:
        print("")
        print("BROKEN ANCHOR -- the file exists but has no heading with that slug:")
        for source, line_number, target in missing_anchors:
            print("  %s:%d  ->  %s" % (source, line_number, target))

    if missing_targets or missing_anchors:
        print("")
        print("verify-docs: FAILED -- %d broken target(s), %d broken anchor(s)."
              % (len(missing_targets), len(missing_anchors)))
        run_symbol_census(markdown_files)
        return 1

    print("verify-docs: links clean -- every intra-repo link and anchor resolves.")
    run_symbol_census(markdown_files)
    return 0


if __name__ == "__main__":
    sys.exit(main())
