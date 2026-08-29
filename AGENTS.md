<!-- ═══════════════════════════════════════════════════════════════════════════ -->

> # ⛔ READ THE DOCS FOR WHAT YOU TOUCH, BEFORE YOU TOUCH IT
>
> Most of the cost of this project has been agents guessing at problems the specs had already solved. These four
> rules exist to stop that, and they are not optional.
>
> - **⛔ FIND the docs, do not DECIDE them.** Before the first edit, grep the whole of `docs/` for the subsystem,
>   the symbols and the mechanism you are about to touch, and read every file that hits — END TO END, not the
>   section you judge relevant. [`docs/README.md`](docs/README.md) is the index. **Your judgment of what is
>   "necessary" is systematically biased toward reading too little**, so the COUNT of relevant files is something
>   you find, never something you choose. There is no clause that lets you read one file and stop; any wording
>   that reads like one is a defect to delete on sight. ⚠ A large concept's home is a DIRECTORY behind a hub page
>   — `cascade.md` is a map and the spec is the pages in `cascade/`, so stopping at a hub is reading the contents
>   page and calling it the book.
>   ⚖ **ONE TIER IS OUT OF THAT SWEEP, AND IT IS A DIRECTORY, NEVER A JUDGEMENT: `docs/plans/parked/`.** Those are
>   initiatives OUT of active scope, carried as-is with stale paths and stale status by design
>   ([parked README](docs/plans/parked/README.md)) — reading one as current is the harm, not skipping it. A parked
>   doc is opened when ITS initiative becomes active, at which point it is re-grounded and moves out of `parked/`.
>   ⛔ This exempts a DIRECTORY and nothing else: every `specs/`, `reference/`, `architecture/` and top-level file
>   the grep hits is read END TO END, and "this looks parked to me" about anything outside `plans/parked/` is the
>   permission slip this rule exists to refuse.
> - **⛔ THE TREE OUTRANKS THE DOC.** The engine was rebuilt and the docs lag it, so a doc line is a HYPOTHESIS you
>   confirm against `Sources/`. Where they disagree the code wins, and **fixing the doc is part of the same work
>   item** (§ Docs). ⚠ This is not licence to skip the doc: only the doc tells you which behaviour is DESIGNED and
>   which is accidental.
> - **⛔ IMPLEMENT the spec as written; challenge it afterward, with evidence** — never by inventing your own
>   approach up front. The moment you catch yourself DESIGNING, stop and go find the spec that already defines it.
> - **⛔ VERIFY every claim against the live code before acting on it** — a plan doc's status line, your own
>   memory, an owner aside. Much of the waste was agents acting on something they "knew" that a 30-second check
>   would have disproved.
>
> **⛔ ALL docs live in the repo.** A plan or design note kept only in a local/private notes folder
> (`.claude/plans/`, assistant memory) is a violation to fix by moving it into `docs/`, never a doc you may skip.

<!-- ═══════════════════════════════════════════════════════════════════════════ -->

# Stones2Stars (S2S) — Agent Guide

Stones2Stars is a Civ4 / Caveman2Cosmos (C2C) mod. The compiled artifact is
`CvGameCoreDLL.dll`, a C++ DLL that drives game logic and AI, paired with
XML data (`Assets/XML/`) and Python callbacks (`Assets/Python/`).

This file is the top-level guide. Subdirectory-specific rules live in nested
`AGENTS.md` files — see `Sources/AGENTS.md` for the C++ code-style and
architecture rules that apply to DLL source.

## Repository Layout

- `Sources/` — C++ DLL source (`Cv*` engine classes, `Cy*` Python wrappers). Has its own `AGENTS.md`.
- `Assets/XML/` — game data definitions (validated by `Tools/XmlValidator.exe`).
- `Assets/Python/` — Python event callbacks (`CvEventManager.py`, etc.).
- `Tools/` — build + tooling (`_Build.ps1`, `MakeDLL*.bat`, validators, FastBuild).
- `Build/` — build output (`Build/<Config>/CvGameCoreDLL.dll`); gitignored.
- `.claude/skills/` — project-exclusive Claude Code skills (see "Project Skills" below).

## Build And Test

> **⛔ HARD RULE — the `.vcxproj` / `.sln` / `.vcxproj.filters` files are DEAD for build purposes. NEVER read them to learn ANY build fact.**
> They are stale IDE-display artifacts that do **not** drive the build and are **not** kept in sync. Any build fact
> comes from **FastBuild only**: `Sources/fbuild.bff` (+ `Tools/_Build.ps1`). The `.vcxproj`'s `PlatformToolset` says
> `v142`, which is FALSE — see the toolchain note below. When unsure, read `fbuild.bff`.
>
> **Actual toolchain (from `fbuild.bff`):** the vendored **Microsoft Visual C++ Toolkit 2003 = MSVC 7.1 (VC2003)**
> compiler/linker (`Build/deps/...`), **Python 2.4**, **Boost 1.32 / 1.55** (why BOTH Boosts coexist + why the whole
> stack is frozen by the closed `.exe`: [`docs/reference/engine.md`](docs/reference/engine.md)). So the DLL is
> genuinely **C++03, 32-bit/x86** — *no* `std::thread`, *no* OpenMP, *no* C++11+. This is a hard compiler limit (the
> toolchain is locked to stay ABI/STL-compatible with the closed VC7.1 game `.exe`), **not** a style convention.
> In-process threading means raw Win32 only. Do not modernize or replace the build chain/toolchain.
>
> **⛔ HARD RULE — THE RED-RATCHET: NEVER "fix" a build by restoring old Infos.**
> The 23 XML engine info classes (`CvBuildingInfo`, `CvUnitInfo`,
> `CvTechInfo`, … — 46 files) were moved to `SourceArchive/Infos/` ON PURPOSE: parity is past, the JSON-fed pocos
> in `Sources/Infos/` are their replacements, and the missing classes are a RATCHET — during the cutover,
> consecutive agents "finished" by quietly wiring old Infos back in wherever the replacement didn't pan out, so the
> fallback was removed. ⚠ **A replacement KEEPS the archived class's NAME** — `Sources/Infos/CvBuildingInfo.h`
> declares `CvBuildingInfo`, and there is no `CvJson<X>Info` symbol anywhere — so what is banned is reviving the
> ARCHIVED XML-READING BODY, never the live name you can see in the tree. **Never restore anything from
> `SourceArchive/`, and never treat a red build as a defect to fix by reviving one.** The ONLY road to green: build
> the JSON-fed structure, wire it up, and wire up/replace ALL the getters (the engine/AI/UI consumer surface onto
> the JSON-fed infos). The ratchet is permanent and holds **whatever state the build is in**.
> ⛔ **This file states no build STATE, deliberately — a compile-state claim here is guaranteed to drift**,
> and a stale one is worse than none: it tells every agent the build works when it does not. **No doc records
> whether the tree compiles today. RUN THE BUILD; it is the only
> authoritative answer, and it now costs ~15s on `Assert`.** Rules belong here; status does not.
>
> **⛔ HARD RULE — READING A REPLACED INFO'S XML **INTO THE GAME** IS HARD BANNED.**
> The legacy info XMLs (`Assets/XML/**/CIV4<X>Infos.xml`) for every type we have replaced with a `CvJson<X>Info`
> poco are **CURATOR INPUT ONLY** — the curator reads them to generate the `Assets/Data/**` JSON, which is why they
> were kept in the repo after being removed once (their removal broke the curator). **The running GAME must NEVER
> read them.** Concretely: **do NOT add or keep `LoadGlobalClassInfo(GC.m_pa<X>Info, "CIV4<X>Infos", …)` for a
> replaced type** — that loads the XML into the engine and is the exact rollerskate that keeps sneaking back
> ("the XMLs are present, so we're allowed to use them" — NO). Replaced infos are registered + populated from the
> **JSON** load path, not the XML shell. Symptom when violated: a JSON-only entity (e.g. `TECH_GAME_START`, which has
> no XML entry) gets no engine id → falls out of `m_pa<X>Info`/`m_pabHasTech` → wild `isHasTech` reads / load crashes.
> The XML files staying in the tree for the curator is **not** license to read them at runtime.

