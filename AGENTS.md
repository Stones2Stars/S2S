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
> closed `.exe`: [`docs/dev/reference/boost-situation.md`](docs/dev/reference/boost-situation.md)). So the DLL is genuinely **C++03, 32-bit/x86** — *no* `std::thread`, *no* OpenMP,
> *no* C++11+. This is a hard compiler limit (the toolchain is locked to stay ABI/STL-compatible with the closed VC7.1 game `.exe`), **not**
> a style convention. In-process threading means raw Win32 only. Do not modernize or replace the build chain/toolchain.

The build is driven by **`Tools/_Build.ps1`** (a FastBuild wrapper). Invoke it
**from the `Sources/` directory**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb> [<verb> ...]
```

- **Configs:** `Assert`, `Debug`, `Release`, `FinalRelease`, `Profile`, `ProfileExtra`.
  Output lands in `Build/<Config>/CvGameCoreDLL.dll` (+ `.pdb`).
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
   `S2S.vcxproj` + `S2S.vcxproj.filters` from disk), or add the entries by hand.
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
  `docs/dev/reference/CvUnitAI.md` (garrison-tiers section).

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

### Cascade observability — the total-observability ("Orwell") bar (#428/#430)

- **The events + logging + diagnostics must make the running game FULLY surveilled (owner ruling 2026-06-18).** The
  bar: *map an accurate game state purely from the endpoints + `/events` + the gated logs — open the game, but never
  look at the SCREEN.* This is **non-negotiable and load-bearing**, not polish: it is the ONLY way to reliably REBUILD
  the state logic on the cascade + tally — you cannot safely delete a maintainer you cannot fully observe
  (map-before-delete). Every state behaviour (enabler-spec §14 H) gets a SHADOW that diffs the cascade's verdict
  against the live engine, turn over turn, until clean — *then* the legacy mechanism is deleted. Full ruling +
  rationale: `docs/dev/plans/cascade-mapping-inventory.md` §A. Live surface: `docs/dev/reference/http-server.md`
  (`/diagnostic/*` shadows incl. `sweep`/`placementSweep`; `[READJSON]`/`[PLACEMENT]` per-turn log lines).
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

- **Run the post-compaction reload hook — RECOMMENDED to mitigate context poisoning (owner ruling
  2026-06-18).** Auto-compaction can silently drop this guide from context (the project-root
  `CLAUDE.md` is re-read on compaction, but its `@AGENTS.md` / `Sources/AGENTS.md` content and
  nested rules are NOT reliably re-injected), leaving an agent operating on a *poisoned* context that
  looks complete but has lost the rules — the failure mode behind the recurring `.vcxproj`-is-truth
  mistake. Mitigation, committed in the repo: a `SessionStart` hook (matcher `startup|clear|resume|compact`
  in `.claude/settings.json`, via `.claude/hooks/session-start.ps1`) that re-injects
  `.claude/post-compaction-reload.txt` — a short notice that orders a re-read of AGENTS.md and restates the
  few absolute non-negotiables — AND lists the active read-gate docs (next bullet). It is the *digest*,
  not a second source of truth; AGENTS.md stays authoritative. The hook runs the digest through
  **PowerShell 7 (`pwsh`)**, which emits UTF-8 cleanly, so emoji/Unicode are fine — do NOT route it
  through legacy Windows PowerShell 5.1 (`powershell.exe`), which mangles non-ASCII on stdout. This is
  the harness-level twin of the in-file banners on the dead `.vcxproj`/`.sln`/`.filters`: put the
  antidote where compaction can't summarize it away.
- **Read-gates — the doc-read before touching a subsystem is MECHANICALLY ENFORCED, not exhorted (owner
  ruling 2026-06-19).** *Why it is load-bearing:* this codebase is a cataclysmic clown fiesta — a
  tightly-coupled, decades-deep Civ4/BTS/C2C tangle we are wrangling back to sanity — and **skipping a
  subsystem's docs has routinely screwed agentic sessions** (an assumption reconstructed from stale code
  or a context summary, then built on, then a regression). Every prior mitigation was an *exhortation*
  ("read the docs first" — in `CLAUDE.md`, here, `MEMORY.md`, the `docs/dev/README.md` banner, the reload
  hook), and exhortation FAILED: it leaves the stop-condition ("have I read enough?") to the agent's
  judgement, which is biased toward getting to the task. So the read is now a **gate**, removing that
  judgement. Two parts, committed:
  - **Session-start read-first:** the `SessionStart` hook (above) lists, at every open/clear/resume/compact,
    the docs of every read-gate marked `sessionStart` — with the directive *your first action is to Read
    each in full before responding* (the owner: "when I open Claude or `/clear`, the first thing I want to
    see is you reading docs — you can do it before accepting input").
  - **PreToolUse hard-deny:** `.claude/hooks/read-gate.ps1` (matcher `Edit|Write|MultiEdit`) parses the
    session transcript for `Read` calls and **DENIES** any edit to a gated subsystem's `paths` until every
    one of that gate's `docs` was Read this session (it lists exactly which are still unread). It catches
    the real recurring failure — *never opening the doc* (comprehension can't be automated; "opened" can).
    It **fails OPEN, loudly** on its own errors, so a hook bug never bricks editing but never fails silently.
  - **Where it lives:** one JSON manifest per subsystem in `.claude/read-gates/` (`docs` + trigger `paths`);
    both hooks scan that dir, so adding a subsystem is a new JSON, no hook edits (`.claude/read-gates/README.md`).
    Seeded with **`cascade`** (the 8 `#428/#430` banner docs gating `Sources/Cascade/**` + `Assets/Data/**`);
    extend to other doc'd subsystems (worker-AI, combat, …) as needed. The gate proves the docs were *opened*,
    not understood — but "never opened them" is the failure that keeps happening, and that is fully gateable.
  - **MINION EXCEPTION (owner ruling 2026-06-19): a spawned sub-agent only needs the context it OPERATES
    UNDER, not the full manifest.** The gate targets the **orchestrator** — the agent with broad edit authority
    over the subsystem, which must hold the whole design. A minion is spawned for a NARROW slice; the orchestrator
    (which HAS read the docs) briefs it with exactly the docs/excerpts/facts that slice needs and owns its
    correctness, so forcing the minion to re-read all N docs is wrong (it also defeats the point of delegating to
    a cheap, scoped context — e.g. the read-only `data-reader`/`Explore` minions). So: the session-start
    read-first directive is for the MAIN session, and a minion is **exempt from the full-manifest read** — it
    reads only what it was given to operate under. *(Enforcement note: the PreToolUse hook keys on the session
    transcript; project minions today are overwhelmingly read-only so they don't hit the `Edit` gate. If an
    edit-capable minion on gated paths ever becomes real, either brief it to Read the one doc its slice needs, or
    refine the hook to recognize sub-agent context — a verify-then-change follow-up, since the hook stdin's
    sub-agent signal is unconfirmed.)*
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
  clobber when a tool's real behaviour wasn't checked first). Verify before you build on it, and
  say what you verified it against.
- **When documentation is lacking or wrong, FIX IT NOW — it is required, not a note-for-later
  (owner ruling 2026-06-17).** If you hit a gap, an ambiguity, or a misleading line in the docs
  (a runner not documented, a footgun undocumented, a stale description), writing/correcting that
  doc is part of THE SAME work item — never "noted for the next agent." A lacking doc that bit you
  will bite the next contributor; close it in the same change. (Specific instance that prompted
  this: the migration curators were undocumented and `engine.py`'s superseded `--write` clobbered
  the curated DB → `Tools/Migration/README.md` written + the toolkit doc corrected.) This sharpens
  the "keep knowledge in the repo" rule below: *encountering* a doc gap obligates *closing* it.
- **ACTIVELY find, READ, and VERIFY the docs for whatever you are working on — BEFORE and WHILE you work,
  not after being told (owner ruling 2026-06-18).** For ANY subsystem you touch: search `docs/dev/` for it
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
- **Nothing here is ever "just a one-liner" — expect hidden consequences.** This is a
  large, tightly-coupled Civ4/C2C codebase with non-obvious cross-cutting wiring (combat
  math shared across UI/AI/resolution, name-tagged save serialization, dual Python-enum
  registration, FastBuild unity grouping that exposes latent missing includes, graphics
  paths that run pre-init, the dead `.vcxproj`, etc. — see "Key Subsystem Knowledge"). Before
  any change, **read the relevant core docs first** (this file, the nearest `AGENTS.md`, and
  the subsystem's `docs/dev/` notes), trace every caller/consumer of what you touch, and
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
- **Docs-only changes may be committed and pushed straight to `main`** (owner ruling
  2026-06-12): the indexes (`docs/indexes/DESPAIR_INDEX.*`, `docs/indexes/REALISM_INDEX.*`), player docs,
  `docs/dev/` notes, AGENTS.md — provided NOTHING else rides in the commit. Anything
  gameplay-affecting (C++ code, `Assets/XML` data, Python) keeps the careful path:
  branch + PR + playtest per the conventions above.
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
- **Runtime-verification division of labor:** the agent builds + deploys the DLL and reads
  the logs; **the owner launches the game**, ensures logging is enabled, and confirms the
  game is loading — agents must not start/kill the game themselves (a headless launch
  spawns a console window the agent cannot manage). Verify via
  `Documents/My Games/Beyond The Sword/Logs/`: `XmlLoad.log` per-category counts, no
  `Xml_MissingTypes.log`, no new `Asserts.log` entries. Known pre-existing assert families
  on mature saves (filter, already filed): `CvContractBroker::makeContract` NULL pJoinUnit
  (#336), `AI_formArmies` army-ID format (#364), unit stuck-in-loop short-circuit (#189 family).
- Prefer minimal, local changes in large core files.
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

All documentation lives under **`docs/`**, split by audience (map: [`docs/README.md`](docs/README.md)):
`docs/dev/` (engineers/agents), `docs/modders/` (data authoring), `docs/players/` (manuals/mechanics/FAQs),
`docs/indexes/` (the hosted DESPAIR/REALISM/COMPLEXITY catalogs), `docs/crap/` (half-outdated holding pen).
(Engine docs used to live under `Sources/docs/` — consolidated into `docs/dev/` on 2026-06-17.)

- How existing code behaves → `docs/dev/reference/`.
- A change or initiative you intend to make (plan, scope, rollout, removal) → `docs/dev/plans/`.
- A superseded dev doc → move it to `docs/dev/archive/` (don't delete history; get it out of the live set).
- Cross-cutting, must-not-rediscover facts → "Key Subsystem Knowledge" above (or the nearest `AGENTS.md`).
- Player-facing rules, manuals, FAQs → `docs/players/`. How to author data / mods → `docs/modders/`.
- A newly-found bug of exceptional absurdity may *additionally* earn an entry in
  [`docs/indexes/DESPAIR_INDEX.md`](docs/indexes/DESPAIR_INDEX.md) (owner-sanctioned, lighthearted,
  optional — never a substitute for the real fix/issue/doc). Its sibling
  [`docs/indexes/REALISM_INDEX.md`](docs/indexes/REALISM_INDEX.md) catalogues "super realistic" *mechanics*
  — absurdities working exactly as designed (same policy: optional, never a substitute
  for the real issue).
- **Rules and conventions for agents/contributors → THIS file (`AGENTS.md`), always.**
  `AGENTS.md` is the one unified place for rules and docs. The root `CLAUDE.md` exists
  only as a session-bootstrap shim that imports this file — never add rules or content
  to `CLAUDE.md` directly.

Any per-developer assistant memory store is a personal *index/cache* only — it is
**not** a substitute for the in-repo copy, and the in-repo copy is authoritative.
If you record something locally, mirror the shareable part into the repo in the same
change, and keep these docs current as the code moves. See `docs/dev/README.md`.

**Handovers are TRANSIENT one-time relays — nothing durable may hinge on one (owner ruling 2026-06-17).** A
handover is **in essence a task list** — work done + work still upcoming — whose only job is to carry state to
the NEXT session; once that session has read it, it stops being a handover. (Writing an end-of-session handover
is a fine practice; it just is never load-bearing.) Therefore:

- **Nothing durable may hinge on data that lives ONLY in a handover.** Any fact, ruling, decision, or state that
  matters beyond the next session MUST be captured in a durable doc (the relevant `docs/dev/` spec/reference, or
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
Conventions; subsystem/design rulings → the relevant `docs/dev/` page. A ruling that
exists only in one developer's local memory is invisible to every other contributor and
agent, and has repeatedly had to be re-requested — treat "saved to memory only" as an
unfinished task.

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
