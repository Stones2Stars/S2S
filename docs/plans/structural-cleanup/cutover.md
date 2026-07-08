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

## ⭐ The master artifact — the comprehensive CODE-CUT MAP (a pure audit) — ✅ PRODUCED

> **✅ Built 2026-07-02 → [`code-cut-map.md`](code-cut-map.md)** (two-pass, adversarial, per
> [DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)): 13 channels × finder+critic, then a fresh
> Pass-2 re-derivation + cross-cutting completeness/StoneBase critics. ~445 grounded cut-sites; the StoneBase
> crosscheck came back PASS (every StoneBase-modelled source has a cut site). It surfaced the genuine Gate-1 gaps —
> the GP-rate BASE economy, building line-of-sight→city vision, `processBonus` health/happiness + commerce-happiness
> accumulators, tally cities-having / religion-corp count semantics, the civic-side policy data gaps (freedomFighter /
> allReligionsActive / bansNonStateReligions), and the property-engine deferral — that must each gain a cascade home
> (or an explicit BLOCKED row) **before their cut**. Work the gates below FROM that map.

**This was a PURE AUDIT: find EVERY place in the code that must cut over to the cascade, and produce a
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

## ⚖ Owner rulings 2026-07-02 — resolving the code-cut-map's gap list

The map's Gate-1 gaps + load-bearing unresolved questions were put to the owner; ruled as follows (full row-level
mapping: [`code-cut-map.md`](code-cut-map.md) §Rulings addendum):

1. **The unported modifier channels are ALL PRE-CUTOVER** — health, happiness, defense, maintenance, buildRate
   (item-cost), GP-rate, and the trade-route scalars each get their calc machine built, shadowed, and cut *within*
   #430; none is deferred past the cutover. They follow the standing machine pipeline
   ([validation.md](../../specs/validation.md)): spec → StoneBase parity → C++ port → in-DLL shadow → cut.
2. **GP-rate BASE economy + building `lineOfSight` → CASCADE HOME** — the Pass-2 whole-subsystem finds
   (`m_iBaseGreatPeopleRate`/`GreatPeopleUnitRate`/`Progress`; `m_iLineOfSight`) are modelled in the cascade
   (pre-cutover), not BLOCKED-deferred. Mechanism per the spec work when their channel is built.
3. **Capabilities (grantor breadth + parameterized grants)** — ⏳ a **dedicated walkthrough, pending** (owner:
   gaps here were expected). Until ruled, the capability rows stay BLOCKED; nothing cuts.
4. **Self-containment classifications** (hasTrait / isGoldenAge / isDisorder / isPower / isGovernmentCenter /
   isActiveCorporation / corporationRevenueModifier) — ✅ **RESOLVED (audit pass-1 2026-07-05 + owner rulings
   same day).** Empirically settled by the audit: hasTrait CLEAN (persisted membership), isActiveCorporation
   CLEAN (raw presence + policy + tech), isGoldenAge/isDisorder covered by ruling 5.3 (raw saved timers),
   corporationRevenueModifier FIXED (derived from held techs). The two remaining, **owner-ruled 2026-07-05**:
   - **`isPower` → KEEP the current power machinery.** *"A city either has power or does not have power"* —
     the cascade reads `CvCity::isPower()` as a raw boolean input; the maintainers (`m_iPowerCount` +
     `changePowerCount` applies + the area clean-power counters) are KEEP rows, NOT in this cutover's
     demolition. **Power mechanics get revisited in a later pass** (the Hoover-Dam area-scope machinery,
     the precipice §4 note) — the cascade power model lands there, never mid-demolition.
   - **`isGovernmentCenter` → a building ATTRIBUTE** (json.md §8 `attributes.governmentCenter` — already the
     model). The counter (`m_iGovernmentCenterCount`) rows are KEEP until the Gate-3 building-attributes lane
     wires; the `IS_GOVERNMENT_CENTER` predicate flips to the cascade operating buildings (active buildings carrying the
     attribute) WITH that lane, not before.
