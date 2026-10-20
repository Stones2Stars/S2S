#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""verify-gettext-widths -- a 64-bit value must never be passed to the EXE's varargs getText,
and a string OBJECT must never be passed to any varargs formatter.

`gDLL->getText(szTextKey, ...)` is the closed EXE's VARARGS text formatter. Arguments are
matched to the key's placeholders POSITIONALLY BY 4-BYTE SLOT, so an 8-byte argument
occupies TWO slots and every LATER placeholder reads one slot early. A `%s` that lands on
a shifted slot receives an INTEGER, and the EXE runs `wcslen` on it -- an ACCESS_VIOLATION
at an address exactly equal to the value.

That is worth a check rather than a rule for the reasons this repo keeps choosing one:

  - the compiler CANNOT see it: varargs accept anything, so every call site compiles clean;
  - it is SILENT until that specific tooltip is actually rendered, in the specific branch
    that reaches it -- so it survives every build and most play sessions;
  - the damage is invisible at the crash site: the fault is inside msvcr71 with only EXE
    frames on the stack, and the faulting value belongs to a DIFFERENT argument than the
    one that is actually wrong;
  - and it is trivially mechanical to detect, which is the whole test for a checker.

Worked case: `CvCity::getCulture()` was widened to int64_t (correct -- it wrapped negative
in long games). It was still passed raw to TXT_KEY_CITY_BAR_CULTURE (`%d1/%d2 (%s3: Lvl
%d4)`), so %d2 read culture's HIGH half and %s3 read the culture THRESHOLD as a wchar_t*.
Hovering the city bar faulted at faultAddr == the threshold, once per session, for months.

THE FIX is never a cast to int -- that re-introduces the wrap the widening exists to fix.
Pre-render the value and hand it to a %s placeholder, which is what the tree already does:

    gDLL->getText("TXT_KEY_X", CvWString::format(L"%I64d", x.getCulture()).GetCString())

SCOPE of the width rule: only the EXE `getText` sink, because that is where the process
faults. Our own Cv(W)String::format is not width-checked -- there the format literal sits at
the call site, so `%I64d` vs `%d` is visible to a reader rather than hidden across a module
boundary.

THE STRING RULE is stated in AGENTS.md § Validation. Its sinks are every function the tree
declares with a format parameter and then `...` (getText, Cv(W)String::format and Format, the
log helpers, showXMLError, ...), the CRT formatters in CRT_FORMATTERS, and an object-like
macro that names one (DEBUG_LOG). A class's variadic member is a sink for a receiver of that
class or of an unknown one, a free function for a call without an object receiver; the
arguments after the fewest fixed parameters are checked. Each value an argument passes is
checked: the argument, both branches of a conditional, each operand of a `+`, each part of a
parenthesised expression. The type of a value comes from the tree:

  - a call: the functions of that name and arity returning a string object or a text
    pointer, looked up in the class the receiver names and its bases first;
  - a variable: its declaration, of whatever type, found through the blocks around the use
    -- the function's own blocks and parameters, then the method's class and its bases, then
    this file's scope, then the globals the headers declare. A declaration starts a statement
    (or a parameter list outside a function body), so `iA * iB` declares nothing;
  - a receiver: a variable's declared class, a function's returned class (matched on arity),
    an object-like macro's expansion (`GC`, `GET_PLAYER`; every #if definition is tried), or
    the enclosing method's class for a bare call;
  - a cast: its target type; `*p`, `a[i]` and `it->` reach the element of a pointer, an array,
    a standard container or its iterator. A map's subscript reads the mapped value, and its
    iterator a std::pair whose `first` and `second` have the declared types.

A call to a template with explicit arguments (`f<T>(x)`) has no harvested return and is not
typed.

`getDescription` returns a wchar_t* on an info and a CvWString on a CvUnit, which is why the
receiver matters. When the class does not decide, every class's declarations of that name
and arity do: all objects FAIL, all pointers pass, and a mix is REPORTED for a human and does
not fail.

