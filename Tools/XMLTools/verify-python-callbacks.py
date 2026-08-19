"""Verify that every Python callback named in the XML resolves to a real function.

The XML names functions by string (`<PythonCallback>doSomething</PythonCallback>`), so nothing
checks them: a renamed or deleted handler leaves the XML pointing at a name that no longer exists
and the callback silently does nothing at runtime.

⛔ THIS RUNS UNDER PYTHON 3 AND MUST NEVER PARSE THE GAME'S PYTHON.  The embedded interpreter is
Python 2.4 (`docs/reference/engine.md`), so `print "x"` and friends are SyntaxErrors to every
modern parser -- `ast.parse` on `Assets/Python` dies on the first file it reads.  Function
discovery is therefore a LINE SCAN, the same shape every other `Tools/verify-*.py` uses.  Do not
"improve" this back into an AST walk, and do not chase a Python 2 interpreter to run it with:
there is none on a dev box, and the vendored `Build/deps/Python24` is embedding headers and libs,
not a runnable interpreter.

⛔ A VALIDATOR NEVER EDITS.  This tool previously blanked each unresolved callback and rewrote the
XML in place, so running the check MUTATED game data.  It reports; fixing is a human's call.
"""

import fnmatch
import os
import re
import sys

from lxml import etree

# A top-level `def` -- column 0 only, matching the original's module-scope-functions-only rule.
# Callbacks are always module-level; a method would not be reachable by bare name from the XML.
TOP_LEVEL_DEF = re.compile(r"^def\s+([A-Za-z_]\w*)\s*\(", re.M)

# The element names that carry a Python function name as their text.
CALLBACK_NODES = [
	'PythonCallback',
	'PythonHelp',
	'PythonExpireCheck',
	'PythonCanDo',
	'PythonCanDoCity',
	'PythonCanDoUnit',
	'Python',
]

# A bare identifier -- anything else in the element is an expression, not a function reference.
FUNCTION_NAME = re.compile(r"(?:[_a-zA-Z][_a-zA-Z0-9]{0,30})\Z")


def namespace(element):
	match = re.match(r'\{(.*)\}', element.tag)
	return match.group(1) if match else ''


def find_top_level_functions(python_file):
	"""Every module-level `def` in one file, by line scan -- never by parsing."""
	handle = open(python_file, 'r', encoding='utf-8', errors='replace')
	try:
		source = handle.read()
	finally:
		handle.close()
	return TOP_LEVEL_DEF.findall(source)


def get_files(root, file_pattern):
	matches = []
	for directory_path, _, filenames in os.walk(root):
		for filename in fnmatch.filter(filenames, file_pattern):
			matches.append(os.path.join(directory_path, filename))
	return matches


def read_bts_dir():
	"""The Civ4/BTS install folder, from the gitignored per-developer `.env`.

	The base game defines many of the callbacks our XML names (`CvGameUtils` and friends), so
	without this the check cannot tell a deleted handler from a base-game one.
	"""
	env_path = os.path.join(os.path.dirname(__file__), '..', '..', '.env')
	if not os.path.isfile(env_path):
		return None
	handle = open(env_path, 'r', encoding='utf-8', errors='replace')
	try:
		for line in handle:
			line = line.strip()
			if line.startswith('S2S_BTS_DIR='):
				return line.split('=', 1)[1].strip().strip('"').strip("'")
	finally:
		handle.close()
	return None


def base_game_python_roots():
	"""`Assets/Python` for Beyond the Sword and for the base game, or [] when unlocatable."""
	bts_dir = read_bts_dir()
	if not bts_dir:
		return []
	candidates = [
		os.path.join(bts_dir, 'Assets', 'Python'),
		os.path.join(bts_dir, '..', 'Assets', 'Python'),
	]
	return [path for path in candidates if os.path.isdir(path)]


def verify_callbacks(xml_files, python_files):
	known_functions = set()
	for python_file in python_files:
		known_functions.update(find_top_level_functions(python_file))

	unresolved = []
	for filename in xml_files:
		tree = etree.parse(filename)
		root = tree.getroot()
		nsmap = {'': namespace(root)}
		for node in CALLBACK_NODES:
			for element in root.findall('.//' + node, nsmap):
				name = element.text
				if not name:
					continue
				name = name.strip()
				if not name:
					continue
				if FUNCTION_NAME.match(name) and name not in known_functions:
					unresolved.append((name, filename, tree.getpath(element)))
	return unresolved, len(known_functions)


def main():
	xml_files = get_files('Assets', '*.xml')
	python_roots = ['Assets']
	base_roots = base_game_python_roots()
	python_roots.extend(base_roots)

	python_files = []
	for root in python_roots:
		python_files.extend(get_files(root, '*.py'))

	unresolved, function_count = verify_callbacks(xml_files, python_files)

	print("scanned %d XML files against %d functions in %d Python files"
		% (len(xml_files), function_count, len(python_files)))

	if not base_roots:
		print("")
		print("WARNING: the base-game Python was not found, so callbacks the BASE GAME defines")
		print("cannot be resolved and will be listed below as if they were missing.")
		print("Set S2S_BTS_DIR in .env (the folder holding Civ4BeyondSword.exe) for a real check.")

	if not unresolved:
		print("callbacks clean -- every XML-named callback resolves to a function.")
		return 0

	print("")
	for name, filename, path in unresolved:
		print("  %s  %s  %s" % (name, filename, path))
	print("")
	print("%d XML callback(s) name a function that does not exist." % len(unresolved))
	print("The callback silently does nothing at runtime -- fix the name or remove the element.")

	if not base_roots:
		print("⚠ base-game Python was unavailable, so some of the above may be base-game callbacks.")
		return 0
	return 1


if __name__ == "__main__":
	sys.exit(main())