5. **⚖ THE GETTER-CONTRACT CUT STRATEGY (owner ruling 2026-07-02)** — resolves BOTH the entry-point question and
   the self-containment classification wholesale. **The getters are fine — they are the stable CONTRACTS.** The
   cut goes *through* the getters, one by one:
   1. **Instrument** — each legacy getter the cascade replaces gets an event-spine emit INSIDE the body ("cascadeValue"),
      logging the cascade's answer against the legacy return **at the real call moment** — the shadow rides the
      actual consumer calls (per validation.md's end-turn discipline). Gate + aggregate like the existing
      `[ENABLER/shadow]` pattern (per-turn diverging/checked counts, capped samples) — these getters are hot paths.
      **✅ LIVE (2026-07-02) for the modifier pair** — `CvCity::getYieldRate100` + `getCommerceRateTimes100` via
      `Cascade/CvCascadeGetterShadow.{h,cpp}` (`[GETTER/diff]` + per-turn `[GETTER/shadow]` summary, `SD_GETTER`):
      once per (city,channel) per turn at the first real call (full-city coverage, memoized), reentrancy-guarded,
      compute-capped (1024/turn), a single gated int compare when logging is off. Extend the same hooks per getter
      as its cascade counterpart lands.
   2. **Flip** — at clean parity the getter BODY returns the cascade value; the legacy accumulator behind it is
      deleted. **Consumers are never rewired** (this IS the answer to the getYieldRate100-vs-its-consumers
      question: rewire the body, not the call sites).
      **⚡ FLIP ATTEMPT #1 (2026-07-02, commit `71b977e27` → REVERTED `899705ec6`) — the finding that gates the
      real flip: the §1 ACCUMULATOR SUBSTRATE must be built first.** The modifier pair was flipped with a full
      net (a `CascadeRates` service: event-invalidated memo — per-city version + tech/civic/GA epochs + slider
      commerce-epoch + turn-stamp self-heal; legacy kept in-body as `get*100Legacy()` oracles; load path legacy).
      Values were RIGHT (pre-turn city reads sane), but the turn ran **25+ minutes**: what is in C++ is the
      StoneBase **parity CALCULATOR** (a from-scratch source re-walk, ~25ms/compute), not modifier.md §1's
      standing accumulator machine ("the target … never re-walks the sources"). Under AI churn every
      invalidation (techs ×11 players, slider moves, citizen juggling) degenerated to whole-city re-walks, and
      the UI reads rates **every frame** (`game.update.accum` alone burned 209s). A cache over a calculator
      cannot fix this — **the pre-flip increment is the §1 substrate**: standing slots per (city, channel);
      the event spine's domain events apply THAT source's deposit/withdraw deltas; a bounded per-turn pass
      re-checks conditioned deposits (§3 dormancy); the getter reads the slot O(1). **The authoring/runtime
      split (owner 2026-07-02): the JSON stays HUMAN-shaped (source-centric — each entity declares what it
      deposits), and the LOAD step programmatically compiles it into the top-down routing** — so runtime flow is
      pure deposit-DOWN into slots, and no read ever walks back up to the sources. The revert restored the
      shadow-era instrument; the flip returns when the substrate exists.
      **⚡ THE SUBSTRATE WAS BUILT AND THE FLIP LANDED THE NEXT DAY (2026-07-03)** —
      [modifier-substrate.md](modifier-substrate.md): the §1 accumulator (plugin-number components, per-player
      epochs, plots pulled from the CvPlot cache, `CvDerivedCache` built, eager load-end warm-up), getters
      flipped with the `[GETTER]` net + the `[SLOT]` accumulator-vs-calculator net both standing, turn feel at
      baseline. The modifier pair now RUNS ON the cascade in a live game.
      **⚖ The retro finding (owner 2026-07-02): the top-down deposit design "has clearly been lost during
      drycalc and implementation."** Each step was locally correct — StoneBase HAD to full-calc (an offline
      drycalc cannot hold continuous game state; that is exactly why the spec exists), and the C++ port's goal
      was StoneBase parity, so it ported the calculator 1:1 — but the §1 machine itself was never built, and
      the calculator quietly became the de-facto implementation. Consequence for the plan: the substrate build
      is **the actual implementation of the modifier machine as specced**, not a perf optimization; the
      calculator is thereby demoted to its true role — the verification ORACLE the shadows compare against.
   3. **Self-containment dissolves at the contract level** — the cascade reading a sibling getter (`hasTrait`,
      `isPower`, `isGovernmentCenter`, …) is legitimate because each getter is itself cascade-backed after its
      flip. Getters over genuinely RAW saved state (`isGoldenAge`, `isDisorder`, trait membership, occupation/
      anarchy timers) never flip — their maintainers aren't being deleted. **Flip in dependency order** (leaf
      state first) so no flipped getter ever reads a dead legacy value.
