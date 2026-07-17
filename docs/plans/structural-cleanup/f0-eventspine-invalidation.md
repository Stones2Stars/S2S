# F0 — the event-spine-driven single cache-invalidation foundation

> The build plan for making the event spine drive ALL cache invalidation, to spec
> ([event-spine.md](../../specs/event-spine.md), [state-repositories.md](../../architecture/state-repositories.md)).
> Grounded against the live code (branch `json-data-migration`). ⛔ The existing code is SUSPECT, not a baseline —
> where it diverges from this design, rebuild to the design.

## The target (single invalidation philosophy)

**state change → DOMAIN `emit` → the cache-invalidation `IEventConsumer` → routed by the deposit index → exactly the
affected packages mark themselves dirty.** One event surface, one consumer, one routing index; every other
invalidation path deleted. A cache invalidates **iff** a DOMAIN event whose source (per the deposit index) feeds it
fires. Consequences: the per-turn cost is event-proportional by construction; completeness is non-negotiable (a
missing emit = a cache that never invalidates, [DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).

**The SAME emit surface builds the cascade on load, too — one surface, two consumptions of state.** During play,
incremental DOMAIN events drive the invalidation consumer above (only touched packages rebuild). On LOAD the
**reseed** ([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed), R6) — the save read itself fires the
DOMAIN events for every fact as it deserializes — so the same consumer builds the WHOLE cascade once, the ONE full
build. Load's events come from the read, play's from live state-changes; there is no separate load path. This **RETIRES the recompute-on-load**
(`playerSliceRebuild` + `worldRebuild` + `recalculateModifiers` — a drift-stabilizing stopgap) and there is **NO
per-turn blanket**: `doTurn` never does a full per-player rebuild; only dirty packages ensure. Building the proper,
COMPLETE spine FIRST is what makes both safe — no unhooked mutation, so nothing stales and no self-heal is needed.

## Current state — the top-line divergence

The target is **not implemented**. The spine and the invalidation machinery are two disjoint systems:

- `CvEventSpine` emits DOMAIN events but its ONLY registered consumers are logging + grants
  (`CvEventSpine.cpp:297-313`). **No cache-invalidation consumer exists.**
- Invalidation is a parallel hand-wired path: direct static calls `CascadeAccumulator::buildingProcessed /
  dirtyCity / markPlayerScopeAndCities / cityHaveChanged` wired straight into the `Cv*` mutation choke points —
  never through `emit`/`onEvent`.

F0 collapses these into one.

## Reusable as-is (spec-conformant — keep)

- The spine dispatch primitive: `emit` / `IEventConsumer` (`CvEventSpine.h:207-213`) / the KIND enum
  (`EVENTKIND_DOMAIN/DIAGNOSTIC/TRACE`, `h:27-33`) / the interest-mask. ✅ Matches event-spine.md.
- The plot-yield PULL model (plot substrate lazy-dirties `m_yieldCache`; city yield combine PULLs `getPlotYield`
  live — `CvCascadeAccumulator.cpp:357`). No city mark needed; conformant to state-repositories.
- The package substrate `CvCascadeScopePackages.h` — **flat and percent packages already carry SEPARATE bits**
  (the percent-vs-flat split), ready for granular routing. The dirty-bit vocabulary IS the (scope × channel) axis:
  - city `CascadeCityPkg` (`:91-116`): `CPK_YPCT/YSPEC/YEXTRA/CSPEC/CPCT/CBASE/WB/SCFLAT/SCPCT/SCSPEC/BR/FRONT_B/U/PP`
  - player `CascadePlayerPkg` (`:195-209`): `PSC_YFLAT/CFLAT/WB/SC/BR/FRONT_P/PROMO`
  - world `CascadeWorldPkg` (`WSC_ALL`)
- The operating-buildings targeted propagation (`EnablerKernel::recomputeOperatingBuildingsInto`; ripples via
  `onBuildingChangedActive`/`onHaveChangedActive`/`onPlayerScopeChangedActive`).
- ✅ **Epochs/stamps are already deleted** (increment I). Grep for `bumpEpoch`/`CvCascadePlayerStamp`/`s_iEpoch` =
  zero live defs. The only blankets left are the `markAllDirty` calls in `playerSliceRebuild` + `markPlayerScopeAndCities`.
- The perf census plumbing/flush (`cvCascadeModifierPerfCensus`, `CvCascadeModifierMath.cpp:160-210`) + the
  `CascadePerf` struct (`CvCascadePerfCount.h`) — the right home for the new reporting counters.

## Must rebuild (the real F0)

### R1. `CvCascadeEvent` DOMAIN payload → carry the SOURCE LOCATION
Today the DOMAIN payload is empire-count-shaped: `CASCADE_EVT_BUILDING_COUNT` carries `iType=building, iA=empire
count, iB=delta, iC=player` (`CvEventSpine.h:177-186`). It **cannot route** to a specific `CvCity`'s packages — no
city/plot/area id. Extend the DOMAIN payload to a source-carrying shape: `(sourceKind, sourceId, ownerPlayer,
cityId|plotId|areaId, delta)`. Keep the logging-mode payload as-is (`aFields[]`).

### R2. Reverse routing surface on `DepositIndex`
`DepositIndex` was a FORWARD map only: `source CvInfo* → its CascadeDeposit records` (compiled interned segments
family/scope/member/target + FK — `CvCascadeDepositIndex.h`, built at readJson push). The routing was re-derived
INLINE, by hand, **for buildings only**, in `buildingProcessed`; tech/civic/trait/bonus/project used the blanket.

**R2a — LANDED (the reverse route at scope×unit×member granularity).** `DepositIndex::routeFor(const CvInfo*)`
returns a compiled `SourceRoute {playerBits, cityBits, world}` — the source's cross-scope reach, unioned over its
deposits and lazily cached (dropped by `clearCompiled` on re-map). The body is a **VERBATIM transcription** of
`buildingProcessed`'s former inline loop, generalized to any source info: a percent empire/area deposit fans to every
owner city's percent stacks (`CPK_YPCT|CPCT|SCPCT|BR`) + the player gp/maint/buildRate sums (`PSC_SC|PSC_BR`); a flat
one to the player flat/wb/keyed sums (`PSC_SC|PSC_CFLAT|PSC_WB`) + the sibling `CPK_CBASE|WB` keyed realization (or
`CPK_YSPEC|CSPEC` for `specialist.perSpecialist`); a world deposit sets the world flag. `buildingProcessed` now
QUERIES it (the inline loop + its duplicate is DELETED — the delete-list item); the derivation is provably identical
(transcription), so building invalidation is unchanged. This is the **single routing-truth surface** the consumer
narrowing needs. It carries ONLY the cross-scope reach — the building's OWN-city conservative-ALL floor (the
operate/provides fixpoint) stays on the consumer.

