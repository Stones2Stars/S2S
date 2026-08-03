"""Fail the Python assets on syntax the game's interpreter cannot parse.

The engine embeds **Python 2.4** (engine.md: the closed VC7.1 EXE freezes the whole stack -- C++03,
32-bit, Python 2.4, Boost 1.32/1.55). Anything newer is a SyntaxError at IMPORT, and an import failure
does not fail politely: every module on the engine's entry chain
(CvEventInterface -> BugEventManager -> CvEventManager -> CvScreensInterface) must import cleanly before
ANY callback fires, so one bad line silently severs the whole engine->Python direction.

This exists because a rule has to be remembered and a check does not (AGENTS.md). The conditional
expression is the specific trap: `X if C else Y` is valid in every Python an agent has ever seen, is
invalid here, and reads as completely ordinary -- it was written into the tree once and very nearly a
second time in the same session.

Run:  python Tools/verify-python24.py
"""

import os
import re
import sys


ASSETS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Assets", "Python")

# Each rule: (name, compiled pattern, the version that introduced it, what to write instead).
RULES = [
	("conditional expression", re.compile(r"=.*\S\s+if\s+.+\s+else\s+"), "2.5",
	 "use an if/else statement"),
	("with statement", re.compile(r"^\s*with\s+.+:\s*$"), "2.5",
	 "use try/finally"),
	("except ... as ...", re.compile(r"^\s*except\s+[^:]+\s+as\s+\w+\s*:"), "2.6",
	 "use `except X, e:`"),
	# Only a STRING LITERAL followed by .format -- a module function or a custom method may legitimately be
	# called `format` (TradeUtil.format is one), and a check that cries wolf is a check nobody runs.
	("str.format", re.compile(r"""(?:"[^"]*"|'[^']*')\s*\.\s*format\s*\("""), "2.6",
	 "use %-formatting"),
	("dict/set comprehension", re.compile(r"[\{]\s*[^\{\}]*\s+for\s+\w+\s+in\s+"), "2.7",
	 "build it with a loop, or dict(...) over a generator"),
	("ternary in a return", re.compile(r"^\s*return\s+.+\s+if\s+.+\s+else\s+"), "2.5",
	 "use an if/else statement"),
]


def strip_noise(line):
	"""Drop comments, and EMPTY every string literal while keeping its quotes.

	Emptying rather than deleting is what lets a literal-anchored rule still fire: `"%d".format(x)` collapses
	to `"".format(x)` and is still recognisable, while prose inside a string can no longer match a rule that
	is looking for code.
	"""
	out = []
	i = 0
	while i < len(line):
		ch = line[i]
		if ch == "#":
			break
		if ch in "\"'":
			quote = line[i:i + 3] if line[i:i + 3] in ('"""', "'''") else ch
			i += len(quote)
			while i < len(line):
				if line[i] == "\\":
					i += 2
					continue
				if line.startswith(quote, i):
					i += len(quote)
					break
				i += 1
			out.append(quote[0] * 2)   # an empty literal stands in for the string
			continue
		out.append(ch)
		i += 1
	return "".join(out)


def main():
	hits = []
	scanned = 0
	for root, dirs, files in os.walk(ASSETS):
		for name in sorted(files):
			if not name.endswith(".py"):
				continue
			path = os.path.join(root, name)
			scanned += 1
			try:
				handle = open(path)
				try:
					lines = handle.read().split("\n")
				finally:
					handle.close()
			except Exception:
				continue
			for number, raw in enumerate(lines, 1):
				code = strip_noise(raw)
				if not code.strip():
					continue
				for label, pattern, version, remedy in RULES:
					if pattern.search(code):
						rel = os.path.relpath(path, ASSETS)
						hits.append((rel, number, label, version, remedy, raw.strip()))

	print("scanned %d Python files under Assets/Python" % scanned)
	if not hits:
		print("clean -- no post-2.4 syntax found")
		return 0

	print("")
	print("FOUND %d line(s) the game's Python 2.4 cannot parse:" % len(hits))
	for rel, number, label, version, remedy, text in hits:
		print("")
		print("  %s:%d" % (rel, number))
		print("    %s (Python %s+) -- %s" % (label, version, remedy))
		print("    %s" % text[:140])
	return 1


if __name__ == "__main__":
	sys.exit(main())