6. **The tally count "gaps" are KEEP, not gaps** — the engine objects already carried most of the tally
   functionality, and the deliberate design was to **read those object-owned counts, not rip them up to replace
   with the same thing**. So `countNumBuildings` (cities-having), `CvTeam::getHasReligionCount` /
   `getHasCorporationCount` stay engine-owned; the tally exposes an accessor over them if/when a cascade consumer
   needs one. Their Gate-1 rows reclassify verify→KEEP.
7. **freeSpecialists are MODIFIERS, never grants** — a free specialist is alive **only as long as its source is**
   (building present / civic adopted / trait active), i.e. the continuous modifier shape
   ([modifier.md](../../specs/modifier.md) §Specialist counts), resolving the map's unresolved #75. The
   grants-inventory freeSpecialist-shaped rows reclassify to the modifier plane.

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
  cascade must compute ALL its active state itself and never read a legacy **computed** output — the
  cascade breaks the moment a legacy field is deleted if one remains.
  **PASS-1 SWEEP DONE (2026-07-05, all 37 Cascade files + the flipped getter bodies; per [DEC-all-means-all]
  an ADVERSARIAL second pass is still owed before any cut).** The former known-open `enables.traits` is
  **CLEAN** (`hasTrait` reads the persisted membership array `m_pabHasTrait`, never a computed output);
  `isActiveCorporation` CLEAN (resolves from raw presence + policy flags); dormancy self-containment CLEAN
  (the operating buildings fixpoint computes it; the only `isActiveBuilding` read is a diagnostic comparison). The
  **confirmed legacy-computed ride-ins that gate cuts** (each must gain its cascade home before its
  channel's demolition):
  1. **Wellbeing (5 reads → 2 EXTRACTED + 3 CUT-COUPLED, 2026-07-05):**
     ✅ the SR pair (`getStateReligionHappiness`/`getNonStateReligionHappiness`) is DERIVED in the verdict
     assembly — INITIAL define-seeds + Σ adopted civics' `stateReligion.empire.happiness` /
     `happiness.empire.nonStateReligion` flats (writer census: init + processCivics + the recalc re-seed;
     no other feeder) — the accumulators ride only the Legacy oracles.
     ⚖ the `getExtraHappiness`/`getExtraHealth` trio (city+player) is **CUT-COUPLED, no pre-cut surgery**:
     the cascade already nets out the recomputed derivable trait/tech parts (`wb_extraParts`) and re-adds
     its own nets, so at the demolition — when the derivable process-applies die — the accumulators become
     PURE event/Python stores by construction and ride in under the E-class clean-store ruling. The
     pre-cut requirement (the derivable parts recomputed cascade-side) is already met.
  2. ✅ **`getCorporationRevenueModifier` — FIXED (2026-07-05):** the commerce corporation term now reads
     `CascadeCapabilities::corporationRevenueModifier` (Σ held techs' Info constant, cached on
     `CascadeTeamCaps`, setHasTech-invalidated; one authoring: TECH_STOCK_BROKERING +15). Interim
     static-Info read (the L5-seed class); durable home = the JSON plug at the corp-system rework.
  3. ✅ **`getNationalGreatPeopleRate` — FOLDED (2026-07-05, the L6 class):** `CascadePlayerScope::gpNationalFlat`
     (Σ active traits' `greatPeopleRate.empire.units.*.flat`, PURE-gated; sole legacy feeder processTrait per
     the writer census) serves the flipped `getBaseGreatPeopleRate` via `scGpNational`; the accumulator rides
     only the `*Legacy` oracle. Attribution pair `gpNationalCasc/Leg` on the wellbeing scalars emit.
  4. ✅ **`isPower` + `isGovernmentCenter` — RULED same day (see Rulings #4 above):** power machinery KEEP
     wholesale (revisit at the later power pass); the government-center counter KEEP until the Gate-3
     attributes lane wires. Both predicate reads are sanctioned (cited at the eval sites); neither gates
     this cutover's demolition.
  Confirm-coverage riders (structurally raw, not named in an explicit ruling): `m_aiTradeYield` (the
  sanctioned live trade input) and the `m_aBuildingHappy/HealthChange` E-class stores.
  **THE ADVERSARIAL SECOND PASS RAN (2026-07-05, same day — six angles, none re-deriving pass-1's grep;
  the discriminator validated by a live contrast pair). It found what pass-1 missed:**
  5. **`getFreeSpecialistCount` (4 cascade sites) — ⚖ RULED SANCTIONED as the OUTPUT SEAM (owner
     2026-07-05, the two-part split now in [modifier.md §6](../../specs/modifier.md)):** free specialists
     are (1) AMOUNTS — the cascade's summed `freeSpecialists` deposits (curated: 200 buildings / 12
     civics / 36 traits) → (2) engine PLACEMENT within its parameters (existing infrastructure) → (3)
     consumers deal with the placement's OUTPUT — so the four count reads are the sanctioned seam, the
     promotion-SPA pattern. ✅ **The AMOUNT computer is WIRED and the `any`-bucket COMPOSITION MATCHES legacy's
     split** (`CvCascadeScalarChannels`: `fillFreeSpecialistsCity` folds ONLY the operating buildings' city-scope
     deposits into `any`+typed — legacy `m_iFreeSpecialist` is buildings-fed; the civic/trait folds fill the
     SEPARATE empire buckets `fsEmpireAny`/`fsEmpireByType`, mirroring the typed counts). ⚠ This is SOLVED — do
     not re-open it as a find. Remaining rows only: the live re-verify via the `fsType*`/`fsAny` endpoint pairs
     once the tree loads, and the placement-FEED swap AT the demolition row.
  6. **`getLandmarkHappiness` — ⚖ RULED KEEP (owner 2026-07-05): *"we just leave the existing
     implementation in the game — it is just straight up state derived from the plot in question."***
     Landmarks are diffuse; the mechanic keeps its engine implementation (the civic-fed amount × the plot
     landmark state), the cascade read is SANCTIONED, and modifier.md §2b's raw-input classing STANDS. The
     test save has no landmarks, so it is not integral to the flip; the data modelling of landmarks is a
     POST-MIGRATION pass (**ticket #448**). NOT a wellbeing extraction member — the class stays at five.
  7. **⏳ The civic-parameter unhappiness cluster** — `getForeignUnhappyPercent` / `getCityOverLimitUnhappy` /
     `getCityLimit`: processCivics-fed accumulators consumed as unhappiness FORMULA PARAMETERS (not flat
     happiness). Which lane demolishes them (the happiness-channel cut vs a civic-policy lane) is an
     **owner classification, pending**. (The validating contrast: `getCivicPercentAnger` computes on-read
     from raw adoption — LEGITIMATE-RAW.)
  8. Enumerated-for-transparency (SANCTIONED, owner-ruled scope boundary): the flipped promotion gate's
     `isPromotionValidLegacy(...,true)` conjunct — the ruled cascade seam ("the scope of the cascade ends
     when we have determined what promotions are available"); the ridden legacy reads raw options/flags only.
  All six pass-2 angles otherwise clean (header-inline 0 reads; flipped gates raw-only; area/game/plot
  indirect raw; EvalCtx wiring raw + the three DEC-cited cascade sets; HTTP boundary honest; leaf functions
  fully classified).
- **The grants apply-loop** ([grants-machine.md](grants-machine.md) increment 5). The grants machine *resolves* +
  *shadows* today; before the grants cut it must **apply** — the per-turn recurring (spawn / heal / freePromotions) +
  the trigger grants — replacing legacy's application.
- **The BLOCKED data tail** ([data-migration-remaining.md](data-migration-remaining.md)): `state`/paralyze,
  unitcombat→`tags`, NPC civs, corp-system rework, ranked-target. Each blocks **its** consumer's cut (not the whole
  cutover).

## Sequencing

0. **The code-cut map** ✅ ([`code-cut-map.md`](code-cut-map.md), built 2026-07-02) — the master artifact; everything below executes from it. Its Gate-1 gap list + BLOCKED tail is the pre-cut worklist.
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