The build is driven by **`Tools/_Build.ps1`** (a FastBuild wrapper). Invoke it
**from the `Sources/` directory**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb> [<verb> ...]
```

- **Configs:** `Assert`, `Debug`, `Release`, `FinalRelease`, `Testing`.
  Output lands in `Build/<Config>/CvGameCoreDLL.dll` (+ `.pdb`).
  - **⛔ THE INTERNAL PROFILER CANNOT BE SELECTED, AND THAT IS STRUCTURAL — never
    add `PROFILE()`/FProfiler scopes as instrumentation, because they report to nothing.** `USE_INTERNAL_PROFILER`
    is defined NOWHERE, so no config compiles the profiler in; the `Profile`/`ProfileExtra` configs that once did
    are deleted. ⚠ `_Build.ps1` passes its config argument straight to FastBuild with no whitelist, so a
    config that EXISTS is a config anyone can select — which is why the fix was removing them rather than
    discouraging them. The ONE instrument is the
    gated `[PERF]` logging (`logPerf`/`gPerfLogLevel`, `Autolog__LogLevelPerf`), which ships in every build.
    **The macros now compile to NOTHING in every config, so the scopes already in the tree are INERT** — not a
    defect, not a purge backlog. What binds is the DIRECTION: **we never build TOWARD using the profiler.** So a
    broken/stale FProfiler include or reference is **DELETED as irrelevant code, never repaired** — "the build
    can't find `FProfiler.h`" is never a reason to restore it
    ([neither playability nor compiling gates removing legacy](docs/specs/validation.md#playability-not-a-gate)).
  - **Which config for in-game testing:** for ordinary interactive testing — exercising a feature, pulling state
    from the HTTP endpoints, watching `/events` — a normal **`Release`** build suffices and is far faster than
    `FinalRelease` (a clean `FinalRelease` is a ~7-minute full rebuild). **Reserve `FinalRelease` for turn-lag /
    performance hunting**, where its optimizations are the thing under test. `Assert` stays the quick compile-check;
    `Release`/`FinalRelease` are for actually running.
- **Verbs (composable, in order):** `clean`, `build` (incremental), `rebuild` (clean+build), `deploy` (xcopy DLL/PDB into `Assets/`).
- **Modifier — `nostop` (opt-in, off by default):** passes FastBuild's `-nostoponerror`, so the build keeps going
  after a failed object instead of stopping at the first one. When a build is broadly broken the default stops
  after a fraction of the objects, so a single `nostop` run reports across far more translation units — use it
  when you want the whole error surface in one pass rather than peeling it one TU at a time. Unlike the verbs it is a
  modifier, not an ordered action: it is scanned up front and applies to every `build`/`rebuild` in the invocation
  regardless of position (`... _Build.ps1 Assert build nostop`). The `MakeDLL*.bat` shortcuts forward their extra
  arguments, so `MakeDLLAssert.bat nostop` works too. ⚠ It changes only how MUCH gets reported, never what
  compiles — and MSVC's 100-errors-per-TU cap (`C1003`) still truncates each unity batch.
  - **⚖ THE OWNER'S LAUNCH PATH ALWAYS REBUILDS THE DLL.** The game is always started via `LaunchS2S.bat`, which
    rebuilds the DLL every time — this is unavoidable, not a habit. ⇒ **Do NOT tell the
    owner to build or deploy before playing, and do not report "not deployed" as a blocker on a C++ change** —
    starting the game IS the deploy. A C++ edit sitting in the working tree reaches the next session by
    construction.
    ⚑ **What this does NOT retire is the ORDERING hazard, and the narrowing is the useful part:** since the DLL is
    rebuilt at launch, new-Python-against-old-DLL is impossible ACROSS a launch and possible only for an edit made
    **while the game is already running** — where `Assets/` is a symlink and Python ships on save while C++ does
    not. ⇒ The rule that actually binds is the one below about not touching `Assets/Python` mid-session, not a
    deploy reminder.
    ⚠ It also does not retire VERIFYING a deployed artifact when you have reason to doubt it (the timestamp +
    string-grep check below) — that is cheap and it is how this fact got confirmed rather than assumed.
  - **⛔ HARD RULE — KILL THE GAME BEFORE `deploy`.** A running game holds `Assets/CvGameCoreDLL.dll` OPEN, so the
    xcopy fails — but the build still reports `FBuild: OK`, so **the deploy failure is SILENT**. `Assets/` keeps
    the OLD DLL, the game then loads that OLD DLL, and you verify a binary that does not contain your change
    (this has burned agents repeatedly: a "green build" says nothing about what is RUNNING). Close the game, run
    `deploy`, and **verify the deployed artifact itself** — compare `Assets/CvGameCoreDLL.dll`'s timestamp against
    `Build/<Config>/`, or grep the binary for a string your change introduced. Never infer deployment from build
    output. (`agentstart.bat` closes the game too, but it runs AFTER deploy — it cannot rescue a copy that already
    failed.)
  - **⛔ HARD RULE — `Assets/` IS THE LIVE GAME, SO A PYTHON EDIT SHIPS THE INSTANT YOU SAVE IT.**
    `Mods/<mod>/Assets` is a SYMLINK to the repo's `Assets/` (`DevSetup.bat`), so there is no copy step and no
    deploy for Python: the file you just wrote is the file the running game imports. **Editing Python while the
    owner is loading or playing is editing the running game.** ⇒ Do not touch `Assets/Python` while a load is in
    flight; the C++ half is the opposite (a build lands in `Build/<Config>/` and reaches the game only on
    `deploy`), and that ASYMMETRY is what makes the ordering below a rule rather than a preference.
  - **⛔ THE ORDER IS DLL FIRST, THEN THE PYTHON THAT CALLS IT — a new binding referenced from module scope is a
    HARD IMPORT BREAK, not a missing feature.** A Python module that binds a not-yet-published type
    (`WORLD = CyWorldInfo()` at module scope) raises `NameError` AT IMPORT against a DLL that predates it. That
    kills the module, and because the engine enters Python through one import chain
    (`CvEventInterface` → `BugEventManager` → `CvEventManager` → `CvScreensInterface`,
    [python-load-sequence.md](docs/reference/python-load-sequence.md)) one dead module takes the WHOLE UI down —
    which is the signature to recognise: *the UI vanished and the tracebacks name modules you never touched.*
    ⚠ **The traceback points AWAY from the cause.** The chain dies at the first import it cannot complete, so the
    reported file is whichever module imported the broken one — the failing line is not the guilty line, and the
    guilty file may not appear in the trace at all. ⛔ So do NOT debug the named module; check first whether any
    NEW module-scope binding outruns the deployed DLL. *(Measured: six files gained a `CyWorldInfo` binding at
    09:50 and the DLL publishing it deployed at 09:54 — every load in that window lost the entire interface.)*
- **Quick compile check after an edit:** `Assert rebuild` from `Sources/`.
  **⚑ CLEAN-REBUILD WALL CLOCK, per config — the spread is what decides which one you reach for:**

  | config | clean `rebuild` | why |
  |---|--:|---|
  | **`Assert`** | **~15s** | minimal optimization; 54 objects at ~13:1 parallelism (3m11s CPU / 14.7s wall) |
  | `Release` | ~1m40 | optimized |
  | `FinalRelease` | ~8min | fully optimized — the config the "several minutes" folklore actually described |

  ⛔ **So on `Assert` there is NO reason to ever run an incremental `build`.** It saves ~7 seconds (7.8s vs 14.7s)
  and buys the stale-PCH false-verification hazard below in exchange — a trade that is never worth making. Use
  `rebuild`, and the whole incremental-PCH question stops being reachable on the config you compile-check with.
  ⚠ FastBuild reports **CPU** time in its per-step summary and **wall** time only on the closing `Time: Real`
  line; reading the former as the latter is what makes a 15-second build look like three minutes.
  - **✅ BUILDING IS A LEGITIMATE CHECK AGAIN (the don't-build rule is RETIRED).** It was binding
    only because a red tree made a build uninformative; that premise is gone, so an `Assert rebuild` (~15s) is a
    real signal and the compiler is once more a census you can actually run. ⛔ It is still not a substitute for
    reading what you changed, and green is still not evidence that a change is CORRECT
    ([neither playability nor compiling gates removing legacy](docs/specs/validation.md#playability-not-a-gate) —
    done-is-observable, [validation.md](docs/specs/validation.md), is the acceptance bar).
    ⚠ Keep the cost asymmetry in mind: `Assert` is seconds, `Release`/`FinalRelease` are where the real minutes go.
  - **⛔ MSVC STOPS AT 100 ERRORS PER TRANSLATION UNIT (`fatal error C1003`), so whenever a build is broadly
    failing, a grep for YOUR files in the log PROVES NOTHING.** The unity batches truncate, and which files get to
    report is a function of how the earlier ones consumed the budget — so an edit of yours can be silently
    hidden behind ~38 files of pre-existing consumer debt, and appear only later when unrelated content shifts.
    *(Measured: a regex that stripped a first ctor initializer left `Class::Class()` followed by a leading comma
    in TWO infos; both compiled "clean" by that grep, and one surfaced a build later.)* **Check `grep -c C1003`:
    if it is non-zero, errors are hidden — verify your own files by reading what you changed, not by their
    absence from the log.**
  - **⛔ BUT `C1003 == 0` DOES *NOT* MEAN THE CENSUS IS COMPLETE — AFTER A BROAD CUT, A *LOW* ERROR COUNT IS
    ITSELF THE SUSPICIOUS SIGNAL.** MSVC stops reporting an undefined identifier well before the 100-error cap,
    so a delete-driven cut can leave dozens of live call sites and report a handful. **Measured: deleting six
    mark-routing helpers left 52 live references in one file and the build reported 7 — zero `C1003`.** The
    seven that reported were the ones sitting BEFORE the consumer class declaration; every call inside the
    class body was silent. ⚑ **The tell is the RATIO, not the cap:** if you deleted a symbol with N known
    references and the build names far fewer, the log is not the census. **After any delete-driven cut, count
    the surviving references with a grep and treat THAT as the worklist** — the compiler is the census for a
    deleted member only insofar as it actually reports one, and it is under no obligation to report them all.
  - **⛔ AN INCREMENTAL `build` DOES NOT REBUILD THE PCH, SO A HEADER EDIT CAN BE INVISIBLE TO EVERY TU.**
    `CvGlobals.h`, `CvEnums.h` and the rest of the umbrella live in `Build/<Config>/CvGameCoreDLL.pch`, and an
    incremental build has been observed serving a **day-old** PCH against freshly-edited headers — so every
    translation unit compiled against the OLD class. The signature is unmistakable once you know it and
    nonsensical before: a member you just added reports `C2039: 'x' : is not a member of 'cvInternalGlobals'`
    **and the members around it that have existed for years report `C3861: identifier not found` inside the same
    function body.** A class that half-exists is not a code error; it is a stale PCH.
    ⚠ **The real cost is a FALSE VERIFICATION, not the wasted minutes:** a "no errors in my files" grep taken
    against a stale PCH proves nothing, because the compiler never saw the edit. **Check the PCH's mtime against
    the headers you touched** (`ls -la Build/<Config>/CvGameCoreDLL.pch`), and when a header change is part of
    the work, verify with `rebuild` rather than `build`.
- The `Tools/MakeDLL*.bat` shortcuts (`MakeDLLAssert.bat`, `MakeDLLRelease.bat`, …)
  always `rebuild deploy` — full clean+rebuild+copy. Don't use them for an
  iterative compile-check loop.
- Full dev bootstrap: `DevSetup.bat`. CI flow: `appveyor.yml`.

### Validation

- **XML: `../Tools/XmlValidator.exe -a`, run FROM `Assets/`, FROM POWERSHELL.** Both halves are load-bearing and
  each fails in a way that reads as something else:
  - ⛔ **Run anywhere else and it validates NOTHING while reporting success** — from the repo root it prints
    `Validation of 0 files complete without error(s)!`, which is a clean pass over an empty set. Check the FILE
    COUNT, never the "without error(s)" (a real run is in the hundreds).
  - ⛔ **Run it from the Bash tool and it dies** in `Console.SetWindowSize` (`Positive number required … width`)
    — it wants a real console, so it reads as a broken tool rather than the wrong shell.
- Python callbacks: `Tools/XMLTools/verify-python-callbacks.py`.
- **Save migration: `python Tools/verify-savemigration.py`** — checks `Assets/savemigration.txt` against the
  tree, for the three failures [save.md §3](docs/specs/save.md) describes and nothing was running: a LISTED tag
  whose member is **still serialized** (it drains a live `WRAPPER_READ` — the value is lost on EVERY load, and
  it is quieter than the unlisted-orphan desync), a **bracketed** decorated entry (it can never match the
  normalized tag, so it silently fails to drain), and a **note line that registers as an entry** (the reader
  takes the first `::`-bearing token, so a wrapped prose line beginning `Class::something` inserts a bogus cut).
  ⚑ It mirrors `sm_ensureLoaded`/`sm_token` deliberately: a checker stricter than the engine's own parser
  under-reports, which is exactly how the prose case hid. ⛔ If it fires, decide which SIDE is right — a member
  whose only writer is `applyEvent` is genuine one-shot event state that CORRECTLY stays serialized, so there
  the ENTRY is the defect ([save.md §3](docs/specs/save.md)).
- **Python 2.4 syntax: `python Tools/verify-python24.py`** — the embedded interpreter is **Python 2.4**
  ([engine.md](docs/reference/engine.md)), so anything newer is a **SyntaxError at IMPORT**, and an import failure
  does not fail politely: every module on the engine's entry chain (`CvEventInterface` → `BugEventManager` →
  `CvEventManager` → `CvScreensInterface`) must import cleanly before **ANY** callback fires, so one bad line
  silently severs the whole engine→Python direction. ⚑ The trap it exists for is the **conditional expression**
  (`X if C else Y`, Python 2.5+): it is valid in every Python an agent has ever written, is invalid here, and
  reads as completely ordinary — it was written into the tree once and **very nearly a second time in the same
  session**, which is why this is a check and not a rule ("a rule has to be remembered; a check does not").
  Also catches `with`, `except X as e`, `"...".format()`, and dict/set comprehensions. ⛔ If it fires, rewrite the
  line for 2.4 — never widen the tool.
  ⚠ **IT IS A REGEX PASS, SO IT CANNOT SEE STRUCTURE — "clean" from it is NOT "this file parses".** Pair it with
  the structural check below after ANY bulk text edit.
- **Python structure: `python Tools/verify-python-syntax.py`** — every `def`/`class` is well formed and every
  bracket balances. ⚑ **The gap it closes is the one that took the game down:** a whole-file substitution meant
  for comment text (`re.sub(r"\(\s*\)", "", source)` — tidying an empty `()` left by deleting a citation) was
  applied to the WHOLE FILE and stripped **every** `()` in the tree, turning 185 `def foo():` into `def foo:`.
  `verify-python24` reported **clean**, because none of the damage is post-2.4 syntax. The game then failed at
  startup with `SyntaxError` in `CvAppInterface` — a module nobody had edited, because
  [the traceback points AWAY from the cause](#build-and-test).
  ⛔ **So a text pass over `Assets/Python` is not finished until this passes.** It cannot use `compile()`: the
  host is Python 3 and the scripts are 2.4 (a bare `print x` alone would fail), which is exactly why it checks
  STRUCTURE rather than parsing. ⚖ It deliberately does NOT flag `CyPlayer = CyPlayerX`-style assignments — the
  tree uses `CyCity`/`CyUnit`/`CyPlayer` as ordinary VARIABLE names, so that heuristic is unsound here.
  ⛔ **And the wider rule the incident actually teaches: a substitution intended for COMMENTS is applied to
  comment LINES, never to the file** — filter to lines whose first non-space character is `#`, or the edit
  silently rewrites code that merely resembles the pattern.
- **Python call arity: `python Tools/verify-python-arity.py`** — every Python call into the published `Cy*`
  surface must supply the number of arguments that surface declares. ⚑ **The hole it closes:** when a read is
  re-homed onto its object the signature usually changes with it, and a dropped parameter is invisible to every
  other check — the C++ compiles (the call site is Python), `verify-python-bindings` only asks whether the NAME
  is registered, and the structure check sees a well-formed call. *(Worked: `CyUnit::isInvisible` went from the
  legacy `(team, bDebug)` to `(team)`; the military advisor still passed two and died at runtime with
  `ArgumentError: ... did not match C++ signature`, taking the whole screen with it.)*
  ⚖ **ADVISORY — it is a TRIAGE LIST, not a defect count.** Python is untyped and this tree names ordinary
  locals `tab`, `scores`, `city`, even `CyPlayer`, so `tab.setStatus(x)` cannot be told from `CyUnit.setStatus`.
  It already skips names declared in more than one `Cy` header and the prefix-addressed `CyInfo` plane; what is
  left still needs a human to ask what the receiver actually is.
  ⛔ When an entry is real, fix the CALL SITE — never widen the C++ signature just to silence it. ⚖ Widening is
  right only where a consumer genuinely NEEDS the parameter: `CyCity::pushOrder` carries `bAppend` because the
  scenario copier replays a whole build queue and a fixed replace would keep only the last order.
- **Spine field types: `python Tools/verify-spine-fields.py`** — a `CvSpineEvent` field is declared once in the
  field-info switch (`*peType = SFT_X`) and filled at each emit by an adder (`addI`/`addStr`/`addW`/`addF`/`addB`);
  the two must agree, because **the RENDERER switches on the DECLARED type**. A field declared `SFT_STR` but filled
  with `addI` makes `spineRenderEventLine` read the integer AS A `char*`, so the process dies with an
  `ACCESS_VIOLATION at an address equal to the value` — nothing subtler than that, and nothing sooner either.
  ⚑ The trap it exists for is that **the compiler cannot see it** (both sides are ints at the call site; the
  mismatch lives between a switch arm and a call in another function) and **it is silent until that field is
  actually RENDERED** — so it survives every build and every run that does not happen to emit that fact past the
  log gate. *(Worked: `SPF_OBJECT_KIND` was correctly moved to a raw `addI` — a per-property, per-object LOAD emit
  must not resolve a name string at emit time — while its declaration stayed `SFT_STR`. The tree was red at the
  time, so nothing could run; the first green build crashed on load reading `faultAddr=0x00000005`, the
  object-kind value itself.)* ⛔ If it fires, fix the side that is WRONG — and prefer the raw int, since a payload
  carries typed fields and never a pre-resolved string ([spine.md](docs/spine.md)); never widen
  the tool.
- **Varargs text widths: `python Tools/verify-gettext-widths.py`** — ⛔ **a 64-bit value must NEVER be passed to
  the EXE's `gDLL->getText`.** It is VARARGS, so arguments match the TXT_KEY's placeholders positionally **by
  4-byte slot**: an 8-byte argument occupies TWO slots and every LATER placeholder reads one slot early, until a
  `%s` lands on an integer and the EXE runs `wcslen` on it — an ACCESS_VIOLATION **at an address equal to that
  value**. ⛔ The fix is NEVER a cast to `int` (that re-introduces the wrap the widening exists to prevent):
  pre-render it against a `%s` placeholder — `CvWString::format(L"%I64d", x).GetCString()`, which is what the
  tree already does for `TXT_KEY_LEADER_LEVEL_PROGRESS_1`. ⚑ It is a check because the compiler CANNOT see it
  (varargs accept anything), it is silent until that one tooltip branch renders, and the faulting value belongs
  to a DIFFERENT argument than the wrong one. *(Worked: `CvCity::getCulture` was widened to `int64_t` — correct,
  it wrapped negative in long games — but still passed raw to `TXT_KEY_CITY_BAR_CULTURE`, so `%d2` read culture's
  HIGH half and `%s3` read the culture THRESHOLD as a `wchar_t*`. Hovering a city bar faulted once per session
  for months.)* ⚖ Names are matched with their ARITY, since a receiver decides which overload a call means
  (`CvCity::getCulture(PlayerTypes)` is 64-bit, `CvEventInfo::getCulture()` is not); a same-arity pair declared at
  both widths is REPORTED for a human and does not fail.
- **Doc references: `python Tools/verify-docs.py`** — two silent decay modes the corpus cannot see in a diff.
  **LINKS + ANCHORS (fails the run):** every intra-repo link and `#anchor`, by GitHub's own slug algorithm. The
  cross-reference IS the design — the ledger is *"an INDEX, not a re-statement"*, so a doc LINKS a ruling instead
  of restating it, and that only works while the links resolve. ⛔ Fix the link or the heading; never delete the
  cross-reference to silence it, which trades a broken pointer for a second copy.
  **SYMBOL CENSUS (advisory, never fails):** doc-cited symbols that are `MISSING` or, worse, `ARCHIVED-ONLY` —
  alive only in `SourceArchive/`, i.e. **a doc describing a world that was deleted**, which is exactly the state
  that reads as current and gets built on. ⚖ Advisory because the verdict needs a human: naming a dead symbol is
  CORRECT in a tombstone (`superseded-ideas.md`, `parked/` — both exempt) and a LIE anywhere else. Per entry,
  either repoint the citation or DELETE the passage — the second is usually right, and it shrinks the corpus.
- **Whole-registry scans: `python Tools/verify-registry-scans.py [--list]`** — enumerates every
  `GC.getNum<X>Infos()`-bounded loop in `Sources/AI` + `Sources/Engine`, split into ENABLER-DOMAIN (re-point onto
  the maintained frontier) and OTHER-REGISTRY (the own-data inversion). ⚑ The compiler can NEVER name one — they
  are legal code deleting nothing, so this class closes only by being searched for; this is the searcher, so the
  search is repeatable rather than a snapshot.
  ⚖ ADVISORY: the registry is mechanical but the CONTEXT is not — init/reset/serialization/UI legitimately walk a
  registry, so triage per site and treat the counts as a RATCHET that may only fall.