Run from the repo root:  python Tools/verify-gettext-widths.py
"""

import bisect
import os
import re
import sys

SOURCE_ROOT = "Sources"

# Names that CONSUME a 64-bit argument correctly -- a wide/narrow format call renders it to
# a string, so a 64-bit getter nested inside one is not reaching getText as a number.
CONSUMING_CALL = re.compile(r"\b(?:CvWString|CvString)::format\s*\(")

# A 64-bit returning function, harvested from the tree so the list cannot go stale.
WIDE_RETURN_DECL = re.compile(
    r"\b(?:int64_t|uint64_t|long\s+long)\s+"          # the return type
    r"(?:[A-Za-z_][A-Za-z0-9_]*::)?"                  # optional Class::
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\("                  # the name we want
)

# The same name declared at 32 bits somewhere -- what makes a name AMBIGUOUS rather than wide.
NARROW_RETURN_DECL = re.compile(
    r"\b(?:int|short|unsigned|uint|size_t|DWORD)\s+"
    r"(?:[A-Za-z_][A-Za-z0-9_]*::)?"
    r"([A-Za-z_][A-Za-z0-9_]*)\s*\("
)

GETTEXT_CALL = re.compile(r"\bgetText\s*\(")

# The CRT's varargs formatters the tree calls, which it does not declare: name -> fixed parameters.
CRT_FORMATTERS = {
    "printf": 1, "fprintf": 2, "sprintf": 2, "_snprintf": 3, "swprintf": 2, "_snwprintf": 3, "_stprintf": 2,
}
STRING_CLASSES = ("CvWString", "CvString", "std::wstring", "std::string")

# A function returning a string OBJECT, and one returning a text POINTER -- the pair that makes a
# name ambiguous. Group 1 is an optional `Class::`, group 2 the name.
STRING_OBJECT_RETURN_DECL = re.compile(
    r"\b(?:const\s+)?(?:CvWString|CvString|std::wstring|std::string)\s*&?\s+"
    r"(?:([A-Za-z_][A-Za-z0-9_]*)::)?([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
TEXT_POINTER_RETURN_DECL = re.compile(
    r"\b(?:const\s+)?(?:wchar_t|wchar|char)\s*\*\s*"
    r"(?:([A-Za-z_][A-Za-z0-9_]*)::)?([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
# A function returning a type by reference or pointer -- a class once the census knows the type
# is one. Group 1 is the type, 2 an optional `Class::`, 3 the name.
CLASS_RETURN_DECL = re.compile(
    r"\b([A-Za-z_][A-Za-z0-9_]*)\s*[&*]\s*(?:([A-Za-z_][A-Za-z0-9_]*)::)?([A-Za-z_][A-Za-z0-9_]*)\s*\("
)
VARIADIC_PARAMETER = re.compile(r"\.\.\.\s*\)")
# An object-like macro: `#define GC cvInternalGlobals::getInstance()`.
OBJECT_MACRO = re.compile(r"^[ \t]*#define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+(\S[^\n]*?)[ \t]*$", re.MULTILINE)

BRACE = re.compile(r"[{}]")
# What a brace block is, read from the text between the previous statement and the brace.
CLASS_BLOCK_HEAD = re.compile(
    r"\b(?:class|struct|union)\s+(?:DllExport\s+)?([A-Za-z_][A-Za-z0-9_]*)?\s*(?:<[^{]*?>)?\s*(?::([^{]*))?$"
)
SCOPE_BLOCK_HEAD = re.compile(r"\b(?:namespace|enum|extern)\b[^()]*$")
FUNCTION_BLOCK_HEAD = re.compile(r"\)\s*(?:const\s*)?(?:throw\s*\(\s*\)\s*)?$")
QUALIFIED_NAME = re.compile(r"\b([A-Za-z_][A-Za-z0-9_]*)::~?[A-Za-z_][A-Za-z0-9_]*\s*\(")
BASE_NAME = re.compile(r"\s*(?:(?:public|protected|private|virtual)\s+)*([A-Za-z_][A-Za-z0-9_:]*)")
TRAILING_NAME = re.compile(r"([A-Za-z_][A-Za-z0-9_]*)\s*$")
QUALIFIER = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)::([A-Za-z_][A-Za-z0-9_]*)$")

# A declaration of a variable, member or parameter, whatever its type. Group 1 is the type, 2 any
# `&`/`*`, 3 the name, 4 what follows it -- `[` makes it an array. MORE_DECLARATOR is each
# further name of `CvWString szFirst, szSecond;`.
VARIABLE_DECL = re.compile(
    r"(?<![A-Za-z0-9_:.>])((?:[A-Za-z_][A-Za-z0-9_]*::)*[A-Za-z_][A-Za-z0-9_]*(?:\s*<[^;(){}]*?>)?"
    r"(?:\s*::\s*[A-Za-z_][A-Za-z0-9_]*)*)"
    r"(?:\s*(?:\bconst\b\s*)?(\*+\s*&?|&(?!&))\s*|\s+)(?:const\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*([=;,)(\[])"
)
MORE_DECLARATOR = re.compile(r"\s*([&*]*)\s*([A-Za-z_][A-Za-z0-9_]*)\s*([=;,\[])")
NOT_A_TYPE = frozenset((
    "return", "else", "delete", "new", "throw", "goto", "case", "typedef", "namespace", "using",
    "sizeof", "operator", "struct", "class", "union", "enum", "template", "typename", "friend",
    "virtual", "const", "static", "extern", "inline", "DllExport", "volatile", "register",
    "mutable", "explicit", "public", "private", "protected", "do", "if", "while", "for", "switch",
))
C_CAST = re.compile(
    r"\s*\(\s*(?:const\s+)?([A-Za-z_][A-Za-z0-9_:]*(?:\s*<[^()]*>)?)\s*(?:const\s*)?([*&]*)\s*\)\s*(?=\S)"
)
CXX_CAST = re.compile(
    r"\b(?:static|const|reinterpret|dynamic)_cast\s*<\s*(?:const\s+)?([A-Za-z_][A-Za-z0-9_:]*(?:\s*<[^()]*>)?)"
    r"\s*(?:const\s*)?([*&]*)\s*>\s*$"
)
# A standard container, or its iterator (group 3).
CONTAINER = re.compile(
    r"^(?:std::|stdext::)?(vector|list|deque|set|multiset|map|multimap|hash_map)\s*<(.*)>"
    r"(\s*::\s*(?:const_)?(?:reverse_)?iterator)?$"
)
PAIR = re.compile(r"^std::pair<(.*)>$")
DECLARATION_PREFIXES = frozenset((
    "const", "static", "extern", "inline", "DllExport", "volatile", "register", "mutable",
    "virtual", "typename", "explicit", "friend",
))
BOOLEAN_OPERATORS = ("==", "!=", "<=", ">=", "&&", "||")
# What a call's value is when the census knows only its kind, not its class.
TEXT_OBJECT = "<string object>"
TEXT_POINTER = "<text pointer>"
AMBIGUOUS_TEXT = "<object or pointer>"
# How deep object-like macros may expand into each other before the chain counts as a cycle.
MACRO_DEPTH = 16


def source_files():
    for root, dirs, files in os.walk(SOURCE_ROOT):
        for name in files:
            if name.endswith(".cpp") or name.endswith(".h"):
                yield os.path.join(root, name)


def read(path):
    handle = open(path, "r")
    try:
        return handle.read()
    except UnicodeDecodeError:
        handle.close()
        handle = open(path, "rb")
        return handle.read().decode("utf-8", "replace")
    finally:
        handle.close()


#	The longest plausible char literal, counting the quotes: 'x', '\n', '\x41'. A lone
#	apostrophe further from its partner than this is PROSE (an English contraction in a
#	comment), not a literal -- and treating one as a literal desynchronizes the mask for
#	the whole rest of the file, which makes the checker silently MISS call sites. A
#	checker that under-reports is worse than no checker, so this bound is load-bearing.
MAX_CHAR_LITERAL = 8


def strip_literals(text):
    """Blank comments and literals, PRESERVING LENGTH AND NEWLINES so indices still align.

    Comments go first: an apostrophe inside one would otherwise open a char literal that
    never closes.
    """
    out = list(text)
    index = 0
    length = len(text)

    def blank(start, stop):
        for position in range(start, min(stop, length)):
            if out[position] != "\n":
                out[position] = " "

    while index < length:
        char = text[index]
        pair = text[index:index + 2]
        if pair == "//":
            end = text.find("\n", index)
            end = length if end < 0 else end
            blank(index, end)
            index = end
            continue
        if pair == "/*":
            end = text.find("*/", index + 2)
            end = length if end < 0 else end + 2
            blank(index, end)
            index = end
            continue
        if char == '"':
            start = index
            index += 1
            while index < length and text[index] != '"':
                index += 2 if text[index] == "\\" else 1
            index = min(index + 1, length)
            blank(start, index)
            continue
        if char == "'":
            close = index + 1
            while close < length and close - index <= MAX_CHAR_LITERAL:
                if text[close] == "\\":
                    close += 2
                    continue
                if text[close] == "'":
                    break
                close += 1
            if close < length and close - index <= MAX_CHAR_LITERAL and text[close] == "'":
                blank(index, close + 1)
                index = close + 1
            else:
                index += 1          # a bare apostrophe in prose -- leave it alone
            continue
        index += 1

    return "".join(out)


def call_extent(text, open_paren):
    """Return the index of the ')' closing the '(' at open_paren, or -1."""
    depth = 0
    index = open_paren
    while index < len(text):
        if text[index] == "(":
            depth += 1
        elif text[index] == ")":
            depth -= 1
            if depth == 0:
                return index
        index += 1
    return -1


def argument_spans(text, open_paren, close_paren):
    """(start, end) of each top-level argument between the call parentheses at these indices."""
    #	⛔ '<' and '>' are NOT nesting here. `pCity->getX()` and a plain comparison are far
    #	commoner in a call's argument list than a template is, and counting them unbalances the
    #	depth so a later top-level comma stops splitting -- i.e. a MISS. A template's comma
    #	splits one argument in two instead, which costs only the reported position: every
    #	piece is still searched, so nothing goes undetected. Declarations split with
    #	type_arguments, where the brackets do nest.
    spans = []
    depth = 0
    start = open_paren + 1
    for index in range(open_paren + 1, close_paren):
        char = text[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif char == "," and depth == 0:
            spans.append((start, index))
            start = index + 1
    spans.append((start, close_paren))
    return spans


def top_level_arguments(text):
    """Split a call's argument list on the commas that sit at nesting depth zero."""
    return [text[start:end] for start, end in argument_spans(text, -1, len(text))]


