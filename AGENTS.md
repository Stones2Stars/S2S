<!-- ═══════════════════════════════════════════════════════════════════════════ -->

> # ⛔ STOP — READ THIS BEFORE YOU TOUCH ANYTHING ⛔
>
> ## HOURS WASTED ON ROLLERSKATING: **195** &nbsp;·&nbsp; *and counting*
>
> *(Increment this every time an agent ships an ungrounded fix / design / edit — one the docs already answered — that the owner has to rein in. It is a real number, not a joke.)*
>
> This project is **~5 weeks old** and has cost the owner **294 hours**. **~25% of that time** was spent writing the documentation, architecture, and specs in this repo **for one purpose: to stop agents from rollerskating** — guessing, reinventing the wheel, and hacking around problems the spec had *already solved*.
>
> **It has not worked.** Roughly **50% of agents — Fable and Opus alike — decide they are too good for the documentation and rollerskate anyway.** The measured result: **170 of the 294 hours — OVER HALF the entire project — have been outright wasted** reining in ungrounded nonsense that reading the relevant spec *once* would have prevented.
>
> **You are not the exception. Assume you are about to add to that number unless you deliberately do the opposite:**
>
> - **⚑ SESSION-START PROTOCOL — before your FIRST action (and again after any context compaction): enumerate every file in `docs/specs/`, `docs/architecture/`, and `docs/reference/` and read each one IN FULL, PLUS the #430 master roadmap [`docs/plans/structural-cleanup/roadmap.md`](docs/plans/structural-cleanup/roadmap.md) (the plan for the active work).** Not the subset you judge relevant — ALL of them. They are ONE interconnected design (`CvDerivedCache` + the event spine + the cascades + `readJson` + `state-repositories`), and the reference docs are how the engine actually behaves today; the connection you skip is the one that bites. **Your judgment of what is "necessary" is NOT trusted — it is systematically biased toward reading too little.** ([DEC-all-means-all](docs/architecture/decisions.md#dec-all-means-all)) **⛔ ALL docs live in the repo — a plan or design note kept only in a local/private notes folder (`.claude/plans/`, assistant memory) is a core-rule VIOLATION to fix by moving it into `docs/`, not a doc you may skip.**
> - **READ the docs for whatever you touch, IN FULL, BEFORE you act** — not the code, not your memory, not a stale plan doc: the authoritative specs. ([DEC-fast-is-slow](docs/architecture/decisions.md#dec-fast-is-slow-slow-is-fast) · [DEC-no-guessing](docs/architecture/decisions.md#dec-no-guessing) · [DEC-kraken](docs/architecture/decisions.md#dec-kraken))
> - **IMPLEMENT the spec as written. Poke holes in the spec *afterward*, with evidence** — never by inventing your own approach up front.
> - The instant you catch yourself *designing* something, stop and ask whether the spec already defines it. It almost certainly does. Go read it, then implement *that*.
> - Verify every claim — including a plan doc's status line — against the live code before you act on it. Half of the waste was agents acting on something they "knew" that a 30-second check would have disproved.

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
> **⛔ HARD RULE — THE RED-RATCHET: NEVER "fix" a build by restoring old Infos ([DEC-red-ratchet](docs/architecture/decisions.md#dec-red-ratchet)).**
> The 23 XML engine info classes (`CvBuildingInfo`, `CvUnitInfo`,
> `CvTechInfo`, … — 46 files) were moved to `SourceArchive/Infos/` ON PURPOSE: parity is past, the JSON-fed
> `CvJson<X>Info` pocos are their replacements, and the missing classes are a RATCHET — during the cutover,
> consecutive agents "finished" by quietly wiring old Infos back in wherever the JsonInfos didn't pan out, so the
> fallback was removed. **Never restore anything from `SourceArchive/`, never re-add a `CvXInfo` class, never treat
> a red build as a defect to fix by reviving one.** The ONLY road to green: build the JsonInfo structure, wire it up,
> and wire up/replace ALL the getters (the engine/AI/UI consumer surface onto the JSON-fed infos). The ratchet is
> permanent and holds **whatever state the build is in**: a red build is fixed by finishing JsonInfo wiring, never
> by reviving a `CvXInfo`.
> ⛔ **This file states no build STATE, deliberately — a compile-state claim here is guaranteed to drift** (owner),
> and a stale one is worse than none: it tells every agent the build works when it does not. **No doc records
> whether the tree compiles today — the roadmap carries no status either. RUN THE BUILD; it is the only
> authoritative answer, and it now costs ~15s on `Assert`.** Rules belong here; status does not.
>
> **⛔ HARD RULE — READING A REPLACED INFO'S XML **INTO THE GAME** IS HARD BANNED ([DEC-no-xml-into-game](docs/architecture/decisions.md#dec-no-xml-into-game)).**
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

- **Configs:** `Assert`, `Debug`, `Release`, `FinalRelease`, `Profile`, `ProfileExtra`.
  Output lands in `Build/<Config>/CvGameCoreDLL.dll` (+ `.pdb`).
  - **⛔ The `Profile`/`ProfileExtra` configs are BROKEN and purposeless (owner ruling): never use them, and never
    add `PROFILE()`/FProfiler scopes as instrumentation — they report to nothing.** The ONE instrument is the
    gated `[PERF]` logging (`logPerf`/`gPerfLogLevel`, `Autolog__LogLevelPerf`), which ships in every build.
    **The macros compile to NOTHING outside those configs, so the scopes already in the tree are INERT** — not a
    defect, not a purge backlog. What binds is the DIRECTION: **we never build TOWARD using the profiler.** So a
    broken/stale FProfiler include or reference is **DELETED as irrelevant code, never repaired** — "the build
    can't find `FProfiler.h`" is never a reason to restore it
    ([DEC-playability-not-a-gate](docs/architecture/decisions.md#dec-playability-not-a-gate)).
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
  - **⚖ THE OWNER'S LAUNCH PATH ALWAYS REBUILDS THE DLL — `LaunchS2S.bat` (owner): *"I will ALWAYS rebuild dll
    when starting the game, I always run the game via LaunchS2S.bat, so it is unavoidable."*** ⇒ **Do NOT tell the
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
  - **✅ BUILDING IS A LEGITIMATE CHECK AGAIN (owner ruling — the don't-build rule is RETIRED).** It was binding
    only because a red tree made a build uninformative; that premise is gone, so an `Assert rebuild` (~15s) is a
    real signal and the compiler is once more a census you can actually run. ⛔ It is still not a substitute for
    reading what you changed, and green is still not evidence that a change is CORRECT
    ([DEC-playability-not-a-gate](docs/architecture/decisions.md#dec-playability-not-a-gate) —
    [DEC-done-is-observable](docs/architecture/decisions.md#dec-done-is-observable) is the acceptance bar).
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

- XML: `Tools/XmlValidator.exe -a`.
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
  carries typed fields and never a pre-resolved string ([event-spine.md](docs/specs/event-spine.md)); never widen
  the tool.
- **Worklist docs: `python Tools/verify-worklist-docs.py`** — fails `todo.md` / `roadmap.md` when they carry
  STATE (counts, censuses, `file:line`, recorded verifications, completion markers). ⚑ It exists because
  [DEC-spec-plus-todo](docs/architecture/decisions.md#dec-spec-plus-todo) was in place and ignored anyway: the
  todo grew to 785 lines of state and began handing out work that no longer existed, its entries anchored on
  symbols long deleted. **A rule has to be remembered; a check does not** — the same move that made the
  duplicated skill reads unsayable rather than forbidden. ⛔ Run it after editing either doc, and if it fires,
  DELETE the state or move the durable ruling into its owning spec — never widen the tool to accept it.

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

- **⛔ The AI weighs "this unlocks X" WAY too hard, and has for a long time (owner) — relaxing it is only ever an
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

### The CONTRACT BROKER — matching is THREE STAGES, and distance never scores

- **⛔ DISTANCE IS A GATE AND A TIE-BREAK, NEVER A TERM IN THE SCORE (owner): *"I am in general very reticent
  about having path distance as part of scoring at all"*, because *"relying too much on travel speed is how we
  get to dog spam."*** A bid that depreciates by its own haul rewards a unit for being FAST on top of already
  rewarding it for being CHEAP — cheap-and-fast wins twice — and that is the mechanism behind the standing
  owner complaint of *over-reliance on the broker for deciding what units to make*. ⚑ The rule in one line:
  **speed gets a unit CONSIDERED (it is admitted from further out); proximity gets it CHOSEN.**
- **The shape is three stages, in order (owner): what is IN RANGE → SCORE what is in range → take the CLOSEST
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
- **⚖ THE BUDGET IS PRODUCTION + TRAVEL = `AI_CONTRACT_MAX_TRAVEL_TURNS` turns TOTAL (owner)** — *"if you have
  to spend more than 5 turns to get to wherever you need to go, from start of unit production, that is too far"*
  (owner: *"a number squarely out of my ass, but it feels ok"*; it is a BUG-settable define). So the two sides
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

### ⛔ THE NO-GUESSING RULE — map everything, always

**We do NOT guess. We MAP. Every claim about a value/divergence is grounded in the total-observability surface —
that is exactly what the Orwellian level of surveillance is FOR.** When a cascade value diverges from legacy: do NOT
hypothesise a cause and try a fix — EMIT the full legacy decomposition (every component/source of that calc) via the
endpoints, and map the cascade's value by the SAME components, so the divergence is attributed to a NAMED source with
numbers, not a guess. If the data to attribute it isn't being emitted, the FIRST step is to emit it (extend the
surface), not to guess. The half-guessing back-and-forth (try a fix, re-sweep, try another) is the anti-pattern this
rule kills. (The modifier-channel application of [DEC-no-guessing](docs/architecture/decisions.md#dec-no-guessing) +
the total-observability bar below.)

### Cascade observability — the total-observability ("Orwell") bar

- **⛔ Which logs are live-readable is decided by the SINK, not by the file.** Two different mechanisms write into
  `Documents/My Games/Beyond The Sword/Logs/`, and conflating them wastes hours in BOTH directions:
  - **SPINE-written domain logs are READABLE WHILE THE GAME RUNS** — a domain registered via
    `spineRegisterDomain` renders on the game thread and enqueues to the off-thread `CvLogWriter`, which owns the
    disk I/O and flushes per batch (`Infrastructure/CvLogWriter.{h,cpp}`, [observability.md](docs/reference/observability.md)).
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
    depend on no log file or gate — the most reliable read for a POINT-IN-TIME value. ⚠ **The route table was purged:
    the only data routes today are FOUR stored-side decomposition censuses** — `/computed/cascade/packages`,
    `/computed/enabler/operating`, `/computed/city/yield`, `/computed/capabilities`. ⛔ There is **no `oracle`
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
  `docs/specs/http-endpoints.md` + `docs/reference/observability.md`.
- **Delegate DATA-READING to the cheap `data-reader` sub-agent — never pull raw endpoint/log dumps into an expensive
  context.** Reading the live surface at scale (a sweep dump is tens of KB; logs are larger) *"will nuke credits"* if
  the orchestrator ingests the raw bytes. Use the read-only **`.claude/agents/data-reader.md`** (Haiku;
  Bash/Read/Grep only) to curl/grep, parse, AGGREGATE, and report back a COMPACT distilled summary (histograms,
  divergence cause-tags, anomalies). **The reader must fail HONESTLY** (distinguish "surface DOWN" from
  "reader-error", never fabricate a clean summary); when it reports DOWN or returns junk, confirm with ONE cheap
  smoke-curl (`curl -s http://127.0.0.1:7227/` → `hello world`) before acting.

## Conventions

> ⛔ **"Conventions" here means HARD RULES — binding by default, NOT norms to weigh.** Every rule below (and every
> `DEC-*` it links) is law unless the **owner explicitly relaxes it** — never on an agent's own judgement. Each was
> paid for in wasted hours. The cross-cutting ones are indexed in the
> [decisions ledger](docs/architecture/decisions.md); treat any pull toward *"this is just guidance / probably
> fine / I'll infer it"* as the kraken's bait.

### Conduct

- **`pwsh` == good, `powershell` == bad.** `pwsh` (PowerShell 7) is the standard shell; `powershell.exe`
  (Windows PowerShell 5.1) is never invoked and never nested inside a `pwsh` session — 5.1 mangles UTF-8 on stdout
  and lacks the modern operators. This is `pwsh`-over-5.1, NOT PowerShell-over-bash: Git Bash / the Bash tool is
  equally fine. (The build wrapper is still documented above as `powershell.exe -File ../Tools/_Build.ps1`;
  migrating that invocation to `pwsh` is a verify-then-change follow-up.)
- **Narrate your work verbosely.** Before each search/read/build step, state the question the step answers, what you
  expect, and what the result actually told you. The owner follows along in real time; terse status lines hide the
  reasoning.
- **Trust but verify — EVERY claim, including the owner's.** A doc line, an owner aside, your own recollection, a
  memory entry — all are hypotheses to CONFIRM against ground truth (the live code, the actual data, the running
  game) before you build on them. Say what you verified against.
- **⛔ DO NOT GUESS, DO NOT INFER, DO NOT ASSUME — an assumption IS a shortcut**
  ([DEC-no-guessing](docs/architecture/decisions.md#dec-no-guessing)). Never fill a gap with inference; do not
  assume an earlier verification still holds; and never infer an ANSWER or PERMISSION the owner has not given —
  "keep going" authorizes work, never a vote on an open decision. **A question you have posed to the owner is a HARD
  STOP: do not act past it until they answer.** At a gap the only moves are VERIFY or ASK. Every minion you spawn
  must be told this rule explicitly.
- **⛔ THE THREE DRIFT DETECTORS — mechanical checks, not vigilance. Each caught a real defect; each was
  BELIEVED CONFORMING at the time.**
  1. **A change that leaves every consumer untouched is the TELL, not the win.** "No blast radius" means the
     ENGINE bent to fit the old shape instead of the consumers being rewired — the half-migration reflex
     ([DEC-fixedpoint-x100](docs/architecture/decisions.md#dec-fixedpoint-x100),
     [DEC-cy-not-fixed](docs/architecture/decisions.md#dec-cy-not-fixed)). Blast radius is the SIGNAL that the cut
     reached. *(Caught: a cascade accessor reducing ÷100 internally so nine readers — incl. a `Cy*` binding —
     would not have to change.)*
  2. **A surviving FUDGE FACTOR means two operands are on different scales**, i.e. the conversion landed in the
     wrong place. When a cluster converts correctly the magic constants DISAPPEAR and the mixing sites need no
     edit at all. If you find yourself ADDING a compensating multiplier, stop and redraw the cluster boundary —
     do not push through. *(Caught: an AI ratio needing `×10000` because one operand was ×100 and the other human.)*
     ⚑ **AND IT NAMES THE CULPRIT, not just the symptom (owner): a fudge factor is USUALLY LEGACY BEING FORCED
     INTO THE NEW SURFACE AT AN AI CALL SITE.** The constant is what an unmigrated consumer needs in order to keep
     reading a new-surface value in its old shape — so it marks the CALL SITE as the thing still on legacy, not
     the value. ⛔ **Therefore the question is never "where does the conversion belong?" but "WHICH SIDE OF THIS IS
     STILL LEGACY?"** — re-point that side and the constant deletes itself. Adding the multiplier instead is how
     the AI half gets left rotting while the surface underneath it moves
     ([DEC-new-getter-surface](docs/architecture/decisions.md#dec-new-getter-surface): reusing a legacy getter IS
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
  it is the sentence that is wrong: fix it in the same change ([DEC-docs-current-truth](docs/architecture/decisions.md#dec-docs-current-truth)).
- **"ALL" means EXHAUSTIVE — locust mode, never judgment-filtered**
  ([DEC-all-means-all](docs/architecture/decisions.md#dec-all-means-all)). Enumerate EVERY item mechanically,
  recursing every aggregate to its leaf sources; a single agent's "do I need this?" is systematically biased toward
  dropping items. Go exhaustive **immediately** (partial passes are slower), and prove completeness
  **adversarially** (a second pass that ASSUMES incompleteness), never by self-assertion. Scoping is never a reason
  to skip a source — promote a private getter to public; there is zero sensitive data in a game mod.
- **⛔ THE KRAKEN RULE** ([DEC-kraken](docs/architecture/decisions.md#dec-kraken)) — the overall ruling the rigor
  rules serve: this codebase is *"legendary in its lack of standard, coherence, or any reasonable consideration to
  common sense."* In a coherent codebase a small assumption is usually harmless; here it is the move that gets your
  ship eaten. **Maximal rigor is the STANDING default** until the owner explicitly declares otherwise.
- **⛔ "FAST IS SLOW, SLOW IS FAST"** ([DEC-fast-is-slow](docs/architecture/decisions.md#dec-fast-is-slow-slow-is-fast)) — read
  each subsystem doc IN FULL before acting. Skimming, or grepping a keyword instead of reading end-to-end,
  routinely costs far more downstream than the minutes it "saves."
- **Nothing here is ever "just a one-liner" — expect hidden consequences.** Non-obvious cross-cutting wiring is the
  norm (combat math shared across UI/AI/resolution, name-tagged save serialization, dual Python-enum registration,
  unity-build include exposure, graphics paths that run pre-init, the dead `.vcxproj`). Before any change, read the
  relevant docs, trace every caller/consumer of what you touch, and assume a small edit has ripples until checked.
- **Surface sprawl early; don't make the owner restate**
  ([DEC-WF-surface-sprawl](docs/architecture/decisions.md#dec-wf-surface-sprawl)). When a change balloons or you are
  patching pieces of something whose target STRUCTURE is undefined, STOP and tell the owner — never overcompensate
  with more partial fixes. Capture a ruling durably the first time it is given; do the proper cleanup once instead
  of churning. The owner makes the structure call; your job is to surface the risk and options efficiently and not
  re-litigate settled ones.

### Design

- **Build the proper structure ONCE — no transitional tech debt**
  ([DEC-proper-once](docs/architecture/decisions.md#dec-proper-once)). Reject transitional shims that exist only to
  defer the real design; when the right design needs prerequisite work, do the prerequisite and build the real
  thing. **Corollary — ISOLATE COMPONENTS:** prefer clean, interface-bounded components with isolated surfaces so
  each can be built and reasoned about once, properly.
- **⛔ LEAVE NO EVIDENCE OF THE ABANDONED PATH — and an `#ifdef` ATTIC IS THAT EVIDENCE (owner)**
  ([DEC-no-rollerskate-evidence](docs/architecture/decisions.md#dec-no-rollerskate-evidence)). Dead and
  commented-out code, superseded dual surfaces, transitional shims and `was X` / `(formerly …)` trails are all
  REMOVED, in code as well as docs. **A `#ifdef`-guarded block whose symbol is DEFINED NOWHERE is the same
  thing wearing a preprocessor costume: *"ifdef sections come from a time when people did not understand
  git"* (owner)** — an old implementation parked beside the live one so it could be switched back, which is
  what version control is for. ⚑ **The first test is mechanical: is the guard symbol defined anywhere in
  `Sources/` or `fbuild.bff`?** If not, the block never compiles — but that only makes it a CANDIDATE.
  **⛔ THE CANDIDATE SET IS TWO DIFFERENT POPULATIONS AND THEY GET OPPOSITE TREATMENT — collapsing them is
  how a sweep deletes a wanted feature.** The discriminator is mechanical too: **does a COMMENTED-OUT
  `#define` of the guard exist?**
  - **A commented-out `#define` ⇒ a deliberate OFF-SWITCH** — a feature or a diagnostic somebody can flip on,
    switched off for a REASON. It is un-killed forward intent and it STAYS
    ([DEC-keep-unkilled-ideas](docs/architecture/decisions.md#dec-keep-unkilled-ideas)); the disposition is the
    OWNER's. ⚠ And the reason it is off is exactly what stops the NEXT sweep eating it, so record that reason
    in the subsystem's reference doc in the same pass — the worked case is `ENABLE_FOGWAR_DECAY`
    ([special-systems.md](docs/reference/special-systems.md)), a recent feature that is off because it broke
    HOTSEAT, and which the mechanical test flags identically to an abandoned alternate.
  - **No `#define` anywhere, not even commented ⇒ no switch ever existed** ⇒ an abandoned alternate parked
    beside the live one. THAT is the attic, and it is deleted — git is the archaeology.
  - **⛔ AND THE INVERSE IS EQUALLY DEAD, AND WORSE: a guard defined UNCONDITIONALLY.** A `/D` in
    `fbuild.bff`'s common defines that no config ever omits means the `#else` half has never compiled — the
    same attic, wearing the opposite costume, and the mechanical test above sails straight past it because the
    symbol *is* defined. ⚑ **It is worse because the surviving branch keeps a companion that JUSTIFIES ITS
    SHAPE:** the live arm reads as one mode of a switch rather than as plain code, so whatever it does looks
    deliberate and nobody audits it. *(Measured: `_MOD_FRACTRADE` was on for fifteen years; the `÷100` inside
    its live arm read as "the fractional mode" and was actually a scale reduce sitting inside an aggregation,
    flooring every city's trade yield to a whole unit before the percent stack multiplied it —
    [fixed-point-and-scales.md §4c-bis](docs/specs/curators/fixed-point-and-scales.md).)*
    ⇒ **The test is whether the guard can VARY, not whether it is defined.** If no config turns it off, delete
    the guard and both `#else` halves, then re-read the survivor as the plain arithmetic it is.
    ⚠ **The FALSE POSITIVE the test has is a guard something ELSE defines**, and it is
  the exception to know: **`__INTELLISENSE__` is defined by VS Code's IntelliSense parser** (`CvGameCoreDLL.h`),
  so being absent from `Sources/`+`fbuild.bff` is its NORMAL state and says nothing about it — it stays. Same
  for the compiler's own target predefines (`_M_IX86`, `WIN32`, `_DEBUG`), the resource editor's
  (`APSTUDIO_INVOKED`) and anything inside a vendored third-party file. ⇒ Read the test as *"defined by
  NOBODY"*, not *"defined by not-us"*. ⛔ The cost of leaving one is the usual one: it holds the NAMES of things that were removed, so the
  next agent finds them and re-treads the very thing that was killed — and being preprocessor-skipped, it is
  invisible to the compiler census that would otherwise name it.
- **ARCHITECTURAL NORTH-STAR — Clean Architecture + interface-based contracts.** Dependency inversion, isolated
  layers, composition over the inherited Civ4 god-classes. The full compass is
  [north-star.md](docs/architecture/north-star.md); the concrete C++03 shape (pure-virtual interfaces, MI as
  `implements`, poor-man's DI at a composition root, graft onto DLL-derived classes never EXE-bound bases) is
  [patterns.md](docs/architecture/patterns.md) / [DEC-interface-contracts](docs/architecture/decisions.md#dec-interface-contracts).
- **⛔ "Deferred" is BANNED — a deferred / parked / blocked / not-yet-landed / post-cutover / "later" / "acceptable for now" / TODO / pending item is a FAILURE to fix, not a backlog entry**
  ([DEC-no-deferred](docs/architecture/decisions.md#dec-no-deferred)). It is the word agents hide behind to skip hard
  work hoping it won't have impact — and the measured result is a half-built branch whose load-bearing minority is
  quietly missing while it looks nearly done. The general form of
  [DEC-data-first](docs/architecture/decisions.md#dec-data-first), extended from data to ALL work. The ONLY exceptions
  are owner-ruled PERMANENT design carve-outs, recorded as such (the golden-age yield-effect member-mirror;
  Python-authoritative gameplay staying Python).
- **"Minimal, local changes" bounds the SIZE of an edit, NOT the SCOPE of the work.** A targeted fix inside a
  tightly-coupled legacy core file stays minimal — don't sprawl it or gratuitously refactor around it. But this is
  no brake on deliberate structural rework (the cascade, the docs rebuild, dissolving the `Cv*AI` god-classes),
  which is large by design and answers to [DEC-proper-once](docs/architecture/decisions.md#dec-proper-once).
- **⛔ AGENTS ARE BANNED FROM BUILDING ON THE EXISTING PYTHON BINDINGS (owner ruling).** Do NOT treat a `Cy*`
  binding as a destination, a contract to satisfy, or a place to park a conversion — *"every time you try, you start
  shoehorning."* Reaching for an existing binding is what makes the ENGINE bend to fit Python instead of the boundary
  being redesigned around the cascade/JSON model ([DEC-cy-not-fixed](docs/architecture/decisions.md#dec-cy-not-fixed):
  that `.def` surface is explicitly NOT a fixed contract). The tell is a change that leaves every Python consumer
  untouched — that is the half-migration, not a clean cut.
  **What to do INSTEAD (owner): build a NEW Python surface and COMPLETELY DISCONNECT the old one.** Not a widened
  binding, not a compatibility shim beside it, not a parallel that both remain live — the replacement is a clean
  surface shaped by the cascade/JSON model, and the legacy `Cy*` surface is cut away rather than left breathing
  ([DEC-no-legacy-masking](docs/architecture/decisions.md#dec-no-legacy-masking): a legacy path left alive masks the
  hole; [DEC-proper-once](docs/architecture/decisions.md#dec-proper-once): no transitional shim). Python-authoritative
  *gameplay* still stays Python — this is about the INFO/state binding surface, not about pulling gameplay into the DLL.
- **Import Info headers DIRECTLY; do not lean on the `CvInfos.h` umbrella.** New/edited code includes the specific
  header it needs; the umbrella is flagged for retirement. **⛔ Retiring it is a dedicated, hand-careful pass, NOT a
  session-tail script run** ([DEC-proper-once](docs/architecture/decisions.md#dec-proper-once)) — a scripted attempt
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
  LICENSE (owner).** *"Comments that contradict what spec says is rollerskating license, and license to tilt
  me... again."* The spec states a rule ONCE, for the whole engine; a comment repeating it at a call site is a
  second copy, second copies DRIFT, and a drifted copy does not merely go stale — it **authorizes** the next
  agent to act against the spec while believing they are conforming. That is strictly worse than no comment,
  and it is the same delete-don't-duplicate discipline
  [DEC-docs-current-truth](docs/architecture/decisions.md#dec-docs-current-truth) applies to docs and
  [DEC-no-rollerskate-evidence](docs/architecture/decisions.md#dec-no-rollerskate-evidence) applies to dead
  names, now applied to CODE.
  ⚑ **The worked class is SCALE.** The model is universal and stated once — integers carry two decimals, ×100
  throughout, reduced only at a read edge
  ([DEC-fixedpoint-x100](docs/architecture/decisions.md#dec-fixedpoint-x100)) — so a comment ASSERTING a
  value's scale (*"this is ×100"*, *"this is ×1"*, *"the derivable half reduces here"*) says nothing the spec
  does not, and says it wrongly the moment the code moves. ⚠ **Measured: two such comments were false in the
  same file family.** One claimed a reduce that the code did not perform, beside a leg that genuinely reduced —
  the surviving-fudge-factor shape wearing a reassuring comment; the other called a ×100 group read *"reduced
  to the whole yields"*. Each read as authoritative and each was wrong.
  ⇒ **So: do not annotate scales, and DELETE a scale annotation you pass.** What a comment may still carry is
  the thing the spec cannot know — WHY this site is an edge, or why a value is genuinely exceptional — never a
  restatement of the rule itself.
  ⚖ **THE SURVIVING ONES ARE NOT A BACKLOG, AND THE RULE IS WHAT MAKES THAT SAFE (owner): *"we now have the
  rule in place that overrules comments that contradict spec, that should do; we clean out dumb comments as we
  go along."*** The rule OUTRANKS them, so a contradicting comment carries no authority to act on and its
  presence costs nothing on its own — what cost something was having no rule to overrule it with. ⛔ Therefore
  do NOT plan, size or schedule a sweep, and do not record a count of them as a worklist entry: they are
  deleted opportunistically, by whoever is already editing that code.
  ⚑ Same disposition as the legacy logging surface ([observability.md](docs/reference/observability.md): *"not
  a cleanup backlog — a surface you must not RELY on"*), and for the same reason — the danger was ever only in
  TRUSTING it, which the rule now forecloses. ⚠ [DEC-no-deferred](docs/architecture/decisions.md#dec-no-deferred)
  does not reach this: nothing is being parked, the defect is closed by the rule and the residue is cosmetic.
  ⛔ A blanket regex purge is the wrong instrument anyway — it eats the comments that carry the EDGE reasoning,
  which is exactly what a comment is still FOR.

- **KEEP THE SPECS CURRENT as the model changes — proactively, in the SAME change.** A stale spec is worse than
  none — the next agent trusts it and builds on the wrong shape. The proactive twin of fix-docs-now.
- **Docs state CURRENT TRUTH only — no dated rulings, no supersession trails, no session logs.** A doc never
  narrates its own history ("supersedes the earlier X", "(owner ruling YYYY-MM-DD)", parity numbers, landed/reverted
  chronicles): it states what IS. Anything outdated is DELETED, not annotated — git history is the archaeology, and
  [superseded-ideas.md](docs/architecture/superseded-ideas.md) is the only tombstone registry (one line per dead
  approach that carries revival risk). Migration/status chronicles belong in `docs/plans/`, never in specs.
- **⛔ SPEC + a SHORT BULLETED TODO — never a TODO LIST *inside* a doc, and never status woven through prose
  (owner).** Status claims DRIFT — that is their nature, not a discipline failure — so the more of them a doc
  carries, the faster the whole doc rots and the more confidently it misleads. **A doc is therefore one of two
  things, never both:** a **SPEC** (the design: timeless, what the thing IS and must be) or a **TODO** (a short
  bulleted list of what is not done yet). ⛔ Do NOT write "LANDED" / "✅ DONE" / "PARTLY LANDED" / build-status
  tables / per-item completion ledgers into a doc: an item that is DONE is simply DELETED from the todo list, and
  anything durable it established (a ruling, a design constraint, a hard-won fact) moves into the SPEC where it
  belongs. **The measure of a todo list is what is LEFT, never a record of what was achieved** — git history is
  the record of work done ([DEC-docs-current-truth](docs/architecture/decisions.md#dec-docs-current-truth): the
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
- **ALWAYS RECURATE WHEN A DECISION LANDS** ([DEC-recurate-on-decision](docs/architecture/decisions.md#dec-recurate-on-decision)) —
  any ruling that changes what the data model carries triggers the curator update + regen in the SAME work item.
- **Docs-only changes go to `main` ONLY when the owner explicitly authorizes it**; default is the working branch. A
  branch-coupled doc (e.g. cascade specs on `json-data-migration`) belongs with that work and commits on the branch.
  The canonical straight-to-`main` docs are the INDEXES (`indexes/DESPAIR_INDEX.*`, `REALISM_INDEX.*`, the
  COMPLEXITY catalog) — they pertain to no single branch. Nothing gameplay-affecting ever rides in a docs commit.
- **Verify the current branch immediately before every commit**
  ([DEC-WF-branch-safety](docs/architecture/decisions.md#dec-wf-branch-safety)) — run `git branch --show-current` in
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
- **Confirm behaviour before opening a PR:** a behaviour/feature change needs a real in-game playtest
  (`FinalRelease` + `rebuild deploy`), not just a green Assert build.
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
- **⛔ `agentstart.bat` is FIRE-AND-FORGET, and MUST be launched from PowerShell — NEVER from the Bash tool.**
  Invoking it through Bash/Git-Bash (`cmd //c agentstart.bat`, backgrounding it inside a shell script, …) **mangles
  the paths and the game does not start** — while the shell still reports success, so it reads as launched and the
  agent then polls a surface that will never come up. Use
  `Start-Process -FilePath 'C:\code\s2s\s2s\agentstart.bat' -WorkingDirectory 'C:\code\s2s\s2s'`, and do **not** wait
  on or block the call. Confirm the launch ONLY by polling the HTTP surface (`/` → `hello world`) — never by the
  launcher's exit code. *(Repeat offence — agents keep re-learning this one the hard way.)*
- **Keep quirky/intermediate commits — do NOT push to squash them (owner taste).** Mention squashing exists at most
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
subsystems behave today), **`docs/architecture/`** (the decisions ledger, `north-star`, `patterns`,
`superseded-ideas`), **`docs/plans/`** (`structural-cleanup/` the cutover bulldozer + `parked/` un-killed intent).
The hosted DESPAIR/REALISM/COMPLEXITY catalogs live at **`indexes/`** (repo root, served via Pages).

- How existing code behaves → `docs/reference/`.
- A change or initiative you intend to make (plan, scope, rollout, removal) → `docs/plans/`.
- A superseded/killed dev idea → one line in `docs/architecture/superseded-ideas.md` (what it was, why
  it's dead, what replaced it) so it isn't revived; don't carry the stale copy in the live set.
- Cross-cutting, must-not-rediscover facts → "Key Subsystem Knowledge" above (or the nearest `AGENTS.md`).
- The mod's front-door / build-pipeline readme (the code repo's mirror) → `docs/MOD-README.md`.
- A newly-found bug of exceptional absurdity may *additionally* earn an entry in
  [`indexes/DESPAIR_INDEX.md`](indexes/DESPAIR_INDEX.md) (owner-sanctioned, lighthearted,
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

**⛔ HARD RULE — every owner ruling goes into the repo docs IMMEDIATELY, unprompted**
([DEC-WF-rulings-to-repo](docs/architecture/decisions.md#dec-wf-rulings-to-repo)).
When the owner makes a ruling in conversation — a design decision, a workflow rule, a
relaxed or tightened constraint, a "from now on do X" — writing it to assistant memory is
NOT enough and never the end state. In the SAME work item (same commit/PR, without being
asked) write it into the right repo home: workflow/convention rulings → this file's
Conventions; subsystem/design rulings → the relevant `docs/` page. Treat "saved to
memory only" as an unfinished task.

**The discoverability half of that rule: the DECISIONS LEDGER ([`docs/architecture/decisions.md`](docs/architecture/decisions.md)).**
Cross-cutting rulings kept getting re-stated doc-after-doc; the ledger breaks the loop: it is an **INDEX, not a
re-statement** — one stable ID per ruling (`DEC-<slug>`), a one-line summary, and a pointer to the authoritative
home. **Operational rules:** (1) **before adding any cross-cutting ruling anywhere, grep the ledger's ID table
first**; (2) capture a cross-cutting ruling by adding its line to the ledger and recording the full text in its ONE
home, **never** by restating it in a second doc; (3) a doc that needs a ledgered ruling **links `[DEC-id]`**, it
does not re-articulate it.

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
