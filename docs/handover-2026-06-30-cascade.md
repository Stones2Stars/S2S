# Handover — cascade engine (#430), 2026-06-30

> **TRANSIENT relay** (a task-list for the next session). Nothing durable hinges on this file — the authoritative
> knowledge is in the specs/plans linked below; this just says *where we are* and *what's next*. Branch: `json-data-migration`.

## Where we are (all committed, game loads clean + green)

The cascade rebuild is well underway, on a proper footing (the first prototype was purged). Built + verified this session:

- **Event spine** + **Tally** ✅ (buildings + units; `[TALLY/shadow] diverging=0`).
- **readJson** ✅ — full json.md PARSE (0 `UNCLASSIFIED` keys) **and MAPS** each entity's JSON to a `CvCascadeData`
  (deposits ×100 + enabled/disabled `BoolExpr`, requires build/operate, the enables/provides edges, allowed caps,
  grants), attached **by game object** via the ABI-safe **side-table** (`cascadeForInfo`/`cascadeAttach`). 13,159
  entities mapped; read-back round-trips.
  - ⛔ **ABI:** widening the **base** `CvInfoBase` crashes the EXE on load (minidump-proven — it binds the base layout).
    But **appending a member to a DERIVED info class is ABI-safe** (standard C2C). So the **target home = a new appended
    member on each SPECIFIC info** (`CvBuildingInfo`/…), reusing the standard fields — faster + clearer than a map. The
    current side-table (`cascadeForInfo`) is the **INTERIM over-correction** ("no `CvInfo` member" instead of "no *base*
    member"); **next task: redesign it to per-derived members** (cascade-engine-430 §3, CvCascadeData.h).
- **Mistake-hunt** ✅ — `obsoletedBy`→edge; tech capabilities→`capabilities` block (curator fold).
- **Modifier machine — percent stack** ✅ (`CvCascadeModifierMath`): `max(0,100+Σ%)` off the mapped deposits, BoolExpr
  gated, shadowed vs legacy `getBaseYieldRateModifier` with sub-term attribution. **Building tier is bit-exact**
  (`bC == bld+bon+pow`); residual is two small named player-tier issues (capital `IS_CAPITAL` term; a ~5
  production↔commerce swap).

## What's next (owner direction, 2026-06-30) — get it ALL in, then compare vs StoneBase

1. **Port the WHOLE modifier `Calc` in, THEN compare the in-DLL shadow vs StoneBase's results** — do NOT chase
   per-increment parity. StoneBase mapped every source + is parity-proven; the in-DLL job is **port fidelity** (does the
   C++ reproduce StoneBase's numbers?). Remaining packages: BASE (plot/specialist/trade/free-city/golden-age), AFTER
   (building flats ×100), the assembler, the §2 commerce stage. Plan: [`modifier-machine.md`](plans/structural-cleanup/modifier-machine.md) §0/§5.
2. **PARITY = full ATTRIBUTION + a showable diff, NOT bit-exact.** Bit-exact is impossible by design (building
   free-specialists were moved into the specialists bucket — accepted overshoot). The bar is: every source attributed,
   every diff nameable; reproduce StoneBase's attribution + its accepted intentional diffs. ([`validation.md`](specs/validation.md) parity bar.)
3. **★ WIRE THE NEW CLASSIFICATION + POLICY BLOCKS (super important — they easily get LOST; a wrong/missing one
   DETERIORATES the game).** They are parsed-but-skipped today (not mapped). Map `skills`/`tags`/`capabilities`
   (json.md §8) + `policies` (§9) onto the game object, and make each engine SYSTEM read the new array the same way it
   uses the legacy flag: **unit `skills.blitz` → multiple-attacks**, **empire `capabilities` → the team ability**,
   **`policies.noForeignTrade` → the trade-route engine**, unit `tags` → `IS_<TAG>` accounting. **Empire capabilities +
   unit skills especially.** (cascade-engine-430 §7 item 3a.)
4. **Enabler — build EARLY, a CO-REQUISITE with the modifier (not later).** Without it the cascade doesn't know what's
   ACTIVE (which bonuses are connected, which buildings non-dormant), which the modifier's `enabled:{HAS_BONUS}` /
   `connection` / dormancy conditions depend on. ⛔ **The modifier shadow must read the ENABLER's active state, NOT the
   live engine** (so the cascade is self-contained post-cutover); the **enabler's active state is independently shadowed
   vs the engine to prove they are EQUAL** (StoneBase proved this). ⛔ **NEVER read legacy COMPUTED/active outputs as a
   cascade INPUT** (the pollution anti-pattern — the repeated pre-StoneBase mistake; validation.md pollution guardrail).
   The current percent-stack reads the live engine's computed-active state (`isActiveBuilding`, connected-bonus eval) —
   that is **DEBT that commits the anti-pattern**, to be replaced by reading the enabler (modifier-machine §2). Then
   **grants**; the **trait simple/complex** fix (§6); the atomic **cutover** LAST.

## Working cadence (so the next session doesn't relearn it)
- Build: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" Assert build` from `Sources/`
  (compile check); `Release build deploy` to test in-game.
- The cascade probes/shadows are **one-shot, gated by `gPlayerLogLevel`**, hooked at `CvGame::doTurn` (readJson →
  modifier shadow). Verify via the `/events` SSE stream: **connect BEFORE the turn ticks**, the owner ends one turn,
  read the `[READJSON/*]` / `[MODIFIER/*]` / `[TALLY/*]` lines. The owner launches the game (`agentstart.bat`); the
  agent builds/deploys/reads. ⛔ Never live-read the `.log` files (held open).
- The info JSONs are derived — regenerate (`python curate_*.py --write`) + commit freely; right-or-wrong lives in the curator.

## Durable docs (authoritative — read these, not this handover)
- [`plans/structural-cleanup/cascade-engine-430.md`](plans/structural-cleanup/cascade-engine-430.md) — the parent #430 plan + status + §7 NEXT + the §3 ABI/mapping model.
- [`plans/structural-cleanup/modifier-machine.md`](plans/structural-cleanup/modifier-machine.md) — the modifier build plan + the port map + the strategy.
- [`plans/structural-cleanup/readjson.md`](plans/structural-cleanup/readjson.md) — the readJson build (parse + map).
- specs: [`modifier.md`](specs/modifier.md) · [`enabler.md`](specs/enabler.md) · [`tally.md`](specs/tally.md) · [`json.md`](specs/json.md) (§8 classification) · [`skills.md`](specs/skills.md)/[`tags.md`](specs/tags.md)/[`capabilities.md`](specs/capabilities.md) · [`validation.md`](specs/validation.md) (parity).
- Reference impl to port: StoneBase `src/Application/Features/Calc/*` over `src/CascadingModifier/ModifierMath.cs`.