def type_arguments(text):
    """Split a parameter list or a template's argument list on its top-level commas; `<` and
    `>` nest here, as template brackets."""
    pieces = []
    depth = 0
    start = 0
    for index, char in enumerate(text):
        if char in "([{<":
            depth += 1
        elif char in ")]}>":
            depth -= 1
        elif char == "," and depth == 0:
            pieces.append(text[start:index])
            start = index + 1
    pieces.append(text[start:])
    return pieces


def argument_count(text):
    return len(top_level_arguments(text)) if text.strip() else 0


def remove_consuming_calls(argument):
    """Drop any CvWString/CvString::format(...) sub-expression -- it renders the value."""
    while True:
        match = CONSUMING_CALL.search(argument)
        if match is None:
            return argument
        close = call_extent(argument, match.end() - 1)
        if close < 0:
            return argument[:match.start()]
        argument = argument[:match.start()] + " " + argument[close + 1:]


def arity_range(params):
    """(min, max) arguments a parameter list accepts. A defaulted parameter may be omitted, and
    a trailing `...` takes any number."""
    stripped = params.strip()
    if not stripped or stripped == "void":
        return 0, 0
    pieces = type_arguments(stripped)
    if pieces[-1].strip() == "...":
        return len(pieces) - 1, sys.maxsize
    required = 0
    for piece in pieces:
        if "=" not in piece:
            required += 1
    return required, len(pieces)


def width_verdict(declarations, name, argument_count):
    """WIDE / NARROW / AMBIGUOUS / None for a call of `name` taking argument_count arguments.

    ⚑ ARITY is what separates the overloads a bare name cannot: CvCity::getCulture takes a
    PlayerTypes and returns int64_t, while CvEventInfo::getCulture takes nothing and
    returns int. Matching on (name, argument count) resolves that pair exactly, and every
    other same-name/same-arity pair stays AMBIGUOUS for a human, as it must.
    """
    matches = [is_wide for is_wide, low, high in declarations.get(name, [])
               if low <= argument_count <= high]
    if not matches:
        return None
    if all(matches):
        return "WIDE"
    if not any(matches):
        return "NARROW"
    return "AMBIGUOUS"


def collect_declarations(sources):
    """Harvest every candidate declaration from the tree, so the census cannot go stale.

    Each entry is (is_wide, min_arity, max_arity) so a call can be matched on NAME AND
    ARGUMENT COUNT rather than on the name alone -- see width_verdict for why that matters.
    A same-name, same-arity pair that still disagrees stays AMBIGUOUS and is reported for a
    human rather than failing the run, exactly as the other context-dependent checks in
    this family do.
    """
    KEYWORDS = ("if", "while", "for", "switch", "return")
    declarations = {}

    def note(text, match, is_wide):
        name = match.group(1)
        if name in KEYWORDS:
            return
        close = call_extent(text, match.end() - 1)
        if close < 0:
            return
        params = text[match.end():close]
        low, high = arity_range(params)
        declarations.setdefault(name, []).append((is_wide, low, high))

    for source in sources:
        masked = source[2]
        for match in WIDE_RETURN_DECL.finditer(masked):
            note(masked, match, True)
        for match in NARROW_RETURN_DECL.finditer(masked):
            note(masked, match, False)
    return declarations


def opening_bracket(text, start, close):
    """Index of the '(' or '[' that the ')' or ']' at close closes, or -1 if it opens before start."""
    closer = text[close]
    opener = "(" if closer == ")" else "["
    depth = 0
    index = close
    while index >= start:
        if text[index] == closer:
            depth += 1
        elif text[index] == opener:
            depth -= 1
            if depth == 0:
                return index
        index -= 1
    return -1


def trimmed_end(text, start, end):
    while end > start and text[end - 1].isspace():
        end -= 1
    return end


def trimmed_start(text, start, end):
    while start < end and text[start].isspace():
        start += 1
    return start


def literal_placeholders(text):
    """text masked like a file, except that each literal keeps a `0` where its quotes were, so
    a macro's `getLabel("key")` still reads as a call with an argument."""
    masked = strip_literals(text)
    return "".join("0" if original in "\"'" and kept == " " else kept for original, kept in zip(text, masked))


