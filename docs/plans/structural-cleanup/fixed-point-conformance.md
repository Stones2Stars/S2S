# Fixed-point conformance — ×100 is the engine's NATIVE representation, out to the consumers

> **Owner ruling (fundamental spec divergence — fix NOW, do NOT defer).** The
> [fixed-point model](../../specs/curators/fixed-point-and-scales.md) / [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100):
> **every value inside the engine is ×100 fixed-point, EVERYWHERE** — through the cascade, through the realized
> getters, and **out to the consumers**. A value is human only at two boundaries: **the reader** (UI / the HTTP API /
> Python — "having any reader do ÷100 is trivial") and **the point it becomes a genuinely discrete game quantity**
> (whole angry citizens, a whole food modifier). **There is no ×100 "variant" of any getter** — `happyLevel()` simply
> IS ×100; we never make a `happyLevel()`+`happyLevel100()` pair.

## Why ×100-out-to-consumers (owner)

- **"then we never have to care about what format inside the structure"** — ×100 is the invariant, so no code ever
  asks "is this scaled?". The answer is always yes.
- **The forcing function.** Making the getter ×100 and pushing it out FORCES every consumer to be examined and
  properly wired (a mis-wired consumer is visibly 100× wrong), while dead consumers are discarded. It surfaces
  remaining legacy the earlier passes missed and collapses redundant call sites.
- **⛔ Blast radius is NEVER a reason to limit scope.** Reducing at the getter to spare the consumers is the exact
  reflex that produced the half-migration — the cascade gets shoehorned into legacy-shaped getters instead of the
  consumers being rewired through. The mapped consumer surface is the worklist, not a warning.

## The violation it fixes

The cascade reduced ×100 → human **early, per-item, mid-chain** — every gather fold did `sumUnit`/`perScale(...)/100`,
truncating each deposit before the sum. Lossless for integers (`200/100=2`), truncating for fractionals (a `-0.4`
happiness deposit — SPECIALIST_SETTLED_SLAVE_HEALTH authors exactly this — became `0`), and the wrong SHAPE
everywhere (a future fractional value in any channel breaks silently). The realized getter then also returned human,
so the cascade's ×100 world and the engine's human world met at a seam *mid-engine*.

## The model (what the code must be)

**Accumulate and carry ×100 through the ENTIRE chain; convert to human ONLY at the two boundaries.**

- **IN boundary** (human → ×100): `readJson`, once, at load.
- **The engine** (cascade gather + combine + the realized getters + every consumer): ×100, no mid-chain ÷100.
- **OUT boundaries** (×100 → human): (a) **readers** — UI display, the `/computed` HTTP fields, the `Cy*` Python
  wrappers — each does its own trivial `÷100`; (b) **discrete realized quantities** — where the value physically
  becomes a whole game count (the game unassigns WHOLE citizens), the quantity reduces `÷100` and is itself human
  onward.

A consumer that only compares SIGN (`happy − unhappy < 0`) or ranks is scale-invariant — it needs no change, ×100
flows through. A consumer that MIXES the value with a whole count (population, an era index, a config threshold)
reduces `÷100` at that use. An aggregate (Σ over cities) stays ×100; its own reader reduces.

## Wellbeing — the PILOT (landed; under live verification)

Wellbeing is the first channel converted, establishing the pattern the other channels follow.

- **Cascade core ×100** (`CvCascadeWellbeing.cpp`): every gather term accumulates at native ×100 (`sumUnit100`, no
  `perScale(...)/100`, `value100` raw; the `getInitialHappiness` commerce-pool seed ×100; the event ledgers ×100;
  `wb_extraParts` ×100). `assemble` computes each of the four verdicts fully in ×100 — every raw-state input (anger
  percents, vassal, handicap, espionage, tax, foreign, landmark, city-over-limit, freshwater, TEMP_HAPPY) ×100; the
  anger math is `anger% × pop × 100 / DIVISOR` (the ×100 keeps sub-unit anger). The `WbSplit`/`CascadeWbTerms`
  structs hold ×100.
