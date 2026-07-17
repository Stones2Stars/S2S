<!-- ═══════════════════════════════════════════════════════════════════════════ -->

> # ⛔ STOP — READ THIS BEFORE YOU TOUCH ANYTHING ⛔
>
> ## HOURS WASTED ON ROLLERSKATING: **191** &nbsp;·&nbsp; *and counting*
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
> **⛔ HARD RULE — THE TREE DELIBERATELY DOES NOT COMPILE ([DEC-red-ratchet](docs/architecture/decisions.md#dec-red-ratchet)).
> NEVER "fix" it by restoring old Infos.** The 23 XML engine info classes (`CvBuildingInfo`, `CvUnitInfo`,
> `CvTechInfo`, … — 46 files) were moved to `SourceArchive/Infos/` ON PURPOSE: parity is past, the JSON-fed
> `CvJson<X>Info` pocos are their replacements, and the missing classes are a RATCHET — during the cutover,
> consecutive agents "finished" by quietly wiring old Infos back in wherever the JsonInfos didn't pan out, so the
> fallback was removed. **Never restore anything from `SourceArchive/`, never re-add a `CvXInfo` class, never treat
> the RED build as a defect.** The ONLY road to green: build the JsonInfo structure, wire it up, and wire up/replace
> ALL the getters (the engine/AI/UI consumer surface onto the JSON-fed infos).
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
  - **Which config for in-game testing:** for ordinary interactive testing — exercising a feature, pulling state
    from the HTTP endpoints, watching `/events` — a normal **`Release`** build suffices and is far faster than
    `FinalRelease` (a clean `FinalRelease` is a ~7-minute full rebuild). **Reserve `FinalRelease` for turn-lag /
    performance hunting**, where its optimizations are the thing under test. `Assert` stays the quick compile-check;
    `Release`/`FinalRelease` are for actually running.
- **Verbs (composable, in order):** `clean`, `build` (incremental), `rebuild` (clean+build), `deploy` (xcopy DLL/PDB into `Assets/`).
- **Quick compile check after an edit:** `Assert build` from `Sources/`.
  Incremental is ~30s; a clean rebuild is several minutes (~25 unity batches × ~30s).
- The `Tools/MakeDLL*.bat` shortcuts (`MakeDLLAssert.bat`, `MakeDLLRelease.bat`, …)
  always `rebuild deploy` — full clean+rebuild+copy. Don't use them for an
  iterative compile-check loop.
- Full dev bootstrap: `DevSetup.bat`. CI flow: `appveyor.yml`.

### Validation

- XML: `Tools/XmlValidator.exe -a`.
- Python callbacks: `Tools/XMLTools/verify-python-callbacks.py`.

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
   only re-paths EXISTING items (it never adds new files), and no `python` runner is installed on the dev box. The
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
  layer. Each `CvUnit` worker independently calls `CvUnitAI` evaluation methods
  (`AI_irrigateTerritory`, `AI_improveBonus`, `AI_fortTerritory`, `AI_findBestFort`),
  re-scoring the same plots/builds as other workers of the same owner. This is
  BTS-era behavior carried through C2C. Future TODO: a `CvPlayer`-level coordination
  layer (potential homes: `CvPlayerAI`, `CvWorkerService`, contract-broker).
- **`CvWorkerAI` is the per-player cache + claim ledger** (one per `CvPlayer`,
  turn-scoped). Holds a BonusEval cache keyed on `(BonusEvalSource, plotIdx, unitType)`
  and a claim ledger (`plotIdx -> unitId`). The ledger is **only** used by
  `CvWorkerService::ImproveBonus`; the legacy `AI_improveBonus`/`AI_bestCityBuild`
  paths already dedup via `AI_plotTargetMissionAIs(plot, MISSIONAI_BUILD, group) < iMaxWorkers`
  (the canonical cross-worker dedup). Don't double up — adding the ledger to the
  legacy path forces `iMaxWorkers=1` and breaks team builds.
- **Observability:** `CvWorkerAI::improveBonus` emits `[WAI/*]`-tagged lines into
  `BuildEvaluation.log`, gated by `gPlayerLogLevel` (1=headline, 2=per-plot, 3=per-candidate).
  The class doc comment in `Sources/CvWorkerAI.h` is the authoritative tag reference.

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

- **⛔ The running game holds its `.log` files OPEN — NEVER try to live-read them (this trips agents EVERY time).**
  While the game is running, `Documents/My Games/Beyond The Sword/Logs/*.log` (incl. `Cascade.log`,
  `BuildEvaluation.log`, …) are held open by the process, so tailing/reading them mid-session gives
  stale/empty/partial results — do **not** do it, and do **not** infer "logging is off" from a quiet log file. The
  live reads are: **(1) the `/events` SSE stream** (`curl -sN http://127.0.0.1:7227/events`) — the gated per-turn
  `[TAG]` lines — which **burst at the TOP of `doTurn`, so you must be CONNECTED BEFORE the turn ticks**
  (connect-then-end-turn); and **(2) the on-demand mailbox-snapshot endpoints `/state/*` + `/computed/*`**, which
  compute a game-thread snapshot via the single-slot mailbox and depend on no log file or gate — the most reliable
  read (see `docs/specs/http-endpoints.md`). Gates are separate and INDEPENDENT: `gPlayerLogLevel` gates the per-domain `.log` files;
  the `/events` stream is its OWN spine consumer — spine DOMAIN facts stream unconditionally, DIAGNOSTIC/TRACE
  at `gStreamLogLevel` — so a line can be in either surface without the other. When in doubt about a
  magnitude/state, hit the endpoint, not the log.
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
- **⛔ "FAST IS SLOW, SLOW IS FAST"** ([DEC-fast-is-slow](docs/architecture/decisions.md#dec-fast-is-slow)) — read
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
- **Import Info headers DIRECTLY; do not lean on the `CvInfos.h` umbrella.** New/edited code includes the specific
  header it needs; the umbrella is flagged for retirement.
- Preserve save compatibility by default; for intentional breaks, coordinate and mark with `@SAVEBREAK`. See
  `Notes for the next breaking of save game compatability cycle.txt`.
- **FRONT-LOAD save-breaking reworks NOW.** S2S is its own project (only the closed Firaxis EXE binds; inherited C2C
  conventions are not constraints). The playerbase is smallest today, so the cost of breaking saves only rises —
  break saves now if ever. C2C→S2S save compat is an explicit NON-GOAL; never constrain a design to keep it.
- Do not modernize or replace the build chain/toolchain.

### Docs

- **When documentation is lacking or wrong, FIX IT NOW — it is part of the SAME work item**, never "noted for the
  next agent." A doc gap that bit you will bite the next contributor; close it in the same change.
- **KEEP THE SPECS CURRENT as the model changes — proactively, in the SAME change.** A stale spec is worse than
  none — the next agent trusts it and builds on the wrong shape. The proactive twin of fix-docs-now.
- **Docs state CURRENT TRUTH only — no dated rulings, no supersession trails, no session logs.** A doc never
  narrates its own history ("supersedes the earlier X", "(owner ruling YYYY-MM-DD)", parity numbers, landed/reverted
  chronicles): it states what IS. Anything outdated is DELETED, not annotated — git history is the archaeology, and
  [superseded-ideas.md](docs/architecture/superseded-ideas.md) is the only tombstone registry (one line per dead
  approach that carries revival risk). Migration/status chronicles belong in `docs/plans/`, never in specs.
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