def statement_start(masked, position):
    """Where the statement holding position begins: after the last ';', '{' or '}' before it."""
    return max(masked.rfind(";", 0, position), masked.rfind("{", 0, position),
               masked.rfind("}", 0, position)) + 1


def expression_start(text, end):
    """Where the postfix expression -- names, calls, subscripts, `.`, `->` and `::` -- that
    ends at end begins."""
    index = trimmed_end(text, 0, end)
    while index > 0:
        char = text[index - 1]
        if char in ")]":
            open_bracket = opening_bracket(text, 0, index - 1)
            if open_bracket < 0:
                break
            index = trimmed_end(text, 0, open_bracket)
        elif char.isalnum() or char == "_":
            index -= 1
        elif text[index - 2:index] in ("->", "::"):
            index = trimmed_end(text, 0, index - 2)
        elif char == ".":
            index = trimmed_end(text, 0, index - 1)
        else:
            break
    return trimmed_start(text, index, end)


def in_parentheses(masked, position):
    """True when position sits inside an open '(' of its statement -- a parameter list."""
    segment = masked[statement_start(masked, position):position]
    return segment.count("(") > segment.count(")")


def preprocessor_line(masked, position):
    line_start = masked.rfind("\n", 0, position) + 1
    return masked[line_start:position].lstrip().startswith("#")


def opens_declaration(masked, position, in_function):
    """True when a type at position starts a declaration: it follows a declaration keyword, a
    preprocessor line or a statement boundary -- in a function body `;`, `{`, `}` or the `(` of
    a for/if/while/switch head, elsewhere also a parameter list's `(` or `,` and an access
    label's `:`. A type that follows an operand (`iA * iB`, `a < b > c`), or an argument's
    `(` or `,` in a function body, is an expression."""
    window = max(0, position - 200)
    before = masked[window:position].rstrip()
    if not before:
        return window == 0 or not masked[:window].strip()
    if before[-1] in ";{}":
        return True
    word = TRAILING_NAME.search(before)
    if word and word.group(1) in DECLARATION_PREFIXES:
        return True
    if before[-1] == "(":
        head = TRAILING_NAME.search(before[:-1].rstrip())
        if head and head.group(1) in ("for", "if", "while", "switch"):
            return True
    if not in_function and before[-1] in "(,:":
        return True
    return preprocessor_line(masked, window + len(before))