- **The verdicts return ×100** (`CvCity::happyLevel/unhappyLevel/goodHealth/badHealth` → the ×100 `aWbVerdict`; the
  pre-init/what-if legacy siblings stay human internally and are ×100'd at the getter dispatch).
- **⛔ The two "discrete boundary" carve-outs are REVOKED (owner ruling): there are NO carve-outs — every channel
  works the same way, and that uniformity is the core of this rework.** `angryPopulation` and `healthRate` currently
  reduce `÷100` INSIDE the getter; that is the same shoehorn as a `getX`+`getX100` pair. Both must carry ×100 like
  every other getter, with the `÷100` moved to each USE that consumes a whole count. Converting them is part of the
  worklist below (~28 `angryPopulation` sites), not an exception to it.
- **Readers ÷100**: `Cy*City` (keeps all Python backward-compatible, untouched), the `/computed/cities/wellbeing`
  realized fields, the CityBar + the happiness/health help screens.
- **Every consumer wired**: the AI happiness/health valuations (`CvCityAI`/`CvPlayerAI` — ÷100 each verdict read,
  reproducing legacy's whole-value behavior), the event-trigger thresholds, the score sum, the player aggregates,
  the `getAdditional*` what-ifs, the property `ATTRIBUTE_HAPPINESS`, and the flipped legacy DECOMPOSITION getters
  (`getFeature/Bonus/Building{Good,Bad}{Health,Happiness}`, CvArea/CvPlayer building rollups) — these last read the
  cascade term via the `CascadeWellbeing::*Wellbeing` accessors and ÷100 to keep their human contract (so
  `happyLevelLegacy` stays consistent human).

**Acceptance:** served-value SANITY on `/computed/cities/wellbeing` (per [DEC-oracle-tautology] the check is a sane
number, NOT oracle parity) — specialist-heavy cities gain their previously-truncated fractional health/happiness;
`angryPopulation` stays a sane whole count; no 100× display. NOT bit-parity with the old per-item-truncated numbers
— matching the old truncation IS the divergence being removed.

## The legacy-accumulator CUT — ALL accumulators, ONE uniform mechanism

> **Owner ruling.** The accumulator cut is **NOT wellbeing-specific — it is EVERY legacy serialized incremental
> accumulator, and they all work exactly the same way.** Wellbeing is the PILOT that proves the mechanism; the same
> cut then repeats UNCHANGED across every cluster below. **Blast radius is not a concern — it is the SIGNAL: a cut
> that does NOT reach broadly means the legacy is not actually being cut.** Anything that sneaks a legacy value back
> in (a `*Legacy` fallback, a masking getter, a member kept alive only to feed one) is an **ERROR**, never a safety
> net ([DEC-no-legacy-masking]); on this non-playable branch a wrong/empty cascade value is the CORRECT exposed
> outcome, and a legacy-correct one is the defect. Ledgered as [DEC-accumulator-cut-uniform].

**What an accumulator IS (the three-part test).** A member on `CvCity`/`CvPlayer`/`CvArea`/`CvTeam` that is ALL of:
(1) **serialized** in `read()`/`write()`; (2) **incrementally maintained** by `change*`/`update*`/`process*` deltas,
never recomputed-from-source; (3) a **per-turn game quantity the cascade now owns** (a yield / commerce / happiness /
health / upkeep / free-specialist amount). These are the **STORED-ACCUMULATOR DRIFT class**
([modifier.md §2b](../../specs/modifier.md)): they carry decades of save history no live source can reproduce, so a
stored-vs-recompute diff is DRIFT (history pollution), never state to preserve — the recompute-from-source is the
correct side ([state-repositories.md](../../architecture/state-repositories.md) "incremental-accumulate ledgers
convert to recompute-from-source").

**The uniform mechanism (IDENTICAL per cluster — the landed building/bonus/feature wellbeing cut is the template).**
1. Add/extend a `Cascade*` **fresh-gather accessor** returning the cluster's term from the cascade, ×100 internally
   (the shape of the landed `CascadeWellbeing::bonusWellbeing`/`buildingWellbeing`/`featureWellbeing`).
2. **Re-point the realized getter** to that accessor, reducing `÷100` at the reader boundary ([DEC-fixedpoint-x100]);
   NO `*Legacy` fallback, NO variant getter.
3. **HARD-DELETE** the member + its `change*`/`update*`/`process*` maintainers.
4. **FULL-DELETE the read + write** and NAME the tag in `Assets/savemigration.txt` — the save reader (`sm_isCut`)
   drains the orphan transparently at load ([engine.md §Save/load](../../reference/engine.md),
   [DEC-save-remove-is-soft]). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the dead member named in the read path — a
   rollerskate target); save-breaking is OBSOLETE. An UNLISTED deleted-read orphan is the one hard desync.
5. **The COMPILER is the census** ([DEC-playability-not-a-gate]) — every surviving consumer is a compile error to
   rewire onto the getter; you cannot flip-and-pretend. **Done = endpoint-observable** on a loaded save
   ([DEC-done-is-observable]), not "it compiles."

⚠ Audit each deleted `change*`/`update*` BODY for side effects before removing it — legacy changers carry non-obvious
riders (trade-network recompute, UI-dirty, power) the surviving trigger site must still fire
([engine.md §Save/load](../../reference/engine.md) "audit the whole BODY").

### The cut INVENTORY (from the exhaustive code map)

| class | cluster | members (cut) |
|---|---|---|
| CvCity | **wellbeing** (pilot) | `m_iExtraBuilding{Good,Bad}{Happiness,Health}`, `m_iExtraBuilding{Happiness,Health}FromTech`, `m_iExtraTechSpecialist{Happiness,Health}`, `m_iReligion{Good,Bad}Happiness`, `m_paiStateReligionHappiness`, `m_iSpecialist{GoodHealth,BadHealth,Happiness,Unhappiness}` + their `update*`/`change*` |
| CvCity | yield/commerce | `m_aiBuildingCommerce`, `m_aiBuildingCommerceTechChange`, `m_aiReligionCommerce`, `m_aiCorporation{Commerce,Yield}`, `m_aiExtraSpecialist{Yield,Commerce}`, `m_commercePerPopFromBuildings`, `m_aiBaseYieldPerPopRate`, `m_aiRiverPlotYield`, `m_aiTradeYield` |
| CvPlayer | yield/commerce | `m_aiFreeCityYield`, `m_aiSpecialistExtra{Yield,Commerce}`, `m_aiStateReligionBuildingCommerce`, `m_extraCommerce`, `m_aiGoldenAge{Yield,Commerce}`, `m_paiFeatureHappiness` |
| CvCity/Player/Area/Team | freeSpecialist (two-part seam) | the `*FreeSpecialist*` AMOUNT ledgers — the summed `freeSpecialists` deposits replace `changeFreeSpecialistCount`; the per-type COUNT getter (`getSpecialistCount+getFreeSpecialistCount`) is the sanctioned output-seam read ([modifier.md §6](../../specs/modifier.md)) and STAYS |

> **freeSpecialist AMOUNT cut — LANDED delete-driven (the two-part seam, [modifier.md §6](../../specs/modifier.md)).**
> The cascade owns the AMOUNT (`CascadeAccumulator::fsAmountAny`/`fsAmountByType` — the summed building/civic/trait
> `freeSpecialists` deposits at city+empire+area), the engine owns PLACEMENT (untouched), consumers read the OUTPUT
> (`getSpecialistCount+getFreeSpecialistCount`). The derivable any/by-type ledgers on `CvCity`/`CvArea`/`CvPlayer`/
> `CvTeam` (`m_iFreeSpecialist`, `m_paiFreeSpecialistCount`, `CvArea::m_aiFreeSpecialist`) + their
> `changeFreeSpecialist(Count)` building/civic/trait process-applies are DELETED (drain-tagged in `savemigration.txt`);
> `CvCity::getFreeSpecialistCount` → `fsAmountByType + unattributed`, `totalFreeSpecialists` → `fsAmountAny` (+ the
> still-legacy improvement + per-wonder legs). **KEPT** (genuine one-shot state, not derivable): the settled-GP store
> (`m_paiFreeSpecialistCountUnattributed`, MISSION_JOIN — still applies its specialist effects at placement) + the
> era-advance pulses. **NOT cut (still legacy-live in `totalFreeSpecialists`):** the improvement-scaled leg
> (`getImprovementFreeSpecialists × plots`) and per-wonder leg (`policies`-pending, json.md §9) — their own follow-ups.
> **EXPOSED (delete-driven — cascade seam has no team leg, so it surfaces as a visible gap, never a legacy ride-in):**
> team-scope by-type free specialists. The non-yield specialist EFFECTS of the derivable free specialists (GP-unit
> rate, free XP, insidiousness, investigation) ride the still-legacy `processSpecialist` push and retire with the F4
> unit/specialist apply-loop; their YIELDS already follow the cascade (recompute over `getFreeSpecialistCount`).

> **⛔ Unit UPKEEP retired with the F4 unit-plane build — DONE.** Per-unit extra-upkeep is a cascade self-accumulator
> (`UPK_UPKEEP`, gathered on-dirty from held promotions + unit-combats) and `CvUnit::getUpkeep100` is the computed
> per-unit read; the legacy per-unit `m_iUpkeep100`/`m_iExtraUpkeep100` push-accumulators are REMOVED.
> `CvPlayer::getUnitUpkeep{Civilian,Military}100` read recompute-Σ caches (`ensureUnitUpkeepBuckets` — Σ over live
> units' `getUpkeep100()` bucketed by `isMilitaryBranch()`), replacing the `changeUnitUpkeep` push. Unit upkeep is a
> **unit-plane channel**, distinct from the cascade `maintenance` channel (CITY maintenance, a separate gold-expense
> component — [economy.md](../../reference/economy.md),
> [DEC-maintenance-bookkeeping](../../architecture/decisions.md#dec-maintenance-bookkeeping)). (The upkeep *percent*
> modifier `m_iUpkeepModifier` + the SizeMatters multiplier stay legacy — `getUpkeep100` applies them live; the
> percent-stack carve-out below.)

The yield channel additionally carries a `getYieldRate100` + `getYield` (÷100) **SPLIT** — the exact "×100 variant"
this ruling dissolves: the single getter returns ×100 and every consumer reduces at its reader/discrete boundary;
each channel's `MMKernel` gather moves off the truncating `sumUnit` onto `sumUnit100` (the truncating variant retires).

> **⛔ The load-time legacy rate FALLBACK is CUT (owner ruling 2026-07-18).** `getYieldRate100`/`getCommerceRateTimes100`
> are now **cascade-only** — the `!isFinalInitialized` branch that returned `getYieldRate100Legacy`/
> `getCommerceRateTimes100Legacy` (the old "the cascade substrate isn't warm pre-init" bridge) and both `*Legacy`
> getters are DELETED. It is SAFE because the cascade rate packages bind **DIRTY** (`CvDerivedCache`,
> [state-repositories.md](../../architecture/state-repositories.md)): a loaded save's first rate read
> **recompute-from-source** off the reseeded state and clears the flag — there is no unwarm-cascade window to bridge.
> Keeping the legacy path alive was legacy-masking ([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)):
> "I want legacy gone, because then I see what is broken" — a wrong/empty cascade rate is the correct EXPOSED outcome,
> a legacy-correct one hides the defect. The `CvCascadeInvalidation` load-skip of `routeModifierMarks` is irrelevant to
> this (it suppresses re-marking already-built packages; at load they are already dirty-by-construction). Live-verified:
> cascade-only rates load sane. This unblocks the 11-member accumulator cut below (the accumulators fed only the now-
> deleted legacy rate chain + the decomposition getters).

> **⛔ CvCity yield/commerce cluster — CUT (owner ruling "cut now, expose the break"), two mechanisms per member.**
> Unlike the wellbeing pilot (pure decomposition), these fed the legacy SUB-COMPONENT getters (`getBaseYieldRate`/
> `getSpecialistYieldTotal`/`getBuildingCommerce100`/…) that AI/UI/tax read directly, with no cascade sub-component
> accessor. The 10 cut members split by whether a per-source recompute already exists:
> - **RECOMPUTE-FROM-SOURCE** (a `get<X>By<Y>` helper exists → the getter sums it on read; state-repositories
>   "incremental ledger → recompute-from-source"): `getCorporationYield`/`getCorporationCommerce` (per-corp),
>   `getReligionCommerce` (per-religion), `getExtraSpecialistYield`/`getExtraSpecialistCommerceTotal` (per-specialist),
>   `getBuildingCommerce` (per-building). The serialized accumulators + their `update*` maintainers + the CvPlayer
>   fan-outs (`updateExtraSpecialistYield`/`updateReligionCommerce`) are deleted; getters recompute cold-path.
> - **EXPOSE (stub 0)** where the value is DEAD decomposition — the cascade rate computes it fresh and no live
>   consumer needs the legacy term: `getRiverPlotYield` (building `HAS_RIVER` `plots` deposit), the per-pop base yield
>   (`getExtraYield100` drops the term), `getBuildingCommerceTechChange`/`getCommercePerPopFromBuildings`
>   (`getBuildingCommerce100` sums the TEAM per-building tech-commerce + building `getCommercePerPopChange` fresh).
> - **`m_aiTradeYield` is HELD** — NOT an accumulator cut: it is the **isolated trade-route input package** the cascade
>   FOLDS (`CvCascadeYieldBasePackages` reads `getTradeYield`), the one sanctioned live-yield input ([modifier.md §2a](../../specs/modifier.md)),
>   outside the cascade's derivation scope. The cascade does NOT read any other legacy sub-component getter (verified).
> - **The `setCommerceDirty` coupling:** `getBuildingCommerce100` is a KEPT recompute cache; its dirty trigger rode the
>   deleted `updateBuildingCommerce`/`change*` maintainers. `updateBuildingCommerce` is retained as PURELY that trigger
>   (`setCommerceDirty(NO_COMMERCE)`), and the tech-commerce apply in `CvTeam::setHasTech` becomes a `setCommerceDirty`.

### Carve-outs — NOT an accumulator cut (do not touch)

- **Event/vote-grant persisted stores** — genuine one-shot non-derivable state: `m_aBuildingCommerceChangeEvents`,
  `m_paiFreeBonusEvents`, `m_aBuildingHappy/HealthChange`. KEEP ([state-repositories.md](../../architecture/state-repositories.md)
  "event/vote grants are NOT cached"). `m_aBuildingYieldChange` is MIXED — the event leg keeps, the bonus-conditioned
  leg is a pending extra-yield cut; split at that channel.
- **Genuine historical counters** — `m_iHighestPopulation`, ever-created/ever-alive counts, original-owner/time.
- **Free-specialist PLACEMENT** — the per-type count OUTPUT read stays; only the amount-maintenance is cut (the seam).
- **Raw-state INPUTS the cascade FOLDS** — the catch-all `m_iExtraHappiness`/`m_iExtraHealth`, `m_iFreshWaterGoodHealth`:
  `assemble` reads these as inputs (subtracting the engine trait/tech parts, folding the remainder), not as cascade
  targets. NOT cut here — they retire with their own input feeders, not this pass.
- **Percent MODIFIER-stack accumulators** — `m_ai*Modifier` (maintenance/upkeep/commerce-rate percents): serialized
  incremental, but the additive percent stack ([modifier.md §2a](../../specs/modifier.md)), not a base magnitude. A
  distinct rework, lower priority — flagged, not folded into the base-magnitude cut.

### Wellbeing — the PILOT cluster (+ the cascade-sourced breakdown, combat-tooltip pattern)

The wellbeing cut also re-sources the **per-source breakdown for the Python/UI tooltip** from the cascade (owner:
"the same treatment as the combat-tooltip rework"), so the breakdown STAYS while its stored accumulators go.

**The pattern (`CvCombatModel.h` `computeCombatPreview`):** ONE cascade-sourced producer returns the realized numbers
**plus** a `detailLines` vector of ready-to-render `{label, signed value, category}` rows; `CvGameTextMgr` is a **pure
renderer** (zero math of its own), coloured by category.

- **Producer** — `CascadeWellbeing::computeBreakdown(city)` returns the four verdicts + `happyLines`/`healthLines`/
  `angerLines` (per-source rows: building, bonus, religion, specialist, civic, feature, extraBuilding, area/empire,
  per-pop, the anger-percent sources, …), every value from the `gather`/`assemble` terms (×100; renderer ÷100). One
  producer, single-source (patterns.md).
- **Pure renderers** — `setHappyHelp`/`setBadHealthHelp`/`setGoodHealthHelp`/`setAngerHelp`; the
  `/computed/cities/wellbeing` decomposition + Python read the SAME producer. No hand-summed decomposition anywhere.
- **DELETE** the wellbeing cluster (extraBuilding / religion / stateReligion / specialist / `*FromTech`) per the
  inventory above — members, `change*`/`update*`/`process*` maintainers, getters re-pointed to the cascade breakdown
  (like the flipped building/bonus/feature getters). ~106 maintainer/consumer sites across `CvCity`/`CvPlayer`/`CvArea`.

Executed in ONE pass per cluster, verified live — no half-cut state.

---

## The STANDING conversion worklist (the mechanical census)

> **This is the map, kept here so the sweep is a worklist and not a rediscovery.** Every entry is a getter that
> currently reduces `÷100` internally — the shoehorn [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)
> bans. The fix per entry is uniform: **the getter carries ×100 (dissolve any `getX`/`getX100` pair down to ONE
> getter), and every consumer reduces at its own reader/whole-count use.** Counts are consumer sites of the HUMAN
> twin — the surface to rewire, mechanically derived, not judgment-filtered ([DEC-all-means-all](../../architecture/decisions.md#dec-all-means-all)).

| # | violation | human twin's body | sites | status |
|---|---|---|--:|---|
| 1 | `getYieldRate` / `getYieldRate100` | `getYieldRate100()/100` | 46 | open — the doc names this one explicitly; largest, and upstream of the yield work |
| 2 | `getExperience` / `getExperience100` | `getExperience100()/100` | 40 | open — self-contained (unit plane), parallelizable |
| 3 | `calculateDistanceMaintenance` · `calculateNumCitiesMaintenance` · `currInterceptionProbability` (`*Times100` twins) | `…Times100()/100` | 48 | open — same shape under the `Times100` naming |
| 4 | `getExtraYield` / `getExtraYield100` | `getExtraYield100()/100` | 12 | open |
| 5 | `angryPopulation` · `healthRate` | internal `÷100` (the revoked carve-out) | ~28 | open — carve-out revoked; convert like any other |
| — | `getCommerceHappinessPer` → `getCommerceHappinessByType` → `getCommerceHappiness` | was `/100` in the accessor | 9 | ✅ DONE — getters ×100; the 9 readers (2 AI, 2 pedia, 3 `/computed`, `CyCity`) reduce |
| 6 | `getUnitUpkeep{Civilian,Military}` / `…100` | `apply*Upkeep(raw100)` — applies a percent mod, returns **×100** | — | open, but a DIFFERENT defect: both twins are ×100, so this is a misleading duplicate pair (gross vs modifier-applied), not a reduce. Dissolve by NAMING (one getter, or names that say which), not by moving a `÷100` |
| — | `getBuildingCommerce` / `getBuildingCommerce100` | recompute-from-source Σ per building | — | NOT a violation (different quantities sharing a stem) |

> **Upkeep + maintenance are IN SCOPE — they are summed GOLD, applied after gold is computed (owner).** Gold is a
> yield ([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)), so it carries ×100 like every
> other channel; landing late in the expense pipeline is a position in the chain, not an exemption. The treasury
> subtraction happens in ×100 and only the displayed/stored value reduces.

**The tell that a conversion is right:** scale-mismatch fudge factors disappear. While #5 stood, an AI ratio needed
`angryPopulation() * 10000 / getCommerceHappinessPer(...)`; with both operands ×100 the units cancel and it returns to
`* 100` with no magic constant. A surviving odd multiplier means two operands are still on different scales.

### ⛔ CONVERT BY ARITHMETIC CLUSTER, NEVER BY GETTER

**A getter cannot be converted alone.** Its co-operands are on the same scale *by arithmetic necessity*: convert one
side and every mixing site needs a compensating `÷100` — manufacturing the very fudge factor that signals a misplaced
reduce. Convert the whole cluster and the mixing sites need **no change at all**, because the units already cancel.
This is why the sweep keeps being re-shoehorned: each getter looks independently convertible, and none is.

**The acceptance gate per cluster: ZERO new fudge factors at the mixing sites.** If a conversion forces compensating
constants, the cluster boundary was drawn wrong — stop and redraw it.

**The full violation census** — every human twin implemented as `<twin>/100`, both naming conventions
(`get*100` and `*Times100`), plus the two getters that reduce with no twin:

| cluster | violations | why they are ONE unit |
|---|---|---|
| **A — yield / food / wellbeing** (the big one) | `getYieldRate`←`getYieldRate100` · `getExtraYield`←`getExtraYield100` · `foodConsumption` · `getFoodConsumedByPopulation` (its `/10000` → `/100`) · `foodDifference` · **`angryPopulation`** · **`healthRate`** | `foodConsumption = getFoodConsumedByPopulation − angryPopulation − healthRate + foodWastage`, and `getYieldRate(FOOD) − foodConsumption()` appears at ~10 sites. **The two revoked carve-outs are a PREREQUISITE here, not a separate item** — the food chain consumes them directly. `happyLevel`/`unhappyLevel`/`goodHealth`/`badHealth` are already ×100 (the landed pilot), so only the two reducers remain |
| **B — commerce** | `getCommerceRate`←`…Times100` · `getBaseCommerceRate`←`…Times100` · `CvPlayer::specialistCommerce`←`…Times100` | joins A at the production→commerce term (`getYieldRate(PRODUCTION) × getProductionToCommerceModifier`) and at `getYieldRate(COMMERCE)`; the `…FromBuilding100` / `getExtraCommerce100` / `getMintedCommerceTimes100` feeders ride along |
| **C — gold / maintenance / upkeep** | `calculateDistanceMaintenance` · `calculateNumCitiesMaintenance` (both ←`…Times100`) · the `calculate{Base,Building,Colony,Corporation}MaintenanceTimes100` family · `getMaintenance`/`…Times100` · `getUnitUpkeep{Civilian,Military}` | **summed GOLD applied after gold is computed (owner)** → gold is a yield ([DEC-universal-yield](../../architecture/decisions.md#dec-universal-yield)), so it converts with B. ⚠ `getFinalExpense` folds `getInflationMod10000` — a **×10000** third scale to reconcile |
| **D — unit experience** | `getExperience`←`getExperience100` | self-contained (`experienceNeeded` + the level ladder); the one cluster genuinely parallelizable |
| **E — trade** | `calculateTradeProfit`←`…Times100` | tradeYield is the sanctioned live-yield INPUT the cascade folds ([modifier.md §2a](../../specs/modifier.md)) — joins A at the base package |
| **F — war weariness** | `getWarWeariness`←`getWarWearinessTimes100` | WW anger feeds the unhappiness term → joins A's wellbeing half |
| **G — ⚠ NOT a yield: AI counts / plot strength** | `CvArea::getEffNumAIUnits` · `CvPlayerAI::AI_getEffNumAIUnits` · `CvPlot::plotCountSM` · `CvPlot::plotStrength` (all ←`…Times100`) | ×100 here carries **fractional SizeMatters unit counts**, not a modifier channel. Same *shape* as a violation, different *nature* — needs an owner ruling before being swept in with the yields |

**Suggested order:** D (isolated, proves the mechanism on a small surface) → A (the keystone; unblocks the rest) →
B+C together (gold/commerce share the seam) → E, F (small, fold into A's tail) → G after its ruling.

### Cluster A — the MECHANIC (set this up to spec FIRST, then wire the consumers)

> **Owner sequencing: "set the mechanic up correctly, to spec, _then_ wire it in."** Do NOT open with 150 consumer
> edits. Build the value chain so it is internally ×100-consistent, then reduce at the readers. ⚠ A does not compile
> between the two halves — flipping a getter's scale breaks its consumers until they are wired — so A lands as ONE
> build, but it is DESIGNED in this order.

**Step 1 — the chain (each becomes ×100; pairs dissolve to ONE getter, [DEC-fixedpoint-x100]):**

| function | today | target |
|---|---|---|
| `CvCity::getYieldRate` | `getYieldRate100()/100` | **IS** the ×100 getter; `getYieldRate100` DELETED, its 14 call sites renamed (pure rename — both are ×100) |
| `CvCity::getExtraYield` | `getExtraYield100()/100` | ditto; `getExtraYield100` DELETED, 5 sites renamed |
| `CvCity::getFoodConsumedByPopulation` | `pop100 * perPop100 / 10000` | `… / 100` — two ×100 operands yielding ×100 food |
| `CvCity::foodConsumption` | human | ×100 — body unchanged once its operands are ×100 |
| `CvCity::foodDifference` | human | ×100 — falls out (`getYieldRate(FOOD) − foodConsumption()`) |
| `CvCity::angryPopulation` | `(unhappy−happy)/100`, clamped `[0, pop]` | ×100: drop the `/100`, clamp `[0, pop*100]` |
| `CvCity::healthRate` | `min(0,(good−bad)/100)` | ×100: `min(0, good−bad)` |
| `CvCity::foodWastage` | **returns `float`** | integer ×100 — see the hazard below; the float is an OOS defect, not a style choice |

**Step 2 — wire the consumers.** Each reduces at its own reader / whole-count use. The mixing sites
(`getYieldRate(FOOD) − foodConsumption()`, ~10 of them) need **NO change** — both operands are ×100 and the units
cancel. That is the gate: **if a mixing site needs a new constant, the cluster boundary was drawn wrong.**

Site counts (both names, all files): `getYieldRate` 47 · `getYieldRate100` 14 · `getExtraYield` 13 ·
`getExtraYield100` 5 · `foodConsumption` 16 · `getFoodConsumedByPopulation` 10 · `foodDifference` 41 ·
`foodWastage` 7 · `angryPopulation` 29 · `healthRate` 20. Files: `CvCity` · `CvCityAI` · `CvPlayer(AI)` · `CvPlot` ·
`CvArea` · `CvUnitAI` · `CvContractBroker` · `CvGameObject` · `CvCascadeAccumulator` · `CvHttpServer` ·
`CvGameTextMgr` · the `Cy*`/loader surfaces (⛔ per AGENTS.md the Python side is a NEW surface with the old
disconnected — never a `÷100` patched into an existing binding to keep it working).

**Two hazards found while mapping, both pre-existing:**
- **`CvCity::foodWastage()` returns `float`** — in a deterministic-lockstep engine where float math desyncs MP
  ([engine.md](../../reference/engine.md)); it is cast `(int)` into `foodConsumption`. Fixed-point exists precisely to
  prevent this. Fix it with cluster A.
- **`getInflationMod10000` is ×10000** — a third scale living alongside ×100 and human; reconcile in cluster C.
