# The cutover — from shadow to cut (#430)

> Scopes the #430 **cutover**: replacing the legacy Info-variable-driven mechanisms with the cascade, and retiring the
> legacy fields. The compute machines (modifier / enabler / grants) + the data + readJson + tally are all **IN** (the
> "get everything in" phase is done); this doc is **what's left to actually CUT**. Authority:
> [validation.md](../../specs/validation.md) §1 (the shadow-then-cut discipline), [cascade-engine-430.md](cascade-engine-430.md)
> §4 (the demolition map).

## ⛔ Cutover is NOT one event — it's several independent cuts

Each legacy mechanism cuts over **separately**, gated by its **own** verification. There is no single "flip the
switch": the modifier, the enabler, the grants, and the classification consumption each cut when *their* verification
is clean. This is deliberate — a monolithic cut is un-verifiable, and the pieces are largely independent.

## ⭐ The immediate next step — the comprehensive CODE-CUT MAP (a pure audit)

**The next work session is a PURE AUDIT: find EVERY place in the code that must cut over to the cascade, and produce a
comprehensive "code-cut map"** (owner ruling 2026-07-01). This map is the **master artifact** of the cutover — every
gate below is executed FROM it. It enumerates:

- every **legacy mechanism to delete** (the accumulators / gates / apply-loops — extends §4's demolition map to the
  full surface);
- every **consumer that reads legacy** and must be rewired (the Gate 3 classification sites + any modifier/enabler
  readers);
- and for each: the **cascade replacement** + the **cut action** (delete / rewire / bridge).

It is simultaneously the **Gate 1 completeness proof**: every StoneBase-mapped source must appear in the map *with a
cut site* — a StoneBase source with NO cut site is exactly the game-breaking gap Gate 1 hunts. Method: **exhaustive +
adversarial** ([DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)) — fan out per subsystem, assume
incompleteness, prove coverage; a self-certified "done" is not enough. Output: a `code-cut-map` doc, the line-item cut
plan the actual cutover then works down.

## Gate 1 — StoneBase-completeness *(the PRIMARY, critical, game-breaking gate — owner 2026-07-01)*

**The critical pre-cutover risk is a source/system StoneBase mapped that is MISSING in the C++ cascade** — when the
legacy is deleted, that contribution silently vanishes (wrong values, missing gates). So the primary gate is
**completeness**: everything StoneBase's parity-proven model mapped must be **taken care of** in the C++ port. A missing
source is game-breaking; a small numeric divergence is not — which is exactly why **parity is SECONDARY** (Gate 2).
*(Owner: reasonably high confidence all systems are in; the job is to CONFIRM nothing StoneBase mapped was dropped.)*

**Status — largely verified.** The modifier + enabler **port-completeness audits** (C++-vs-StoneBase, code-to-code)
already did this per-machine: each enumerated every StoneBase source/term and found + closed the gaps (modifier — civic
building-keyed percent, projects, the `minPosThreshold` MIN; enabler — self-containment, SpecialBuilding caps). readJson
maps every entity with **0 unclassified**. *(Grants is NOT a StoneBase concern — StoneBase never modelled it, being an
offline dry-calc; so grants completeness is judged against the legacy engine, not StoneBase.)* So the critical
pre-cutover step is a **final, comprehensive, ADVERSARIAL StoneBase-completeness sweep** — re-confirm that NO
StoneBase-mapped source/system is unhandled, per the
[DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all) bar (a self-certified pass is not enough — prove
it adversarially; a careful solo pass already missed 77 sources once). **This gate must be clean before ANY cut.**

## Gate 2 — shadow-parity *(SECONDARY — after completeness)*

Parity — the exact numbers matching — is **secondary** to completeness (owner ruling): drive it once Gate 1 is assured.
The machines are built + shadow-wired but **un-run** (owner rule: *no live parity until everything is in* — and now
everything IS in). The step: run the game, drive each machine's per-turn shadow diff to **`diverging=0`**, then delete
its legacy accumulators/gates (§4 demolition map).

- **modifier** — `YieldRate::yieldRate100` / `CommerceCalc::commerceRate100` vs the legacy accumulators
  (`getYieldRate100` / `getCommerceRateTimes100`). Shadow: `cvCascadeModifierShadow`.
