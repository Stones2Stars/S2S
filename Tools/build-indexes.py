#!/usr/bin/env python
"""Generate the hosted index pages (docs/indexes/*_INDEX.html) from their markdown sources.

The .md is the source; the .html is what Pages serves. Hand-maintaining both let the pages drift (missing
entries, stale numbering, a dead Discord link), so the html body is generated and never edited by hand.

  python Tools/build-indexes.py           rewrite every *_INDEX.html from its .md
  python Tools/build-indexes.py --check   report the pages that differ from what would be generated (exit 1)

What a page keeps from its existing html: <head> (title, description, CSS), the header's emoji line, and the
closing </div></body></html>. Everything else is generated:

  - <h1> from the '# ' title; the header tagline from the text before the "X is measured in" sentence (in the
    leading blockquote, or the paragraph between the title and the first '---').
  - the preamble from that sentence onward.
  - one <article class="entry"> per '## [medal] N. Title -- score unit *(note)*' heading; the medal picks
    gold/silver/bronze, and a score above 100 adds 'over' to the meter.
  - paragraphs, '-' and 'N.' lists, '*Status: ...*' as the status line; **bold**, *italic*, `code`, links
    (.md -> .html).
  - '## Honorable Mentions' bullets ('- **Name** -- text') as the hm section.
  - the closing italic paragraphs as fineprint, except 'Sibling publications', which goes in the footer
    beside the fixed project line below.
"""
import io
import os
import re
import sys
import textwrap

INDEX_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "docs", "indexes")
DISCORD = "https://discord.gg/RyqQ3jAMx"
PROJECT_LINE = ('  <p><strong>Stones2Stars</strong> — a Civ4 / Caveman2Cosmos rework ·\n'
                '  <a href="https://github.com/Stones2Stars/S2S">github.com/Stones2Stars/S2S</a> ·\n'
                '  <a href="%s">%s</a></p>' % (DISCORD, DISCORD.replace("https://", "")))
MEDALS = {u"\U0001F947": "gold", u"\U0001F948": "silver", u"\U0001F949": "bronze"}
WIDTH = 94


def inline(text):
    codes = []

    def stash(match):
        body = match.group(1).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        codes.append("<code>%s</code>" % body)
        return "\x00%d\x00" % (len(codes) - 1)

    text = re.sub(r"`([^`]+)`", stash, text)
    text = text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
    text = re.sub(r"\[([^\]]+)\]\(([^)]+)\)",
                  lambda m: '<a href="%s">%s</a>' % (m.group(2).replace(".md", ".html"), m.group(1)), text)
    text = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", text)
    text = re.sub(r"(?<![\*\w])\*(?!\s)(.+?)(?<!\s)\*(?![\*\w])", r"<em>\1</em>", text)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], text)


def wrap(prefix, text, indent):
    return textwrap.fill(prefix + text, width=WIDTH, subsequent_indent=indent,
                         break_long_words=False, break_on_hyphens=False)


def blocks(lines):
    out, current = [], []
    for line in lines:
        if line.strip() in ("", "---"):
            if current:
                out.append(current)
            current = []
        else:
            current.append(line)
    if current:
        out.append(current)
    return out


def list_items(block):
    items = []
    for line in block:
        if re.match(r"^(- |\d+\. )", line):
            items.append(re.sub(r"^(- |\d+\. )", "", line))
        else:
            items[-1] += " " + line.strip()
    return items


def render_block(block):
    if re.match(r"^(- |\d+\. )", block[0]):
        tag = "ul" if block[0].startswith("- ") else "ol"
        rows = ["  <%s>" % tag]
        rows += [wrap("    <li>", inline(item) + "</li>", "    ") for item in list_items(block)]
        rows.append("  </%s>" % tag)
        return "\n".join(rows)
    text = " ".join(line.strip() for line in block)
    if text.startswith("*Status:") and text.endswith("*"):
        return wrap('  <p class="status">', inline(text[1:-1]) + "</p>", "  ")
    return wrap("  <p>", inline(text) + "</p>", "  ")


def split_intro(lines, first_entry):
    """Return (title, tagline, preamble paragraphs) from the lines above the first entry."""
    title = next(l[2:].strip() for l in lines if l.startswith("# "))
    quoted = [l[1:].strip() for l in lines[:first_entry] if l.startswith(">")]
    if quoted:
        paragraphs = [" ".join(b) for b in blocks(quoted)]
    else:
        body = [l for l in lines[:first_entry] if not l.startswith("# ")]
        paragraphs = [" ".join(l.strip() for l in b) for b in blocks(body)]
    joined = "\n\n".join(paragraphs)
    cut = re.search(r"(?:^|(?<=\. )|\n\n)(\w+ is measured in )", joined)
    tagline = joined[:cut.start(1)].strip()
    preamble = [p.strip() for p in joined[cut.start(1):].split("\n\n") if p.strip()]
    return title, tagline, preamble