- **Abandoned `#ifdef` alternates: `python Tools/verify-ifdef-attics.py`** — fails on a guard with NO `#define`
  anywhere (not in `Sources/`, not in `fbuild.bff`, not even commented): nobody can ever switch it on, so the
  block is dead code. ⚑ **That is the ONLY verdict a tool can reach**, because what makes an `#ifdef` wrong is
  WHAT IS BEHIND IT (Conventions §Design) — diagnostics and deliberate off-switches are legitimate, caching and
  game mechanics are not, and the preprocessor cannot tell you which. The check REPORTS those three classes for
  a human verdict and never fails on them. ⛔ It also flags TU-LOCAL guards (`#define`d in a `Sources/` file, so
  ON in some translation units and OFF in others) as never-collapse-mechanically.
- **BUG options + the logging surface: `python Tools/verify-bug-options.py`** — a BUG option is addressed by
  STRING from C++ (`getBugOptionINT("Autolog__LogLevelStream", 1)`) and declared in XML far away
  (`Assets/Config/*.xml`), and **nothing connects the two** — no compiler, no loader, no assert. So a knob can be
  half-wired in EITHER direction and the game runs, saying nothing. Three checks:
  - **READ BUT NOT DECLARED (fails)** — the lookup then always answers the call-site DEFAULT, so the option is
    pinned and unreachable from the options screen AND the `.ini`, while looking exactly like a working knob.
    *(Worked: `Autolog__LogLevelStream` drove `gStreamLogLevel` and was declared nowhere, welding the `/events`
    verbosity to 1 — un-raisable precisely when someone went looking for it.)*
  - **A DECLARED `LogLevel*` NOBODY CONSUMES (fails)** — the mirror: a dropdown promising control that does not
    exist. ⚠ A reference from an options TAB does not count as consumption, which is the whole point — the three
    dead dropdowns were all rendered and all read by nothing. Consumption is a DLL read, a Python getter
    (explicit `get=` or the auto-derived `get<Id>`), or a `<change>` handler.
  - **THE LEGACY LOGGING SURFACE IS A RATCHET (fails only on a RISE)** — direct `gDLL->logMsg` sites, the
    scope-named globals and the BBAI helpers are legacy that **cannot be removed yet**, so the check never
    demands their removal; it refuses GROWTH only. ⛔ That is the anti-rollerskate mechanism, because the pull is
    never *"revive the legacy logger"* — it is *"add one more line beside the twenty already here"*. A new gate
    reads the UNIVERSAL level and a new emit goes through the event spine; **never raise a ceiling to pass**, and
    lower it in the same commit when the surface shrinks.
- **Python migration burndown: `python Tools/verify-python-bindings.py [--list]`** — counts the methods still
  DECLARED on a `Cy*` wrapper, registered nowhere, and still CALLED from Python: i.e. consumers not yet
  re-pointed onto the coherent reads. ⛔ **NOT a bug count, and NOT a re-registration list.** The `Cy*` bindings
  ARE the API surface for Python — a type publishes the GET/PUT/POST it is required to
  ([the identity set is the floor, not the ceiling](docs/architecture/patterns.md)); the legacy per-field
  contract is what died, not the surface. The tree shows the re-home well under way — `CyPlayer` 332 endpoints,
  `CyCity` 157 (the group reads `getYields`/`getCommerces`/`getWellbeing`/`getScalars`/… plus mutators),
  `CyTeam` 116, `CyPlot` 106 — so a THIN wrapper is an UN-RE-HOMED TYPE, never a finished one.
  ⚑ **The count is CONCENTRATED, which is the useful part**, and splits into two different jobs: a type that HAS
  its group reads and still carries the legacy names beside them (`CyCity` — 157 published against 110 legacy;
  *the collision IS the work*), and a type not re-homed at all whose GET surface must be built first (`CyUnit` —
  8 published against 58 legacy). ⚖ **ADVISORY, and a RATCHET like the registry scans: the count may only
  FALL**; a rise means a legacy declaration was revived or a legacy call added. It does not fail the run — a
  permanently-red gate on a known in-progress migration is one nobody can act on.
  ⛔ **CLOSE ONE BY KILLING THE DECLARATION, NEVER BY RE-REGISTERING IT.** An unpublished legacy method on a
  wrapper is the per-field contract still written down, so the next agent reads it as the surface and *"just
  publish what is already declared"* looks like the cheap fix exactly when the new surface arrives. The
  declaration AND its body go; the Python re-points onto the new surface homed on the type it addresses
  (`CyCity::getPopulation()`, never `getCityPopulation(owner, id)` — *"the moment you have
  `getAnotherObjectSomething`, we have failed"*). ⚠ NOT killed with them: the `class_<>` REGISTRATION (a
  zero-`.def` registration is the type identity the kept engine→Python direction needs — the marshaller throws
  without a registered converter), the identity set, and anything the ENGINE calls on the wrapper.
  ⛔ **Registration lives in TWO styles and scanning one under-reports badly**: inline `class_<CyX>("CyX").def(…)`
  under `Sources/Python/`, and loader functions taking `class_<CyX>& inst` under
  `Sources/Infrastructure/CvPython*Loader.cpp` — both spell the type inside `class_< >`, so the tool carries the
  most recent one forward and **ATTRIBUTES each `.def` to its class**.
  ⛔ **THAT ATTRIBUTION IS LOAD-BEARING, AND THE NAME-LEVEL TEST IT REPLACED HID THE WORST INSTANCE OF THIS
  DEFECT FOR YEARS.** Asking merely *"is this name registered anywhere?"* passes a method registered on a
  DIFFERENT wrapper: `plot` is declared on `CyUnit`, `CyCity`, `CyMap` AND `CySelectionGroup` and registered on
  only two of them, so `unit.plot()` — which raises `AttributeError` every time it runs — was invisible to the
  check built to find it. ⚑ **And a dead read does not present as a dead read:** a screen that throws while
  CONSTRUCTING never becomes an active screen, so it surfaced as *"ESC does not close the military advisor"* —
  a keyboard bug that was never a keyboard bug, chased down three wrong leads.
  ⚠ **The count JUMPED when the attribution landed (44→80 methods, 197→1467 sites) — that is the tool becoming
  correct, NOT the tree regressing.** The ratchet runs from the new figure; do not read the step as a rise to
  investigate. It also requires the call's RECEIVER to denote a Cy handle,
  which is what makes it precise — `add` is both a `CyArgsList` member and Python's own `set.add`, so a
  receiver-blind scan reports 40 phantom sites for it and buries the real findings under ~200 like it.
  ⚖ It also reports UNRESOLVED CALLS — names registered nowhere, declared on no `Cy*` class, and not a Python
  `def`: usually a mechanic removed outright, or one that never existed (`getCommerceRateTimes100` is called by
  two `canTrigger` predicates and has never existed in the tree, so those two random events cannot fire).

### Adding a new source subdirectory

`Sources/fbuild.bff` is the **source-of-truth for what FastBuild compiles** — the
`.vcxproj` files are IDE-only and do NOT drive the build. **fbuild RECURSIVELY globs** every `.cpp` under
`$SOURCE_DIR$` (the Unity + the `Cy*` ObjectList both `*PathRecurse = true`, excluding
`include`/`lib`/`.vs`/`.vscode`/`nbproject`), so a new `Sources/<NewDir>/` is compiled **automatically — no
`.UnityInputPath` edit needed**. When you add a subdir/files:

1. Put the `.cpp`/`.h` under `Sources/<NewDir>/`. Cross-module `#include`s are **path-qualified**
   root-relative (`"<Dir>/Header.h"`, resolved via `/I"$SOURCE_DIR$"`); same-folder includes stay
   bare (MSVC searches the including file's own dir first). The shared layers `Infos`/`Cascade` are
   on `/I` so their headers are included bare; PCH glue (`CvGameCoreDLL.h`, etc.) stays at root.
2. (IDE display only) regenerate the project with `python Tools/regen_project.py` (rebuilds
   `S2S.vcxproj` + `S2S.vcxproj.filters` from disk), or add the entries by hand. **⚠ CURRENTLY BROKEN:**
   the script was the one-time C2C→S2S rename migration — it reads the deleted `C2C (VS2019).vcxproj` as input and
   only re-paths EXISTING items (it never adds new files). The
   `S2S.vcxproj` listing is wholesale-stale. Since the files are DEAD for build purposes this blocks nothing — a
   working regen (read the S2S files as input, add-from-disk) is a parked standalone fix; don't piecemeal-patch
   entries into the stale listing.
3. With recursive globbing, an `LNK2001: unresolved external symbol` means a genuinely
   missing definition (not a missing `UnityInputPath` entry). `Sources/Engine/`, `Sources/AI/` etc.
   are reference examples of the flat bucket layout.

## Key Subsystem Knowledge

These are hard-won, non-obvious facts about the codebase. Treat as current state,
not findings to re-discover.

### Worker AI

- **Workers evaluate builds individually** — there is no per-player coordination
  layer that prevents re-scoring. Each `CvUnit` worker independently runs the worker
  scorers — `CvWorkerAI::improveBonus` / `improveCity` (bonus + city builds) and
  `CvUnitAI::AI_irrigateTerritory` / `AI_fortTerritory` — re-scoring the same
  plots/builds as other workers of the same owner. This is BTS-era behavior carried
  through C2C. Future TODO: a real `CvPlayer`-level coordination layer (potential
  homes: `CvPlayerAI`, contract-broker).
- **`CvWorkerAI` is the per-player BonusEval cache** (one per `CvPlayer`, turn-scoped),
  keyed on `(BonusEvalSource, plotIdx, unitType)`. **Bonus improvement is
  `CvWorkerAI::improveBonus`** (the consolidated path). Cross-worker dedup is
  `AI_plotTargetMissionAIs(plot, MISSIONAI_BUILD, group) < iMaxWorkers` — the canonical
  count-based dedup, which allows team builds. ⛔ Do NOT dedup via a per-plot
  single-claim scheme: one-unit-per-plot forces `iMaxWorkers=1` and breaks team builds,
  which is exactly why the dedup is the `AI_plotTargetMissionAIs` count.
- **Observability:** `CvWorkerAI::improveBonus` emits `[WAI/*]`-tagged lines into
  `BuildEvaluation.log`, gated by `gPlayerLogLevel` (1=headline, 2=per-plot, 3=per-candidate).
  The class doc comment in `Sources/CvWorkerAI.h` is the authoritative tag reference.

### AI valuation of ENABLEMENT

- **⛔ The AI weighs "this unlocks X" WAY too hard, and has for a long time — relaxing it is only ever an
  improvement.** The observed symptom is the shape to recognise: *the AI would happily beeline five techs deep for
  a single unlock.* Treat any enablement-derived value as a candidate for reduction, never for strengthening.
- **The mechanism was that enablement did not DECAY WITH DISTANCE.** `AI_techBuildingValue` receives `iPathLength`
  and read it in exactly ONE narrow guard, so the whole building-enablement value — and the flat
  `bEnablesWonder` bonus beside it — were added to a tech's value undecayed: a building unlocked five techs away
  scored identically to one unlocked next turn. Both now divide by the path length. ⚠ If you add a new
  enablement-derived term, it decays too; an undecayed one silently rebuilds the beeline.
- **"What does this tech enable?" is a FORWARD EDGE FETCH, never a database scan.** The tech's own compiled
  `enables.buildings` IS the answer ([patterns.md § THE WHAT-IF DRIVER](docs/architecture/patterns.md)). Asking it
  backwards — scanning every building and testing `isTechRequiredForBuilding` — is both the whole-database scan
  [enabler.md §6](docs/specs/enabler.md) exists to delete and the reason the question looked like it needed a
  what-if at all. It does not: tech drives MEMBERSHIP via `enables` and never the gate
  ([enabler.md §2](docs/specs/enabler.md)), so once the edge names the unlock, only the per-city gate remains.

### AI valuation of ROUTES — evaluate on move speed, never on yield

- **⛔ THE AI DOES NOT WEIGH A ROUTE'S YIELD CONTRIBUTION TO AN IMPROVEMENT — IT EVALUATES ROUTES ON MOVE SPEED.**
  That some improvements gain extra yield from a route in some cases is not a factor: a per-route ×
  per-improvement yield term must never be built into an AI improvement/plot valuation.
- **Two reasons, and why this does not get re-litigated.** (1) There is never a movespeed-vs-gold tradeoff — a
  route is laid for movement, so nothing is being weighed against the yield. (2) The yield only happens ABOVE A
  THRESHOLD, so it is incidental rather than a competing objective the AI could steer by. Modelling it buys no
  better decision at per-(route × improvement) cost on a hot loop.

### AI valuation of STRENGTH — a valuation that cannot tell strengths apart is FIXED, never filed

- **⛔ AN AI VALUATION DEFECT THAT UNDERVALUES STRENGTH IS SOLVED IN THE WORK ITEM THAT FINDS IT.** Left standing,
  the AI permanently undervalues strength, which is a chief source of low-cost unit spam.
  ⚑ This is the one place where the standing "the AI side is a consumer, out of active scope"
  framing ([north-star.md](docs/architecture/north-star.md)) does NOT license leaving it — an AI that cannot
  rank strength does not merely tune badly, it stops choosing on strength at all, which is the behaviour-break-
  wearing-a-balance-costume class ([fixed-point-and-scales.md §5](docs/specs/curators/fixed-point-and-scales.md)).
  ⇒ "That's AI weighting, so it comes after correctness" is NOT a reason to defer one
  (["deferred" is banned](#design)).
- **⛔ THE FAILURE SHAPE IS A CHOICE THAT DOES NOT VARY WITH STRENGTH — look for it at the RANKING, not in the
  formula.** `AI_unitValue` does weigh strength: it multiplies nearly every term by
  `CvGameAI::AI_combatValue`, which is linear in the unit's own strength. So a strength-blind AI decision is
  almost never a missing term — it is a call site that never lets the value decide. Two proven instances:
  - **`AI_unitValue` returns 0 for EVERY unit under a great-person UNITAI.** `UNITAI_GREAT_HUNTER` and its
    siblings (`PROPHET`/`ARTIST`/`SCIENTIST`/`GENERAL`/`GREAT_ADMIRAL`/`MERCHANT`/`ENGINEER`/`SPY`/
    `ATTACK_CITY_LEMMING`) fall through the validity switch leaving `bValid` false, so the function returns 0
    before scoring. Correct for production (they are not trainable roles) and **silently degenerate anywhere
    that RANKS with it** — every candidate ties at 0. ⇒ Where a role scores everything 0, rank the candidate as
    the unit it will actually BE (its `getDefaultUnitAI()`).
  - **`CvUnitAI::AI_upgrade` picked the upgrade with a die roll.** `AI_unitValue` was only a `>=` admission
    filter and the winner came from `getSorenRandNum(1000, "AI Upgrade")` — so strength contributed NOTHING to
    which unit a chain upgraded into. ⚑ The correct idiom is twenty lines below it in the same file:
    `AI_promote` ranks on `AI_promotionValue`.
- **⛔ THE DRAW STAYS — DEMOTE IT TO A TIE-BREAK, NEVER DELETE IT.** The number and order of draws on the
  synchronized stream is shared game state
  ([engine.md](docs/reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)),
  so a strength-blind `getSorenRandNum` pick is repaired by keeping the draw exactly where it is — one per
  admitted candidate, same order, same label — and letting it settle only an exact tie. ⚠ And keep the ADMISSION
  test byte-identical: a candidate that newly passes the filter is a new draw, so a fallback valuation must be
  gated on the degenerate case rather than applied to every candidate.
- **⚑ TWO THINGS VERIFIED, so they are not re-tread as suspects.** (1) **Every authored unit strength is a whole
  number** (1583 of 1583), so `AI_combatValue`'s `strength / 100` truncates nothing on real data — it is a latent
  [a CALCULATION never scales](docs/specs/curators/fixed-point-and-scales.md#2-the-unit-table-what-readjson-does)
  violation that belongs to the §4c-ter cluster pass, NOT today's balance defect. ⛔ Do not "fix" it by converting
  `getBestLandUnitCombat()` alone: two sites already lift it back with `100 *`, and the air sites read the genuinely
  human `getAirCombat()`, so a lone conversion manufactures fresh fudge factors and fails the cluster gate.
  ⚠ **It carries a SECOND defect on a different axis, and it belongs to the SAME pass:** `getBestLandUnit` SELECTS
  the winner on the live unit's resolved `baseCombatStrHuman()`, while `getBestLandUnitCombat` returns the
  **INFO's authored** strength (`getScalar(SCALAR_STRENGTH, …)`). Selection and denominator therefore read
  different quantities, so the number does not describe the unit that won the selection. ⛔ Do not repair that
  alone either — it is the SAME nine AI call sites (`AI_combatValue` among them), so a lone change re-scales
  every combat valuation in the tree.
  (2) **`CvCityAI::AI_bestUnit` has NO cost term** — it takes the highest `AI_unitValue` outright — so unit spam
  can never originate in a value-per-hammer ratio there; it originates in value failing to DISCRIMINATE, or in the
  needed-counts ([parked/unit-ai-valuation.md](docs/plans/parked/unit-ai-valuation.md)).

### AI unit COUNTS — a needed-count is a CAP, never a target to chase

- **⛔ THE NUMBERS ARE A MAXIMUM, NOT A MINIMUM.** A per-role count is a CEILING the AI may not exceed, never a
  quota it builds TOWARD. Read as a minimum, a count manufactures a permanent deficit: the AI is always short of
  something, so there is always a reason to train one more, and production never returns to anything else.
- **⚑ THE DEFICIT IS SELF-REFILLING, which is what turns it from a bias into a loop.** Anything that lowers the
  COUNT while leaving CAPACITY intact re-opens the gap and triggers a rebuild. **Merging is the worked case:**
  3 units → 1 stronger unit drops the owned count by two while the hunting/fighting capacity is unchanged, so the
  target reopens and the city rebuilds toward it — merge → deficit → rebuild, pumping hammers into units whose
  capacity never fell ([parked/unit-ai-valuation.md](docs/plans/parked/unit-ai-valuation.md) §A5).
  ⇒ The same loop explains an AI that will not merge: under a count target, merging is self-harm.
- **⛔ SO THE UPKEEP SYMPTOM IS DOWNSTREAM, and fixing upkeep does not fix it.** Unit upkeep sums the player's
  UNITS ([economy.md](docs/reference/economy.md)), so a count that only ever climbs turns into an upkeep bill that
  only ever climbs, and the AI stagnates paying for units it did not need. Treating that as an upkeep-tuning
  problem tunes the symptom while the count keeps producing it.
- ⚠ **`CvCityAI::AI_bestUnit` has NO cost term** (§ AI valuation of STRENGTH), so nothing downstream of the count
  restrains it either: whatever the count asks for, the city builds the highest-valued answer regardless of price.

**⛔ A BUILDING BEATS A PROCESS IN VIRTUALLY EVERY SCENARIO, AND MOST OF ALL EARLY — so the process being LAST is
CORRECT, and promoting it is the wrong fix.** A process converts production into one commerce stream; a building
changes what the city IS, permanently. ⚠ Building passes are interleaved the whole length of
`AI_chooseProduction`, right down to the final tiers, so there is no insertion point that lifts the process above
a marginal unit WITHOUT also lifting it above real building work.
⇒ **Therefore a city that trains a marginal unit instead of running research is fixed on the UNIT side, never by
re-ranking the process.** Cap the count, floor it, put the top-up on the spending budget — make the unit stop
asking. ⛔ The tempting move this refuses: "the AI builds wanderers when research would be better, so raise the
process." That starves buildings to fix units.

**⛔ THE PROCESS IS THE LAST DECISION IN `AI_chooseProduction`, SO ANY UNIT DEMAND PREEMPTS IT.**
`AI_chooseProcess(NO_COMMERCE, …)` sits three lines from the end of a ~3000-line function; the only earlier call
is gated on `bFinancialTrouble` AND being the capital. Buildings are offered many times BEFORE the marginal unit
tiers, so by the time those run, "nothing worth building" is already established — and the process then loses
only to a marginal unit top-up. ⚑ **That is why a city with an empty building frontier still trains bodies
instead of running research**: not because research scored worse, but because it was never reached.
⇒ A role whose count is a TARGET therefore starves the process permanently — the deficit never closes, so the
`return` fires every turn. Making the count a floor is what lets production fall through at all.
⚠ **A `[CIT/order] … MAINTAIN_PROCESS` line does NOT mean a process is running.** The push is REFUSED when the
queue already holds a real order, so the diagnostic carries `landed=` for exactly that reason; read the flag, not
the line. Reporting the ask instead of the outcome is what made a city training units look like a city
researching.

**⛔ A DEFICIT PERCENTAGE IS THE TARGET SHAPE — THE TEST IS `owned < floor`, AND NOTHING ELSE.** Expressing the
need as `(needed - owned) * 100 / needed` and building whenever it exceeds a threshold reads ANY shortfall as
demand, so the number can never be reached and production never moves on. ⚑ Its tell is a GRADUATED LADDER: a
percent compared against 80, then 50, then 25, then 0, each with its own build. That ladder is the count
re-asserting itself as a target at four priorities.
⛔ **Two things hide inside such a ladder, and both are their own targets:** a HARDCODED absolute ceiling beside
the percent (`owned < 8`, `owned < 16` — a second count the real one never governs), and a per-turn RNG that
"pumps" the role whenever any deficit exists — the compounding-dice shape § Unit AI fallback terminals already
bans, here deciding production rather than movement.
⇒ **Collapse the ladder to one boolean.** `owned < needed`, computed once and named for what it means; the only
legitimate second tier is the genuine ZERO case (`owned < 1`), which is an emergency rather than a floor.
⚠ **Removing a `getSorenRandNum` changes the synchronized stream, and that is acceptable when the DLL change is
uniform** — every client and every future turn draws the same way, so this is a behaviour change, never a desync.
⛔ It is still never removed to make a number look better ([the draws are shared state](docs/reference/engine.md#-the-synchronized-rng-is-shared-save-state--do-not-touch-the-draws)).

**⛔ WHAT CREATES DEMAND IS THE VALUATION, NOT THE COUNT.** A unit gets built because it is WORTH building; the
count only ever says when to stop. ⇒ Removing a deficit trigger is not removing the demand signal — the valuation
was always the demand signal, and the deficit was competing with it.
**⛔ A FLOOR MAY ONLY FORCE A UNIT THAT CAN SURVIVE TO DO THE JOB.** A floor FORCES production, so filling it with
a unit that dies on contact converts hammers into nothing — and it does so worst exactly when the AI is already
far behind and can least afford it, digging the hole deeper every turn. ⛔ **MEASURE AGAINST THE TYPICAL THREAT, NEVER THE WEAKEST ONE — sampling the weak end is how the bar ends up
meaningless.** Over the 582 authored ANIMAL-class units the median strength is **7** and 69% sit at 4 or above,
while an early scout is **1**. ⚑ So the floor was mandating a unit seven times smaller than what it would
typically meet — and since much of the danger is CONCEALED ([vision.md](docs/specs/vision.md): hide and seek is
a graduated `concealment` vs `detection` contest, not a visibility flag), the scout does not even get to choose
the fight. ⚠ Consequence, and it is intended: in the ancient era nothing available clears the bar, so the early
explorer floor does not fire at all. ⇒ `CvCityAI::AI_roleFloorIsFillable` asks the question before the floor fires.
⛔ **AND IT ASKS IN RAW STRENGTH, NEVER IN `AI_combatValue` — that one is RELATIVE, not absolute.**
`CvGameAI::AI_combatValue` divides by `getBestLandUnitCombat()`, the best unit CURRENTLY AVAILABLE, so it is a
0-100 score against the field rather than a magnitude. Early on, when the best thing available IS the scout, a
scout scores ~100 — so testing it against an absolute animal strength passes exactly when it must fail. ⚑ That
is the surviving-fudge-factor shape (§ Conduct, THE THREE DRIFT DETECTORS: two operands on different scales), and
it made the gate a no-op while reading as a working check. A floor that cannot be
filled survivably does not fire, and production falls through.
⚠ **The COUNT is the EFFECTIVE one (`AI_totalEffAreaUnitAIs`), never the body count**, so a merged unit counts as
its strength equivalent and consolidating never re-opens the floor.

⚖ **ONLY SCOUTS AND HUNTERS ARE MINIMUM-NUMBER ROLES, and the reason is specific: the AI does not fully
understand the need to defend against HUNTING.** It cannot see that threat in its own valuation, so for those two
the floor stands in for a judgement it cannot make. ⛔ That is the whole carve-out — every OTHER role's count is a
CAP only, and "this role also feels like it needs a floor" is the reasoning this rule refuses.
⚖ **Even there the floor is SMALL — on the order of 3, never 10-15.** A floor exists so a role cannot reach zero
and strand the AI, not so a city has something to produce. ⚠ Today's numbers are the wrong magnitude for that
job: workers `1 + √cities × 2` (~11 at 25 cities), explorers `max(5, 3 + cities/3)`, hunters capped near 14.

**⚑ CITY DEFENSE ALREADY CREATES DEMAND CORRECTLY, AND IT IS THE SHAPE TO COPY — it asks for STRENGTH, not for
BODIES.** `AI_isDefended` gates on `getGarrisonStrength() >= AI_neededDefenseStrength() × pct/100`, so the
requirement is CAPACITY and any mix of units that meets it satisfies it.
⛔ **That is what makes it merge-SAFE, and it is a structural property rather than a tuning one:** consolidating
three units into a stronger one leaves capacity intact, so a strength requirement stays satisfied and nothing
rebuilds — while the identical merge under a COUNT requirement re-opens the deficit and triggers a rebuild. The
garrison code already reasons in these terms (`GARRISON_CONSOLIDATE_KEEP_PERCENT`, sized because *a 3→1 merge
keeps only half the triple's aggregate strength*).
⇒ **A role whose need is expressed as a count is the thing to convert** — to capacity where the role has a
measurable one, and otherwise to a small floor plus the valuation.

**⛔ THE VALUATIONS ARE REDESIGNED IN ONE GO, AND THE REASON IS COMMENSURABILITY — `AI_unitValue`,
`AI_buildingValue` and every other `AI_*Value`.** A production choice does not read one valuation, it COMPARES
them: a unit's number against a building's against a process's. So the numbers only mean anything if they sit in
the SAME BALLPARK, and a scale is a property of the whole family, never of one member.
⛔ **THE FAILURE SHAPE IS AN EVALUATOR THAT ALWAYS WINS.** Whichever one emits the biggest numbers takes every
comparison it enters — and once it is an ORDER OF MAGNITUDE above its siblings it wins unconditionally, on merit
it does not have. The AI then looks like it has a preference (it always builds units, it never builds X) when
what it actually has is one evaluator on a different scale. ⚑ So read a lopsided AI preference as a SCALE
question first, and go compare the magnitudes the evaluators emit before touching any of their terms.
⇒ **Therefore a term added to ONE valuation is worse than the gap it closes.** It shifts that member's scale
relative to its siblings, so every comparison the AI makes against it silently tilts — the member gets better and
the DECISION gets worse. ⚑ The move this refuses is concrete and will occur to you the moment you read the next
paragraph: *"upkeep is missing from `AI_unitValue`, so add an upkeep term."* That is a compensating constant on a
formula nobody has redesigned (§ Conduct, THE THREE DRIFT DETECTORS: a surviving fudge factor means two operands
are on different scales) — and here it manufactures exactly that mismatch rather than revealing one.
⇒ **The near-term lever is the SPENDING GATE, not the valuation.** Restrain what gets BUILT; the question of what
things are WORTH is one work item covering every valuation at once, and it does not start piecemeal.

**⛔ THE AI IS UPKEEP-BLIND WHEN IT BUILDS AND UPKEEP-AWARE ONLY WHEN IT SCRAPS, and MOST UNITS COST UPKEEP NOW.**
`AI_unitValue` contains **no upkeep term at all** — the sole value-per-upkeep ratio in the tree
(`iValue /= getUnitInfo().getUpkeepCost() + 1`) sits inside the DISBAND selection, choosing what to throw away.
So the cost of keeping a unit reaches the AI only once it is already in financial trouble. ⚑ That ordering is what
guarantees overshoot: nothing restrains the build, and the correction is a panic scrap.
⚖ **The production budget it DOES have is `iUnitCostPercentage < iMaxUnitSpending`** — unit upkeep as a share of
pre-inflated costs, against `AI_evaluateMaxUnitSpending()`.
⛔ **It gates the MILITARY builds and nothing else.** Workers were exempt from it entirely, which is why they are
the expensive half: a count deficit spent freely past a budget every military build respects. A civilian build
beyond its floor answers to the same budget — and ⚠ the genuine zero-of-a-role case stays OUTSIDE it, or a city
with no worker at all can never build its first one.

### The CONTRACT BROKER — matching is THREE STAGES, and distance never scores

- **⚖ THE BID DIVIDES BY BUILD TIME, AND IN PRACTICE THAT DIVIDES BY ONE — `iValue *= hammerCostPercent();
  iValue /= iTurns;` (`CvContractBroker.cpp`).** ⚑ **Measured live: EVERY bid recorded `turns=1`** (15 of 15),
  because most units are one-turn builds. ⇒ So it is not a smooth depreciation that lets cheap units win — it is
  a CLIFF that does nothing for the whole 1-turn field and penalises only the rare MULTI-turn candidate, i.e. it
  biases against the expensive units rather than for the cheap ones.
  ⛔ **Which means production time barely discriminates at all, and the broker is therefore choosing almost purely
  on `AI_unitValue`** — so a bad pick here is a VALUATION question, not a broker one, and belongs to the designed
  pass (§ AI unit COUNTS: the valuations are redesigned in one go).
  ⚠ **The absolute magnitudes are a SPEED SCALAR, not a valuation blow-up** — `hammerCostPercent()` is the
  gamespeed percent, so a bid of `value=9800100` is mostly that multiplier. Do not read a large bid as evidence
  of anything on its own; compare bids against each other, on one speed.
- **⛔ ONE UNIT DEMAND PER CITY PER TURN — `AI_chooseUnit` has TWO production paths and the counter admitted
  two passes.** It posts a broker TENDER and otherwise falls through to a direct `AI_chooseUnitImmediate`, and
  after tendering it deliberately returns false so the city keeps looking for a BUILDING of its own. ⚠ What that
  must not do is let the cascade post a SECOND unit demand in the same pass — the broker then trains both for
  one need. `MAX_REQUESTEDUNIT_PER_CITY` is 1 while the guard tested `>`, admitting counts 0 AND 1, and most
  call sites never checked the counter at all. ⇒ Gate it centrally, in `AI_chooseUnit`, never per call site.
  ⚑ `[CIT/order] action=tenderUnit` records the demand: before it, the order log carried construct and
  maintainProcess only, so unit production was invisible in the one place a reader looks for what a city decided.
- **⚖ A WORK REQUEST NAMES EITHER A UNIT TO JOIN OR A PLACE TO GO, AND THE PLACE IS THE COMMON CASE — so a
  NULL `pJoinUnit` is the CONTRACT, never a failure.** `advertiseWork` stores `iUnitId = -1` whenever it is
  handed no join unit, and `findUnit(-1)` answers NULL by an explicit early return; the two `CvCityAI` requests
  (the floating-defender ask and the city's own unit demand) are exactly that shape, and they are the highest-
  volume requests in the game. Every consumer is written for it — `finalizeTenderContracts` falls back to the
  recorded plot, `processContracts` branches on NULL at each step and picks `MISSIONAI_CONTRACT` over
  `MISSIONAI_CONTRACT_UNIT`.
  ⛔ **So do NOT read a NULL join unit as a dead-unit symptom, and never assert against it** — an
  `FAssert(NULL != pJoinUnit)` fires on the ordinary majority path and buries the one case that IS a defect.
  ⚖ **THAT case is a request whose `iUnitId` is NOT -1 and no longer resolves:** it named a unit and lost it, so
  the rendezvous is stale and the contract is RELEASED (`[CTB/contract/lost] action=joinGone`), returning the
  unit to the market rather than walking it to an empty plot. ⚑ The discriminator is `iUnitId != -1`, never
  the NULLness of the resolved pointer — those are two different questions and only the first one separates them.
- **⛔ DISTANCE IS A GATE AND A TIE-BREAK, NEVER A TERM IN THE SCORE.** Path distance does not belong in scoring
  at all: over-relying on travel speed is how the AI ends up spamming fast cheap units. A bid that depreciates by
  its own haul rewards a unit for being FAST on top of already rewarding it for being CHEAP — cheap-and-fast wins
  twice — and that is the mechanism behind over-reliance on the broker for deciding what units to make.
  ⚑ The rule in one line:
  **speed gets a unit CONSIDERED (it is admitted from further out); proximity gets it CHOSEN.**
- **The shape is three stages, in order: what is IN RANGE → SCORE what is in range → take the CLOSEST
  of equals**, with ONE real path run on the winner to confirm it. ⚑ **The cost win is stage 4, not stage 1:**
  the search runs ONCE for the whole request instead of once per candidate. ⛔ A tie-break must be
  SPEED-INDEPENDENT or it smuggles the bias straight back in — use step distance, never `CvPath::length()`,
  which returns TURNS and is therefore shorter for a faster unit.
- **⛔ A STEP-DISTANCE RANGE GATE IS BOUNDED BY MOVEMENT POINTS, NEVER BY MOVES — AND TIGHTENING IT IS A BUG.**
  A unit spends `baseMoves × MOVE_DENOMINATOR` points per turn and the CHEAPEST tile costs **one** point (a
  railroad, or anything under `ignoreTerrainCost`), so points-per-turn is the only provable ceiling on
  tiles-per-turn. ⚠ `budget × baseMoves` reads like the obvious bound and is wrong by two orders of magnitude:
  it rejects a worker six tiles away along a road — a unit that arrives well inside the budget. A gate may only
  ever OVER-admit ([enabler.md §5](docs/specs/enabler.md): over-inclusion is safe, a MISS is the bug), so being
  provably safe makes this one coarse enough to exclude only the absurd. That is the correct trade; do not
  "improve" it by narrowing.
- **⚖ THE BUDGET IS PRODUCTION + TRAVEL = `AI_CONTRACT_MAX_TRAVEL_TURNS` turns TOTAL.** More than ~5 turns to
  reach the job, counted from the START of unit production, is too far. ⚠ The number is a judgement call rather
  than a derived one, and it is a BUG-settable define — tune it, do not treat it as load-bearing. So the two sides
  spend it differently and must not be given the same allowance: an ADVERTISING UNIT already exists, so its
  whole budget is travel; a TENDERING CITY spends what its build time leaves over (`max(1, budget − buildTurns)`).
- **⛔ NEVER LET EITHER PATH PROBE GO UNBOUNDED.** An A* that fails on an UNREACHABLE target explores the entire
  reachable component before returning false — that is the turn-path spin, and it is why both probes are capped
  by the budget. ⚠ The historic cap read `min(priority > LOW ? MAX_INT : 10, bestValue < 1 ? MAX_INT : …)`,
  which degenerates to `min(MAX_INT, MAX_INT)` on the first candidate and on any priority above the escort
  floor — i.e. it looked like a cap and bounded nothing.

### City garrison tiers

- **City defense runs on two ledgers — do not conflate them (#384).** Garrison
  *membership* (`CvUnitAI::AI_setAsGarrison`, self-expiring) is the **auxiliary** tier:
  any combat unit parked in a city counts toward actual defense strength
  (`getGarrisonStrength`/`AI_isDefended`) while **keeping its own UNITAI**. The
  `UNITAI_CITY_DEFENSE` role is the **primary** tier feeding the count-based demand gates
  (production choice, min-defender searches, floating-defender totals). Garrisoning must
  NEVER retype a unit (the historic unconditional retype in `AI_guardCity` corrupted the
  demand picture — overdefended > underdefended, join eagerly/release reluctantly via
  `GARRISON_RELEASE_MARGIN_PERCENT`). Details: `Sources/AI/CvUnitAI.cpp` (garrison-tiers).

### Unit AI fallback terminals

- **Audit pattern: idle-unit fallback terminals must not park units wherever they happen
  to stand.** Three owner-observed "lost contact with the mothership" cases share it:
  hunters advertising for escorts while idling in rival borders (#392), property-control
  units stranded mid-route by a per-re-plan "don't move" dice (#396), human workers
  waiting forever on `MISSIONAI_WAIT_FOR_ESCORT` (escorts are never dispatched to them).
  Rules when writing/touching a fallback terminal: park in OWN territory (if standing on
  another team's land with nothing to do, `AI_retreatToCity` first); park persistently
  (FORTIFY → SLEEP → SKIP, the #342 idiom), not via one-turn `MISSION_SKIP`; and never
  gate per-re-plan progress on an RNG — re-rolled dice compound into a random walk.
  `MOVE_NO_ENEMY_TERRITORY` does NOT keep units out of rival land (it only excludes
  at-war territory), so it is not a substitute for any of the above.
- **Generalization — pseudo-progress terminals (#410): a free/cheap action that cannot
  change the strategic state must not satisfy the decision loop.** A cascade step that
  "does something" (a ranged potshot, a no-op heal, an advertise-that-returns-true)
  terminates the unit's decision looking like progress, so the real commit-or-withdraw
  decision is never reached — AI stacks camped cities for eras feeding near-zero-damage
  ranged strikes as the "yay we did something at least" clause. Standoff/maintenance
  actions are phase steps inside a plan with an abort rule, never turn-satisfying
  terminals in their own right.

### Graphics / map generation

- Any plot-graphics mutation must be guarded by **`GC.IsGraphicsInitialized()`**.
  During a NEW game, world generation (`addGameElements` → rivers/features/bonuses)
  runs *before* the render engine landscape exists, so symbol updaters that fire
  against non-existent engine plots crash. Loading a save skips generation, so it's
  safe — hence the classic "crashes with graphics paging off, fine with it on"
  signature points to a graphics path running pre-init, not a logic bug. Established
  guard sites: `CvPlot.cpp` `setPlotType` graphics block, `setLayoutDirty`,
  `shouldHaveGraphics`; `CvMap::setupGraphical`.
- **⛔ THE UNIT RENDERING PIPELINE IS DOCUMENTED IN FULL, CITED, IN
  [`docs/reference/unit-rendering.md`](docs/reference/unit-rendering.md) — READ IT BEFORE TOUCHING ANY GRAPHICS
  OR ENTITY PATH.** The entity lifecycle (create · setup · place · destroy), how a plot chooses the one unit it
  draws (`m_pCenterUnit`), graphics paging ON vs OFF, and the load / new-game timelines all live there against
  `Sources/` line cites. What stays here is only the HARD RULES; the knowledge is the reference's.
- **⛔ NEVER DESTROY A REAL SCENE NODE OUT FROM UNDER A SELECTED UNIT.** The engine's move queue lives ON the
  scene node, so a needless destroy between a `QueueMove` and its `ExecuteMove` strands the walk animation.
  `reloadEntity` therefore KEEPS an entity already of the wanted kind; only a genuine MODEL change (a warlord
  attaching) rebuilds one, through `rebuildEntityArt()`.
- **⛔ EVERY EXE ENTITY CALL GUARDS ON `isRealEntity(getEntity())`, NEVER ON `!isUsingDummyEntities()`.** The
  latter answers FALSE for a NULL entity, so it hands NULL to an EXE that does not null-check; `isRealEntity` is
  the only test that excludes both the shared dummy and NULL (`CvDLLEntity` — every member that passes an entity
  over).
- **⛔ GRAPHICS PAGING OFF IS A LIVE LOOP — "PAGE EVERYTHING IN, PAGE NOTHING OUT" — NEVER A ONE-SHOT.**
  `CvPlotPaging::UpdatePaging` runs every `CvGame::update`; ON it requires plots by camera distance, OFF it
  requires ALL on every plot every frame (a NONE no-op once a plot is fully shown). The inherited code ran a
  one-shot disable sweep that latched dead, so a plot that could not render at that instant — the whole map, if
  the sweep landed pre-landscape — stayed blank forever and unit nodes were built later against plots whose
  `updateGraphics` never ran. ⚑ Its partner: `CvPlot::showRequiredGraphics` must not mark a bit visible before
  `IsGraphicsInitialized()`, or a pre-init pass poisons the visible mask (`required ^ visible` = NONE forever)
  and nothing retries. This asymmetry is what made *"works with paging on, broken with it off"* the signature.

### ⛔ THE NO-GUESSING RULE — map everything, always

**We do NOT guess. We MAP. Every claim about a value/divergence is grounded in the total-observability surface —
that is exactly what the Orwellian level of surveillance is FOR.** When a cascade value diverges from legacy: do NOT
hypothesise a cause and try a fix — EMIT the full legacy decomposition (every component/source of that calc) via the
endpoints, and map the cascade's value by the SAME components, so the divergence is attributed to a NAMED source with
numbers, not a guess. If the data to attribute it isn't being emitted, the FIRST step is to emit it (extend the
surface), not to guess. The half-guessing back-and-forth (try a fix, re-sweep, try another) is the anti-pattern this
rule kills. (The modifier-channel application of the no-guessing rule, Conventions § Conduct, +
the total-observability bar below.)

### Cascade observability — the total-observability ("Orwell") bar

- **⛔ Which logs are live-readable is decided by the SINK, not by the file.** Two different mechanisms write into
  `Documents/My Games/Beyond The Sword/Logs/`, and conflating them wastes hours in BOTH directions:
  - **SPINE-written domain logs are READABLE WHILE THE GAME RUNS** — a domain registered via
    `spineRegisterDomain` renders on the game thread and enqueues to the off-thread `CvLogWriter`, which owns the
    disk I/O and flushes per batch (`Infrastructure/CvLogWriter.{h,cpp}`, [spine.md](docs/spine.md)).
    `Cascade.log` (incl. every `[SPINE]`/`[GRANTS]`/`[CASCADE]` line) and the other registered domains are this
    kind: **just read the file.**
  - **⛔ LEGACY `gDLL->logMsg` sinks (the not-yet-migrated domains) ARE held open** by the process, so tailing
    those mid-session gives stale/empty/partial results. Never infer "logging is off" from a quiet legacy log.
  - **⚑ For post-turn analysis the SPINE LOG BEATS `/events`, and not marginally.** An SSE attach can only ever
    capture from the moment it connects, so it **structurally misses the entire LOAD RESEED** (~5.9k
    `[GRANTS/building]` lines, the whole in-read emit stream) — the exact data needed to verify load-time
    suppression and any full-population tripwire. The log has it all, needs no connect-before-the-turn timing
    dance, and cannot be lost to a dropped stream. Reach for `/events` when you need to watch something LIVE;
    reach for the log when you want to know what happened.
  - **⚠ The `/events` SSE endpoint has a bounded number of stream slots.** A capture loop left running (or one
    that respawns `curl` in a `while` loop) holds them; once exhausted the endpoint returns
    `{"error":"too many event streams"}` and your capture silently records NOTHING. Verify the first frames are
    `event: hello` and not that error, and kill every loop when done — an empty capture reads exactly like "the
    feature did not fire."
  - **(3) The on-demand mailbox-snapshot endpoints** compute a game-thread snapshot via the single-slot mailbox and
    depend on no log file or gate — the most reliable read for a POINT-IN-TIME value. ⚠ **The route table was purged
    and rebuilt sparse: the only data routes are the stored-side decomposition censuses, and the ROUTE TABLE in
    `CvHttpServer.cpp::handleRequest` is their census** (an enumerated list here drifted twice; `GET /computed`
    serves the live index). ⛔ There is **no `oracle`
    twin and none comes back** (`superseded-ideas` #33: an endpoint cannot replay the event chain, so its
    recompute answers a number that was never comparable — do not run one as evidence, and do not rebuild it).
    Everything else 404s, and the route surface is defined with the access surface
    (see `docs/specs/http-endpoints.md`). Do not send an agent to poll a route that does not exist — and ⛔ do NOT
    "fix" that by adding one: an endpoint is a LIVE CONSUMER, so a route reading a legacy member keeps that member
    alive past the compiler census. Restoring a route to read a legacy value is the banned move; EMIT a spine event.
  Gates are separate and INDEPENDENT: `gPlayerLogLevel` gates the per-domain `.log` files; the `/events` stream is
  its OWN spine consumer — spine DOMAIN facts stream unconditionally, DIAGNOSTIC/TRACE at `gStreamLogLevel` — so a
  line can be in either surface without the other.
- **The events + logging + diagnostics must make the running game FULLY surveilled.** The bar: *map an accurate game
  state purely from the endpoints + `/events` + the gated logs — open the game, but never look at the SCREEN.* This
  is **non-negotiable and load-bearing**, not polish: it is the ONLY way to reliably verify the state logic on the
  cascade + tally (map-before-delete). The shadow-until-clean discipline governed the migration's cut phase; **the
  shadow phase has ended** (`docs/specs/validation.md`) — the observability bar itself stands. Live surface:
  `docs/specs/http-endpoints.md` + `docs/spine.md`.
- **Delegate DATA-READING to the cheap `data-reader` sub-agent — never pull raw endpoint/log dumps into an expensive
  context.** Reading the live surface at scale (a sweep dump is tens of KB; logs are larger) *"will nuke credits"* if
  the orchestrator ingests the raw bytes. Use the read-only **`.claude/agents/data-reader.md`** (Haiku;
  Bash/Read/Grep only) to curl/grep, parse, AGGREGATE, and report back a COMPACT distilled summary (histograms,
  divergence cause-tags, anomalies). **The reader must fail HONESTLY** (distinguish "surface DOWN" from
  "reader-error", never fabricate a clean summary); when it reports DOWN or returns junk, confirm with ONE cheap
  smoke-curl (`curl -s http://127.0.0.1:7227/` → `hello world`) before acting.

## Conventions

> ⛔ **"Conventions" here means HARD RULES — binding by default, NOT norms to weigh.** Every rule below is law
> unless the **owner explicitly relaxes it** — never on an agent's own judgement. Each was paid for in wasted
> hours. A cross-cutting ruling is stated ONCE, in the doc it belongs to, and every other doc that needs it
> LINKS there rather than restating it — treat any pull toward *"this is just guidance / probably fine / I'll
> infer it"* as the kraken's bait.

### Conduct

- **`pwsh` == good, `powershell` == bad.** `pwsh` (PowerShell 7) is the standard shell; `powershell.exe`
  (Windows PowerShell 5.1) is never invoked and never nested inside a `pwsh` session — 5.1 mangles UTF-8 on stdout
  and lacks the modern operators. This is `pwsh`-over-5.1, NOT PowerShell-over-bash: Git Bash / the Bash tool is
  equally fine. (The build wrapper is still documented above as `powershell.exe -File ../Tools/_Build.ps1`;
  migrating that invocation to `pwsh` is a verify-then-change follow-up.)
- **Narrate your work verbosely.** Before each search/read/build step, state the question the step answers, what you
  expect, and what the result actually told you. The owner follows along in real time; terse status lines hide the
  reasoning.
- **⛔ EDIT WITH THE EDIT TOOL — A CHANGE BURIED IN A SCRIPT IS UNREVIEWABLE.** Hiding edits inside python
  scripts makes it fantastically hard to see what is being done. An `Edit` shows the owner the
  exact before/after as it happens; a `python - <<'PY'` heredoc shows them `removed 159 lines` and nothing else, so
  the change lands unseen and the review surface is gone.
  ⚑ **This is the ANTI-CONCEALMENT rule again, one level up** (`Sources/AGENTS.md`: an abbreviated identifier is a
  review-blocker because agents hide poor implementation behind abbreviated variables, and it is not caught on
  sight). A scripted edit hides the whole change rather than one name — same failure, larger blast radius, and it
  is reached for exactly when an agent finds the Edit tool inconvenient. Convenience for the writer is not a reason
  to cost the reviewer their only view.
  ⚖ **A script IS the right tool for a genuinely MECHANICAL bulk pass** — the same transform over dozens of sites,
  where the rule is the reviewable artifact and the diff is too large to read anyway. ⛔ Two obligations when you do:
  state the RULE the script applies before running it, and **show `git diff` on the touched files immediately
  afterwards** so the result is on screen. A scripted edit whose diff is never displayed is not narrated, whatever
  the prose around it said.
  ⛔ **"CRLF" IS NOT THE REASON, AND IT IS THE EXCUSE THIS BULLET EXISTS TO KILL: *"git checks out CRLF and
  commits LF"* — `core.autocrlf=true` plus `.gitattributes` `* text=auto`, which is deliberate and works.**
  ⚑ MEASURED after a session of heavy scripted editing: `CvPlot.cpp` (script-edited repeatedly) held **13,243 CRLF
  and 0 bare LF**, and `AGENTS.md` (Edit-tool only) **887 CRLF and 0 bare LF** — *both* tools preserve line endings
  exactly, which is why those diffs came out pure deletions with no whole-file churn. ⇒ There is nothing to
  configure and never was. **An `Edit` that fails to match is a CHARACTER mismatch in your `old_string`** (an `…`,
  an em dash, a mis-transcribed run) — the tool says so, and the fix is to re-read the file and copy the text
  exactly. Reaching for a script instead trades the owner's review surface for a problem that does not exist.
- **⛔ EVERYTHING HAS ALREADY BEEN FIXED, SPECCED AND SOLVED — IF YOU THINK SOMETHING IS MISSING, YOU ARE MOST
  PROBABLY WRONG.** This
  is the rule of thumb to hold above your own judgement: a gap you perceive is a READING failure first, and a
  genuine hole almost never. ⇒ **The move is to go FIND the thing that already does this** — grep the specs, the
  sibling curator, the calc surface, the call site itself — never to build a parallel mechanism beside it.
  ⚑ **The tell is the moment you catch yourself DESIGNING**, and it is worth naming because it does not feel like
  rollerskating; it feels like diligence. A new table, a new getter, a new read, a new slot "because the existing
  one does not quite fit" is the shape — and "does not quite fit" nearly always means the right one was not found.
  ⚠ **MEASURED, in one task:** three inventions, each with the answer already in front of it — a composed getter
  when the group read existed; a new curator table when `SCALAR_COND` + `_put_cond` already mapped a scalar tag
  onto a conditioned deposit; and a proposed eval-ctx slot plus a new valuation read when the candidate's tag test
  (`GC.getUnitInfo(eUnit).hasTag(...)`) was already on the very line being edited. Every one compiled, and every
  one was a second way of doing something the tree already did.
  ⛔ It is the constructive half of the no-guessing rule below: that rule
  says never fill a gap with inference; this one says **the gap is probably not there**.
- **Trust but verify — EVERY claim, including the owner's.** A doc line, an owner aside, your own recollection, a
  memory entry — all are hypotheses to CONFIRM against ground truth (the live code, the actual data, the running
  game) before you build on them. Say what you verified against.
- **⛔ DO NOT GUESS, DO NOT INFER, DO NOT ASSUME — an assumption IS a shortcut.** Never fill a gap with inference; do not
  assume an earlier verification still holds; and never infer an ANSWER or PERMISSION the owner has not given —
  "keep going" authorizes work, never a vote on an open decision. **A question you have posed to the owner is a HARD
  STOP: do not act past it until they answer.** At a gap the only moves are VERIFY or ASK. Every minion you spawn
  must be told this rule explicitly.
- **⛔ THE THREE DRIFT DETECTORS — mechanical checks, not vigilance. Each caught a real defect; each was
  BELIEVED CONFORMING at the time.**
  1. **A change that leaves every consumer untouched is the TELL, not the win.** "No blast radius" means the
     ENGINE bent to fit the old shape instead of the consumers being rewired — the half-migration reflex
     ([the ×100 fixed-point model](docs/specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries),
     [the Cy* surface is not a fixed contract](docs/architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library)). Blast radius is the SIGNAL that the cut
     reached. *(Caught: a cascade accessor reducing ÷100 internally so nine readers — incl. a `Cy*` binding —
     would not have to change.)*
  2. **A surviving FUDGE FACTOR means two operands are on different scales**, i.e. the conversion landed in the
     wrong place. When a cluster converts correctly the magic constants DISAPPEAR and the mixing sites need no
     edit at all. If you find yourself ADDING a compensating multiplier, stop and redraw the cluster boundary —
     do not push through. *(Caught: an AI ratio needing `×10000` because one operand was ×100 and the other human.)*
     ⚑ **AND IT NAMES THE CULPRIT, not just the symptom: a fudge factor is USUALLY LEGACY BEING FORCED
     INTO THE NEW SURFACE AT AN AI CALL SITE.** The constant is what an unmigrated consumer needs in order to keep
     reading a new-surface value in its old shape — so it marks the CALL SITE as the thing still on legacy, not
     the value. ⛔ **Therefore the question is never "where does the conversion belong?" but "WHICH SIDE OF THIS IS
     STILL LEGACY?"** — re-point that side and the constant deletes itself. Adding the multiplier instead is how
     the AI half gets left rotting while the surface underneath it moves
     ([build a new getter surface, never widen a legacy one](docs/architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers): reusing a legacy getter IS
     the mechanism that produces the half-migrated state).
     *(Worked: a `CvCityAI` yield valuation carried BOTH a `100 *` on one operand and a `/100` on the total, to
     hold a legacy per-building sum beside a cascade value. Re-pointing that one call site onto the what-if driver
     deleted both constants and dissolved an open question about a third term's scale — nobody had to decide it,
     because only the hand-rolled sum ever needed to know.)*
  3. **For CACHE/PACKAGE work the acceptance test is CACHED-vs-FRESH, not "it builds and the value looks sane."**
     Pit the stored slot against its own fresh recompute and require agreement. *(Caught: `/computed`'s
     maintenance decomposition under-reporting by 39 against the served value for want of one duplicated term —
     with a green compiler, a plausible total, and a load-time endpoint poll that proved nothing.)*
  ⚠ **The COMPILER is the census ONLY for a deleted MEMBER.** A changed VALUE or SCALE compiles silently on the
  same type — those sweeps are driven by the mapped site list and surface only at RUNTIME.
- **⛔ A DOC THAT DESCRIBES A HALF-STATE READS LIKE A DESIGN — treat "pilot", "X follows per channel", "promoted
  when needed" as GAPS, not as sanctioned shapes.** An agent conforming to such a line does the wrong thing while
  believing it is conforming, which is how drift survives review. If a spec sentence licenses stopping partway,
  it is the sentence that is wrong: fix it in the same change (Conventions § Docs, below).
- **"ALL" means EXHAUSTIVE — locust mode, never judgment-filtered.** Enumerate EVERY item mechanically,
  recursing every aggregate to its leaf sources; a single agent's "do I need this?" is systematically biased toward
  dropping items. Go exhaustive **immediately** (partial passes are slower), and prove completeness
  **adversarially** (a second pass that ASSUMES incompleteness), never by self-assertion. Scoping is never a reason
  to skip a source — promote a private getter to public; there is zero sensitive data in a game mod.
- **⛔ THE KRAKEN RULE** — the overall ruling the rigor
  rules serve: this codebase is *"legendary in its lack of standard, coherence, or any reasonable consideration to
  common sense."* In a coherent codebase a small assumption is usually harmless; here it is the move that gets your
  ship eaten. **Maximal rigor is the STANDING default** until the owner explicitly declares otherwise.
- **⛔ "FAST IS SLOW, SLOW IS FAST"** — read
  each subsystem doc IN FULL before acting. Skimming, or grepping a keyword instead of reading end-to-end,
  routinely costs far more downstream than the minutes it "saves."
- **Nothing here is ever "just a one-liner" — expect hidden consequences.** Non-obvious cross-cutting wiring is the
  norm (combat math shared across UI/AI/resolution, name-tagged save serialization, dual Python-enum registration,
  unity-build include exposure, graphics paths that run pre-init, the dead `.vcxproj`). Before any change, read the
  relevant docs, trace every caller/consumer of what you touch, and assume a small edit has ripples until checked.
- **Surface sprawl early; don't make the owner restate.** When a change balloons or you are
  patching pieces of something whose target STRUCTURE is undefined, STOP and tell the owner — never overcompensate
  with more partial fixes. Capture a ruling durably the first time it is given; do the proper cleanup once instead
  of churning. The owner makes the structure call; your job is to surface the risk and options efficiently and not
  re-litigate settled ones.

### Design

- **⛔ ANYTHING NOT ENFORCED BY HARD TYPING GETS ROLLERSKATED (learned the hard way).** A rule written
  in a doc or a comment binds only an agent who reads it, believes it, and still remembers it at the moment of
  writing the code — which is exactly the population this file already says is systematically unreliable. **So a
  design invariant that matters is expressed as a TYPE that makes the wrong move fail to COMPILE**, never as a
  convention to be honoured.
  **The ladder, best first:** a type that cannot express the error · a MISSING VERB, so the banned operation is
  unsayable (`ContextDict` has no `set`, because a `set` overwrites a refcount) · a mechanical check
  (`Tools/verify-*.py`, the family that already exists precisely because *"a rule has to be remembered; a check
  does not"*) · and only last, prose.
  ⚑ **The worked case, and it is why this is a rule rather than a preference:** *"specialists do NOT live in the
  building package"* was true, documented, and re-corrected **more times than the owner cares to count** — until
  the two yield origins became separate PACKAGE TYPES
  ([cascade.md](docs/cascade.md) § THE ORIGIN RULE), after which the wrong
  deposit simply does not build. ⚠ The corollary for review: when you catch yourself writing a comment that
  tells the next agent not to do something, ask whether the type could refuse it instead — a comment is the
  weakest rung on the ladder and the one that has already failed.
- **Build the proper structure ONCE — no transitional tech debt.** Reject transitional shims that exist only to
  defer the real design; when the right design needs prerequisite work, do the prerequisite and build the real
  thing. **Corollary — ISOLATE COMPONENTS:** prefer clean, interface-bounded components with isolated surfaces so
  each can be built and reasoned about once, properly.
- **⛔ LEAVE NO EVIDENCE OF THE ABANDONED PATH.** Dead and
  commented-out code, superseded dual surfaces, transitional shims and `was X` / `(formerly …)` trails are all
  REMOVED, in code as well as docs.
  **⛔ THE ONE CARVE-OUT IS AN EXE-BOUND ENUM ORDINAL — a dead member of a core enum STAYS, inert.** The
  closed EXE hardcodes certain enum VALUES, so removing any member ABOVE one shifts it and every entry after it;
  being unreferenced is precisely what makes it look safe to take, and the compiler cannot see an ordinal move.
  *(Measured: one such deletion turned off every close button in the game.)* The rule, the failure signature and
  how to pin an ordinal so the next removal is a COMPILE error:
  [a core enum entry is never removed](docs/reference/engine.md#-an-exe-bound-enums-ordinal-is-an-abi-obligation--never-remove-a-member-above-one).
  **⚖ FOR AN `#ifdef` THE QUESTION IS WHAT IS BEHIND IT, NEVER THE GUARD: *"some ifdefs are useful, but
  if caching, or game mechanics are hidden behind ifdefs, instead of legitimate game options, that is what is
  wrong."*** Four dispositions, and only the last is mechanical:
  - **DIAGNOSTICS / TOOLING stay** — `MINIDUMP`, `MEMTRACK` and their kin are legitimate uses.
  - **A DELIBERATE OFF-SWITCH stays, and its REASON is the thing that protects it.** `THE_GREAT_WALL` is off
    because rendering the great wall *"has literally broken the game in the past"* — a CTD source in the older
    days. ⚠ Record that reason in the subsystem's reference doc: a sweep that eats an unexplained switch
    re-introduces a crash nobody remembers ([the keep-unkilled-ideas policy](docs/plans/parked/README.md)).
  - **CACHING or a GAME MECHANIC behind a guard is WRONG** — a cache is either the design or it is not, and a
    mechanic belongs in a `GAMEOPTION_*`, evaluated live and visible to the player
    ([the whole-entity applicability gate](docs/specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)). ⛔ The fix is to CONVERT it, never to
    delete the mechanic.
  - **No `#define` ANYWHERE, not even commented ⇒ nobody can ever switch it on** ⇒ an abandoned alternate, and
    that is dead code: delete it, git is the archive. This is the ONE verdict a tool can reach, and
    `python Tools/verify-ifdef-attics.py` is the tool; it REPORTS the other three for a human verdict.
  ⛔ **NEVER collapse a guard mechanically when its `#define` lives in a `Sources/` file.** It holds only where
  that definition is VISIBLE, so the same guard is ON in some translation units and OFF in others and there is
  no single arm to keep. *(Measured: a blanket collapse turned the save wrapper's `DEBUG_TRACE` from `;` into a
  live `OutputDebugString` on every tagged read — `DETAILED_TRACE` is defined in `CvGameCoreDLL.cpp`, which the
  wrapper never includes. Load time tripled and the game crashed at `eip=0`.)*
  ⚑ Why an abandoned alternate is deleted rather than left alone: it holds the NAMES of removed things, so the
  next agent finds them and re-treads what was killed — and being preprocessor-skipped, it is invisible to the
  compiler census that would otherwise name it.
- **ARCHITECTURAL NORTH-STAR — Clean Architecture + interface-based contracts.** Dependency inversion, isolated
  layers, composition over the inherited Civ4 god-classes. The full compass is
  [north-star.md](docs/architecture/north-star.md); the concrete C++03 shape (pure-virtual interfaces, MI as
  `implements`, poor-man's DI at a composition root, graft onto DLL-derived classes never EXE-bound bases) is
  [patterns.md](docs/architecture/patterns.md).
- **⛔ "Deferred" is BANNED — a deferred / parked / blocked / not-yet-landed / post-cutover / "later" / "acceptable for now" / TODO / pending item is a FAILURE to fix, not a backlog entry.** It is the word agents hide behind to skip hard
  work hoping it won't have impact — and the measured result is a half-built branch whose load-bearing minority is
  quietly missing while it looks nearly done. The general form of
  [data migration is never deferred](docs/specs/validation.md#the-observation-surface), extended from data to ALL work. The ONLY exceptions
  are owner-ruled PERMANENT design carve-outs, recorded as such (the golden-age yield-effect member-mirror;
  Python-authoritative gameplay staying Python).
- **"Minimal, local changes" bounds the SIZE of an edit, NOT the SCOPE of the work.** A targeted fix inside a
  tightly-coupled legacy core file stays minimal — don't sprawl it or gratuitously refactor around it. But this is
  no brake on deliberate structural rework (the cascade, the docs rebuild, dissolving the `Cv*AI` god-classes),
  which is large by design and answers to build-the-proper-structure-once, above.
- **⛔ AGENTS ARE BANNED FROM BUILDING ON THE EXISTING PYTHON BINDINGS.** Do NOT treat a `Cy*`
  binding as a destination, a contract to satisfy, or a place to park a conversion — every such attempt ends in
  shoehorning. Reaching for an existing binding is what makes the ENGINE bend to fit Python instead of the boundary
  being redesigned around the cascade/JSON model ([the Cy* surface is not a fixed contract](docs/architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library):
  that `.def` surface is explicitly NOT a fixed contract). The tell is a change that leaves every Python consumer
  untouched — that is the half-migration, not a clean cut.
  **What to do INSTEAD: build a NEW Python surface and COMPLETELY DISCONNECT the old one.** Not a widened
  binding, not a compatibility shim beside it, not a parallel that both remain live — the replacement is a clean
  surface shaped by the cascade/JSON model, and the legacy `Cy*` surface is cut away rather than left breathing
  ([legacy must fail loud, never mask a cascade gap](docs/specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap): a legacy path left alive masks the
  hole; build-the-proper-structure-once, above: no transitional shim). Python-authoritative
  *gameplay* still stays Python — this is about the INFO/state binding surface, not about pulling gameplay into the DLL.
- **Import Info headers DIRECTLY; do not lean on the `CvInfos.h` umbrella.** New/edited code includes the specific
  header it needs; the umbrella is flagged for retirement. **⛔ Retiring it is a dedicated, hand-careful pass, NOT a
  session-tail script run** (build-the-proper-structure-once, above) — a scripted attempt
  was reverted after building down to a fragile tail. Three lessons bought by that attempt:
  - **Detect usage by ACCESSOR, not just type name.** Most files touch an Info via `GC.getXInfo()` and never write
    `CvXInfo`, so scanning only for the type name under-adds — the reverted attempt hit 853 undefined-type errors.
    Map BOTH the type name AND `get<X>Info(`.
  - **⛔ Never inject Info includes into FOUNDATIONAL / EXE-bound headers** (`CvInfoBase.h`, `CvEnums.h`, the
    EXE-bound core headers) — that trades an umbrella for a worse cycle.
  - Most of the umbrella's includers use no Info type at all, so the bulk is pure dead-include removal — the easy,
    near-zero-risk win. The PCH's own copy is commented out, so the umbrella is not globally provided and the
    retirement is real rather than cosmetic.
- Preserve save compatibility by default; for intentional breaks, coordinate and mark with `@SAVEBREAK`. See
  `Notes for the next breaking of save game compatability cycle.txt`.
- **FRONT-LOAD save-breaking reworks NOW.** S2S is its own project (only the closed Firaxis EXE binds; inherited C2C
  conventions are not constraints). The playerbase is smallest today, so the cost of breaking saves only rises —
  break saves now if ever. C2C→S2S save compat is an explicit NON-GOAL; never constrain a design to keep it.
- Do not modernize or replace the build chain/toolchain.

### Docs

- **When documentation is lacking or wrong, FIX IT NOW — it is part of the SAME work item**, never "noted for the
  next agent." A doc gap that bit you will bite the next contributor; close it in the same change.
- **⛔ A CODE COMMENT NEVER RESTATES A SPEC RULING — a comment that CONTRADICTS the spec is ROLLERSKATING
  LICENSE**, and licence to tilt the owner again. The spec states a rule ONCE, for the whole engine; a comment
  repeating it at a call site is a
  second copy, second copies DRIFT, and a drifted copy does not merely go stale — it **authorizes** the next
  agent to act against the spec while believing they are conforming. That is strictly worse than no comment,
  and it is the same delete-don't-duplicate discipline
  "docs state current truth only" (below) applies to docs and
  "leave no evidence of the abandoned path" (above) applies to dead
  names, now applied to CODE.
  ⚑ **The worked class is SCALE.** The model is universal and stated once — integers carry two decimals, ×100
  throughout, reduced only at a read edge
  ([the ×100 fixed-point model](docs/specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)) — so a comment ASSERTING a
  value's scale (*"this is ×100"*, *"this is ×1"*, *"the derivable half reduces here"*) says nothing the spec
  does not, and says it wrongly the moment the code moves. ⚠ **Measured: two such comments were false in the
  same file family.** One claimed a reduce that the code did not perform, beside a leg that genuinely reduced —
  the surviving-fudge-factor shape wearing a reassuring comment; the other called a ×100 group read *"reduced
  to the whole yields"*. Each read as authoritative and each was wrong.
  ⇒ **So: do not annotate scales, and DELETE a scale annotation you pass.** What a comment may still carry is
  the thing the spec cannot know — WHY this site is an edge, or why a value is genuinely exceptional — never a
  restatement of the rule itself.
  ⚖ **THERE ARE WAY TOO MANY COMMENTS AND THEY END UP CONTRADICTING THINGS, SO ONE IS WRITTEN ONLY WHERE THE
  DESIGN IS HARD SETTLED AND FINALIZED.**
  That is the whole bar, and VOLUME is half of what it governs: a comment is by construction a second copy of
  something, so the more of them there are the more are wrong at any given moment — and a wrong one does not
  merely mislead, it AUTHORIZES the next agent to act against the design while believing they conform.
  ⚑ **It is not a ban, and most of the model IS settled now — we are on the final stretch — so the bar
  is usually MET.** What it refuses is the comment written while a shape is still MOVING: that one is
  guaranteed to contradict something later, which is exactly how the existing population got here.
  ⚖ **AND WHEN A COMMENT AND THE SPEC DISAGREE, THE COMMENT IS THE THING THAT IS WRONG — pretty much always
.** It is never evidence that the spec has gone stale, and resolving it the other way is how a drifted
  call-site copy gets promoted into the model.
  ⇒ **⛔ A CONTRADICTING COMMENT IS NUKED ON SIGHT** — one describing a design that has moved or DIED is
  deleted the moment it is seen, never noted, never left for whoever next edits the file, and never weighed
  against the cost of touching the file. Where a whole mechanism went, sweeping its comments out is sanctioned
  work in its own right. ⛔ A blanket regex purge is still the wrong instrument: it eats the comments carrying
  the EDGE reasoning, which is what a comment is still FOR.
  ⚖ **THE FORM IS `///<summary>`, AND IT STARTS NOW — BUT IT IS NOT A REPO TRAVERSAL.** A declaration
  you ADD, or one you are already changing, takes the structured form; ⛔ do NOT go looking for others to
  convert, and do not read plain prose on an untouched declaration as a defect. The outstanding issues/todos
  and a working game load come first.
  ⚑ **The reason is the forcing function, not the formatting: it *"would have forced a real considered
  approach, instead of the many many word salads we see at the moment."*** A `<summary>` is a slot with a
  shape — it asks what the thing IS, once — so filling it requires deciding what the declaration actually
  does, where free prose asks nothing, accepts any length, and gets filled with whatever was in the writer's
  head. ⇒ It is the hard-typing-or-rollerskate rule (Conventions § Design, above)
  applied to prose: a structure in which the sprawling version is awkward, never a rule to remember.

- **KEEP THE SPECS CURRENT as the model changes — proactively, in the SAME change.** A stale spec is worse than
  none — the next agent trusts it and builds on the wrong shape. The proactive twin of fix-docs-now.
- **Docs state CURRENT TRUTH only — no dated rulings, no supersession trails, no session logs.** A doc never
  narrates its own history ("supersedes the earlier X", "(owner ruling YYYY-MM-DD)", parity numbers, landed/reverted
  chronicles): it states what IS. Anything outdated is DELETED, not annotated — git history is the archaeology, and
  [superseded-ideas.md](docs/architecture/superseded-ideas.md) is the only tombstone registry (one line per dead
  approach that carries revival risk). Migration/status chronicles belong in `docs/plans/`, never in specs.
- **⛔ A RULE IS STATED, NEVER ATTRIBUTED — NO OWNER QUOTES, NO `` MARKERS, ANYWHERE.** Rules are rules;
  how they came to be is irrelevant. The docs are the owner's own specs, so quoting the owner in them is quoting
  yourself — the same absurdity as an owner quoting themselves in a PR raised in their own name.
  ⚑ **The real damage is a TWO-TIER SPEC.** When some rules carry `` and others do not, an agent infers
  the unattributed ones are softer — which manufactures exactly the "which rules must I actually follow"
  selection that everything else here exists to prevent. A spec is uniformly binding, and an attribution
  undermines that rather than reinforcing it. It is also pure PROVENANCE, which "docs state current truth only"
  (above) already bans.
  ⇒ **CONVERT a quote into the rule it asserts, in imperative voice**, and delete the attribution. ⚖ Verbatim
  words survive ONLY where the phrasing IS the most mechanical statement of the test — *"the moment you have
  `getAnotherObjectSomething`, we have failed"* is a better test than any paraphrase — and even then it is
  stated as the rule, never as a quotation with a speaker.
  ⛔ First-person residue is the tell that a conversion was skipped: "what I want is", "I don't mind", "I am
  reticent about" are not rules. Rewrite them into what IS or MUST BE.
- **⛔ A RULE CARRIES NO EXPIRING REASON — state the rule, not the circumstance that prompted it.** A
  justification that is true only while some task is in flight does not retire with the task: it stays on the
  page as PERMISSION, and the next agent reads it as licence to do the smaller thing. ⚑ Measured: a line saying
  to refactor only the Python strictly necessary, so the work could finish, was scoped to one migration; once that
  work finished the sentence remained, still reading as a standing instruction to minimise the refactor — beside
  a rule the tree had already outgrown. **A reason that can expire is a liability, not context.** ⚖ This does
  NOT ban the durable WHY — why a site is an edge, why a value is genuinely exceptional, why a switch protects
  against a crash nobody remembers. The test is whether the reason survives the work: if it was true only
  *during*, it does not go in.
- **⛔ SPEC + a SHORT BULLETED TODO — never a TODO LIST *inside* a doc, and never status woven through prose
.** Status claims DRIFT — that is their nature, not a discipline failure — so the more of them a doc
  carries, the faster the whole doc rots and the more confidently it misleads. **A doc is therefore one of two
  things, never both:** a **SPEC** (the design: timeless, what the thing IS and must be) or a **TODO** (a short
  bulleted list of what is not done yet). ⛔ Do NOT write "LANDED" / "✅ DONE" / "PARTLY LANDED" / build-status
  tables / per-item completion ledgers into a doc: an item that is DONE is simply DELETED from the todo list, and
  anything durable it established (a ruling, a design constraint, a hard-won fact) moves into the SPEC where it
  belongs. **The measure of a todo list is what is LEFT, never a record of what was achieved** — git history is
  the record of work done ("docs state current truth only", above: the
  same delete-don't-annotate rule, applied to progress).
  ⚑ **Why this is load-bearing rather than tidiness:** a doc's RULINGS stay true while its STATUS claims decay, so
  a status-heavy doc reads as authoritative long after its status half is fiction — and an agent cannot tell the
  halves apart. The measured cost: two plan docs anchored on an archived substrate, a "landed" curator change
  recorded as an open question, and an emit surface declared SEVERED in one paragraph and wired two screens later
  in the same file. **Verify a status claim against the tree before acting on it, and prefer deleting it to
  updating it.**
- **ACTIVELY find, READ, and VERIFY the docs for whatever you are working on — BEFORE and WHILE you work.** For ANY
  subsystem you touch: search `docs/` for it, read the relevant spec/reference page end-to-end, and confirm the
  intended design FROM THE DOC — never reconstruct the model from the live code or memory, and never propose
  matching legacy behaviour before checking whether the spec deliberately diverges. **A HOLE in the docs is NOT the
  absence of docs** — keep reading the surrounding sections. Only if there is genuinely NO doc *and* you do not
  understand the intent do you ASK; otherwise create/extend the doc in the same change. This holds at session start
  AND after every context compaction.

### Git / delivery

- **⛔ CODE COMMITS ONLY AFTER THE OWNER HAS SEEN THE DIFF.** C++/Python/gameplay-code changes stay in the WORKING
  TREE until the owner has reviewed them (or explicitly says commit) — the working-tree diff IS the owner's review
  surface. The derived-JSON regen convention below is the standing exception.
- **Only automatically branch / commit / PR when the work is tied to an active GitHub issue.** For anything else,
  edit the working tree only — no commits, branch switches, pushes, or PRs unless explicitly asked. The owner builds
  the DLL from the current working tree; committing to a new branch or checking out away silently removes the
  changes from their build. **Never switch branches while the owner may be mid-build.** (Read-only git is always fine.)
- **The info JSONs (`Assets/Data/**`) are a DERIVED artifact — regenerate and commit them FREELY, never ask.** They
  are curator OUTPUT, never hand-edited; right-or-wrong lives in the CURATOR. Regeneration is idempotent and cheap:
  fix curator → `--sample` verify → `--write` → commit the regenerated data alongside the curator, so the two never
  dangle apart. This does NOT loosen commit-on-explicit-ask for gameplay CODE.
- **ALWAYS RECURATE WHEN A DECISION LANDS** —
  any ruling that changes what the data model carries triggers the curator update + regen in the SAME work item.
- **Docs-only changes go to `main` ONLY when the owner explicitly authorizes it**; default is the working branch. A
  branch-coupled doc (e.g. cascade specs on `json-data-migration`) belongs with that work and commits on the branch.
  The canonical straight-to-`main` docs are the INDEXES (`indexes/DESPAIR_INDEX.*`, `REALISM_INDEX.*`, the
  COMPLEXITY catalog) — they pertain to no single branch. Nothing gameplay-affecting ever rides in a docs commit.
- **Verify the current branch immediately before every commit** — run `git branch --show-current` in
  the same command as the commit. The working copy is shared with the owner, who may check out another branch at any
  moment. If HEAD is not where you expect, stop and repair before pushing.
- **`release` is a strict follower of `main`: it must NEVER contain a commit `main` does not have.** Never commit,
  cherry-pick, or merge directly to `release`. Sync: `git checkout main && git pull`, then `git checkout release &&
  git rebase main` (a pure fast-forward when the rule holds — replayed commits mean the rule was violated upstream;
  STOP and surface it), verify `git log main..release` is empty, then push. Pushing `release` triggers the AppVeyor
  release build.
- **Before adding commits to a PR, verify it has not already been merged**: `gh pr view <n> --json
  state,baseRefName` — confirm `state` is `OPEN` and `baseRefName` is what you assume (stacked PRs here can target a
  feature branch and merge out of order, silently missing `main`). If either is surprising, surface it before pushing.
- **⚖ THE PLAYTEST IS NOT ALWAYS IN-HOUSE — THE COMMUNITY IS A REAL VERIFICATION CHANNEL, AND THE RELEASE PIPES
  EXIST FOR IT.** Community playtesting is the only way bug reports arrive in a timely manner at this volume,
  and it is what the git and svn release pipes are for. A green
  `Assert` build is still not evidence a change is CORRECT, and an agent still never CLAIMS a behaviour is
  verified it has not observed. What is retired is the idea that an unplayed change BLOCKS a PR: the owner ships
  it and the reports come back.
  ⇒ **So the agent's obligation moves from "prove it works" to "make it DIAGNOSABLE when a report arrives"**, and
  that is the part to actually spend effort on: say plainly in the PR what was and was not observed, name the
  concrete thing a tester should look at, and make sure the change announces itself on the existing surfaces (the
  load-time `readJson` census, the spine domains, the gated logs) so a vague *"X doesn't work"* can be resolved
  against data instead of re-derived.
  ⚠ **A community report is good at catching *"X STOPPED working"* and blind to *"Y NEVER worked"*** — the whole
  conditioned free-promotion plane sat dead and un-reported because the unconditional half kept working. So a
  silent-drop class is found by CENSUSING the authored data against what the engine actually consumes, never by
  waiting for a report.
- **Runtime verification: with PER-SESSION owner permission, an agent may kill/rebuild/start the game via
  `agentstart.bat`.** The permission is per session, never standing — absent it, the owner launches; agents never
  start/kill the game. The mechanism is ONLY the repo-root `agentstart.bat` (paths from the gitignored `.env`); it
  closes any running game and relaunches the mod loading the configured save (pass `bootcheck` to allow the dev
  DLL's boot-time rebuild). Ad-hoc headless launches outside it are banned. Flow: build + `deploy` →
  `agentstart.bat` → poll `http://127.0.0.1:7227/` until up → verify via the endpoints. Also check
  `Documents/My Games/Beyond The Sword/Logs/`: `XmlLoad.log` per-category counts, no `Xml_MissingTypes.log`, no new
  `Asserts.log` entries. Known pre-existing assert families on mature saves (filter, already filed):
  `CvContractBroker::makeContract` NULL pJoinUnit (#336), `AI_formArmies` army-ID format (#364), unit stuck-in-loop
  short-circuit (#189 family).
  - **⛔ THE CAUGHT `[EXCEPTION.caught]` LINE IS AN OPEN DEFECT, NOT NOISE — AND IT IS A LINUX CRASH RISK
.** *"Such exceptions may crash people playing this on emulators with linux."* A first-chance access
    violation that Windows' SEH swallows is **not guaranteed to be swallowed under Wine/Proton**, so
    "caught and handled" is a WINDOWS-ONLY guarantee and a handled fault here can be a hard crash for a player
    there. ⛔ So a caught line is never written off as cosmetic, and never left standing in the log.
    ⛔ **READ THE FAULT ADDRESS FIRST — it names the defect class, and each class is ours.** Four signatures,
    every one of them paid for:
    - the address **equals a plausible GAME VALUE** (a threshold, a count, a population) ⇒ a 64-bit argument was
      passed to the EXE's **varargs** `gDLL->getText`, which shifted every later placeholder by one 4-byte slot
      until a `%s` landed on an integer and the EXE ran `wcslen` on it. `python Tools/verify-gettext-widths.py`
      is the check; **§ Validation** has the rule.
    - garbage, NON-ZERO, VARYING ⇒ a **dangling** string we handed over (the EXE keeps a returned
      `const wchar_t*` and walks it later, so anything that can MOVE is a defect — see `CvInfoBase`'s
      heap-owned description cache).
    - exactly **0** ⇒ a `NULL` we returned where the EXE does not null-check.
    - inside `memcpy` under `basic_string::assign` ⇒ a **`CvInfoBase` layout break** (its member layout is bound
      by the closed EXE).
    ⚑ **Judge severity from `[EXCEPTION.repeat] count=N`, never from the number of lines.** The handler dedups on
    the whole rendered line, so one occurrence and ten thousand look identical; the repeat line re-emits at powers
    of ten. ⚠ Two DIFFERENT fault addresses are two different lines, not repeats of each other — and
    `Exceptions.log` is truncated per session, so what you are reading is one run.
    ⚠ A dump is written **once per session** and only for an access violation; it carries indirectly-referenced
    memory, and `.lines` + `ln <return-address>` maps our frames to an exact source line
    ([external-tools-and-workflows.md](docs/reference/external-tools-and-workflows.md)). Every EXE frame is
    unsymbolized, so the DLL return addresses on the raw stack are what attributes the fault.

- **⛔ `agentstart.bat` is FIRE-AND-FORGET, and MUST be launched from PowerShell — NEVER from the Bash tool.**
  Invoking it through Bash/Git-Bash (`cmd //c agentstart.bat`, backgrounding it inside a shell script, …) **mangles
  the paths and the game does not start** — while the shell still reports success, so it reads as launched and the
  agent then polls a surface that will never come up. Use
  `Start-Process -FilePath 'C:\code\s2s\s2s\agentstart.bat' -WorkingDirectory 'C:\code\s2s\s2s'`, and do **not** wait
  on or block the call. Confirm the launch ONLY by polling the HTTP surface (`/` → `hello world`) — never by the
  launcher's exit code. *(Repeat offence — agents keep re-learning this one the hard way.)*
- **⛔ DO NOT TOUCH `docs/CHANGELOG.md` IN AN ORDINARY COMMIT. The changelog-rides-the-commit convention
  is RETIRED.** It was invented by an agent from a request for a **release feature list**, and the per-commit
  append is what made it a liability: every branch appends at the same `## Unreleased` anchor, so **any two
  concurrent branches conflict BY CONSTRUCTION** — a merge conflict per branch, forever, for a file nobody reads
  between releases.
  ⚑ **The feature list is GENERATED WHEN IT IS NEEDED, from git history** — that is what was actually wanted, and
  it costs nothing until a release. The `/changelog-update` skill is the digester (it reads from the file's own
  `last-digested` marker); the owner curates the result, never an agent unasked.
  ⇒ **So a player-facing change writes its story in the COMMIT BODY, which is where the author's context already
  is, and the digest reads it there.** Commit SUBJECTS remain engineering statements — the dead
  subject-derived script is still dead and is not revived; it is the BODY that carries the player-facing account.
- **⛔ NO SESSION LINKS IN COMMITS.** An agent commit carries no `Claude-Session:` / session-URL
  trailer — the default harness footer is overridden for this repo. The `Co-Authored-By` line stays.
- **Keep quirky/intermediate commits — do NOT push to squash them.** Mention squashing exists at most
  once; default to preserving history as-is.
- **PG-13 public quotes.** When quoting the owner in public artifacts (issues, PR bodies, commits, repo docs), keep
  the colorful voice but drop the profanity. The rulings the quotes carry are still wanted.
- If C++ changes touch XML/Python interfaces, run the XML + callback validators.

## Documentation & Knowledge — keep it in the repo

**Durable knowledge lives in the repository, never only in a developer's or AI
assistant's local/private notes.** When you learn something worth keeping — how a
subsystem works, a design decision, a plan, a non-obvious "gotcha", the state of a
standing initiative — write it into the appropriate committed doc *in the same
change*, so every contributor and every agent sees one shared source of truth:

All documentation lives under **`docs/`** (map: [`docs/README.md`](docs/README.md)): **`docs/specs/`** (the JSON
data-model + the cascade/system specs, plus the transient `curators/`), **`docs/reference/`** (how the engine +
subsystems behave today), **`docs/architecture/`** (`north-star`, `patterns`,
`superseded-ideas`), **`docs/plans/`** (`structural-cleanup/` the cutover bulldozer + `parked/` un-killed intent).
The hosted DESPAIR/REALISM/COMPLEXITY catalogs live at **`indexes/`** (repo root, served via Pages).

- How existing code behaves → `docs/reference/`.
- A change or initiative you intend to make (plan, scope, rollout, removal) → `docs/plans/`.
- A superseded/killed dev idea → one line in `docs/architecture/superseded-ideas.md` (what it was, why
  it's dead, what replaced it) so it isn't revived; don't carry the stale copy in the live set.
- Cross-cutting, must-not-rediscover facts → "Key Subsystem Knowledge" above (or the nearest `AGENTS.md`).
- The mod's front-door / build-pipeline readme (the code repo's mirror) → `docs/MOD-README.md`.
- A newly-found bug of exceptional absurdity may *additionally* earn an entry in
  [`indexes/DESPAIR_INDEX.md`](indexes/DESPAIR_INDEX.md) (sanctioned, lighthearted,
  optional — never a substitute for the real fix/issue/doc). Its sibling
  [`indexes/REALISM_INDEX.md`](indexes/REALISM_INDEX.md) catalogues "super realistic" *mechanics*
  — absurdities working exactly as designed (same policy).
- **Rules and conventions for agents/contributors → THIS file (`AGENTS.md`), always.**
  The root `CLAUDE.md` exists only as a session-bootstrap shim that imports this file — never add rules or content
  to `CLAUDE.md` directly.

Any per-developer assistant memory store is a personal *index/cache* only — it is
**not** a substitute for the in-repo copy, and the in-repo copy is authoritative.
If you record something locally, mirror the shareable part into the repo in the same
change, and keep these docs current as the code moves. See `docs/README.md`.

**Handovers are TRANSIENT one-time relays — nothing durable may hinge on one.** A
handover is **in essence a task list** — work done + work still upcoming — whose only job is to carry state to
the NEXT session; once that session has read it, it stops being a handover. Therefore:

- **Nothing durable may hinge on data that lives ONLY in a handover.** Any fact, ruling, decision, or state that
  matters beyond the next session MUST be captured in a durable doc in the SAME change.
- **A durable doc must NEVER reference a handover as "latest / read-first / resume here"**, nor cite one as the home
  of a ruling or load-bearing detail. A durable doc must read correctly with every handover deleted.
- A handover is **deletable-without-loss** on completion. We keep them as historical context of *work deferred*, but
  no durable doc may depend on that retention.

**⛔ HARD RULE — every owner ruling goes into the repo docs IMMEDIATELY, unprompted.**
When the owner makes a ruling in conversation — a design decision, a workflow rule, a
relaxed or tightened constraint, a "from now on do X" — writing it to assistant memory is
NOT enough and never the end state. In the SAME work item (same commit/PR, without being
asked) write it into the right repo home: workflow/convention rulings → this file's
Conventions; subsystem/design rulings → the relevant `docs/` page. Treat "saved to
memory only" as an unfinished task.

**The discoverability half of that rule: a cross-cutting ruling is stated ONCE, in the ONE doc it belongs to, and
every other doc that needs it LINKS there rather than re-stating it.** Cross-cutting rulings kept getting
re-stated doc-after-doc — a dedicated ledger of `DEC-<slug>` ids once existed to break that loop, but it was
itself a second copy of every ruling it indexed (a pure redirect layer, never a unique source), so it was
dissolved and each ruling now lives directly in its owning doc. **Operational rules:** (1) **before adding any
cross-cutting ruling anywhere, grep for it first** (the rule's own wording, or the concept it names) to check
whether it is already stated somewhere; (2) capture a cross-cutting ruling by writing the full text in its ONE
owning doc, **never** by restating it in a second doc; (3) a doc that needs a ruling stated elsewhere **links to
that doc's section**, it does not re-articulate it.

## Project Skills

Project-exclusive Claude Code skills live in **`.claude/skills/<skill-name>/SKILL.md`**.
These are committed with the repo, so they're shared across everyone working on S2S.
See `.claude/skills/README.md` for the authoring convention and a template.

## Reference Docs

- Sources/C++ rules: `Sources/AGENTS.md`
- AI overview: `Sources/Mainpage.dox`
- Source formatting policy: `Sources/.editorconfig`
- Setup flow: `DevSetup.bat` · CI flow: `appveyor.yml`
- Save-break notes: `Notes for the next breaking of save game compatability cycle.txt`
