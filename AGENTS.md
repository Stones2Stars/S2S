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
> They are stale IDE-display artifacts that do **not** drive the build and are **not** kept in sync. Do **not** trust them for the
> compiler, platform toolset, C++ standard/language version, preprocessor defines, include paths, optimization flags, or anything else.
> Any build fact comes from **FastBuild only**: `Sources/fbuild.bff` (+ `Tools/_Build.ps1`). Treating the `.vcxproj` as truth has already
> caused a wrong conclusion (its `PlatformToolset` says `v142`, which is FALSE — see the toolchain note below). When unsure, read `fbuild.bff`.
>
> **Actual toolchain (from `fbuild.bff`):** the vendored **Microsoft Visual C++ Toolkit 2003 = MSVC 7.1 (VC2003)** compiler/linker
> (`Build/deps/...`), **Python 2.4**, **Boost 1.32 / 1.55** (why BOTH Boosts coexist + why the whole stack is frozen by the
> closed `.exe`: [`docs/reference/engine.md`](docs/reference/engine.md)). So the DLL is genuinely **C++03, 32-bit/x86** — *no* `std::thread`, *no* OpenMP,
> *no* C++11+. This is a hard compiler limit (the toolchain is locked to stay ABI/STL-compatible with the closed VC7.1 game `.exe`), **not**
> a style convention. In-process threading means raw Win32 only. Do not modernize or replace the build chain/toolchain.