def value_operands(text, start, end):
    """The spans of an argument whose VALUE is what the formatter receives: both branches of a
    top-level conditional, each operand of a top-level `+`, or else the whole argument. A
    comparison or a negation is a bool and has none."""
    depth = 0
    for index in range(start, end):
        char = text[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif depth == 0 and char == "?":
            colon = conditional_colon(text, index + 1, end)
            if colon < 0:
                return []
            return value_operands(text, index + 1, colon) + value_operands(text, colon + 1, end)
    depth = 0
    pieces = []
    piece_start = start
    index = start
    while index < end:
        char = text[index]
        pair = text[index:index + 2]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif depth == 0:
            if pair in BOOLEAN_OPERATORS or (char == "!" and pair != "!="):
                return []
            #	A spaced `<` or `>` compares; an unspaced one is a template or cast bracket.
            if char in "<>" and text[index - 1:index] == " " and text[index + 1:index + 2] == " ":
                return []
            if pair in ("::", "++", "+="):
                index += 2
                continue
            if char == "+":
                pieces.append((piece_start, index))
                piece_start = index + 1
        index += 1
    pieces.append((piece_start, end))
    return pieces


def conditional_colon(text, start, end):
    """Index of the ':' that ends the true branch of a conditional whose '?' is just before start."""
    depth = 0
    nested = 0
    index = start
    while index < end:
        char = text[index]
        if text[index:index + 2] == "::":
            index += 2
            continue
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
        elif depth == 0 and char == "?":
            nested += 1
        elif depth == 0 and char == ":":
            if nested == 0:
                return index
            nested -= 1
        index += 1
    return -1


def type_parts(type_name, modifiers):
    """(class, is_pointer, element) of a declared type, the element being the (class, is_pointer)
    that a subscript, or an iterator's `*` and `->`, reads: `std::vector<CvWString>` has the
    element CvWString, a map's is a std::pair, and an iterator counts as a pointer. A bare
    `wstring` is std::wstring."""
    type_name = " ".join(type_name.replace("const ", "").split())
    if type_name in ("wstring", "string"):
        type_name = "std::" + type_name
    container = CONTAINER.match(type_name)
    if container is None:
        return type_name, "*" in modifiers, None
    pieces = [piece.strip() for piece in type_arguments(container.group(2))]
    if container.group(1) in ("map", "multimap", "hash_map"):
        element = ("std::pair<%s>" % ",".join(pieces[:2]), False)
    else:
        element = piece_type(pieces[0])[:2]
    return type_name, container.group(3) is not None or "*" in modifiers, element


def piece_type(piece):
    """type_parts of one template argument such as `CvWString*`."""
    piece = piece.strip()
    return type_parts(piece.rstrip("*& "), "*" if piece.endswith("*") else "")


def pointee(found):
    """(class, is_pointer) that `*` or `->` reaches through the (class, is_pointer, element)
    found: an iterator's element, or the pointed-to class."""
    if found[0].endswith("iterator") and found[2]:
        return found[2]
    return found[0], False


def is_text_object(found):
    return found is not None and not found[1] and (found[0] in STRING_CLASSES or found[0] == TEXT_OBJECT)


def worst(verdicts):
    for verdict in ("OBJECT", "AMBIGUOUS"):
        if verdict in verdicts:
            return verdict
    return None


class Block(object):
    """One brace block of a file. KIND is "class" (NAME is the class), "scope" (a namespace,
    enum or extern block), "function" (the outermost other block that follows a parameter
    list; NAME is its `Class::` or the class it sits in), "data" (an initializer), or "inner"
    (a block inside a function or initializer). HEAD_START is where the text that opens it
    begins, which for a function takes in its parameters. BASES are a class's base classes."""

    def __init__(self, open_brace, parent, kind, name, bases, head_start):
        self.open = open_brace
        self.close = sys.maxsize
        self.parent = parent
        self.kind = kind
        self.name = name
        self.bases = bases
        self.head_start = head_start


class Layout(object):
    """A file's brace blocks, in the order they open."""

    def __init__(self, masked):
        self.blocks = []
        stack = []
        for match in BRACE.finditer(masked):
            index = match.start()
            if masked[index] == "{":
                parent = stack[-1] if stack else -1
                head_start = statement_start(masked, index)
                kind, name, bases = self.classify(masked[head_start:index].strip(), parent)
                self.blocks.append(Block(index, parent, kind, name, bases, head_start))
                stack.append(len(self.blocks) - 1)
            elif stack:
                self.blocks[stack.pop()].close = index
        self.opens = [block.open for block in self.blocks]

    def classify(self, head, parent):
        """(kind, name, bases) of a block opened by head."""
        if parent >= 0 and self.blocks[parent].kind in ("function", "data", "inner"):
            return "inner", None, []
        klass = CLASS_BLOCK_HEAD.search(head)
        if klass:
            bases = [BASE_NAME.match(base).group(1) for base in (klass.group(2) or "").split(",")
                     if BASE_NAME.match(base)]
            return "class", klass.group(1), bases
        if SCOPE_BLOCK_HEAD.search(head):
            return "scope", None, []
        if FUNCTION_BLOCK_HEAD.search(head):
            qualifier = QUALIFIED_NAME.search(head)
            if qualifier:
                return "function", qualifier.group(1), []
            return "function", self.class_of_block(parent), []
        return "data", None, []

    def class_of_block(self, block):
        while block >= 0 and self.blocks[block].kind != "class":
            block = self.blocks[block].parent
        return self.blocks[block].name if block >= 0 else None

    def innermost(self, offset):
        """The innermost block around offset, or -1."""
        block = bisect.bisect_right(self.opens, offset) - 1
        while block >= 0 and self.blocks[block].close < offset:
            block = self.blocks[block].parent
        return block

    def encloses(self, block, offset):
        return block < 0 or self.blocks[block].open < offset < self.blocks[block].close

    def function_of(self, offset):
        """The function block around offset, or -1 at class, namespace or file scope."""
        block = self.innermost(offset)
        while block >= 0 and self.blocks[block].kind != "function":
            block = self.blocks[block].parent
        return block

    def class_of(self, offset):
        return self.class_of_block(self.innermost(offset))

    def owner_of(self, offset):
        """The class a bare name inside the code at offset is a member of."""
        function = self.function_of(offset)
        if function >= 0:
            return self.blocks[function].name
        return self.class_of(offset)


class Declaration(object):
    def __init__(self, start, type_name, modifiers, terminator):
        self.start = start
        self.type_name = type_name
        self.modifiers = modifiers
        self.terminator = terminator


def file_declarations(masked, layout):
    """Every declaration of a variable, member or parameter in a file, by name, in file order."""
    declarations = {}
    for match in VARIABLE_DECL.finditer(masked):
        type_name = match.group(1)
        if type_name in NOT_A_TYPE or match.group(3) in NOT_A_TYPE or preprocessor_line(masked, match.start()) \
                or not opens_declaration(masked, match.start(), layout.function_of(match.start()) >= 0):
            continue
        declarations.setdefault(match.group(3), []).append(
            Declaration(match.start(3), type_name, match.group(2) or "", match.group(4)))
        terminator = match.group(4)
        position = match.end()
        while terminator in "=,":
            if terminator == "=":
                position = next_declarator(masked, position)
                if position < 0:
                    break
            more = MORE_DECLARATOR.match(masked, position)
            if more is None or more.group(2) in NOT_A_TYPE:
                break
            declarations.setdefault(more.group(2), []).append(
                Declaration(more.start(2), type_name, more.group(1), more.group(3)))
            terminator = more.group(3)
            position = more.end()
    for entries in declarations.values():
        entries.sort(key=lambda declaration: declaration.start)
    return declarations


def next_declarator(masked, position):
    """The index after the top-level ',' that ends an initializer starting at position, or -1
    when the statement ends first."""
    depth = 0
    for index in range(position, len(masked)):
        char = masked[index]
        if char in "([{":
            depth += 1
        elif char in ")]}":
            depth -= 1
            if depth < 0:
                return -1
        elif depth == 0 and char == ";":
            return -1
        elif depth == 0 and char == ",":
            return index + 1
    return -1


class Census(object):
    """What the string check reads from the whole tree: the varargs sinks, every function
    returning a string object, a text pointer or a class, every declaration, each class's bases
    and bodies, the object-like macros, and the globals the headers declare."""

    def __init__(self, files):
        self.texts = {}
        self.masks = {}
        self.layouts = {}
        self.declarations = {}
        self.text_returns = {}
        self.class_returns = {}
        self.sinks = dict((name, [(None, fixed)]) for name, fixed in CRT_FORMATTERS.items())
        self.bases = {}
        self.bodies = {}
        self.macros = {}
        self.globals = {}
        self.expanding = 0
        for source in files:
            path = source[0]
            masked = source[2]
            self.texts[path] = source[1]
            layout = Layout(masked)
            declarations = file_declarations(masked, layout)
            self.masks[path] = masked
            self.layouts[path] = layout
            self.declarations[path] = declarations
            for match in OBJECT_MACRO.finditer(masked):
                #	A name defined in several #if branches keeps every definition.
                self.macros.setdefault(match.group(1), []).append(
                    literal_placeholders(source[1][match.start(2):match.end(2)]))
            for block in layout.blocks:
                if block.kind == "class" and block.name:
                    self.bodies.setdefault(block.name, []).append((path, block.open, block.close))
                    self.bases.setdefault(block.name, []).extend(block.bases)
            if path.endswith(".h"):
                for name, entries in declarations.items():
                    for declaration in entries:
                        if self.at_file_scope(path, declaration.start):
                            self.globals.setdefault(name, declaration)
            for match in VARIADIC_PARAMETER.finditer(masked):
                open_paren = opening_bracket(masked, 0, match.end() - 1)
                name = TRAILING_NAME.search(masked, 0, open_paren) if open_paren > 0 else None
                if name and layout.function_of(match.start()) < 0:
                    qualifier = re.search(r"([A-Za-z_][A-Za-z0-9_]*)::\s*$", masked[max(0, name.start() - 80):name.start()])
                    owner = qualifier.group(1) if qualifier else layout.class_of(match.start())
                    fixed = arity_range(masked[open_paren + 1:match.end() - 1])[0]
                    #	A format takes at least the format itself; `f(...)` alone is no formatter.
                    if fixed > 0:
                        self.sinks.setdefault(name.group(1), []).append((owner, fixed))
            for match in CLASS_RETURN_DECL.finditer(masked):
                close = call_extent(masked, match.end() - 1)
                if close >= 0 and layout.function_of(match.start()) < 0:
                    owner = match.group(2) or layout.class_of(match.start())
                    low, high = arity_range(masked[match.end():close])
                    self.class_returns.setdefault(match.group(3), []).append((owner, match.group(1), low, high))
            for pattern, is_object in ((STRING_OBJECT_RETURN_DECL, True), (TEXT_POINTER_RETURN_DECL, False)):
                for match in pattern.finditer(masked):
                    close = call_extent(masked, match.end() - 1)
                    if close < 0 or layout.function_of(match.start()) >= 0:
                        continue
                    owner = match.group(1) or layout.class_of(match.start())
                    low, high = arity_range(masked[match.end():close])
                    self.text_returns.setdefault(match.group(2), []).append((owner, is_object, low, high))
        #	A macro that names a formatter (`#define DEBUG_LOG logging::logMsg`) is that formatter.
        for name, expansions in self.macros.items():
            for expansion in expansions:
                target = re.match(r"^(?:[A-Za-z_][A-Za-z0-9_]*::)*([A-Za-z_][A-Za-z0-9_]*)$", expansion)
                if target and target.group(1) in self.sinks and name not in self.sinks:
                    self.sinks[name] = [(None, fixed) for owner, fixed in self.sinks[target.group(1)]]

    def sink_fixed(self, path, masked, name):
        """How many fixed parameters precede the varargs of the call named at `name`, or None
        when the call reaches no varargs declaration: a class's variadic member counts only for
        a receiver of that class or an unknown one, a free function only for a call with no
        object receiver."""
        before = masked[:name.start()].rstrip()
        has_receiver = before.endswith(".") or before.endswith("->")
        owner = self.receiver_owner(path, masked, expression_start(masked, name.start()), name.start(), None) \
            if has_receiver else None
        lineage = self.lineage(owner)
        counts = [fixed for declarer, fixed in self.sinks[name.group(1)]
                  if (declarer is None and not has_receiver)
                  or (declarer is not None and (not has_receiver or owner is None or declarer in lineage))]
        return min(counts) if counts else None

    def at_file_scope(self, path, position):
        layout = self.layouts[path]
        block = layout.innermost(position)
        while block >= 0 and layout.blocks[block].kind == "scope":
            block = layout.blocks[block].parent
        return block < 0 and not in_parentheses(self.masks[path], position)

    def lineage(self, owner):
        """owner, then its bases, nearest first."""
        order = []
        pending = [owner]
        while pending:
            klass = pending.pop(0)
            if klass and klass not in order:
                order.append(klass)
                pending.extend(self.bases.get(klass, []))
        return order

    def text_verdict(self, owner, name, count):
        """OBJECT / POINTER / AMBIGUOUS / None for a call of `name` with count arguments, looked
        up in owner and its bases first, and in every class when that finds nothing."""
        entries = [(entry_owner, is_object) for entry_owner, is_object, low, high in self.text_returns.get(name, [])
                   if low <= count <= high]
        kinds = []
        for klass in self.lineage(owner):
            kinds = [is_object for entry_owner, is_object in entries if entry_owner == klass]
            if kinds:
                break
        if not kinds:
            kinds = [is_object for entry_owner, is_object in entries]
        if not kinds:
            return None
        if all(kinds):
            return "OBJECT"
        if not any(kinds):
            return "POINTER"
        return "AMBIGUOUS"

    def returned_class(self, owner, name, count):
        """The one class a call of `name` with count arguments returns, or None."""
        entries = [(returner, returned) for returner, returned, low, high in self.class_returns.get(name, [])
                   if low <= count <= high and returned in self.bodies]
        classes = set()
        for klass in self.lineage(owner):
            classes = set(returned for returner, returned in entries if returner == klass)
            if classes:
                break
        if not classes:
            classes = set(returned for returner, returned in entries)
        return classes.pop() if len(classes) == 1 else None

    def callee(self, path, text, start, name, anchor):
        """(class the call is looked up in, the function's name) for the call named at `name`."""
        for expansion in self.macros.get(name.group(1), []):
            accessor = QUALIFIER.match(expansion)
            if accessor:
                return accessor.group(1), accessor.group(2)
        return self.receiver_owner(path, text, start, name.start(), anchor), name.group(1)

    def receiver_owner(self, path, text, start, name_start, anchor):
        """The class the name at name_start is looked up in, from what precedes it."""
        before_end = trimmed_end(text, start, name_start)
        before = text[start:before_end]
        if before.endswith("::"):
            qualifier = TRAILING_NAME.search(text, start, before_end - 2)
            return qualifier.group(1) if qualifier else None
        if before.endswith("->"):
            found = self.expression_type(path, text, start, before_end - 2, anchor)
            return pointee(found)[0] if found else None
        if before.endswith("."):
            return self.expression_class(path, text, start, before_end - 1, anchor)
        return self.layouts[path].owner_of(name_start if anchor is None else anchor)

    def expression_type(self, path, text, start, end, anchor):
        """(type, is_pointer, element type) of the expression text[start:end], or None. A
        position inside text is a position in the file when text is the file's mask; anchor
        is used instead when text is a macro's expansion."""
        start = trimmed_start(text, start, end)
        end = trimmed_end(text, start, end)
        if start >= end:
            return None
        if text[start] == "*":
            inner = self.expression_type(path, text, start + 1, end, anchor)
            if inner is None or not inner[1]:
                return None
            return pointee(inner) + (None,)
        cast = C_CAST.match(text, start, end)
        if cast and (cast.group(2) or cast.group(1) in self.bodies
                     or type_parts(cast.group(1), "")[0] in STRING_CLASSES):
            return type_parts(cast.group(1), cast.group(2))
        if text[end - 1] == "]":
            open_bracket = opening_bracket(text, start, end - 1)
            inner = self.expression_type(path, text, start, open_bracket, anchor) if open_bracket > start else None
            if inner is None:
                return None
            if inner[2]:
                #	A map's subscript reads its mapped value, not the pair its iterator reaches.
                pair = PAIR.match(inner[2][0])
                if pair and not inner[0].endswith("iterator"):
                    return piece_type(type_arguments(pair.group(1))[1])
                return inner[2] + (None,)
            return (inner[0], False, None) if inner[1] else None
        if text[end - 1] == ")":
            open_paren = opening_bracket(text, start, end - 1)
            if open_paren < start:
                return None
            cast = CXX_CAST.search(text, start, open_paren)
            if cast:
                return type_parts(cast.group(1), cast.group(2))
            if text[start:open_paren].rstrip().endswith(">"):
                return None         # a template called with explicit arguments: its return is not harvested
            name = TRAILING_NAME.search(text, start, open_paren)
            if name is None:
                inner = [self.expression_type(path, text, operand_start, operand_end, anchor)
                         for operand_start, operand_end in value_operands(text, open_paren + 1, end - 1)]
                for found in inner:
                    if is_text_object(found):
                        return found
                for found in inner:
                    if found and found[0] == AMBIGUOUS_TEXT:
                        return found
                return inner[0] if len(inner) == 1 else None
            before = text[start:name.start()].rstrip()
            if name.group(1) in ("CvWString", "CvString") and not before.endswith(("::", ".", "->")) \
                    or name.group(1) in ("wstring", "string") and before.endswith("std::"):
                return type_parts(text[start:open_paren].split()[-1], "")
            here = name.start() if anchor is None else anchor
            owner, function = self.callee(path, text, start, name, here)
            #	The mask blanks a literal, so an argument list is empty only when the source is.
            source = self.texts[path] if anchor is None else text
            count = len(argument_spans(text, open_paren, end - 1)) if source[open_paren + 1:end - 1].strip() else 0
            if function == "substr" and (owner in STRING_CLASSES or owner == TEXT_OBJECT):
                return TEXT_OBJECT, False, None
            verdict = self.text_verdict(owner, function, count)
            if verdict == "OBJECT":
                return TEXT_OBJECT, False, None
            if verdict == "AMBIGUOUS":
                return AMBIGUOUS_TEXT, False, None
            if verdict == "POINTER":
                return TEXT_POINTER, True, None
            klass = self.returned_class(owner, function, count)
            return (klass, False, None) if klass else None
        name = TRAILING_NAME.search(text, start, end)
        if name is None or name.start() != start and not text[start:name.start()].rstrip().endswith(("->", ".", "::")):
            return None
        here = name.start() if anchor is None else anchor
        if name.group(1) == "this":
            return self.layouts[path].owner_of(here), True, None
        declaration = self.declaration(path, text, start, name, here)
        if declaration is not None:
            declared = type_parts(declaration.type_name, declaration.modifiers)
            if declaration.terminator == "[":
                return declared[0], True, declared[:2]
            return declared
        if name.start() != start or self.expanding >= MACRO_DEPTH:
            return None
        self.expanding += 1
        try:
            #	Each definition is tried, a string object first, as the check means the worst case.
            found = [self.expression_type(path, expansion, 0, len(expansion), here)
                     for expansion in self.macros.get(name.group(1), [])]
        finally:
            self.expanding -= 1
        for each in found:
            if is_text_object(each):
                return each
        return next((each for each in found if each), None)

    def expression_class(self, path, text, start, end, anchor):
        found = self.expression_type(path, text, start, end, anchor)
        return found[0] if found else None

    def declaration(self, path, text, start, name, here):
        """The Declaration the variable or member `name` refers to, or None."""
        before_end = trimmed_end(text, start, name.start())
        before = text[start:before_end]
        if before.endswith("->"):
            found = self.expression_type(path, text, start, before_end - 2, here)
            return self.member(pointee(found)[0], name.group(1)) if found else None
        if before.endswith("."):
            return self.member(self.expression_class(path, text, start, before_end - 1, here), name.group(1))
        if before.endswith("::"):
            qualifier = TRAILING_NAME.search(text, start, before_end - 2)
            return self.member(qualifier.group(1), name.group(1)) if qualifier else None
        return self.scoped(path, name.group(1), here)

    def scoped(self, path, name, here):
        """The declaration of `name` visible at here: the nearest one in the blocks around it or
        among its function's parameters, then a member of the method's class or its bases, then
        one at this file's scope, then a header's global."""
        layout = self.layouts[path]
        entries = self.declarations[path].get(name, [])
        function = layout.function_of(here)
        if function >= 0:
            block = layout.blocks[function]
            visible = None
            for declaration in entries:
                if declaration.start >= here:
                    break
                if declaration.start < block.head_start:
                    continue
                if declaration.start < block.open:
                    if in_parentheses(self.masks[path], declaration.start):
                        visible = declaration
                elif layout.encloses(layout.innermost(declaration.start), here):
                    visible = declaration
            if visible is not None:
                return visible
            member = self.member(block.name, name)
            if member is not None:
                return member
        for declaration in reversed(entries):
            if declaration.start < here and self.at_file_scope(path, declaration.start):
                return declaration
        return self.globals.get(name)

    def member(self, owner, name):
        """The declaration of member `name` in owner's class body or a base's, or None."""
        pair = PAIR.match(owner or "")
        if pair and name in ("first", "second"):
            piece = type_arguments(pair.group(1))[name == "second"].strip()
            return Declaration(-1, piece.rstrip("*& "), "*" if piece.endswith("*") else "", ";")
        for klass in self.lineage(owner):
            for path, open_brace, close_brace in self.bodies.get(klass, []):
                layout = self.layouts[path]
                for declaration in self.declarations[path].get(name, []):
                    if open_brace < declaration.start < close_brace \
                            and layout.function_of(declaration.start) < 0 \
                            and not in_parentheses(self.masks[path], declaration.start):
                        return declaration
        return None

    def operand_verdict(self, path, start, end):
        """OBJECT / AMBIGUOUS / None for the value the file's text[start:end] passes."""
        found = self.expression_type(path, self.masks[path], start, end, None)
        if found is not None and found[0] == AMBIGUOUS_TEXT:
            return "AMBIGUOUS"
        return "OBJECT" if is_text_object(found) else None


def string_findings(files):
    """(findings, review, calls checked, sinks, string object names) for string objects passed
    to a varargs formatter."""
    census = Census(files)
    sink_call = re.compile(r"\b(" + "|".join(sorted(re.escape(name) for name in census.sinks)) + r")\s*\(")
    findings = []
    review = []
    calls_checked = 0
    for path, text, masked in files:
        layout = census.layouts[path]
        for match in sink_call.finditer(masked):
            close_paren = call_extent(masked, match.end() - 1)
            if close_paren < 0 or layout.function_of(match.start()) < 0:
                continue
            spans = argument_spans(masked, match.end() - 1, close_paren)
            first = census.sink_fixed(path, masked, match)
            if first is None or len(spans) <= first:
                continue
            calls_checked += 1
            line = text.count("\n", 0, match.start()) + 1
            for position in range(first, len(spans)):
                start, end = spans[position]
                verdict = worst([census.operand_verdict(path, operand_start, operand_end)
                                 for operand_start, operand_end in value_operands(masked, start, end)])
                entry = (path, line, position, " ".join(text[start:end].split()))
                if verdict == "OBJECT":
                    findings.append(entry)
                elif verdict == "AMBIGUOUS":
                    review.append(entry)
    object_names = len([name for name, entries in census.text_returns.items()
                        if any(is_object for owner, is_object, low, high in entries)])
    return findings, review, calls_checked, len(census.sinks), object_names


def main():
    if not os.path.isdir(SOURCE_ROOT):
        print("verify-gettext-widths: run me from the repo root (no %s/ here)." % SOURCE_ROOT)
        return 2

    sources = []
    for path in source_files():
        text = read(path)
        masked = strip_literals(text)
        #	The mask is index-for-index with the source. If that ever stops holding, every
        #	offset below points at the wrong place and the run reports CLEAN while seeing
        #	nothing -- so this is a hard stop, not a warning.
        if len(masked) != len(text):
            print("verify-gettext-widths: INTERNAL -- mask desynchronized on %s" % path)
            return 2
        sources.append((path.replace(os.sep, "/"), text, masked))

    declarations = collect_declarations(sources)
    wide_names = set(name for name, entries in declarations.items()
                     if any(is_wide for is_wide, low, high in entries))
    if not wide_names:
        print("verify-gettext-widths: found no 64-bit returning functions -- refusing to")
        print("report a clean run off an empty census.")
        return 2

    candidate_call = re.compile(
        r"\b(" + "|".join(sorted(re.escape(n) for n in wide_names)) + r")\s*\(")

    review = []
    findings = []
    calls_checked = 0
    for path, text, masked in sources:
        for match in GETTEXT_CALL.finditer(masked):
            close_paren = call_extent(masked, match.end() - 1)
            if close_paren < 0:
                continue
            spans = argument_spans(masked, match.end() - 1, close_paren)
            if len(spans) < 2:
                continue          # no varargs -- nothing can shift
            calls_checked += 1
            line = text.count("\n", 0, match.start()) + 1
            for position in range(1, len(spans)):
                start, end = spans[position]
                bare = remove_consuming_calls(text[start:end])
                for hit in candidate_call.finditer(bare):
                    inner_open = hit.end() - 1
                    inner_close = call_extent(bare, inner_open)
                    if inner_close < 0:
                        continue
                    own_count = argument_count(bare[inner_open + 1:inner_close])
                    verdict = width_verdict(declarations, hit.group(1), own_count)
                    if verdict == "WIDE":
                        findings.append((path, line, position, hit.group(1), own_count))
                    elif verdict == "AMBIGUOUS":
                        review.append((path, line, position, hit.group(1), own_count))

    for path, line, position, name, count in review:
        print("REVIEW    %s:%d  argument %d calls %s() with %d argument(s) -- declared at"
              % (path, line, position, name, count))
        print("          BOTH widths at that arity, so which overload it reaches is a human verdict.")

    object_findings, object_review, object_calls, sinks, object_names = string_findings(sources)
    if not object_names or sinks <= len(CRT_FORMATTERS):
        print("verify-gettext-widths: found no functions returning a string object or no varargs")
        print("functions -- refusing to report a clean run off an empty census.")
        return 2
    for path, line, position, argument in object_review:
        print("REVIEW    %s:%d  argument %d `%s` -- its name returns a string object in one class and"
              % (path, line, position, argument))
        print("          a pointer in another, and the call site does not say which: a human verdict.")

    if findings:
        print("")
        for path, line, position, name, count in findings:
            print("WIDE ARG  %s:%d  argument %d passes %s()/%d -- 64-bit into varargs getText"
                  % (path, line, position, name, count))
        print("")
        print("getText calls checked: %d -- %d WIDE ARGUMENT(S), %d to review"
              % (calls_checked, len(findings), len(review)))
        print("An 8-byte argument takes TWO 4-byte slots, so every later placeholder reads")
        print("one slot early and a %s lands on an integer -- the EXE then runs wcslen on")
        print("it and faults at an address equal to that value. Do NOT cast to int; render")
        print("the value and use a %s placeholder:")
        print("    CvWString::format(L\"%I64d\", x).GetCString()")
    if object_findings:
        print("")
        for path, line, position, argument in object_findings:
            print("STRING ARG %s:%d  argument %d `%s` -- a string object into a varargs formatter"
                  % (path, line, position, argument))
        print("")
        print("varargs calls checked: %d -- %d STRING OBJECT ARGUMENT(S), %d to review"
              % (object_calls, len(object_findings), len(object_review)))
        print("The object is copied into the varargs whole (28 bytes, 7 slots): the %s reads")
        print("its first slot as the pointer and every later placeholder reads six slots late.")
        print("Pass a pointer of the format's width -- getText and CvWString::format take wchar_t*:")
        print("    x.GetCString()                (a CvWString; .c_str() for a std::wstring)")
        print("    CvWString(x).GetCString()     (a narrow CvString or std::string)")
        print("and the narrow formatters take char*: x.GetCString() on a CvString,")
        print("CvString(x).GetCString() on a wide one.")
    if findings or object_findings:
        return 1

    print("getText calls checked: %d, against %d names declared 64-bit somewhere"
          % (calls_checked, len(wide_names)))
    print("getText widths clean -- no 64-bit value reaches the EXE's varargs formatter.")
    print("varargs calls checked: %d, to %d varargs names, against %d names returning a string object"
          % (object_calls, sinks, object_names))
    print("varargs strings clean -- no string object reaches a formatter, %d to review." % len(object_review))
    return 0


if __name__ == "__main__":
    sys.exit(main())