**R2b — the PERCENT per-channel narrowing (LANDED).** `routeFor`'s percent branch is now per-CHANNEL, grounded from
the fills' channel strings: a yield-percent empire/area deposit (`food`/`production`/`commerce`) marks the siblings'
`CPK_YPCT` **only** and NO player bit (the player scope holds no yield/commerce percent — verified against
`CascadePlayerScope`, so `buildingProcessed`'s old coarse `PSC_SC|PSC_BR` on it was pure waste); commerce-percent
(`gold`/`research`/`culture`/`espionage`) → `CPK_CPCT`; `greatPeopleRate`/`maintenance` → `CPK_SCPCT`+`PSC_SC`;
`buildRate` → `CPK_BR`+`PSC_BR`. The GROUPED families (`defense`, `stateReligion` — seg[0] is not a plain channel) and
any unrecognized family fall to the coarse-safe percent mask, so it never under-marks. The FLAT branch stays coarse:
its pulled-live-vs-city-realized routing is NOT nailed per family and is NOT guessed (below).

**R2b — the CONDITIONER narrowing is NOT a `routeFor` swap; it is the invalidation-runtime hard-look (needs a
condition-reverse-index).** The empire sources (tech/civic/trait/SR/project) still ride `markPlayerScopeAndCities`'s
broad `PSC_ALL` + every-city `CPK_ALL`. It is **wrong to narrow this to `routeFor(source)`**: a source's own deposits
are only half its footprint — the other half is **every OTHER info's deposit gated on it** (`enabled:{CIVIC_X}`,
`{TECH_X}`, era-threshold flats). When CIVIC_X is adopted, a Forge's `happiness enabled:{CIVIC_X}` deposit goes
live and that city's package restales — a fan-out `routeFor(civic)` does NOT see. The broad mark is load-bearing for
exactly this; narrowing it correctly needs a **condition-reverse-index** (`referenced-type → the deposits/packages
gated on it`), a NEW compiled index that intersects the predicate registry + the existing hand-mask events (power/
bonus/religion/GA already emit) — squarely the "real hard look at the cascades" (the consumer/invalidation runtime),
sequenced AFTER nailing the deposits. It must also keep the frontier + operate ripple + policy re-arm (which ride the
enables/requires index, not the deposit index). Correctness-gated at crutch-removal (playtest), never a blind flip.

**⚠ Latent gap the R2 grounding exposed (crutch-removal completeness, not an R2 regression):** `buildingProcessed`'s
FLAT branch marks siblings only for `buildings`-keyed (`CPK_CBASE|WB`) / `specialist` (`CPK_YSPEC|CSPEC`) empire flats
— a plain `{happiness|health}.empire.flat` (fed into every city's BAKED `aWbVerdict` via the player `wbEmpireByFam`
fold) gets only the player `PSC_WB` mark, NOT the sibling `CPK_WB` re-bake. The self-heal covers it today; it is a
real invalidation to add before the crutch comes off (it predates R2 — `routeFor` transcribes it faithfully).

**THE DEPOSIT→PACKAGE-BIT ROUTING TABLE (the known-data spec — verified from the fills, all cited; abbrevs: PS
`CvCascadePercentStack`, YBP `…YieldBasePackages`, BP `…BuildingPackage`, CC `…CommerceCalc`, WB `…Wellbeing`, SC
`…ScalarChannels`, MMK `…MMKernel`, Acc `…Accumulator`). `yield`=food/production/commerce; `commerce`=gold/research/
culture/espionage; each channel traverses an identical shape set (expand `{ch}` mechanically).**

*City packages:*
- **CPK_YPCT / CPK_CPCT** (`percent`): `{ch}.city.percent` + `{ch}.area.percent` (building) + `{ch}.empire.percent`
  (building/civic/trait/project) + `{ch}.empire.buildings.{B}.percent` (civic, city-realized keyed — MMK:401). PS:98-124.
- **CPK_YSPEC / CPK_CSPEC**: `{ch}.city.flat`+`.city.percent` (specialist intrinsic/local) + `{ch}.empire.flat`
  (specialist perType) + `{ch}.empire.specialists.{SPEC}.flat` (trait) + `{ch}.empire.specialist.perSpecialist`
  (building/civic/trait). YBP:80-127.
- **CPK_YEXTRA**: `yield.city.flat` + `yield.city.perPopulation` (building; incl. `whenObsolete`). BP:58-67.
- **CPK_CBASE**: `commerce.city.flat` (religion) + building own-flat/perPop (BP) + `commerce.empire.headquarters.
  perCorporationLevel` (corp) + doubleTime re-adds; **+ realized** `cKeyed100`←`commerce.empire.buildings.{B}.flat`
  (building grantor, from PSC_CFLAT) + `iCSrMatch`×`cSrPool`. CC:38-303, Acc:134-143.
- **CPK_WB**: building `commerceHappiness.city.{c}.flat` + `{happiness|health}.city.flat|perPopulation`
  (BASE/BONUS_/TECH_/STATE_RELIGION-classified); civic/trait `{fam}.empire.flat`, `.empire.buildings.{B}.flat`,
  `.empire.features.{F}.flat`, `.empire.cities.flat`{unit:IS_MILITARY|ranked}; bonus/corp/tech/project/specialist
  `{fam}.*.flat`; feature `{fam}.plot.percent`; civic `stateReligion.empire.happiness.flat` + `happiness.empire.
  nonStateReligion.flat`. **+ realized** from PSC_WB (empire/area/buildingKeyed). WB:89-483.
- **CPK_SCFLAT**: `greatPeopleRate.city.flat` (building) + `tradeRoutes.city.flat` (building) + `tradeRoutes.empire.
  flat` (civic/trait/tech) + `tradeRoutes.empire.coastal.flat` (civic/trait) + `freeSpecialists.city.{any|SPEC}.
  count` (building). SC:143-567.
- **CPK_SCPCT** (`percent`, +`scDefMin` flat): `greatPeopleRate.city|empire.percent` (bld+civic/trait) +
  `stateReligion.empire.greatPeopleRate.percent` + `defense.city.{amount|bombardDefense}.percent` + `defense.city.
  min.flat` + `maintenance.city|empire|area.percent` (+tech) + `maintenance.empire.{homeArea|otherArea}.percent`. SC:204-545.
- **CPK_SCSPEC**: `greatPeopleRate.city.flat` (specialist × count). SC:154.
- **CPK_BR** (`percent`): `buildRate.{city|empire}.{units|buildings|domains|unitCombats|specialBuildings}.{KEY}` +
  `.{city|empire}.{military|space}` (bld+civic/trait+corp) + `stateReligion.empire.{unit|building}Production` +
  `buildRate.empire.{world|team|national}Wonder` (civic/trait). SC:762-816.

*Player packages (building-sourced per source-city unless noted):*
- **PSC_YFLAT**: trait `yield.empire.flat` + `yield.empire.goldenAge.flat` (ungated). YBP:30-54.
- **PSC_CFLAT**: trait `commerce.empire.flat` + heritage `commerce.empire.flat` (era-gated) + trait `.empire.
  goldenAge.flat` + **building** `commerce.empire.buildings.{B}.flat` (grantor ledger `cKeyedLedger`) + `cSrPool`
  (config). CC:58-177.