The build is driven by **`Tools/_Build.ps1`** (a FastBuild wrapper). Invoke it
**from the `Sources/` directory**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb> [<verb> ...]
```

- **Configs:** `Assert`, `Debug`, `Release`, `FinalRelease`, `Profile`, `ProfileExtra`.
  Output lands in `Build/<Config>/CvGameCoreDLL.dll` (+ `.pdb`).
  - **Which config for in-game testing (owner ruling 2026-06-19):** for ordinary interactive testing — exercising a
    feature, pulling an HTTP `/diagnostic` dump, watching `/events` — a normal **`Release`** build suffices and is
    far faster than `FinalRelease` (a clean `FinalRelease` is a ~7-minute full rebuild). **Reserve `FinalRelease`
    for turn-lag / performance hunting**, where its optimizations are the thing under test ("we are not here for max
    performance… yet"). `Assert` stays the quick compile-check; `Release`/`FinalRelease` are for actually running.
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
`.vcxproj` files are IDE-only and do NOT drive the build. **As of 2026-06-19 fbuild
RECURSIVELY globs** every `.cpp` under `$SOURCE_DIR$` (the Unity + the `Cy*` ObjectList both
`*PathRecurse = true`, excluding `include`/`lib`/`.vs`/`.vscode`/`nbproject`), so a new
`Sources/<NewDir>/` is compiled **automatically — no `.UnityInputPath` edit needed**. When you
add a subdir/files:

1. Put the `.cpp`/`.h` under `Sources/<NewDir>/`. Cross-module `#include`s are **path-qualified**
   root-relative (`"<Dir>/Header.h"`, resolved via `/I"$SOURCE_DIR$"`); same-folder includes stay
   bare (MSVC searches the including file's own dir first). The shared layers `Infos`/`Cascade` are
   on `/I` so their headers are included bare; PCH glue (`CvGameCoreDLL.h`, etc.) stays at root.
2. (IDE display only) regenerate the project with `python Tools/regen_project.py` (rebuilds
   `S2S.vcxproj` + `S2S.vcxproj.filters` from disk), or add the entries by hand. **⚠ CURRENTLY BROKEN
   (found 2026-07-03):** the script was the one-time C2C→S2S rename migration — it reads the deleted
   `C2C (VS2019).vcxproj` as input and only re-paths EXISTING items (it never adds new files), and no
   `python` runner is installed on the dev box. The `S2S.vcxproj` listing is wholesale-stale (lists the
   purged `CvScopedAccumulator`; none of the current `Cascade/` files). Since the files are DEAD for
   build purposes this blocks nothing — a working regen (read the S2S files as input, add-from-disk) is
   a parked standalone fix; don't piecemeal-patch entries into the stale listing.
3. With recursive globbing, an `LNK2001: unresolved external symbol` now means a genuinely
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
  demand picture — owner ruling: overdefended > underdefended, join eagerly/release
  reluctantly via `GARRISON_RELEASE_MARGIN_PERCENT`). Details:
  `Sources/AI/CvUnitAI.cpp` (garrison-tiers; the dedicated `CvUnitAI.md` reference doc was retired in the
  docs rebuild — read the behaviour from the source).

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
- **Generalization — pseudo-progress terminals (owner ruling 2026-06-12, #410): a
  free/cheap action that cannot change the strategic state must not satisfy the decision
  loop.** A cascade step that "does something" (a ranged potshot, a no-op heal, an
  advertise-that-returns-true) terminates the unit's decision looking like progress, so
  the real commit-or-withdraw decision is never reached — AI stacks camped cities for
  eras feeding near-zero-damage ranged strikes as the "yay we did something at least"
  clause. Standoff/maintenance actions are phase steps inside a plan with an abort rule,
  never turn-satisfying terminals in their own right.

### Graphics / map generation

- Any plot-graphics mutation must be guarded by **`GC.IsGraphicsInitialized()`**.
  During a NEW game, world generation (`addGameElements` → rivers/features/bonuses)
  runs *before* the render engine landscape exists, so symbol updaters that fire
  against non-existent engine plots crash. Loading a save skips generation, so it's
  safe — hence the classic "crashes with graphics paging off, fine with it on"
  signature points to a graphics path running pre-init, not a logic bug. Established
  guard sites: `CvPlot.cpp` `setPlotType` graphics block, `setLayoutDirty`,
  `shouldHaveGraphics`; `CvMap::setupGraphical`.

### ⛔ THE NO-GUESSING RULE — map everything, always (owner ruling 2026-06-19, HARD RULE)

**On the #428/#430 rework we do NOT guess. We MAP. Every claim about a value/divergence is grounded in the
total-observability surface — that is exactly what the Orwellian level of surveillance is FOR.** *"We do not guess,
we do not faff about, we map all the things, all the time."* Operationally, when a cascade value diverges from legacy:
do NOT hypothesise a cause and try a fix — instead EMIT the full legacy decomposition (every component/source of that
calc) via the dump (`/diagnostic/cityInput` etc.), and map the cascade's value by the SAME components, so the
divergence is attributed to a NAMED source with numbers, not a guess. If the data to attribute it isn't being
emitted, the FIRST step is to emit it (extend the dump), not to guess. The half-guessing back-and-forth (try a fix,
re-sweep, try another) is the anti-pattern this rule kills: build the complete map once, then every fix is grounded.
(This is the modifier-channel application of the map-before-delete / total-observability bar below.)

### Cascade observability — the total-observability ("Orwell") bar (#428/#430)

- **⛔ The running game holds its `.log` files OPEN — NEVER try to live-read them (owner ruling 2026-06-19; this trips
  agents EVERY time).** While the game is running, `Documents/My Games/Beyond The Sword/Logs/*.log` (incl. `Cascade.log`,
  `BuildEvaluation.log`, …) are held open by the process, so tailing/reading them mid-session is unreliable and gives
  stale/empty/partial results — do **not** do it, and do **not** infer "logging is off" from a quiet log file. The live
  reads are: **(1) the `/events` SSE stream** (`curl -sN http://127.0.0.1:7227/events`) — the gated per-turn `[TAG]`
  lines (the surviving AI-trace domains, e.g. `[STATE/game]`; ⚠ the cascade-shadow tags
  `[MODSHADOW]`/`[PLACEMENT]`/`[DORMANCY]`/`[READJSON]` were the PURGED prototype's and **no longer emit**) — which
  **burst at the TOP of `doTurn`, so you must be CONNECTED BEFORE the turn ticks** (connect-then-end-turn); and
  **(2) the on-demand mailbox-snapshot endpoints `/computed/*` + `/state/*`**, which compute a game-thread snapshot via
  the single-slot mailbox and **do NOT depend on `gPlayerLogLevel` or on any log file** — the most reliable read, no
  timing games (⚠ the old `/diagnostic/*` sweep endpoints were RETIRED → split into `/state`+`/computed`, see
  `docs/specs/http-endpoints.md`). Gates are separate: `gPlayerLogLevel ≥ 1` makes the per-turn `[TAG]` lines
  *generate* (to the per-domain `.log`), and the `/events` tee is a *further* gate — so a line can be in the log yet
  absent from `/events`. When in doubt about a magnitude/state, hit the endpoint, not the log.
- **The events + logging + diagnostics must make the running game FULLY surveilled (owner ruling 2026-06-18).** The
  bar: *map an accurate game state purely from the endpoints + `/events` + the gated logs — open the game, but never
  look at the SCREEN.* This is **non-negotiable and load-bearing**, not polish: it is the ONLY way to reliably REBUILD
  the state logic on the cascade + tally — you cannot safely delete a maintainer you cannot fully observe
  (map-before-delete). Every state behaviour (enabler-spec §14 H) gets a SHADOW that diffs the cascade's verdict
  against the live engine, turn over turn, until clean — *then* the legacy mechanism is deleted. Full ruling +
  rationale: `docs/specs/validation.md` §1 + `docs/specs/logging.md` §7.
  Live surface: `docs/specs/http-endpoints.md` (the live `/state` + `/computed` + `/events` surface) +
  `docs/reference/observability.md`. ⚠ The `/diagnostic/*` sweeps + the `[MODSHADOW]`/`[PLACEMENT]`/`[READJSON]`
  cascade-shadow tags are GONE (purged prototype); only `CvEventSpine` (AI-trace `[TAG]` logging) survives.
- **Delegate DATA-READING to the cheap `data-reader` sub-agent — never pull raw endpoint/log dumps into an expensive
  context (owner ruling 2026-06-18).** Reading the live surface at scale (a `placementSweep`/`sweep` dump is tens of KB;
  logs are larger) *"will nuke credits"* if the Opus/Sonnet orchestrator ingests the raw bytes. Use the read-only
  **`.claude/agents/data-reader.md`** (Haiku; Bash/Read/Grep only) to curl/grep, parse, AGGREGATE, and report back a
  COMPACT distilled summary (histograms, divergence cause-tags, anomalies) — the big tokens burn on Haiku, the
  orchestrator consumes a few lines. Always prefer it over reading endpoint/log output directly. **Fallback (owner
  2026-06-18): the reader must fail HONESTLY** (distinguish "surface DOWN" from "reader-error", never fabricate a
  clean summary); when it reports DOWN or returns junk, the orchestrator confirms with ONE cheap smoke-curl
  (`curl -s http://127.0.0.1:7227/` → `hello world`) before acting — an 11-byte check, not a data re-pull.

## Conventions

> ⛔ **"Conventions" here means HARD RULES — binding by default, NOT norms to weigh.** The section label is
> historical; the content is **law**. Every ruling below (and every `DEC-*` it links) is a rule you MUST obey unless
> the **owner explicitly relaxes it** — never on an agent's own judgement. Each was paid for by an agent before you
> charging ahead and getting its context **eaten by the kraken** (this codebase's standardless tangle), so reading
> and obeying them up front is *far cheaper* than re-learning by failure (**"fast is slow, slow is fast"**). The
> cross-cutting ones are indexed — with a front-and-centre binding banner — in the
> [decisions ledger](docs/architecture/decisions.md); treat any pull toward *"this is just guidance / probably
> fine / I'll infer it"* as the kraken's bait. *(Owner ruling 2026-06-22: the decisions are hard rules, not
> suggestions; if a decision doc leaves that in any way unclear, the phrasing is wrong and gets redone.)*

- **The real rule: `pwsh` == good, `powershell` == bad (owner ruling 2026-06-18).** `pwsh`
  (PowerShell 7) is the standard shell and the owner's primary shell; `powershell.exe` (Windows
  PowerShell 5.1) is the bad one — never invoke it, and never nest it inside a `pwsh` session. 5.1
  defaults to a non-UTF-8 console encoding (mangles emoji/Unicode on stdout — the exact bug that forced
  an ASCII-only digest until the hook moved to `pwsh`) and lacks the modern operators (`&&`/`||`,
  ternary, null-coalescing) `pwsh` provides. This is `pwsh`-over-5.1, NOT PowerShell-over-bash: Git Bash
  / the Bash tool is equally fine. (The build wrapper is still documented below as `powershell.exe -File
  ../Tools/_Build.ps1`; migrating that invocation to `pwsh` is a verify-then-change follow-up — confirm
  `_Build.ps1` runs clean under `pwsh` first, per "nothing is a one-liner".)
- **Narrate your work verbosely (owner ruling 2026-06-11).** While investigating or
  changing anything, explain *what you are looking for and why* as you go — before each
  search/read/build step, not just in a final summary. The owner follows along in real
  time; terse status lines ("checking X…") hide the reasoning. State the question each
  step is meant to answer, what you expect to find, and what the result actually told you.
- **Trust but verify — EVERY claim, including the owner's (owner ruling 2026-06-17).** Treat
  every stated fact — a doc line, a `.vcxproj` value, an owner aside ("units have no OR-techs"),
  your own recollection, a memory entry — as a starting hypothesis to CONFIRM against ground
  truth (the live code, the actual data, the running game), not as settled. The owner explicitly
  flags their own statements as "trust but verify, not a confirmation." A stale doc line loses to
  the code; a guess that "looks obvious" has repeatedly produced regressions (and a whole-database
  overwrite — running a superseded emitter — when a tool's real behaviour wasn't checked first). Verify before you build on it, and
  say what you verified it against.
- **⛔ DO NOT GUESS, DO NOT INFER, DO NOT ASSUME — an assumption IS a shortcut (owner ruling 2026-06-20).**
  This is the conduct twin of trust-but-verify and THE NO-GUESSING RULE: where those say CONFIRM every claim and MAP
  every divergence, this says NEVER fill a gap with inference. An assumption is a shortcut, and shortcuts "bring the
  entire demonclown circus of this codebase on your head." It applies to EVERYTHING, not just modifier divergences:
  do not infer a CAUSE you have not mapped; do not assume an earlier verification still holds (re-verify); and — the
  one that bit repeatedly — **do not infer an ANSWER or PERMISSION the owner has not given.** "Keep going" authorizes
  work; it is NEVER a vote on a specific open decision. **A question you have posed to the owner is a HARD STOP: do
  not act past it — not the "obviously correct" part, not the "settled content" part — until they answer.** When you
  reach a gap, the only moves are VERIFY it against ground truth, or ASK; inventing the answer is the banned move.
  Minions carry the same rule in their base brief (`.claude/agents/data-reader.md`), and every minion you spawn must
  be told it explicitly. Ledgered as [DEC-no-guessing](docs/architecture/decisions.md#dec-no-guessing).
- **"ALL" means EXHAUSTIVE — locust mode, not judgment-filtered (owner ruling 2026-06-20).** When the owner says do
  ALL of something (every source, every field, every call site), it means *"run over the codebase like locusts in a
  cornfield"*: enumerate EVERY item mechanically, **recursing into every aggregate down to its leaf sources**, and
  emit/handle each — NEVER filtering by a judgment call ("is this needed / dead / python-only / private / probably
  fine"). **That judgment IS the bug:** a single agent's "do I need this?" is *systematically biased toward dropping
  items*, so a self-certified "exhaustive" pass is not exhaustive — a careful solo pass on the diagnostic dump still
  missed **77** sources, found only by a mechanical recurse-everything plus an **adversarial re-check** (a second
  pass that ASSUMES incompleteness and hunts for a miss; in this codebase, fan it out — one minion per channel/area).
  Two operational consequences: **(1)** go exhaustive **immediately** — partial passes / per-item asking / "do this
  subsection first" is the *"untold hours of infinite and endless wrangling"* anti-pattern the owner called out and
  is far slower than the sweep; **(2)** prove completeness **adversarially**, never by self-assertion. *Why it is
  load-bearing:* on the live-shadow parity path (StoneBase validates against the live engine) a single un-emitted source hides
  inside an aggregate → the divergence is unattributable → the guess/despair spiral. (Scoping is never a reason to
  skip a source: promote a private getter to public — there is zero sensitive data in a game mod.) Ledgered as
  [DEC-all-means-all](docs/architecture/decisions.md#dec-all-means-all).
- **⛔ THE KRAKEN RULE — the OVERALL ruling these all serve (owner ruling 2026-06-20).** Skipping something,
  assuming something, guessing something, taking a shortcut, or in general **"perceived laziness" is the cardinal
  sin here, and the consequence is literal: the despair index.** *Why, stated plainly by the owner:* "this codebase
  is the kraken that will eat your ship, spit you out and crush you. It is **legendary in its lack of standard,
  coherence, or any reasonable consideration to common sense.** So we act accordingly." That is the WHY behind every
  rigor rule above — [[DEC-no-guessing]] (assumption = shortcut), [[DEC-all-means-all]] (more is always better than
  less), "nothing is ever just a one-liner", the total-observability ("Orwell") bar. In a coherent
  codebase a small assumption is usually harmless; in THIS one it is the move that gets your ship eaten, because there
  is no underlying standard to make the assumption safe. So the operating posture is **maximal rigor by default**:
  verify everything, enumerate exhaustively, take zero shortcuts, and treat any pull toward "this is probably fine /
  I don't need that / good enough" as the kraken's bait. **This is the STANDING default, not a phase of this task
  (owner ruling 2026-06-20):** the *earliest* the absolute completeness could relax is **AFTER a complete refactor
  has literally dropped ~half the existing lines** — and per the owner, *"honestly, maybe not even then."* Until the
  owner **explicitly declares otherwise**, maximal rigor stands by default. Ledgered as [DEC-kraken](docs/architecture/decisions.md#dec-kraken).
- **When documentation is lacking or wrong, FIX IT NOW — it is required, not a note-for-later
  (owner ruling 2026-06-17).** If you hit a gap, an ambiguity, or a misleading line in the docs
  (a runner not documented, a footgun undocumented, a stale description), writing/correcting that
  doc is part of THE SAME work item — never "noted for the next agent." A lacking doc that bit you
  will bite the next contributor; close it in the same change. (Specific instance that prompted
  this: the migration curators were undocumented and `engine.py`'s superseded `--write` raw-emitter was
  a footgun — it overwrites the curated DB with old shapes (recoverable; regen is idempotent) →
  `Tools/Migration/README.md` written + the toolkit doc corrected.) This sharpens
  the "keep knowledge in the repo" rule below: *encountering* a doc gap obligates *closing* it.
- **KEEP THE SPECS CURRENT as the model changes — proactively, in the SAME change (owner ruling 2026-06-23).**
  When a model/mechanic changes, update its spec to match in the same work item; never leave the spec describing
  the old shape. *"Always endeavour to keep specs up to date; not doing so invites the kraken."* A stale spec is
  **worse than none** — the next agent trusts it and builds on the wrong shape. This is the **proactive** twin of
  fix-docs-now (which is reactive). (Instance: the cargo model grew `space`(carries-what)/`size`(footprint) and
  `perUnit:`→`unit:` — specced in the same pass as the curator change.)
- **ACTIVELY find, READ, and VERIFY the docs for whatever you are working on — BEFORE and WHILE you work,
  not after being told (owner ruling 2026-06-18).** For ANY subsystem you touch: search `docs/` for it
  (grep the topic; read the relevant spec/reference page end-to-end) and confirm the intended design FROM THE
  DOC. Do **not** reconstruct the model from the live code or from memory, and do **not** propose matching legacy
  behaviour before checking whether the spec deliberately diverges from it. **A HOLE in the docs is NOT the
  absence of docs:** if the one detail you want isn't where you looked but the subsystem is documented, the docs
  still exist — keep reading the surrounding sections. Only if there is genuinely NO doc *and* you do not
  understand the intent do you **ASK**; otherwise **create/extend the doc** in the same change (the fix-docs-now
  rule above). This holds at session start AND after every context compaction — re-read the subsystem's docs;
  never assume the summary carried the design. **Deferring, skimming, or assuming is how this codebase slaps you —
  and not just with C++03/VC2003, but with the entire accumulated past (decades of Civ4/BTS/C2C ad-hoc wiring).**
  (Concrete failure that prompted this: working the #428 cascade *vicinity* logic, I traced legacy
  `isValidTerrainForBuildings` and proposed to match it — when `enabler-cascade-spec` §8 had already defined a
  *different, deliberately-more-permissive* vicinity model that should have been read first. Twice in one session
  the design was in the docs and I worked from code/memory instead.)
- **⛔ "FAST IS SLOW, SLOW IS FAST" — read the docs FULLY; skimming is NEVER faster (owner ruling 2026-06-21, GLOBAL).**
  If you think reading a doc partially — or only the section you *assume* is relevant, or grepping a keyword instead of
  reading it end-to-end — is faster, **you are wrong.** The 5% "saved" by skimming routinely costs far more downstream:
  re-derivation, wrong fixes built on a misread, and burned context. *Instance that prompted it:* skimming
  `migration-renames.md` on the simple/complex **trait split** led to **five** rounds of
  reverse-engineering a procedure that was already fully documented and "nailed" — ~half a context window wasted to save
  a few minutes of reading; the moment it was read in full the fix was obvious and landed at once. So **read each
  subsystem doc IN FULL before acting.** This is a sibling of THE KRAKEN RULE / no-guessing (skimming is a shortcut; a misread becomes an assumption).
  Ledgered as [DEC-fast-is-slow](docs/architecture/decisions.md#dec-fast-is-slow).
- **Nothing here is ever "just a one-liner" — expect hidden consequences.** This is a
  large, tightly-coupled Civ4/C2C codebase with non-obvious cross-cutting wiring (combat
  math shared across UI/AI/resolution, name-tagged save serialization, dual Python-enum
  registration, FastBuild unity grouping that exposes latent missing includes, graphics
  paths that run pre-init, the dead `.vcxproj`, etc. — see "Key Subsystem Knowledge"). Before
  any change, **read the relevant core docs first** (this file, the nearest `AGENTS.md`, and
  the subsystem's `docs/` notes), trace every caller/consumer of what you touch, and
  assume a small edit has ripples until you've checked. Do not skip this because a change
  "looks trivial" — that assumption has repeatedly produced regressions and contradicted
  documented design.
- **Build the proper structure ONCE — no transitional tech debt (owner ruling 2026-06-18).** The owner
  *hates technical debt with a passion* and would rather get a structure in **properly the first time** than
  rework it 2-3 times because a piece "isn't ready yet." So **reject transitional shims** that exist only to
  defer the real design (this is exactly why R-1 chose the full raw-field catalog over a string-carrying
  spine event — the shim would have been debt). When the right design needs prerequisite work, do the
  prerequisite (e.g. the field catalog) and build the real thing — don't ship a placeholder you'll tear out.
  **Corollary — ISOLATE COMPONENTS:** prefer clean, interface-bounded components with isolated surfaces
  (the spine / its consumers / the tally; a system's data block + its predicate query-surface) so each can be
  built and reasoned about once, properly, without entangling the next. (This sharpens, not contradicts,
  "minimal local changes": minimal = don't sprawl the edit; proper-once = don't ship debt. Shadow discipline
  — old+new coexist, diff, cut — is *verification* of the proper structure, not building-then-reworking it.)
- **Surface sprawl early; don't make the owner restate; optimise for EFFICIENCY (owner rulings 2026-06-20).**
  Two linked frustrations to design against. **(1) The partial-fix spiral.** The docs mess that forced the
  full rebuild accreted because *one partial fix was piled on another instead of a defined structure being
  followed.* When a change starts to sprawl — many small fixes piling up, or you are patching pieces of
  something whose **target STRUCTURE is undefined** — STOP and TELL the owner there's a risk of it getting out
  of hand, rather than agent-overcompensating with more partial fixes. The owner is *aware* of agentic
  limitations but must be **told** when the structure is undefined or the change is ballooning, so the call
  can be made to define the structure and do ONE proper cleanup. **(2) Restating / inefficiency.** The owner
  is **not omnipotent and does not want to be treated as such** (trust-but-verify their words like any other),
  but gets frustrated by inefficiency and **having to restate the same thing repeatedly** — so capture a
  ruling durably the first time it is given ([DEC-WF-rulings-to-repo](docs/architecture/decisions.md#dec-wf-rulings-to-repo)
  + the [decisions ledger](docs/architecture/decisions.md)) and do the proper cleanup once instead of
  churning. The owner makes the structure call; your job is to surface the risk and the options efficiently
  and not re-litigate settled ones. Conduct-level twin of [DEC-proper-once](docs/architecture/decisions.md#dec-proper-once).
  → [DEC-WF-surface-sprawl](docs/architecture/decisions.md#dec-wf-surface-sprawl).
- **ARCHITECTURAL NORTH-STAR — CLEAN ARCHITECTURE + interface-based contracts (owner is a .NET dev at core,
  2026-06-18).** The owner thinks in **Clean Architecture**: dependency inversion (depend on interfaces/contracts,
  not concretions), isolated layers with explicit boundaries, and a **functionality-based implementation approach**
  (compose behaviour via small contracts rather than deep inheritance hierarchies). This is the design compass for
  *all* structural work here. Concretely: program to interfaces (`IEventConsumer` is the model — the spine, tally,
  grants, logging are pluggable behind it); keep dependencies pointing inward at stable contracts; favour composition
  over the inherited Civ4 god-classes. It is the through-line behind the standing goals — dissolving
  `CvCityAI`/`CvUnitAI` into interface-bounded composition (the "shrink AI inherited classes" standing goal), the
  cascade's consumer/contract surfaces, and the dream of a pluggable external AI backend. It is also why the owner
  **immediately reframed the two trait systems** (the simple traits + the complex/Thunderbrd traits) into isolated
  systems the moment the entanglement showed — each system gets its own data block + predicate query-surface
  (enabler-spec §3 predicate-alignment pass), rather than leaking into each other. **The payoff of that isolation:
  once both implement the ONE trait contract, the composition root selects by a single option check — `if(complexTraits)`
  inject the complex impl into the game object, else vanilla — and the game object depends only on the contract, never
  knowing which it got.** We **cannot DI it for real** (no DI container in C++03/VC7.1; the EXE binds concrete classes), so
  it is **"poor-man's DI": a literal `if`/`switch` gate at the composition root** picks the concrete and assigns it to the
  contract pointer. The decoupling is fully real (the consumer depends only on the interface); only the *wiring* is manual
  instead of container-resolved. (Dovetails with the §5a game-option override-by-design swaps.) C++03/VC7.1 constrains the *syntax*
  (virtual interfaces, no lambdas/template gymnastics) but **not** the architecture — the contracts-and-boundaries
  discipline holds regardless of the dated compiler. (See also the design-style preference in assistant memory.)
  - **The concrete C++03 SHAPE of "interface contracts" here:** an *interface* = an abstract base class with only
    pure-virtuals + a virtual dtor and **NO data members** (`IEventConsumer` is the model). **Multiple inheritance is
    the `implements IA, IB` mechanism** — one concrete class can satisfy several role-contracts via MI — but it's the
    *implements* axis, **NOT** a DI substitute (you still inject by holding a base pointer assigned at the `if`/`switch`
    composition root). Two guardrails: **(1) MI only of stateless pure-virtual interface bases** (MI of stateful
    concretes invites the diamond/layout/`virtual`-base mess); **(2) graft interfaces onto the DLL-internal derived
    classes (`CvCityAI`/`CvUnitAI`/…), NEVER widen an EXE-bound base** (`CvCity`/`CvUnit` — the closed `.exe` binds their
    vtable/layout; adding bases there risks the ABI). The derived side is the safe lane and the lever for the
    shrink-the-god-classes goal.
- **Only automatically branch / commit / PR when the work is tied to an active GitHub
  issue.** For anything else (experiments, behaviour tuning, undocumented fixes we are
  still iterating on), **edit the working tree only** — do not commit, create or switch
  branches, push, or open a PR unless the user explicitly asks for a specific git action.
  The user builds the DLL from the current working tree, so committing to a new branch or
  `git checkout`-ing away silently removes the changes from their build. **Never switch
  branches while the user may be mid-build.** (Read-only git — `status`/`log`/`diff` — is
  always fine.)
- **The info JSONs (`Assets/Data/**`) are a DERIVED artifact — regenerate and commit them
  FREELY, never ask (owner ruling 2026-06-22).** They are curator OUTPUT, **never hand-edited**,
  so **right-or-wrong lives in the CURATOR, never in the JSON** — a stale-vs-curator JSON is not
  a risk, only out-of-sync, fixed by `python curate_<x>.py --write`. **Regeneration is IDEMPOTENT and CHEAP** — re-running `--write` reproduces the same files; nothing about it
  is "heavy" or "dangerous." Therefore: (a) **regenerating the JSON is a just-do-it operation — never prompt for
  it**, and it is the **routine step immediately after a verified curator change** (fix curator → `--sample`
  verify → `--write` → **commit the regenerated data alongside the curator**, so the two never dangle apart); (b) **commit the JSONs routinely**,
  even mid-migration / "half done" (leaving the hundreds of modified JSONs uncommitted just bloats
  the working tree; committing the current regen state keeps it sane). This does NOT loosen "commit
  only on explicit ask" for gameplay CODE — it means the derived JSONs ride along when you commit,
  and a regen never needs a prompt.
- **Docs-only changes go to `main` ONLY when the owner explicitly authorizes it** (owner ruling 2026-06-23,
  tightening 2026-06-12 — an agent never unilaterally pushes docs to `main`; default is the working branch). When
  authorized, eligible docs: the indexes (`indexes/DESPAIR_INDEX.*`, `indexes/REALISM_INDEX.*`), player docs,
  `docs/` notes, AGENTS.md — provided NOTHING else rides in the commit. Anything
  gameplay-affecting (C++ code, `Assets/XML` data, Python) keeps the careful path:
  branch + PR + playtest per the conventions above.
  - **Promotion to `main` is the OWNER's explicit call, never the agent's judgement (owner ruling 2026-06-23).**
    The allowance exists mostly to avoid later MERGE CONFLICTS — a convenience for docs with no branch home — but
    absent an explicit say-so, docs commit on the working branch. Docs that pertain to an in-flight branch's work
    (e.g. the `#428/#430` cascade specs on `json-data-migration`) **belong with that work and commit on the
    branch.** Branch-coupled doc → its branch; a standalone doc the owner approves for `main` → `main` (to
    dodge the conflict). **The canonical "→ `main`" docs are the INDEXES** (owner 2026-06-19:
    `indexes/DESPAIR_INDEX.*`, `REALISM_INDEX.*`, the COMPLEXITY catalog) — they pertain to no single
    branch, so straight-to-`main` is exactly right for them. A cascade spec on `json-data-migration` is the
    opposite case: branch-coupled, stays on the branch.
- **Verify the current branch immediately before every commit.** Run
  `git branch --show-current` (or `git status`) in the same command as the commit and
  confirm it is the branch you intend. The working copy is shared with the owner, who may
  check out another branch at any moment between an agent's commands — this has happened:
  a mid-session checkout to `main` put two #248 commits on local `main` even though the
  agent had created and verified a work branch earlier in the session. Never assume the
  branch from earlier context; if HEAD is not where you expect, stop and repair
  (`git branch -f <work-branch> <commits>`, restore `main` to `origin/main`) before pushing.
- **`release` is a strict follower of `main` (owner ruling 2026-06-11): it must NEVER contain a
  commit that `main` does not already have.** Never commit, cherry-pick, or merge anything directly
  to `release`; anything wanted in a release lands on `main` first. Sync flow: `git checkout main &&
  git pull`, then `git checkout release && git rebase main` (a pure fast-forward when the rule holds
  — if the rebase reports replayed commits, STOP: the rule has been violated upstream; surface it),
  then `git push origin release`. Verify with `git log main..release` (must be empty) before
  pushing. Pushing `release` is what triggers the AppVeyor release build.
- **Before adding commits to a PR, verify it has not already been merged.** Run
  `gh pr view <n> --json state,baseRefName` first and confirm (1) `state` is `OPEN` —
  pushing to a merged/closed PR's branch lands the commit on a dead branch that never
  reaches `main` — and (2) `baseRefName` is what you assume: stacked PRs here can target a
  feature branch instead of `main`, and the stack can merge out of order (PR #332 merged
  into its base *after* that base had already gone to `main` via #331, so its content
  silently missed `main` and needed a re-delivery PR, #334). If state or base is
  surprising, surface it before pushing.
- **Confirm behaviour before opening a PR:** a behaviour/feature change needs a real
  in-game playtest (the user runs `FinalRelease` + `rebuild deploy`), not just a green
  Assert build. A merged or committed change does nothing in a running game until the DLL
  is rebuilt and deployed.
- **Runtime-verification division of labor — AMENDED (owner ruling 2026-07-02): with PER-SESSION owner permission,
  an agent may kill/rebuild/start the game via `agentstart.bat`.** The permission is granted per session, never
  standing — absent it, the old rule holds (the owner launches; agents never start/kill the game). The mechanism is
  ONLY the repo-root `agentstart.bat` (paths from the gitignored `.env`: BTS dir, mod name, test save): it closes
  any running game and relaunches the mod loading the configured save, skipping the dev DLL's boot-time rebuild by
  default (pass `bootcheck` to allow it) — unattended-safe; ad-hoc headless launches outside it remain banned
  (unmanageable console windows). The agent flow: build + `deploy` → run `agentstart.bat` → poll
  `http://127.0.0.1:7227/` until up → verify via the endpoints. Verify via
  `Documents/My Games/Beyond The Sword/Logs/`: `XmlLoad.log` per-category counts, no
  `Xml_MissingTypes.log`, no new `Asserts.log` entries. Known pre-existing assert families
  on mature saves (filter, already filed): `CvContractBroker::makeContract` NULL pJoinUnit
  (#336), `AI_formArmies` army-ID format (#364), unit stuck-in-loop short-circuit (#189 family).
- **"Minimal, local changes" bounds the SIZE of an edit, NOT the SCOPE of the work (owner clarification
  2026-06-20).** A *targeted* fix or feature touch inside a large, tightly-coupled legacy core file stays
  minimal and local — don't sprawl it or gratuitously refactor around it, because ripples bite (the
  no-one-liner rule). But this is **not** a brake on deliberate structural rework: the #428/#430 cascade,
  the docs rebuild, and dissolving the `Cv*AI` god-classes are **large by design** and never subordinate to
  "minimal" — they answer to [DEC-proper-once](docs/architecture/decisions.md#dec-proper-once). Same
  instinct, not opposites: don't make an edit bigger or riskier than its goal needs; when the goal *is* the
  structure, build it whole and right the first time. (The same clarification applies to the identical line
  in `Sources/AGENTS.md`.)
- **Import Info headers DIRECTLY; do not lean on the `CvInfos.h` umbrella (owner 2026-06-18).** `CvInfos.h` is only an
  umbrella aggregator and should be RETIRED — new/edited code includes the specific `CvXInfo.h` (or `Infos/CvXInfo.h`)
  it needs directly. (Flagged future cleanup: migrate existing `#include "CvInfos.h"` sites to direct imports + retire the file.)
- Preserve save compatibility by default; for intentional breaks, coordinate and
  mark with `@SAVEBREAK`. See `Notes for the next breaking of save game compatability cycle.txt`.
- **FRONT-LOAD save-breaking reworks NOW (owner ruling 2026-06-17).** S2S is its own project
  (forked from C2C to rework freely — inherited C2C *conventions* are not constraints, only the
  closed Firaxis EXE binds). The playerbase is very small today, so the cost of breaking saves is
  at its lowest and only rises — break saves now if ever, while the window is open. (C2C→S2S save
  compat is already broken and an explicit NON-GOAL; never constrain a design to keep it.)
- **Keep quirky/intermediate commits — do NOT push to squash them (owner taste).** The owner
  deliberately leaves oddly-named or intermediate commits (e.g. a `temp: rename Docs -> banana`
  step of a case-rename dance) in history "as a protest." Mention squashing exists at most once;
  default to preserving history as-is.
- **PG-13 public quotes (owner ruling 2026-06-12).** When quoting the owner in public artifacts
  (GitHub issues, PR bodies, commit messages, repo docs), keep the colorful voice but drop the
  profanity (the "a lot of f***ing about" → "a great deal of faffing about" treatment).
  Colorful-but-clean ("this has been nuts") is fine; the rulings the quotes carry are still wanted.
- If C++ changes touch XML/Python interfaces, run the XML + callback validators.
- Do not modernize or replace the build chain/toolchain.

## Documentation & Knowledge — keep it in the repo

**Durable knowledge lives in the repository, never only in a developer's or AI
assistant's local/private notes.** When you learn something worth keeping — how a
subsystem works, a design decision, a plan, a non-obvious "gotcha", the state of a
standing initiative — write it into the appropriate committed doc *in the same
change*, so every contributor and every agent sees one shared source of truth:

All documentation lives under **`docs/`** (map: [`docs/README.md`](docs/README.md)) — the condensed spec surface
rebuilt 2026-06-23: **`docs/specs/`** (the JSON data-model + the cascade/system specs — `json`/`naming`,
`enabler`/`modifier`/`tally`, `event-spine`, `logging`, `validation`, the unit `skills`/`tags`/`state`/
`capabilities`, `http-endpoints` — plus the transient `curators/`), **`docs/reference/`** (how the engine +
subsystems behave today), **`docs/architecture/`** (the decisions ledger, `north-star`, `patterns`),
**`docs/plans/`** (`structural-cleanup/` the cutover bulldozer + `parked/` un-killed intent). The hosted
DESPAIR/REALISM/COMPLEXITY catalogs live at **`indexes/`** (repo root, served via Pages).

- How existing code behaves → `docs/reference/`.
- A change or initiative you intend to make (plan, scope, rollout, removal) → `docs/plans/`.
- A superseded/killed dev idea → record it in `docs/architecture/superseded-ideas.md` (what it was, why
  it's dead, what replaced it) so it isn't revived; don't carry the stale copy in the live set.
- Cross-cutting, must-not-rediscover facts → "Key Subsystem Knowledge" above (or the nearest `AGENTS.md`).
- The mod's front-door / build-pipeline readme (the code repo's mirror) → `docs/MOD-README.md`.
- A newly-found bug of exceptional absurdity may *additionally* earn an entry in
  [`indexes/DESPAIR_INDEX.md`](indexes/DESPAIR_INDEX.md) (owner-sanctioned, lighthearted,
  optional — never a substitute for the real fix/issue/doc). Its sibling
  [`indexes/REALISM_INDEX.md`](indexes/REALISM_INDEX.md) catalogues "super realistic" *mechanics*
  — absurdities working exactly as designed (same policy: optional, never a substitute
  for the real issue).
- **Rules and conventions for agents/contributors → THIS file (`AGENTS.md`), always.**
  `AGENTS.md` is the one unified place for rules and docs. The root `CLAUDE.md` exists
  only as a session-bootstrap shim that imports this file — never add rules or content
  to `CLAUDE.md` directly.

Any per-developer assistant memory store is a personal *index/cache* only — it is
**not** a substitute for the in-repo copy, and the in-repo copy is authoritative.
If you record something locally, mirror the shareable part into the repo in the same
change, and keep these docs current as the code moves. See `docs/README.md`.

**Handovers are TRANSIENT one-time relays — nothing durable may hinge on one (owner ruling 2026-06-17).** A
handover is **in essence a task list** — work done + work still upcoming — whose only job is to carry state to
the NEXT session; once that session has read it, it stops being a handover. (Writing an end-of-session handover
is a fine practice; it just is never load-bearing.) Therefore:

- **Nothing durable may hinge on data that lives ONLY in a handover.** Any fact, ruling, decision, or state that
  matters beyond the next session MUST be captured in a durable doc (the relevant `docs/` spec/reference, or
  this `AGENTS.md`) in the SAME change — never left only in the handover.
- **A durable doc must NEVER reference a handover as "latest / read-first / resume here"** (that is literally the
  handover's own job), nor cite one as the home of a ruling or load-bearing detail. A durable doc must read
  correctly with every handover deleted.
- A handover is **deletable-without-loss** on completion. We don't actually delete them — they are kept as
  historical context of *work deferred* — but no durable doc may depend on that retention.

**⛔ HARD RULE — every owner ruling goes into the repo docs IMMEDIATELY, unprompted.**
When the owner makes a ruling in conversation — a design decision, a workflow rule, a
relaxed or tightened constraint, a "from now on do X" — writing it to assistant memory is
NOT enough and never the end state. In the SAME work item (same commit/PR, without being
asked) write it into the right repo home: workflow/convention rulings → this file's
Conventions; subsystem/design rulings → the relevant `docs/` page. A ruling that
exists only in one developer's local memory is invisible to every other contributor and
agent, and has repeatedly had to be re-requested — treat "saved to memory only" as an
unfinished task.

**The discoverability half of that rule: the DECISIONS LEDGER ([`docs/architecture/decisions.md`](docs/architecture/decisions.md))
(owner ruling 2026-06-19).** Cross-cutting rulings kept getting re-stated doc-after-doc because of a
self-reinforcing loop: the capture-immediately rule above is correct → but compaction wipes an agent's memory
that it ever read the ruling → and there was no discoverable canonical home, so "is this already recorded?" was
unanswerable → so the agent re-added it defensively → and each re-add made the next agent's existence-check
harder (now in N places with wording drift, none authoritative) → re-add again, *ad infinitum*. The ledger
breaks the loop: it is an **INDEX, not a re-statement** — one stable ID per ruling (`DEC-<slug>`), a one-line
summary, and a pointer to the authoritative home. **Operational rules:** (1) **before adding any cross-cutting
ruling anywhere, grep the ledger's ID table first** — that one-grep existence check is the whole point; (2)
capture a cross-cutting ruling by adding/seeing its line in the ledger and recording the full text in its home
(this file's Conventions, or the relevant `docs/` page), **never** by restating it in a second doc; (3) a
doc that needs to invoke a ledgered ruling **links `[DEC-id]`**, it does not re-articulate it. The ledger does
not replace this HARD RULE — it makes it cheap to obey and impossible to not-know-about.

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
