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
> (`Build/deps/...`), **Python 2.4**, **Boost 1.32 / 1.55**. So the DLL is genuinely **C++03, 32-bit/x86** — *no* `std::thread`, *no* OpenMP,
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
`.vcxproj` files are IDE-only and do NOT drive the build. When you create a new
`Sources/<NewDir>/`:

1. Add `$SOURCE_DIR$/<NewDir>` to the `.UnityInputPath` array in `Sources/fbuild.bff` (~line 201).
2. Add the files to `Sources/C2C (VS2019).vcxproj` and `…vcxproj.filters` (IDE display).
3. Both are required. Symptom of a missing `fbuild.bff` entry: compiles in the IDE
   but FastBuild fails at link with one `LNK2001: unresolved external symbol` per
   symbol in the new directory. `Sources/Repos/` and `Sources/Utils/` are reference examples.

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

## Conventions

- **Run the post-compaction reload hook — RECOMMENDED to mitigate context poisoning (owner ruling
  2026-06-18).** Auto-compaction can silently drop this guide from context (the project-root
  `CLAUDE.md` is re-read on compaction, but its `@AGENTS.md` / `Sources/AGENTS.md` content and
  nested rules are NOT reliably re-injected), leaving an agent operating on a *poisoned* context that
  looks complete but has lost the rules — the failure mode behind the recurring `.vcxproj`-is-truth
  mistake. Mitigation, committed in the repo: a `SessionStart` hook (matcher `compact|resume|clear`
  in `.claude/settings.json`) that re-injects `.claude/post-compaction-reload.txt` — a short notice
  that orders a re-read of AGENTS.md and restates the few absolute non-negotiables. It is the *digest*,
  not a second source of truth; AGENTS.md stays authoritative. The hook runs the digest through
  **PowerShell 7 (`pwsh`)**, which emits UTF-8 cleanly, so emoji/Unicode are fine — do NOT route it
  through legacy Windows PowerShell 5.1 (`powershell.exe`), which mangles non-ASCII on stdout. This is
  the harness-level twin of the in-file banners on the dead `.vcxproj`/`.sln`/`.filters`: put the
  antidote where compaction can't summarize it away.
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
- Preserve save compatibility by default; for intentional breaks, coordinate and
  mark with `@SAVEBREAK`. See `Notes for the next breaking of save game compatability cycle.txt`.
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