- **PSC_WB**: building `{fam}.empire.flat`→`wbEmpireByFam`, `.area.flat`→`wbAreaByFam`, `.empire.buildings.{B}.
  flat`→`wbBuildingKeyedByFam`. WB:178-229.
- **PSC_SC**: building `greatPeopleRate.empire.percent`, `maintenance.empire|area.percent` (+connectedCity/otherArea
  members, +project), `tradeRoutes.empire|coastal|world.flat`, `defense.empire.{amount|bombardDefense}.percent`
  (+civic/trait), `freeSpecialists.empire|area.count` (+civic/trait), trait `greatPeopleRate.empire.units.{U}.flat`.
  SC:570-705.
- **PSC_BR**: building `buildRate.empire.{keyed|military|space}.percent`. SC:818-844.

*World:* **WSC_ALL** = Σ players' `tradeRoutes.world.flat` (building, via PSC_SC) + project world flats. Acc:364-381.

**⛔ THE 8 CROSS-SCOPE REALIZATIONS — an invalidation on the deposit's ADDRESS scope must mark the DESTINATION
package; the reverse index MUST encode these, NOT a naive scope→package map:**
1. civic `{ch}.empire.buildings.{B}.percent` → CITY CPK_YPCT/CPCT (paid while B active in the reading city).
2. building `commerce.empire.buildings.{B}.flat` → PSC_CFLAT ledger, realized into CITY CPK_CBASE ×active B (two-hop).
3. building `stateReligionCommerce` (config) → PSC_CFLAT `cSrPool`, realized via CPK_CBASE `iCSrMatch` (city).
4. building `{fam}.empire|area.flat` → PSC_WB, threaded into every same-area/all-city CPK_WB verdict.
5. building `{fam}.empire.buildings.{B}.flat` → PSC_WB keyed, realized into CPK_WB ×active B.
6. **civic/trait `*.empire.*` are CITY-REALIZED, not player-scope**: the whole percent stack (YPCT/CPCT), gp/maint/
   trade scalars, all buildRate legs, `maintenance.empire.{home|other}Area` — evaluated against THIS city's ctx and
   stored city-side (conditions reference the city). The player packages hold ONLY the per-source-city *building* walks.
7. building `greatPeopleRate.empire.percent` → PSC_SC (player half), composed with the civic/trait CITY half at read.
8. `tradeRoutes.world.flat` → world→player→city three-level cascade (WSC_ALL read by every city).

**Non-package LIVE reads (cached NOWHERE — NOT missing invalidations):** `buildRate.self.percent` (produced item,
live in `productionModifier`/`acc_brSelf`); the `unit:IS_MILITARY` ×count military-happiness fold (live at read);
`commerce.city.percent` on a Process (live in `combineSplit`).

### R3. The cache-invalidation `IEventConsumer`
`wantedKinds() = 1<<EVENTKIND_DOMAIN`; `onEvent(e)` queries the R2 reverse index for `e.sourceKind/id` → the affected
(scope, packageBits) → marks them dirty on the right owner (via `dirtyCity` / player-scope / world), including the
operating-buildings ripple where relevant. Registered in `cascadeRegisterConsumers()`. This REPLACES the hand-wired
choke-point calls and both blankets.

