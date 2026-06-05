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
- **Confirm behaviour before opening a PR.** Do not open a PR for a behaviour/feature
  branch that has no tied, confirmed issue until the change has been **playtested
  in-game and confirmed working** — a green Assert build is not enough. Keep such work
  on a pushed branch until then; open the PR after confirmation. (Bug fixes tied to a
  verified issue, and docs-only/process changes, may be PR'd normally.) Remember a DLL
  change needs `rebuild deploy` to take effect in a running game.
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

- How existing code behaves → `Sources/docs/reference/`.
- A change or initiative you intend to make (plan, scope, rollout, removal) → `Sources/docs/plans/`.
- Cross-cutting, must-not-rediscover facts → "Key Subsystem Knowledge" above (or the nearest `AGENTS.md`).
- Player-facing rules, manuals, FAQs → top-level `docs/`.

Any per-developer assistant memory store is a personal *index/cache* only — it is
**not** a substitute for the in-repo copy, and the in-repo copy is authoritative.
If you record something locally, mirror the shareable part into the repo in the same
change, and keep these docs current as the code moves. See `Sources/docs/README.md`.

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
