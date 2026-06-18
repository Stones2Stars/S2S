# Finding your XML errors with the XML Validator

Stones2Stars ships a validator that checks **every** XML file under `Assets\Xml\`
and `Assets\Modules\` against the game's schema. It is the **same check the CI/CD
pipeline runs on every push** (`appveyor.yml` → `XmlValidator.exe -a`), so a clean
local run means your edits will pass the build. Run it after any XML change before
you commit or share a module.

The tool is `Tools\XmlValidator.exe` (a small .NET program, "Civ4 XML-Validator
C2C Edition" by Alberts2).

## Running it

It uses **relative paths** (`..\Assets\Xml`, `..\Assets\Modules`), so it must be
run from the `Tools\` folder.

**Easy way — double-click:**

1. Open the `Tools\` folder.
2. Double-click `XmlValidator.exe`.
3. It scans everything, prints the results, and waits on **"Press any key to
   exit…"** so you can read the output before the window closes.

**Terminal way — same as CI:**

```powershell
cd Tools
.\XmlValidator.exe -a
```

The `-a` flag is the automated/CI mode: it does not pause for a keypress and just
returns when finished. Run it from a **real** PowerShell or cmd window — not piped
or redirected into another program — because the tool resizes the console window
and will crash if no real console is attached.

## Reading the results

- **Success:** the final line reads
  `Validation of <N> files complete without error(s)!`
- **Failure:** error lines appear in the output and it ends with
  `Validation failed!`

Every result is also written to **`Tools\XmlValidator.log`**, which is easier to
search than scrolling the console. Each line is one of:

- `Validating <path>` — progress only; ignore these.
- an error in the form `<file>,<line>: <message>` — **these are what you fix.**

**Tip:** open `Tools\XmlValidator.log` and look for any line that does *not* start
with `Validating` — those are your actual problems, each with the file and line
number to jump to. Fix, re-run, repeat until you get `complete without error(s)!`.

## What it does and does not catch

**Catches** (these fail the build):

- Malformed XML — unclosed or mismatched tags, stray/illegal characters, broken
  encoding.
- Schema violations — wrong or misspelled tag names, tags in the wrong order,
  values of the wrong type.

**Does *not* catch** — logical/content mistakes such as referencing a `TYPE_…`
that does not exist, a building that requires a tech you deleted, or an art define
pointing at a missing file. Those surface at **game load**, not here. Check the
game's log files instead:

- `…\Documents\My Games\Beyond the Sword\Logs\XmlLoad.log` — per-category load
  counts.
- `…\Xml_MissingTypes.log` — references to types that were never defined.

(Enable logging in `…\My Games\Beyond the Sword\CivilizationIV.ini` with
`LoggingEnabled = 1`.)

## Related tools

The `Tools\` folder also has helpers for keeping XML tidy (see `.vscode/tasks.json`
for the wired-up VS Code tasks):

- **Sort by tag** (`Tools\XMLTools\SortByTag.bat <file>`) — reorders a file's tags
  into schema order, which removes a whole class of "tag in the wrong order" errors.
- **Autocorrect** (`Tools\Autocorrect\Autocorrect.bat --interactive <file>`) —
  interactive fix-ups for common mistakes.