**STATUS (built, STAGED — `CvCacheInvalidationConsumer` in `CvCascadeInvalidation.cpp`).** The consumer is built + registered,
routing every play-time DOMAIN event to the per-source mask (masks lifted VERBATIM from the `CvCity.cpp` mutation
sites, so it is mask-equivalent to the hand-wiring; R2's derived masks are the follow-up). It is **load-inert**
(`spineGameLoadInProgress()` → return): mid-reseed the targeted ripples are invalid because the frontier/operating-
building reverse indices aren't built until `onFinalInitialized`; the load warm-up builds the cascade. Registered
**ADDITIVELY** — the hand-wired mutation-site marks AND the per-turn self-heal remain in place, so R3 currently
double-marks (harmless) and changes no behaviour; this lets the routing be verified firing live with zero corruption
risk.

**⛔ THERE IS NO "FLIP" — it already happened (owner correction).** The getters are ALREADY on the cascade. The flip
was done, and it went **catastrophically wrong** (bad rollerskating built a broken invalidation), so the owner
reverted to the **self-heal + hand-wired marks as a CRUTCH** and is forcing each invalidation piece (reseed → R3 →
this observability) to be built + **VERIFIED WORKING individually** before the crutch comes off. The remaining work is
therefore not a flip — it is **removing the self-heal crutch** once the invalidation is proven complete. (Performance
is NOT a concern until the cascade is actually rebuilt this way — the crutch's per-turn cost is irrelevant now.)

**Why the crutch can't come off blind:** with the getters already on the cascade, a missed invalidation is a wrong
value that **IS the `/computed` oracle** — no legacy left to diff, so polls can't detect it. The self-heal's own
comment names "power flips, bonus-network shifts, **timers**" as mutations it silently covers. Removing it needs a
proven-complete invalidation surface + a live playtest for value-correctness — never a blind deletion. **This is why
`SEVT_CACHE_INVALIDATE` exists: so each invalidation ANNOUNCES itself (`[CASCADE] invalidate scope/id/pkg/src`) and
the pieces can be VERIFIED, not assumed.**

**CRUTCH-REMOVAL RECIPE (do with the owner playtesting, once each piece is verified):**
1. **The per-turn self-heal blankets are ALREADY DELETED** — `playerSliceRebuild` (`CvPlayer::doTurn`) and the
   per-turn `worldRebuild` (`CvGame::doTurn`) are gone (tombstone comments at `CvPlayer.cpp:3710` / `CvGame.cpp:6167`
   name the removal); the LOAD warm-up (`CvGame::onFinalInitialized`) + `cityCreated` are KEPT. So the standing crutch
   is now ONLY the hand-wired play-time marks (step 3) running alongside R3 (harmless double-mark).
2. **Completeness audit — DONE (see the R4 completeness map above).** Every source-kind's emit is wired. The gap table
   status: **G1/G2/G3/G4/G5 WIRED**, **G6 resolved** (slider = live calc, no emit), the **connection:vicinity leg WIRED**
   (`SEVT_VICINITY_BONUS_CHANGED` routes the building-active mask + the bonus operate ripple; the network half rides
   the provides→plot-group fan-out). The **era/nukes/holy-city consumer routes are WIRED** (see the Tier-1 sweep).
   **REMAINING: the L1 culture-level latent** + the noted bare-`IS_HOLY_CITY` `requires.build` frontier gate (a
   predicate with no FK bucket — surfaced, not skipped). Confirmed NOT gaps: the
   **timer/counter mutations** the self-heal was thought to cover (war-weariness, anger/espionage timers, culture,
   inflation) are RAW-STATE **live-at-read** wellbeing inputs (modifier §2b), folded at the combine and never stored —
   they stale NO package and need NO emit. The `[CASCADE] invalidate` stream stays the live instrument to confirm the
   fixes as they land.
3. **Remove the hand-wired play-time marks** now duplicated by R3 — the `CascadeAccumulator::` mark calls still at the
   `CvCity`/`CvPlayer`/`CvTeam` mutation sites (line numbers drift; grep `CascadeAccumulator::(dirtyCity|buildingProcessed|
   markPlayerScopeAndCities|cityHaveChanged)` in `Sources/Engine`). ⚠ Per-site check required: a mark whose choke does
   NOT emit must STAY — e.g. `CvPlayer::changeLeader`'s `markPlayerScopeAndCities` (G5) is the ONLY invalidation on that
   path (it bypasses `setHasTrait`'s emit), and the `CPK_YRATE` worked-plot/citizen-churn marks (`CvCity`/`CvPlot`) are
   NOT R3-duplicated (no event carries them) — both KEEP until they get their own events or a KEEP ruling.
4. **Verify LIVE + PLAYTEST** — `/computed` oracle values stay correct across many turns; **the owner plays** to
   confirm no drift the oracle can't see.

### R4. The DOMAIN emit surface — COMPLETENESS MAP (audited)

The per-source emit surface is WIRED: every source-kind emits a source-carrying DOMAIN event at a play-time choke,
and the reseed fires the same events from the save read. Wired play-time chokes (verified):
`emitBuildingChanged` (`CvCity.cpp:5254`, `processBuilding`), `emitBonusChanged` (`:4621`, `processBonus` — the sole
access choke, `changeNumBonuses`→`processNumBonusChange`→`processBonus`), `emitPopulationChanged` (`:7046`),
`emitPowerChanged` (`:10309`, `changePowerCount` — the sole power choke; bonus/building/direct power all route
through it), `emitSpecialistChanged` (`:14288`), `emitReligionChanged` (`:15441`), `emitCorporationChanged`
(`:15678`), `emitGoldenAgeChanged` (`CvPlayer.cpp:9467`), `emitStateReligionChanged` (`:12595`), `emitCivicAdopted`
(`:14406`), `emitTraitChanged` (`:29433`, `setHasTrait`), `emitProjectChanged` (`CvTeam.cpp:4266`), `emitTechChanged`
(`:4917/4943`), plot substrate + owner/working-city (`CvPlot`). `buildingProcessed` derives its cross-scope
player/world masks from the building's compiled deposits (`CvCascadeAccumulator.cpp:905-950`), not a blanket.

**So the completeness question is (a) is each choke the COMPLETE set for that fact, (b) is the R3 mask correct, (c)
is any package-feeding fact emitted NOWHERE. The audited gap list (adversarial second pass):**

| # | Gap | Package(s) stale | Root | Fix |
|---|---|---|---|---|
| **G1** | **Heritage emits nothing** — `CvPlayer::processHeritage` (`:31117`), the `m_myHeritage` push/erase (`:31096`), and the `recalculateModifiers` re-run (`:28928`) all fire no spine event | `PSC_CFLAT` (heritage empire commerce flats, era-stacked) | no emit at all; mid-game via `MISSION_HERITAGE` | add `emitHeritageChanged` endpoint + R3 route → `PSC_CFLAT` |
| **G2** | **Specialist route omits `CPK_WB`** — R3 `SEVT_SPECIALIST_CHANGED` marks `YSPEC\|CSPEC\|SCSPEC` only (`CvCascadeInvalidation.cpp:76-85`) | `CPK_WB` | specialists deposit §2b happiness/health (`wbTerms.spec`, folded at fill) | add `CPK_WB` to the specialist mask |
| **G3** | **Bonus route lacks the operate-dormancy ripple** — R3 `SEVT_BONUS_CHANGED` does `dirtyCity` only (`:111-121`); unlike religion/corp it never calls `cityHaveChanged`/`onHaveChangedActive` | operating-building set + every active-gated package (`YEXTRA`, `BR`, …) | a bonus connect/disconnect flips building `requires.operate` dormancy (enabler §3.2) | add the operate ripple + broaden the mask |
| **G4** | **`buildingProcessed` excludes `YSPEC\|CSPEC`** (`CvCascadeAccumulator.cpp:900`) yet 7 buildings deposit `perSpecialist` (Sistine `:26`, Westminster `:31`, Karnak `:20`, Crystal Palace `:39`, Penicillin `:17`, East India Co `:22`, Louvre `:22`) folded into `CPK_YSPEC/CSPEC` | `CPK_YSPEC`, `CPK_CSPEC` | building mask excludes the specialist bits | include `YSPEC/CSPEC` when the building authors a specialist-yield deposit (derive from its deposits) |
| **G5** | **`changeLeader` bypasses `setHasTrait`** — `CvPlayer::changeLeader` (`:1551`, Python/WorldBuilder-reachable) removes old + adds new leader traits via direct `processTrait` (`clearLeaderTraits:29930`, `:1583-1610`), no `emitTraitChanged`, no `markPlayerScopeAndCities` | every trait-fed player/city package | a runtime trait-change path that isn't the emit choke | emit trait changes (or `markPlayerScopeAndCities`) from `changeLeader` |
| **G6** | **Commerce slider → `CPK_WB` is baked at FILL** — `setCommercePercent` (`:13041`) is read in `CascadeWellbeing::assemble` (`:488-492`), so the slider's happiness rides the stored verdict | `CPK_WB` (within-turn only) | a LIVE calc wrongly cached at fill | RESOLVED (owner): a slider yield is **1 live calculation — it changes NO cache**. Move the commerce-slider happiness to the LIVE-at-read fold (on top of the military-free verdict, where tax-rate unhappiness et al. already live per modifier §2b) — NOT an emit. `extraHappiness/Health`: same treatment if slider-cheap; otherwise ordinary deposit-driven marks |

**WIRING STATUS.** G1/G2/G4/G5 are WIRED (working tree, Assert-green): G1 = `emitHeritageChanged` endpoint
(`SEVT_HERITAGE_CHANGED`) + `setHeritage` emits + the load reseed + the R3 route → `PSC_CFLAT` (mark only the player
package; the boundary ensure rolls it to cities next slice, matching the plain empire-commerce-flat case); G2 =
`CPK_WB` added to the specialist route; G4 = `buildingProcessed` adds sibling `CPK_YSPEC|CSPEC` on a
`<ch>.empire.specialist` (`perSpecialist`) deposit — the empire-wide `getBuildingCount` fold reaches every city;
G5 = `markPlayerScopeAndCities` in `changeLeader` (the runtime path that bypassed `setHasTrait`). **G3 is WIRED** —
the bonus operate-dormancy ripple: `EnablerKernel::onBonusAccessChangedActive(city, bonus)` re-checks ONLY that
bonus's operate consumers via the existing `s_opBonusConsumers` reverse-FK index (`CvEnablerKernel.cpp`); the
whole-set variant `CASC_HAVE_BONUS` (`s_opAnyBonus`) fires on a plot-group MEMBERSHIP shift. Routed from the three
forward bonus events (`SEVT_VICINITY_BONUS_CHANGED` + `SEVT_PLOTGROUP_BONUS_CHANGED` per member city →
`cityBonusAccessChanged`; `SEVT_CITY_NETWORK_CHANGED` → `cityHaveChanged(CASC_HAVE_BONUS)`), and their masks broadened
from the bonus-conditioned subset to the **building-active footprint** (`CPK_ALL & ~(YSPEC|CSPEC|SCSPEC)`, matching
`buildingProcessed`) so a flipped operate-gated building's YEXTRA/SCFLAT/BR deposits re-sum against the ripple-updated
active set. The legacy per-turn dynamic poll (bonuses set `d.dynamic`) stays as the backstop until the
crutch-removal playtest verifies the discrete path.

**BONUS-ACCESS EMIT MODEL (owner-ruled — supersedes the single narrow bonus route).** A bonus condition reads TWO
independent sources (`CvCascadeConditionEval.cpp`): `CASC_CONN_TRADE → hasBonus` (the **network**/connected count,
`m_paiNumBonuses`) and `CASC_CONN_VICINITY → hasVicinityBonus` ∪ `vicinityProvidedBonuses` (the **radius**). They
change at different times, so bonus access needs THREE triggers, all keyed on the plot-group identity the city
already carries (`getPlotGroupId(owner)`; a city is in exactly ONE group per owner, and a group SPANS continents —
it's connectivity, not area):

| Trigger | Hook | Fans out to | Invalidates |
|---|---|---|---|
| **Vicinity** — a resource appears/leaves a city's workable radius (an improved tile, or an active provider) | `SEVT_VICINITY_BONUS_CHANGED` (a per-CITY event — emitted at `doVicinityBonus` for tile flips + `processBuilding` for provider flips) | the emitting city only (vicinity is a CITY-LOCAL fact) | vicinity packages (`YPCT\|CBASE\|CPCT\|WB\|SCPCT`) |
| **Network resource** — a plot-group gains/loses a resource (trade import lands at the CAPITAL's group `CvPlayer:13425-13433`; route connect; deplete) | new `emitPlotGroupBonusChanged` at `CvPlotGroup::changeNumBonuses:505` (on the group presence transition) | the group's member cities (`plotGroup(owner)==id`, the filter the engine already uses at `:509`) | network packages + operate ripple (G3) |
| **Network membership** — a city becomes part of a group (merge/split reassigns its plot; a new city joins) | the city→group association change (`CvPlot::setPlotGroup` on a city plot; founding covered by `cityCreated`) | that one city | network packages + operate ripple |

The engine's connectivity model IS the network model — the capital is the entrypoint of a traded resource, and a
resource reaches exactly the cities connected to the capital (its plot-group). We ride it; no cascade-native network
model.

**WIRING STATUS (Assert-green, ADDITIVE — staged like R3).** The NETWORK axis is wired, both triggers:
- **Network resource** — `SEVT_PLOTGROUP_BONUS_CHANGED`, emitted at `CvPlotGroup::changeNumBonuses:505` on the group
  presence transition; R3 fans out to the group's member cities (`getPlotGroupId(owner) == iSrcLoc`).
- **Network membership** — `SEVT_CITY_NETWORK_CHANGED`, emitted at `CvPlot::setPlotGroup:8877` when a city's OWN
  center plot moves group, **owner-gated** (`pCity->getOwner() == ePlayer` — a plot is in one group PER PLAYER, so
  only the owner's move touches the city's network); R3 invalidates that one city.

Both mark the bonus-conditioned mask `YPCT|CBASE|CPCT|WB|SCPCT` and run ALONGSIDE the legacy per-city
`SEVT_BONUS_CHANGED` (no retirement yet → harmless double-mark) so they verify live first. The resource + membership
pair is complete by construction: on a merge, the ABSORBED-group city gets the membership event (its center moved),
the SURVIVING-group city gets the resource event (its group gained resources).

The **connection:vicinity (RADIUS)** axis is WIRED (owner model — supersedes both the fat-cross recompute AND the
pending "city plot-gain/loss hook" / plot→cities ring-walk). Vicinity is a **CITY-LOCAL presence fact** (enabler.md
§8 — "VICINITY belongs to the CITY"), so it is announced by the per-CITY `SEVT_VICINITY_BONUS_CHANGED` (emitted at
`CvCity::doVicinityBonus` for tile flips and `processBuilding` for provider flips) and its R3 route marks **only the
emitting city's** bonus-conditioned packages (`YPCT|CBASE|CPCT|WB|SCPCT`, same mask as `SEVT_BONUS_CHANGED`). There is
**no plot→cities structure** — the network half is a SEPARATE fact carried by the group: a provider that makes a bonus
tradeable injects it into the city's plot group (`processBuilding` → `CvPlotGroup::changeNumBonuses`), which fires
`SEVT_PLOTGROUP_BONUS_CHANGED` on the presence transition (`CvPlotGroup.cpp:525`) → the member-city fan-out. The city
already carries its plot-group id and the group its member cities — the plot group **is** the reverse-mapped
membership, so no ring walk and no per-plot city list is ever built. STILL STAGED for crutch removal: the
**operate-dormancy ripple** (G3 — the `CASC_HAVE_BONUS` enabler follow-on: `YEXTRA/BR` + frontier re-check when a
`requires.operate` vicinity/network bonus flips; `CASC_HAVE_BONUS` is not yet a `CascadeHaveKind`), and retiring the
per-city `SEVT_BONUS_CHANGED` cache role (`CvCity::processBonus` keeps only its legacy accumulators).

**Areas** are the orthogonal geography axis (`recalculateAreas`, plot-type land↔water only, NOT ownership) — the
`maintAreaPct`/`wbAreaByFam` latent, rare, no emit yet.

**Wellbeing verdict — live, not baked (owner direction).** The wellbeing splits into cached TERMS (`wbTerms.*`, the
deposit sums — the expensive part, invalidated by their source events, e.g. G2's specialist→`wbTerms.spec`) and the
VERDICT (`happy/unhappy/good/bad`, a cheap combine of the terms + the live inputs). Once the term caches are solid,
the verdict is the state-repositories §1 "only live calc is adding the packages at read" — so it is **computed LIVE,
never baked at fill** (`aWbVerdict` stored field retires). Then every live input (commerce slider G6, military,
anger/espionage timers) folds in with zero staleness and zero invalidation; only the deposit-fed TERMS carry marks.
This lands AFTER the term caches are rebuilt — not part of the emit wiring.

**SETTER SWEEP — the literal "every state change emits" ledger.** A mechanical pass over EVERY state-mutating
function in `CvCity`/`CvPlayer`/`CvPlot`/`CvTeam`/`CvGame` (each cross-checked against `read()`/`write()` for
serialization + the cascade's own consumer surface). Beyond the package-audit gaps (G1–G6), the un-emitted state
changes, tiered:

*Tier 1 — cascade-relevant (a live cascade consumer FOLDS the state; needs an emit for cached-package correctness):*
- **era** — ✅ WIRED, emit + consumer route (`SEVT_ERA_CHANGED`, `emitEraChanged` at `setCurrentEra` + reseed). Broad
  player-scope input: `PSC_CFLAT` (heritage era-stacked commerce, applied IN the setter) + every `ERA`-counter-gated
  deposit + `ERA` requires atoms (frontier). The consumer route is `markPlayerScopeAndCities` (the era-threshold
  deposits re-sum); the frontier `ERA` build atoms ride the `GATE_DYNAMIC` per-turn re-check (`ERA` is a count token,
  not an FK), so no enabler route is added.
- **holy-city designation** — ✅ WIRED, emit + consumer route (`SEVT_HOLY_CITY_CHANGED`, per-affected-city at
  `CvGame::setHolyCity` — old loses / new gains). Flips `IS_HOLY_CITY` / `IS_STATE_RELIGION_HOLY_CITY` on a holy-city
  relocation. Consumer route: mark the city's building-active footprint (holy-city-conditioned deposits) +
  `cityHaveChanged(CASC_HAVE_RELIGION)` (IS_HOLY_CITY buckets under `religion` in the operate index, so its operate
  buildings re-check). At religion FOUNDING the co-firing `SEVT_RELIGION_CHANGED` already covers the city; the
  standalone route matters for a relocation without a religion-presence change. ⚠ The bare-`IS_HOLY_CITY`
  `requires.build` frontier gate (a predicate, not FK-bucketed) is the one KNOWN follow-on — surfaced for verification,
  not silently skipped. **Reseed:
  `CvCity::read`** (the players-loaded window, where the event's owner field can render) via the read-safe accessor
  `CvGame::isHolyCityByOwnerId(religion, owner, id)` — compares the loaded `m_paHolyCity` `IDInfo` DIRECTLY (no
  `getCity`). Two earlier live-caught misses: `CvCity::read` + `getHolyCity()==this` (a city isn't resolvable via
  `getCity()` mid-deserialization) and `CvGame::read` + direct `IDInfo` (fires before players load → the owner field
  can't render a line, so 0 in the log despite the array being populated). — `CvGame::setHolyCity` (`m_paHolyCity`) → `CASC_PRED_IS_HOLY_CITY` /
  `IS_STATE_RELIGION_HOLY_CITY`; a holy-city relocation flips the predicate for old+new city with no event.
- **nuke state** — ✅ WIRED as a per-player **3-state** `SEVT_NUKES_CHANGED` (0 DISABLED / 1 ENABLED / 2 BANNED,
  Assert-green): availability (`m_bNukesValid`, per-player) emits at `CvPlayer::makeNukesValid`; the world ban
  (`isNoNukes`) fans out per-player at `CvGame::changeNoNukesCount`; + load reseed. `getNukeState()` = `isNoNukes ? 2
  : (isNukesValid ? 1 : 0)`. (Grounded correction: the cascade `NO_NUKES` predicate reads only the world BAN half —
  Manhattan/`isNukesValid` is the separate per-player availability axis, NOT the `NO_NUKES` gate.) Consumer route
  WIRED: `NO_NUKES` is an unrecognized predicate → the `GATE_DYNAMIC` frontier bucket, so the route is
  `onPlayerGateClass(GATE_DYNAMIC)` for buildings + units (a Manhattan-type `requires.build.disabled: NO_NUKES` flips);
  no modifier mark (no `NO_NUKES`-conditioned deposits).
- **culture level + vicinity membership** — ✅ WIRED (`SEVT_CITY_CULTURE_LEVEL_CHANGED`, `emitCultureLevelChanged` at
  `CvCity::setCultureLevel` + reseed, Assert-green). ONE hook: culture level is the cascade input (wonder caps,
  defense, frontier) AND the city's workable RADIUS grows with it, so this single fact IS the vicinity-membership
  signal (the city gains/loses plots into its vicinity). The low-level per-plot `changePlayerCityRadiusCount` is just
  its consequence — not separately hooked.
- **area split/merge** — `CvMap::recalculateAreas` (via `CvPlot::setPlotType`) → area-scoped deposits
  (`maintAreaPct`/`wbAreaByFam`); ALSO needs an `area` scope added to the invalidation surface (only `0=city/1=empire/2=world` today).

- **⛔ FEATURE change → cached CPK_WB (adversarial-audit GAP, was MISSED entirely).** `CvPlot::setFeatureType`
  (`emitFeatureChanged`) routes to `default:` in BOTH switches, yet feature presence in a city's workable radius feeds
  the **cached** wellbeing term (`CvCascadeWellbeing.cpp` builds `featureCounts` over the radius plots and folds each
  feature's `health.plot.percent` — e.g. `FEATURE_SWAMP` health −50 — into the CPK_WB-cached verdict). A routine
  worker-clear / feature spread / `popDestroys` removal stales the radius cities' cached health/happiness with nothing
  to invalidate it. This is a PLOT event needing a plot→radius-cities fan-out for CPK_WB (the same shape as vicinity,
  but features have NO city-local event analog — DESIGN CALL, not yet wired; see the note below).

*Tier 2 — literal-bar-only (yield CAUSES feeding the PULL-computed plot-yield cache — `updateYield` self-dirties, city
reads live; so gaps only against the OOS/observability bar, NOT cache correctness):* `setPlotType`, river
(`setNOfRiver`/`setWOfRiver`/`changeRiverCrossingCount`), `setIrrigated`. *(Feature is NOT here — it feeds the cached
WB term, above, not just the pull-computed yield.)*

*N/A — live-folded inputs (no emit needed, correctly):* commerce sliders (`setCommercePercent`), trade-yield (the
§-input), culture-anger + occupation/anarchy/war-weariness/espionage timers (§2b raw-state / `isDisorder()` gates).
**⛔ EVENT/VOTE building grants (`m_aBuildingYieldChange`, `m_aBuildingCommerceChangeEvents`) + plot `setExtraYield`
stay OUT of the spine (owner):** they are the EXTRAYIELD BUCKETS the cascade reads as yield STATE, filled by game
EVENTS/votes — and events are the #425 Python-authoritative carve-out; perturbing the event surface risks a "black
hole of madness." The cascade cares only about the resulting yield in the bucket, never the granting event.
*Borderline:* `CvPlayer::setCapitalCity` (partially shadowed by the palace `emitBuildingChanged`). *Clean (no gaps):*
`CvTeam` — techs/projects emit; the capability counters are retired/derived-on-query.

**Latent (no current staleness, but the emit must land WHEN the deposit is consumed):**
- **L1 — Culture level.** `setCultureLevel`/`updateCultureLevel` (`CvCity.cpp:10750/10833`) emit nothing. No stored
  package reads culture level yet (`defenseAmount` folds buildings only; the CultureLevel wonder-caps are an unwired
  BuildingEnabler follow-on, `CvBuildingEnabler.cpp:78`). So the authored `defense.city.amount.percent` +
  wonder-cap deposits on culturelevels are currently **unconsumed**; the moment either is wired into a fold it becomes
  a live gap because the emit is absent — wire the emit in the SAME change.

**⛔ THE OPERATE-FLIP FOOTPRINT MASK CLASS (adversarial mask-audit — a documented "NOT a gap" was FALSE).** Every
event that fires the operate-dormancy ripple (`cityHaveChanged` → `ek_recheckActiveSet`) flips buildings
active↔dormant, so a flipped building's FULL active footprint (`YEXTRA/SCFLAT/BR` + the source-conditioned packages)
must re-mark — not just the source-conditioned subset. This was fixed for **bonus** (G3) but the SAME gap was live
for **population** (465 pop-operate buildings — the worst by frequency, every growth/starve), **power** (934 buildings
— the doc previously, WRONGLY, "Cleared" it as in-mask), **religion** (235), **corporation** (22). All four masks are
now broadened to the building-active footprint `CPK_ALL & ~(YSPEC|CSPEC|SCSPEC)` (matching `buildingProcessed`/G3).
RESIDUAL (surfaced, not hidden — G4-class): a flipped building with a `perSpecialist` deposit also stales
`YSPEC/CSPEC`, which these masks exclude; that needs the enabler ripple to mark the flipped building's specialist bits
(the derive-from-deposits refinement), shared across ALL operate-flip events. Small (the ~7 `perSpecialist` wonders,
only when operate-gated), and a co-firing `SEVT_SPECIALIST_CHANGED` covers the pop case in practice.

**⛔ CONFIRMED GAP — city PROPERTY changes never invalidate property-`operate` dormancy (`s_opDynamic` is WRITE-ONLY).**
A building's `requires.operate` may be a `PROPERTY_*` band (`{PROPERTY_CRIME, min:450, …}` — crime/disease/education/
pollution tiers; `operate`+`PROPERTY_` co-occur in **1735** building files, e.g. `building_crime_bank_robbery.json`
deposits happiness/health/maintenance flats gated on the crime band). The property solver mutates
`CvCity::m_Properties` every turn **emit-free** — no `SEVT_PROPERTY*` exists — and a property operate-atom falls to
`d.dynamic` in `scanCondDeps`, landing in `EnablerKernel::s_opDynamic`. **`s_opDynamic` is never read** (grep: the decl
`CvEnablerKernel.cpp:378` + the push `:529`, zero consumers) — the operate-side twin of the frontier's
`s_gateClass[GATE_DYNAMIC]` (which `onCityTurn` DOES read) was never wired. So a band crossing stales both the
operating-building set AND the flipped building's CPK_WB/SCPCT/SCFLAT/YEXTRA/BR packages until an unrelated event
re-touches the city. **Deliberately NOT wired in this F0 increment: it is the PROPERTY subsystem** — F5 (the property
feed is *mangled*, [property-audit.md](property-audit.md)) + [roadmap](roadmap.md) #12 (bands pending #430 wiring);
invalidating a known-broken feed is backwards. Fix shape when F5 lands: the G3 pattern — a per-city `SEVT_PROPERTY_CHANGED`
on the operate-threshold crossings → `cityHaveChanged(CASC_HAVE_PROPERTY)` → an `onHaveChangedActive` case that FINALLY
reads `s_opDynamic` (the bounded operate re-check enabler.md §7.1 sanctions for live non-HAVE clauses) + the
building-active footprint mask. Same dead-`s_opDynamic` root: an **IS_CAPITAL-`operate`** building would keep a stale
active flag through a capital relocation (concrete only if such a building exists — the property case is the certain one).

**Cleared (checked, NOT gaps):** vassal/relations (not folded into any maintenance/wellbeing term); commerce-RATE
slider (live-read at the combine, not folded); `doVicinityBonus` (yield-only, not an access mutator — access is fully
choked through `processBonus`); freshwater / government-center (single caller is `processBuilding` → shadowed by
`SEVT_BUILDING_CHANGED`, same-city packages covered by its CPK_ALL mask). *(Power was REMOVED from this list — it was
the false-confirmation above. existedFor is IGNORED by the evaluator today — an accuracy issue, not an emit gap.)*

The BROAD-mask events (tech/civic/GA/trait/state-religion/project → `markPlayerScopeAndCities` + `PSC_ALL`) are
correct-but-over-broad (a perf-narrowing follow-on via the R2 derived masks, not a correctness gap).

### R6. The load RESEED — event-source the save read
A loaded save deserializes the game objects directly; the incremental setters never fire, so the cascade (packages +
enabler) has nothing to build from. The reseed makes the events come from **inside the save read** — `CvGame::read` →
`CvPlayer`/`CvCity`/`CvTeam`/`CvPlot::read` fire the DOMAIN event as they deserialize each fact (north-star: the event
SETS the state, read → emit → populate — object-populated-by-events is a step too far for now, but the events must
still originate from the genuine read). The R3 consumer (load-active) builds the cascade from them. This is the ONE
full build; it REPLACES `worldRebuild` + `playerSliceRebuild` + the `recalculateModifiers` content
([DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed)). (The plot-yield cache is a game-object cache
and keeps its own dirty-on-load recompute — not the cascade recalc being removed.)
⛔ NOT a separate post-deserialization pass that fabricates events by walking already-populated objects — banned
([superseded-ideas](../../architecture/superseded-ideas.md) #13).
**⚠ Sequencing:** the object `read()`s run BEFORE `onFinalInitialized`, where consumers register today — so the
load-active consumers (the R3 cache-build) must be set up at `GAME_LOAD_STARTED` (before the reads) for the in-read
events to land. **Load lifecycle:** emit `GAME_LOAD_STARTED` before the read + `GAME_LOAD_FINISHED` after;
result-producers (grants) rely purely on the spine and suppress between them (a load is not a genuine acquisition).

**The in-read emit shape, by fact storage.** A scalar field emits right after its own read (the terrain prototype,
`CvPlot::read`). A field read **element-by-element in a loop** emits INSIDE that loop (buildings, the `CvCity::read`
BuildingLedger loop — the cleanest hook). A field read **WHOLESALE** (`WRAPPER_READ_CLASS_ARRAY`, no per-element
hook) emits from a loop placed **immediately after** that read, still inside the object's `read()` and co-located
with its deserialization (religion/corporation/bonus/specialist in `CvCity::read`). All three are "the events come
from the genuine read"; NONE is the banned separate post-deserialization walk over already-populated objects
([superseded-ideas](../../architecture/superseded-ideas.md) #13) — the distinction is the emit is coupled to *this*
object's read, not a decoupled global pass. Present/nonzero-gated (an absent feature / zero count is no fact).

**Reseed emit map (per read site):**
- `CvPlot::read` — terrain (prototype), feature, improvement, route, **plot resource** (`SEVT_PLOT_BONUS_CHANGED`).
  ✅ wired. *(Plot resource is a genuine plot-substrate event: a Great-Farmer build places one, a discovery event
  reveals one from an improvement, removal clears one — all route through `CvPlot::setBonusType`, which now emits the
  SAME event play-time; the reseed fires it too. Distinct from the city resource-ACCESS count `m_paiNumBonuses`.)*
- `CvCity::read` — buildings (ledger loop); religion, corporation, bonus, specialist (wholesale-array loops);
  population, power (scalars). ✅ wired.
- **Tech** — reseeded from `CvPlayer::read`, NOT `CvTeam::read`. **VERIFIED: the EXE reads teams BEFORE players are
  set up** (a per-member emit from `CvTeam::read` fired 0 — no alive members yet). So the team's techs ARE loaded by
  the time a player reads: each player emits `techChanged` per-self for every tech its team holds
  (`GET_TEAM(getTeam()).isHasTech`) — the owner's "one per alive member player" ruling, realized player-side. Proper
  end-state remains a **team-scope cascade component**. ✅ wired.
- `CvPlayer::read` — traits (wholesale-array loop), civics (per-slot, AFTER the load-time civic fixup), state-religion
  (scalar), golden-age + tech (emitted AFTER `m_eID` reads + `updateTeamType()`). ✅ wired.
  **⚠ GOTCHA (cost real time):** `reset()` at the top of `CvPlayer::read` clears `m_eID` to `NO_PLAYER`, and it is
  re-read partway down; `m_eTeamType` is NOT saved and is rebuilt by `updateTeamType()` further down. So `getID()` is
  `-1` until its read and `getTeam()` is `NO_TEAM` until `updateTeamType()` — any reseed emit that needs owner/team
  must sit AFTER both, not at the top of `read()` (an emit at the top renders `owner=?(-1)`).
- Change-shaped owner events (`null → current`): plot owner + working-city (`CvPlot::read`), city owner
  (`CvCity::read`). ✅ wired.

**Representation rulings (owner-resolved):**
1. **Change-shaped events on reseed fire as `null → current`** — exactly like a real acquisition: `plotOwnerChanged` /
   `cityOwnerChanged` / `workingCityChanged` emit `old = NO_PLAYER/NO_TEAM/no-city (-1) → new = the loaded owner`.
   ✅ wired.
2. **Plot resource IS a first-class plot-substrate event** — added as `SEVT_PLOT_BONUS_CHANGED` (see the `CvPlot::read`
   map row). ✅ resolved + wired.

### R5. The `(scope,channel)` reporting counter + `/computed/perf` endpoint
Today's `CascadePerf` counters (`CvCascadePerfCount.h:32-67`) count refresh PASSES + eval leaves, split by CALLER
domain (`CascadeCondCaller`), NOT by (scope × channel). Add a `(scope,channel)` calc-count: a counter keyed on
`(scopeLevel, packageBit)` — the package-bit vocabulary above — incremented at each package REFRESH (where
`accRefresh`/`scRefresh`/`pctStack`/`operatingBuildingsRecomputed` already fire, the refresh knows its scope + which
bits it recomputes). Reset per turn (extend `CascadePerf::reset`). Flush: extend the census
(`cvCascadeModifierPerfCensus`, mind the 16-field-per-event spine cap — add lines or a structured emit). Expose at a
new `/computed/perf` route (add a `ROUTES[]` row `CvHttpServer.cpp:4194` + a `strcmp` branch `:1616`, mirror the
`tally` handler `:4069`; game-thread mailbox snapshot), returning the `scope × channel` histogram + total; tee to
`/events`. **The 50k gate** (DEC-calc-count-gate): total/turn < 50k (thousands steady-state, ~0 on a quiet turn);
histogram names the culprit scope/channel on a breach.

## Delete-list (verifiable — grep must show zero residual after F0)
- The per-turn / per-slice `markAllDirty` BLANKET (`playerSliceRebuild` — mark ALL + eager ensure of every package,
  each turn) and its world twin `worldRebuild` — DELETED (tombstone comments at `CvPlayer.cpp:3710` / `CvGame.cpp:6167`).
  There is no per-turn / per-slice blanket ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
  **NB `CascadeAccumulator::markPlayerScopeAndCities` is NOT deleted** — it is the per-PLAYER broad **conditioner** mark
  (one player + his cities), which R3 routes the broad-fan-out sources through (tech/civic/GA/trait/SR/project — the
  deliberate over-broad correctness floor; narrowing to the condition-reverse-index is the R2b follow-on, not a
  delete-list item).
- The recompute-on-load CASCADE recalc — `worldRebuild` + `playerSliceRebuild` (the cascade half of the
  `CvGame::onFinalInitialized` warm-up block) + the `recalculateModifiers` content — DELETED. The **reseed** (R6) is
  the cascade's load build. (The plot-yield cache's own dirty-on-load recompute is a game-object cache and stays.)
- ✅ The inline hand-derived routing in `buildingProcessed` — DELETED; `buildingProcessed` queries
  `DepositIndex::routeFor` (R2a).
- `CvCity::refreshOperatingBuildings` mask-ignoring reseed (`CvCity.cpp:11399`) — honor the mask / rely on ripples.
- KEEP (not blankets): `dirtyCity` (the mark primitive — masks become index-derived), `cityHaveChanged`/
  `unitCountChanged` (frontier targeted re-check), `cityCreated` (the one ruled eager-ensure).

## Ordered increments
1. **R5 first (observability)** — the `(scope,channel)` counter + `/computed/perf` + `/events` tee. Measure the
   current blanket baseline (prove the millions) BEFORE touching anything, and make every later step provable.
2. **The EMIT surface FIRST — the spine emits ALL events, during play AND from the load process** (R1 + R4 + R6's
   emit half): the source-carrying payload, the complete play-time emit surface (the R4 gaps), and the **load reseed
   emitting every present-fact**. This is the PREREQUISITE — the caches cannot build from events until the events are
   actually, completely emitted. VERIFY the emit is complete + correct (observable: every present-fact emitted on
   load) BEFORE anything consumes it.
3. **THEN the cache build/invalidation consumer** (R2 + R3) — the reverse routing surface + the one consumer that
   consumes those events to POPULATE the caches (the eager full build on load) and INVALIDATE them (incremental
   during play). The step AFTER the emit surface is proven complete.
4. **Delete-list** — remove the blankets + inline routing + the recompute-on-load recalc. Grep proves zero residual.
5. **Verify LIVE** — the counter collapses to event-proportional (quiet turn ≈ 0, never > 50k); the `/computed`
   oracle values stay correct (manifestation). NO re-shadow ([DEC-verify-in-game-not-reshadow]).

## Ambiguities to resolve during build (do not guess — verify)
1. `setHasBuilding` (`CvCity.cpp:14785`) vs `processBuilding` (`:4627`) — which is the true count choke point
   carrying the mark.
2. Power (`changePowerCount:10306`) — verify whether power-gated yield/commerce deposits exist that need a rate mask
   (currently only frontier/operating-buildings marked).
3. State-religion — confirm whether the SR-pool / `iCSrMatch` realization needs an explicit fill on a switch (the
   live gates cover most reads).

## Acceptance bar (all four, proven — never "kinda")
1. **Complete** — the completeness map has ZERO gaps (every state-change source emits), proven by an adversarial
   second pass.
2. **Singular** — every other invalidation path deleted, proven by grep (no residual `markAllDirty` blanket /
   hand-wired routing).
3. **Observable** — the `(scope,channel)` counter proves it live: quiet turn ≈ 0, event-proportional, never > 50k.
4. **Manifesting** — the `/computed` oracle values are correct in the running game.