def generate(md_text, html_text):
    newline = "\r\n" if "\r\n" in html_text else "\n"
    lines = md_text.replace("\r\n", "\n").split("\n")
    first_entry = next(i for i, l in enumerate(lines) if l.startswith("## "))
    title, tagline, preamble = split_intro(lines, first_entry)

    hm_start = next((i for i, l in enumerate(lines) if l.startswith("## Honorable Mentions")), None)
    entry_lines = [l for l in lines[first_entry:hm_start]] if hm_start else lines[first_entry:]
    tail_lines = lines[hm_start + 1:] if hm_start else []

    entries = []
    for line in entry_lines:
        if line.startswith("## "):
            entries.append([line, []])
        else:
            entries[-1][1].append(line)

    if not hm_start:
        # Without an Honorable Mentions section the closing italics hang off the last entry's body.
        last_body = entries[-1][1]
        last_rule = max(i for i, l in enumerate(last_body) if l.strip() == "---")
        entries[-1][1], tail_lines = last_body[:last_rule], last_body[last_rule + 1:]

    parts = ['<div class="preamble">', "\n  <br><br>\n".join(wrap("  ", inline(p), "  ") for p in preamble),
             "</div>", ""]

    for heading, body in entries:
        match = re.match(r"^## (?:(\S+) )?(\d+)\. (.+?) — (\d+) (\w+)(?: \*\((.+)\)\*)?$", heading)
        if not match:
            raise ValueError("unrecognized entry heading: %s" % heading)
        medal, number, entry_title, score, unit, note = match.groups()
        css = MEDALS.get(medal)
        parts.append('<article class="entry%s">' % (" " + css if css else ""))
        medal_html = '<span class="medal">%s</span> ' % medal if css else ""
        parts.append("  <h2>%s%s. %s</h2>" % (medal_html, number, inline(entry_title)))
        score_text = "%s %s" % (score, unit) + (" (%s)" % inline(note) if note else "")
        meter = "meter over" if int(score) > 100 else "meter"
        parts.append('  <div class="score-row"><span class="score">%s</span><span class="%s">'
                     '<i style="width:%d%%"></i></span></div>' % (score_text, meter, min(int(score), 100)))
        parts.extend(render_block(block) for block in blocks(body))
        parts.append("</article>")
        parts.append("")

    mentions, fineprint, siblings = [], [], None
    for block in blocks(tail_lines):
        if block[0].startswith("- "):
            mentions.extend(list_items(block))
            continue
        text = " ".join(l.strip() for l in block).strip("*")
        if text.startswith("Sibling publications"):
            siblings = text
        else:
            fineprint.append(text)

    if mentions:
        parts += ['<section class="hm">', "  <h2>Honorable Mentions</h2>"]
        for item in mentions:
            match = re.match(r"^\*\*(.+?)\*\* — (.+)$", item)
            parts += ["", '  <article class="entry">', "    <h3>%s</h3>" % inline(match.group(1)),
                      wrap("    <p>", inline(match.group(2)) + "</p>", "    "), "  </article>"]
        parts += ["</section>", ""]
    if fineprint:
        parts.append('<div class="fineprint">')
        parts.extend(wrap("  <p>", inline(text) + "</p>", "  ") for text in fineprint)
        parts += ["</div>", ""]

    parts += ["<footer>", PROJECT_LINE]
    if siblings:
        parts.append(wrap("  <p>", inline(siblings) + "</p>", "  "))
    parts += ["</footer>", ""]

    head, rest = html_text.split("<header>", 1)
    header, rest = rest.split("</header>", 1)
    closing = rest[rest.index("</footer>") + len("</footer>"):]
    header = re.sub(r"<h1>.*?</h1>", lambda m: "<h1>%s</h1>" % inline(title), header, flags=re.S)
    header = re.sub(r'<p class="tagline">.*?</p>',
                    lambda m: wrap('<p class="tagline">', inline(tagline) + "</p>", "  "),
                    header, flags=re.S)
    generated = "\n".join(parts)
    result = head + "<header>" + header + "</header>\n\n" + generated + "\n" + closing.lstrip("\r\n")
    result = result.replace("\r\n", "\n").replace("\n", newline)
    return result


def main():
    check = "--check" in sys.argv[1:]
    stale = []
    for name in sorted(os.listdir(INDEX_DIR)):
        if not name.endswith("_INDEX.md"):
            continue
        md_path = os.path.join(INDEX_DIR, name)
        html_path = md_path[:-3] + ".html"
        md_text = io.open(md_path, encoding="utf-8").read()
        html_text = io.open(html_path, encoding="utf-8", newline="").read()
        result = generate(md_text, html_text)
        if result == html_text:
            continue
        stale.append(os.path.basename(html_path))
        if not check:
            io.open(html_path, "w", encoding="utf-8", newline="").write(result)
    if check:
        for name in stale:
            print("STALE  %s -- run python Tools/build-indexes.py" % name)
        return 1 if stale else 0
    for name in stale:
        print("wrote  %s" % name)
    return 0


if __name__ == "__main__":
    sys.exit(main())