- **enabler** — the frontier gates (`canConstruct` / `canTrain` / …) vs the legacy `can*`. Shadow: `CvCascadeEnabler`.
- **grants** — the resolution vs legacy application (needs the **apply-loop** first — see prerequisites).

Mechanical once it runs — the shadows are already wired and decomposed to sub-terms, so a divergence localises to a
named source (the total-observability bar). This is the *"then parity at the end"* phase.

## Gate 3 — the classification CONSUMPTION *(the biggest, largely-parallel piece)*

`tags` / `skills` / `capabilities` / `attributes` / `policies` are **mapped** into `CvJsonInfo`, but the **engine/AI
still read the scattered legacy XML fields**. Before those fields retire, every consumer must read the cascade
classification instead:

| block | consumers to rewire |
|---|---|
| unit **`skills`** | the combat / movement ability code (blitz, amphib, walk-on-peaks, …) |
| unit **`tags`** | the `IS_<TAG>` predicates + the counting (military-happiness / -production / -support, …) |
| empire **`capabilities`** | the team-ability systems (`canTrade`, found-on-peaks, water-move, bridge-building, …) |
| building **`attributes`** | `CvCity` (nukeImmune, governmentCenter, providesFreshWater, borderObstacle, …) |
| civic/trait **`policies`** | the empire-state systems (noForeignTrade, noCorporations, allReligionsBanned, …) |

**Key property — this is mostly INDEPENDENT of the machine shadows.** The machines read the classification *internally*
(the enabler's predicates, the modifier's active traits); *this* gate is the engine-**behaviour** consumption. It is a
large, breadth-first rewiring ("a lot of places") and can proceed **in parallel** with the shadow runs. It is the
**long pole** of the cutover.

## Prerequisites feeding the cuts

- **Self-containment audit ([DEC-calc-zero-ride-in](../../architecture/decisions.md#dec-calc-zero-ride-in)).** The
  cascade must compute ALL its active state itself and never read a legacy **computed** output. Known-open:
  `enables.traits` → the empire active-trait HAVE. Before any cut, **audit** for other legacy-computed reads — the
  cascade breaks the moment a legacy field is deleted if one remains.
- **The grants apply-loop** ([grants-machine.md](grants-machine.md) increment 5). The grants machine *resolves* +
  *shadows* today; before the grants cut it must **apply** — the per-turn recurring (spawn / heal / freePromotions) +
  the trigger grants — replacing legacy's application.
- **The BLOCKED data tail** ([data-migration-remaining.md](data-migration-remaining.md)): `state`/paralyze,
  unitcombat→`tags`, NPC civs, corp-system rework, ranked-target. Each blocks **its** consumer's cut (not the whole
  cutover).

## Sequencing

0. **The code-cut map** (pure audit — the next session) — the master artifact; everything below executes from it.
1. **StoneBase-completeness** (Gate 1) — the final adversarial sweep; confirm nothing StoneBase mapped is missing. THE gate.
2. **Shadow-parity** (Gate 2, secondary) — drive each machine to `diverging=0`; cut its legacy as it goes clean.
3. **Classification consumption** (Gate 3) — in parallel; the long pole.
4. **Grants apply-loop + self-containment audit** — before their respective cuts.
5. **→ push to `main`.** The cascade on `main` is the endgame of #430. (Then the leaderhead trait remap, by another modder.)

## Explicitly POST-CUTOVER (after `main`)

- **⏳ Leaderhead trait remap (traits → leaders) — POST-CUTOVER, AFTER `main` (owner ruling 2026-07-01).** This is
  **NOT a cutover blocker**: **leaders work WITHOUT traits**, and developing leaders works fine — so the trait-to-leader
  assignment is deliberately deferred until *after* the cascade is merged to `main`. **Why `main` specifically:** the
  trait-to-leader work will be done by **another modder**, and we want the new cascade system **on `main` before they
  start**, so they build on the new system rather than the old. (The leaderhead trait assignments were already
  **stripped** in the data migration; this remap is the deferred, hand-off-to-another-modder step — hence its home is
  here, not the pre-cutover tail.)

## See also
- [validation.md](../../specs/validation.md) — shadow-then-cut. · [cascade-engine-430.md](cascade-engine-430.md) §4 —
  the demolition map. · [grants-machine.md](grants-machine.md) — the grants apply (increment 5). ·
  [data-migration-remaining.md](data-migration-remaining.md) — the blocked data tail.
