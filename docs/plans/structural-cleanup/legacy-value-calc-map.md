# Legacy value-calc map — which getter computes each value, and what the dump must emit

> **Status:** reference · **Verified against:** `Sources/Engine/CvCity.cpp`, `CvPlayer.cpp`, `CvUnit.cpp`,
> `CvGameCoreDLL.cpp`, `CvProperties*.cpp`, `Assets/Python/Revolution/` — 2026-06-19 parallel agent sweep
> (waves 1–4).
> **Grounding:** every formula below was read at its named getter. **Line numbers DRIFT — confirm against
> the named function, not the integer.** **Getter VISIBILITY is NOT a reason to skip a source (owner ruling
> 2026-06-20):** if a getter the dump needs is `private`/`protected`, **PROMOTE it to `public`** — there is zero
> sensitive data in a game mod, so encapsulation never justifies leaving a calc source un-emitted. Never let
> "it's private, meh, don't need it" drop a source.
> **A realized getter may return a STORED value, not a live calc (owner ruling 2026-06-20):** distinguish
> `calcX()` (computes the value) from `getX()` (fetches the last-stored). For a stored/cached getter you MUST
> (a) emit the calc's INTERNAL sources so the value is REPRODUCIBLE, not merely read, and (b) confirm **WHEN**
> the store is set (its freshness) — a `getX()` read of a stale store is a silent wrong value. *Example:* per-unit
> `getUpkeep100()` is the store; `calcUpkeep100()` = `100×unitInfo.getBaseUpkeep() + extraUpkeep100`, then
> `×upkeepModifier`, then `×upkeepMultiplierSM` (NPC skipped) — maintained fresh on unit init + every source
> mutation + `recalculateUnitUpkeep` (so the store is current). Dump BOTH the store and the four calc sources.
>
> This is the **per-calc DESTROY-pass map**: for each per-turn value the engine realizes, *which legacy
> getter computes it, what components feed it, the x1/x100 + clamp gotchas, and what the diagnostic dump
> must emit so StoneBase reproduces it exactly.* You cannot delete a legacy calc you have not
> mapped ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)); this is that map.

## BLUF

Two jobs in one map:
1. **The DESTROY-pass map** (§1–§11) — every per-turn city / empire / unit-plane value calc traced to its
   realized getter + per-source components + gotchas, so the legacy mechanism can be shadowed and cut.
2. **The dump spec + audit** — what `/diagnostic/cityInput` / `playerInput` must emit per channel (the
   per-§ "Dump:" lines), and **§12 the audited truth** of what the dump *actually* emits today (the real
   gap list). Where a per-§ "Dump:" line conflicts with §12, **§12 is ground truth.**

**Scope verdicts** (carried through, not re-derived): SPATIAL → deferred to #429 (property propagators,
culture-spread, cultureDistance); **PYTHON-authoritative** → revolution index (deferred port), score (OUT
of scope, not gameplay-affecting per owner); STATEFUL/event-driven → live readings only (war-weariness,
property solver, golden-age/anarchy timers); stochastic → spawnRate (not a value-channel); float/OOS-care →
populationGrowthRate; nonexistent → `byCargo`; dead → `pillageGold` (building).

**The calc map is COMPLETE; the dump is NOT.** §12 is the actionable add-list.

> **Scales.** Every ×100 / per-100 / human / multiplier claim in this doc is governed by the
> [scale registry](../../specs/curators/fixed-point-and-scales.md) — that is the single source of truth for scales
> ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)). Where a getter name ends in
> `…100` or `…Times100`, the result is ×100 fixed-point; this doc names the scale per calc but does not
> restate the rules.

---

## 1. City CORE YIELDS — food / production / commerce (the full pipeline) ✅ BUILT (cityInput)

> **The emulation-grade trace** (owner ruling 2026-06-20: the core-yield calc is foundational to the outside legacy
> emulation and to parity — map it THOROUGHLY, every source/scale/clamp/order, not a one-liner). All values verified
> at the named getter in `CvCity.cpp`. Line numbers DRIFT — trust the function name. Four stages, in order:

### 1.0 Plot → city base (the worked-tile sum)
`CvCity::updateYield()` (`:1577`) just runs `CvPlot::updateYield` over `plots()`; each worked plot deposits its
`CvPlot::calculateYield` into the city accumulator `m_aiBaseYieldRate[y]` (via `changePlotYield`), read back as
`getPlotYield(y) = m_aiBaseYieldRate[y]` (`:11257`). The plot-level yield itself (nature = terrain+feature+river+
hills/peak + bonus; + improvement + route + cityChange + popChange + terrain/seaPlot/workingCity changes + landmark +
goldenAge; `max(0,·)`, city plots `max(getMinCityYield,·)`) is decomposed in **§10.1** — the §1 `base` bottoms out there.

### 1.1 `getBaseYieldRate(y)` (`:22906`) — the additive base [x1]
`= getPlotYield(y) + getTradeYield(y) + player.getFreeCityYield(y) + (player.isGoldenAge() && goldenAgeYield(y)>0 ? goldenAgeYield(y) : 0)`.
The worked-tile sum + trade-route yield + player free-city yield + golden-age yield, BEFORE specialists and modifiers.

### 1.2 `getYieldRate100(y)` (`:11246`) — the realized city yield [x100]
```
getYieldRate100(y) = min(CITY_MAX_YIELD_RATE, max(100,
        (getBaseYieldRate(y) + getSpecialistYieldTotal(y)) * getBaseYieldRateModifier(y)   // (base+specialist)[x1] × %[100+Σ] → x100
      +  100 * getExtraYield(y)))                                                           // extra[x1] re-scaled → x100
```
- The modified core `(base + specialist) × modifier` produces the x100 scale (modifier carries the ×100 because it is `100+Σ%`).
- Clamps: `max(100, ·)` floors at 1.00; `min(CITY_MAX_YIELD_RATE, ·)` caps. `getYieldRate(y) = getYieldRate100(y)/100` (the x1 twin, §7).
- **Why specialist is INSIDE the `×modifier` term but extra is OUTSIDE:** specialist yields take the city modifier exactly like worked tiles (#317); the extra bucket (corp/per-building-flat/per-pop) does not.

### 1.3 `getBaseYieldRateModifier(y, iExtra=0)` (`:11217`) — the 7-way % stack [base 100]
`= max(0, 100 + iExtra + bonus + building + event(city) + player + power? + area? + capital?)`, where:
| term | getter | dump key |
|---|---|---|
| bonus | `getBonusYieldRateModifier(y)` | `modBonus` |
| building | `getBuildingYieldModifier(y)` (`m_buildingYieldMod`) | `modBuilding` |
| event (city) | `getYieldRateModifier(y)` | `modEvent` |
| player | `player.getYieldRateModifier(y)` | `modPlayer` |
| power | `getPowerYieldRateModifier(y)` — **only if `isPower()`** | `modPower` |
| area | `area()->getYieldRateModifier(owner, y)` — only if `area()!=NULL` | `modArea` |
| capital | `player.getCapitalYieldRateModifier(y)` — **only if `isCapital()`** | `modCapital` |

`max(0, ·)` floors the WHOLE stack (a net-negative modifier becomes 0, not negative).

### 1.4 `getExtraYield(y)` — the unmodified flat bucket [x1, ⚠ truncated]
`getExtraYield100(y)` (`:11323`) `= m_aiExtraYield[y]*100 + getBuildingExtraYield100(y) + getBaseYieldPerPopRate(y)*getPopulation()`.
`getExtraYield(y) = getExtraYield100(y)/100` — ⚠ **truncated to x1 BEFORE the `×100` re-scale in 1.2**, so sub-unit
precision is lost (the documented x1-truncation gotcha; the `×100` is the proof the bucket is human-scale —
[scale registry §3](../../specs/curators/fixed-point-and-scales.md#3-how-to-figure-a-fields-scale-the-method--do-not-eyeball-the-name)).
Sources: flat extra-yield, per-building extra yields (already ×100), per-pop yields × population.

### 1.5 `getSpecialistYieldTotal(y)` (`:11351`) = `m_aiSpecialistYieldTotal[y]` — a maintained accumulator of specialist yields (city-modified, see 1.2).
Per assigned specialist of type X, `count[X] ×` the **FIVE** engine terms (`processSpecialist` :5156 + `getExtraSpecialistYield` :11745-57) — each with its curated home (own-output on the specialist unless noted):
| term | engine source | curated home (scope) |
|---|---|---|
| **intrinsic** | `SpecialistInfo.getYieldChange` | specialist `{y}.city.flat` (base) |
| **local** | `getLocalSpecialistExtraYield` ← building `LocalSpecialistYieldChange` (`CvCity` :4811, THIS city) | specialist `{y}.city.flat`, gated by the building **in-city** |
| **perType (building)** | `player.getExtraSpecialistYield` ← building `getSpecialistYieldChange` **non-local** (`CvPlayer` :7493, per instance, EMPIRE-wide) | specialist `{y}.empire.flat`, gated by the player **HAVING the building** (`{BUILDING, scope:empire}`) — mirrors `getGlobalYieldModifier`(empire) vs `getYieldModifier`(city) |
| **perType (trait)** | `player.getExtraSpecialistYield` ← trait `getSpecialistYieldChange` (`CvPlayer` :28582) | on the **TRAIT**, keyed by specialist (`{y}.empire.specialists.{SPEC}.flat`, governing-deliverer; active set, PURE-filtered — the trait simple/complex callout, [modifier](../../specs/modifier.md)) |
| **all** | `player.getSpecialistExtraYield` ← civic/trait `SpecialistExtraYields` (per ANY specialist) | civic/trait `{y}.empire.specialist.perSpecialist`, × the city's TOTAL specialists |
| **pct** | `player.getSpecialistYieldPercentChanges` ← civic `SpecialistYieldPercentChanges` (applied `/100` INTEGER, additive) | specialist `{y}.city.percent`, read as `Σpercent/100` |

**Dump:** base, specialist, modifier + the full 7-way breakdown (`modBonus/modBuilding/modPlayer/modEvent/modPower/modArea/modCapital`), extraYield (x1), extraYield100, legacy100, cap. **DONE + verified live** (London 3/3).

## 2. COMMERCE split — `CvCity::getCommerceRateTimes100`

Two-stage; result x100. `getCommerceRateAtSliderPercent(eC, slider)` (`CvCity.cpp` ~11953):
```
if isDisorder(): 0
iRate  = min(CITY_MAX_YIELD_RATE100, getYieldRate100(YIELD_COMMERCE))
iExtra = min(CITY_MAX_YIELD_RATE100, getBaseCommerceRateExtra(eC))
iRate  = iRate * slider / 100 + iExtra
if iRate < CITY_MAX_YIELD_RATE:
    iRate = (iRate>0) ? iRate*getTotalCommerceRateModifier(eC)/100 : iRate*100/getTotalCommerceRateModifier(eC)
    iRate += getYieldRate(YIELD_PRODUCTION) * getProductionToCommerceModifier(eC)
if iRate<0 and (eC==CULTURE or RESEARCH): return 0
if iRate < MIN_TOL_FALSE_ACCUMULATE (-9999): return CITY_MAX_YIELD_RATE
return min(CITY_MAX_YIELD_RATE, iRate)
```
- `getBaseCommerceRateExtra` (x100) = 100×(specialist + extraSpecialist) + 100×religion + 100×corp + `getBuildingCommerce100` + 100×player.getExtraCommerce100… + minted (gold) + 100×goldenAge.
- `getTotalCommerceRateModifier` (base 100) sums bonus + building + player-from-buildings + event + player (− event − from-buildings, **double-count subtraction**) + capital; `max(1, ·)`.
- Gold is RESIDUAL: `getCommerceFromPercent` for gold = yield×(100 − Σother-sliders)/100.

**Dump (per commerce gold/research/culture/espionage):** slider (`player.getCommercePercent`), baseExtra100 (`getBaseCommerceRateExtra`), totalModifier (`getTotalCommerceRateModifier`), prodToCommerce (`getProductionToCommerceModifier`), realized100 (`getCommerceRateTimes100`). City-level once: yieldCommerce100 (`getYieldRate100(YIELD_COMMERCE)`), prodRate (`getYieldRate(YIELD_PRODUCTION)`), isDisorder, consts CITY_MAX_YIELD_RATE100 + MIN_TOL_FALSE_ACCUMULATE.

> ~~⚠ verify visibility of getBaseCommerceRateExtra / getTotalCommerceRateModifier / getProductionToCommerceModifier~~ — **STALE (§12): all three are public, called directly.**

**StoneBase §2 implementation (2026-06-27) — `CommerceRateCascade`.** Reproduces `getCommerceRateTimes100` term-by-term,
leaning on the already-parity'd §1 yield cascade: `yieldCommerce100` = `YieldRate100("commerce")`; `specialistCommerce`
(+extra) = `SpecialistYieldTotal(channel)`; `buildingCommerce100` = `BuildingFlatYield100(channel)` **+ shrine + corp-HQ**
(see below); `totalModifier` = `TotalYieldModifier(channel)`; `goldenAge` = `GoldenAgeYield(channel)`; `playerExtra` =
trait `CommerceChanges` (empire.flat); slider from `/state` (`world`-plumbed `EvalState.CommerceSlider`); combine bit-exact
to `CvCity.cpp:11969-11996`. gold/research/espionage reach near-parity; **culture is the open channel** (its own
non-§2 sources below). Two §2 sub-mechanics that fold into per-building `buildingCommerce100` (the engine attributes them
to the holding building via `getBaseCommerceRateFromBuilding100`), both modelled like the building's own flat:
> - **Shrine commerce** — a building with `identity.shrine = RELIGION` adds `religion.shrine.{c} × countReligionLevels(R)`
>   (the `shrine` bespoke section × the `world.religionLevels` tally). Verified exact (Church-of-the-Nativity gold 163).
> - **Corp-HQ commerce** — a building with `identity.corporationHQ = CORP` adds `corp.{c}.empire.headquarters.perCorporationLevel
>   × countCorporationLevels(C)` (`world.corporationLevels` tally). Verified exact (HQ_CEREAL_MILLS gold 44).

> **⚠ FIXED-POINT (OOS): flat commerce must sum in ×100, not truncate (2026-06-27).** Fractional human commerce flats
> (e.g. a Folklore building's research `[1, −0.6@TECH_LANGUAGE, −0.1@…×4]` → nets 0) MUST be summed as `Σ round(human×100)`,
> never `(int)human × 100` (which truncates −0.6→0 and kept a phantom +1 ×93 buildings). The StoneBase reader is
> `YieldModifierCascade.SumUnit100`. Yields are integer so unaffected; only commerce has fractional flats. This is the
> [scale registry](../../specs/curators/fixed-point-and-scales.md) integer-math rule applied in the cascade.

> **⚠ TODO — per-building FREE-SPECIALIST commerce (2026-06-27, TRACED).** `getBaseCommerceRateFromBuilding100`
> (`CvCity.cpp`) folds a building's **free specialists' commerce** into that building's per-building commerce:
> `for iI in 1..kBuilding.getFreeSpecialist(): += 100 × player.specialistCommerce(getBestSpecialist(iI), eIndex)`
> (plus a typed `getFreeSpecialistCount` → `getAdditionalBaseCommerceRateBySpecialist` term). So a property
> pseudo-building like `EDUCATION_ENLIGHTENED` (`iFreeSpecialist=3`, JSON `freeSpecialists.city.any=3`, no own commerce,
> `culBld100=0`) reports engine `culFlat100=2100` = its 3 free specialists × their per-spec culture. This was the
> culture-channel residual: the cascade's `buildingCommerce100` (and the engine's per-building dump) attribute
> free-specialist commerce to the BUILDING, but StoneBase's `CommerceRateCascade` only sums each building's OWN
> flat — it does not yet add the building's free-specialist commerce.
>
> **FOUNDATIONAL (owner ruling 2026-06-27): StoneBase must calculate FREE SPECIALISTS early, or chase ghosts forever.**
> Free specialists feed many channels (specialist commerce, yields, GP-rate, happiness/health) — wrong here ⇒ every
> downstream residual is a ghost. Grounded trace:
> - A building's **generic** free specialists (`iFreeSpecialist` = JSON `freeSpecialists.any=N`) are **NOT stored** in the
>   city specialist counts; they are computed **on-the-fly per building** in each `getXBySpecialist` getter
>   (commerce `:12327`, yield `:11078`, GP-rate `:7288`, happiness `:9032/:9179`): `for iI in 1..getFreeSpecialist(): getBestSpecialist(iI)`.
> - So `/state`'s specialist block (`getSpecialistCount + getFreeSpecialistCount`, per-type) **EXCLUDES** them (London
>   reads 150; the engine has the building free specialists on top). NB the *unattributed* array
>   `m_paiFreeSpecialistCountUnattributed` (`:22461`, Python/GP-joined) is a DIFFERENT thing and IS folded into the
>   per-type count — the building generic ones are not.
> - **The wall:** `getBestSpecialist` (`CvCityAI.cpp:12355`) picks the slot via `AI_specialistValue` — an AI heuristic,
>   NOT a clean rule — so the *assignment* is not cleanly reproducible offline.
> - **Correction (owner 2026-06-27): free specialists are NOT normal-specialist assignment.** The `getBestSpecialist`/
>   `AI_specialistValue` path is for assigning CITIZENS to specialist slots — a different mechanism. No AI valuation
>   enters the free amount.
> - **⛔ Correction #2 (owner 2026-06-27): they are NOT a `grant` — they are the REVERSIBLE `freeSpecialists` MODIFIER
>   family.** "Free specialists from buildings go away if the building goes away" — so by [json](../../specs/json.md) §5
>   they are emphatically **not** `grants` (a `grant` PERSISTS — a granted unit survives its granter). They are a
>   [modifier](../../specs/modifier.md) deposit: re-evaluated every recompute, contributing **only while the source is
>   active (constructed, non-dormant)**, and removed the instant the source is. The engine proves the reversibility —
>   `changeFreeSpecialist(kBuilding.getFreeSpecialist() × iChange)` runs with `iChange = −1` on removal (`CvCity.cpp:4639`),
>   so `getFreeSpecialist()` is a maintained accumulator, not a one-shot. **The data is already correct** — the curator
>   emits every building/civic source under the `freeSpecialists` *family*, never `grants` (verified
>   `curate_building.py:93-95`, `curate_civic.py:78`); the word "grant" was a description mistake in THIS note, now fixed.
> - **The COMPLETE source map of `totalFreeSpecialists()` (`CvCity.cpp:5747`), every term grounded to its JSON source:**
>   `total = max(0, Σ of the below)`, and `0` if `pop < 1`. All are reversible modifier deposits from **ACTIVE** sources:
>   - `getFreeSpecialist()` (city; London = **9**) = Σ active buildings' `iFreeSpecialist` → **`freeSpecialists.city.any`** (`:4639`).
>   - `area()->getFreeSpecialist(owner)` = Σ active buildings' `iAreaFreeSpecialist` → **`freeSpecialists.area.any`** (`CvPlayer.cpp:7394`).
>   - `player.getFreeSpecialist()` = Σ active buildings' `iGlobalFreeSpecialist` (→ **`freeSpecialists.empire.any`**, `CvPlayer.cpp:7395`)
>     + Σ adopted civics' `iFreeSpecialist` (→ **`freeSpecialists.empire.any`**, `:18045`) + Σ active traits' `iFreeSpecialist` (→ **`freeSpecialists.empire.any`**, `:28515`).
>   - improvement term = `Σ_imp getImprovementFreeSpecialists(imp) × countNumImprovedPlots(imp)`, where the per-improvement
>     rate is Σ active buildings' `ImprovementFreeSpecialists[imp]` (`:4885`). ⚠ **VERIFIED CURATOR BUG** — the engine scales
>     by the **count of improved plots** (a `per` count-scaler), but `curate_building.py:592-600` models it as a *presence*
>     `enabled:{improvement}` flat (and self-flags it unverified at `:593`). Real fix: emit `freeSpecialists.city.any = n,
>     per:{improvement count in city}`, not a presence gate. (Contributes only if London has such a building+improvement.)
>   - per-wonder = `(anyTrait isFreeSpecialistperWorldWonder ? numWorldWonders : 0) + (…National ? numNationalWonders : 0)
>     + (…TeamProject ? numTeamWonders : 0)` — trait flags (`CvPlayer.cpp:28559-28561`), × per-city wonder counts.
> - **Typed free specialists are a SEPARATE ledger, already in `/state`.** `getFreeSpecialistCount(SPECIALIST_X)` (the
>   `FreeSpecialistCounts` per-type family + the `Unattributed` array, `CvCity.cpp:14132`) is counted per-type in `/state`'s
>   specialist block — NOT part of `totalFreeSpecialists`. The genuinely *persistent* grant-shaped ones (event `:19008`,
>   era-advance `:12452`, GP-join/unit-disband `:8788`/`:2664`, all `bUnattributed`) live there; the generic city/area/
>   empire/improvement/wonder ones (this note) do not.
> - **Both output into the same bucket** (owner): the free amount + the `/state` assigned per-type counts feed the
>   specialist yield/commerce/GP-rate/happiness/health output; watch the overlap to avoid double-count. Order (owner):
>   get the free AMOUNT right FIRST.
> - **Oracle emitted + verified** (`/computed/cities/yields`, committed): `totalFreeSpecialists` (London = **59**) +
>   `cityFreeSpecialist` (the `freeSpecialists.city.any` part, London = **9**) — vs `/state` assigned 150. The 59 free
>   specialists' output is what the cascade silently missed.
> - **Build plan (StoneBase, foundational):** (1) compute the free amount by summing the `freeSpecialists.any` count-leaf
>   over **active** buildings (city/area/empire) + adopted civics + active traits (empire) + the improvement `per`-scaler
>   + per-wonder, diff vs `totalFreeSpecialists`; (2) resolve the OUTPUT typing (which specialist type the generic `any`
>   free ones produce as — verify against the engine getters, don't guess); (3) wire the free amount into the specialist
>   output bucket, avoiding double-count with the `/state` per-type assigned counts.

## 3. HEALTH + HAPPINESS — good/bad signed-split

`goodHealth()` (`CvCity.cpp` ~5831) = Σ `max(0, source)` over: freshWater, feature, bonus, totalGoodBuildingHealth, extraHealth, handicap, improvement/100, specialist/100, corporation, extraTechHealth, player.{extra,civic,civilization,world,project}Health. `badHealth()` (~5858) = `unhealthyPopulation − Σ min(0, source)` (same sources, negative parts) − espionageHealthCounter. `healthRate = min(0, good − bad)`.

`happyLevel()` (~5689) = Σ `max(0, source)` over ~22 sources (revSuccess, largestCity, military, stateReligion, building good, feature/bonus good, religion good, commerce, area/player building, extra, handicap, vassal, civic, specialist/100, world, project, corporation, celebrity, techHappiness, +temp). `unhappyLevel()` (~5606) = `(Σ anger% × (pop+extra) / PERCENT_ANGER_DIVISOR) − Σ min(0, good source) + Σ bad source`; gated 0 by `isNoUnhappiness`/no-capital-unhappiness. anger% = overcrowding+noMilitary+culture+religion+hurry+conscript+defy+warWeariness+revRequest+revIndex+Σcivic.

**Gotchas:** specialist/improvement health & happiness are **/100** (the legacy latent /100 — see [scale registry §4c](../../specs/curators/fixed-point-and-scales.md#4c-the-100-space-addends-that-lack-a-100-getter--the-heuristics-blind-spot)); espionage counters clamped to 8; flags `isNoUnhappiness`/`isNoUnhealthyPopulation`/`isBuildingOnlyHealthy` zero-out; per-pop via `calculatePopulationHealth/Happiness`.

**Dump:** realized `goodHealth`/`badHealth`/`healthRate` + `happyLevel`/`unhappyLevel`/`angryPopulation`; for fidelity-reproduction dump the component buckets (building good/bad, bonus, feature, specialist/100, extra, per-pop, civic/trait/player, corporation, tech) + the anger%-sum + pop + the gate flags. (Many components — itemize the buckets the maps list. **§12: happiness dump has REAL GAPS** — anger% sources, several happy/unhappy terms, and gate flags are missing.)

## 4. City DEFENSE — `CvCity::getDefenseModifier`

`getTotalDefense(bIgnoreBuilding)` (`CvCity.cpp` ~10198) = `max(bIgnoreBuilding?0:getBuildingDefense(), getNaturalDefense()) + player.getCityDefenseModifier() + calculateBonusDefense()`.
`getDefenseModifier(bIgnoreBuilding)` (~10204) = `isOccupation() ? 0 : max(getExtraMinDefense(), getTotalDefense() * (MAX_CITY_DEFENSE_DAMAGE − getDefenseDamage()) / MAX_CITY_DEFENSE_DAMAGE)`.
- `getBuildingDefense` = `m_iBuildingDefense` (aggregate of all buildings' defenseModifier, not per-building). `getNaturalDefense` = culture-level cityDefenseModifier. `calculateBonusDefense` = Σ over had bonuses. Flat percents, integer division in the damage decay; floored at `getExtraMinDefense`. (`getMinimumDefenseLevel` is a SEPARATE production-gate floor, only under `GAMEOPTION_COMBAT_REALISTIC_SIEGE`.)

**Dump:** buildingDefense, naturalDefense, cityDefenseModifier (player), bonusDefense (`calculateBonusDefense`), defenseDamage, MAX_CITY_DEFENSE_DAMAGE, extraMinDefense, isOccupation, realized getTotalDefense + getDefenseModifier.

## 5. MAINTENANCE / UPKEEP — cost-style via `getModifiedIntValue`

`getModifiedIntValue(v, mod)` (`CvGameCoreDLL.cpp:689`) = `mod>0 ? v*(100+mod)/100 : mod<0 ? v*100/(100−mod) : v` — the shared **cost-asymmetric** combiner (the §7 hub).
- CITY maintenance `getMaintenanceTimes100` (`CvCity.cpp` ~7579, x100): `era.getInitialCityMaintenancePercent() + getModifiedIntValue(calculateBaseMaintenanceTimes100(), getEffectiveMaintenanceModifier())` (skipped if disorder/WeLoveTheKing/pop 0). Base = Σ building+distance+numCities+colony+corporation (each `…Times100`). EffectiveModifier = city `getMaintenanceModifier` + player + `area()->getTotalAreaMaintenanceModifier` (+ connected-to-capital). Caps: numCities ≤ 2,000,000; colony capped; rebels ×50%.
- CIVIC upkeep `getSingleCivicUpkeep`/`getCivicUpkeep` (`CvPlayer.cpp` ~14219): `(max(0,(pop+OFFSET)*UpkeepInfo.populationPercent/100) + max(0,(cities+OFFSET)*cityPercent/100))` → getModifiedIntValue(upkeepModifier) → ×handicap.civicUpkeep% → (AI) ×AI mods; `max(1,·)`; rebels halve total.
- UNIT upkeep `getFinalUnitUpkeep` (`CvPlayer.cpp` ~10327): `(civilianNet + militaryNet) × handicap.unitUpkeep%/100 × (AI) aiUnitUpkeep%/100 × (100+aiPerEra×era)/100`. Per-unit `calcUpkeep100` (`CvUnit.cpp` ~15798, x100): `(100×baseUpkeep + extraUpkeep100)` → getModifiedIntValue(upkeepModifier) → getModifiedIntValue(upkeepMultiplierSM). (`iExtraUpkeep100` is one of the 6 closed per-100 fields — [scale registry §4b](../../specs/curators/fixed-point-and-scales.md#4b-the-closed-per-100-set--100-to-humanize).)

**Dump:** city — eraInitialPercent, the 5 base components (×100), effectiveMaintenanceModifier, realized getMaintenanceTimes100, disorder/WLTK flags. player — civicUpkeep, finalUnitUpkeep. **§12: only the realized totals are dumped — NO decomposition** (the "+ the handicap/AI/era mults as inputs" claim is FALSE today).

## 6. UNIT-plane stats — `CvUnit::maxCombatStr` / `currCombatStr` (separate `unitInput` endpoint)

`baseCombatStrPreCheck` (`CvUnit.cpp` ~11341) = `(m_iBaseCombat + getExtraStrength()) × (100 + getExtraStrengthModifier())/100`. `maxCombatStr(plot,attacker)` (~11465, ~730 lines) = `baseCombatStr × modifier` where modifier = Σ ~40 situational sources (getExtraCombatPercent, plot/city/hills/feature/terrain, vs-unitCombat/domain/animal/religious/size, surrounded, attack/defense mods); `max(1, ·)`; SM divides by 100. `currCombatStr = maxCombatStr × getHP()/getMaxHP()`.

**⚠ CRITICAL build constraint:** CvUnit exposes **AGGREGATE `getExtra*` ONLY — no per-source getter.** Per-source attribution = iterate the unit's promotions (`getPromotionKeyedInfo`/`isHasPromotion`) + unitcombats (`getUnitCombatKeyedInfo`/`isHasUnitCombat`) and sum each from `CvPromotionInfo`/`CvUnitCombatInfo` externally. The dump should emit the aggregate `getExtra*` set + the unit's promotion/unitcombat lists (the emulator attributes per-source from the Info JSON).

**Stat vocabulary (the dumpable `getExtra*` set, `CvUnit.h`):** strength group (combatPercent, strength, strengthModifier, city/hills attack&defense, vsBarbs, religious, attack/defenseCombatModifier, damage, unnerve/enclose/lunge/dynamicDefense, maxHP), withdrawal, firstStrikes/chanceFirstStrikes, collateral, bombardRate, air(range/intercept/evasion), moves/moveDiscount, enemy/neutral/friendlyHeal, visibilityRange, workMod-per-build, capture(prob/resist), vs-keyed terrain/feature/unitCombat/domain/flanking. **Aggregate-fidelity first; per-source = the promotion/unitcombat attribution pass.** **§12: ENTIRELY ABSENT today — there is no `unitInput` endpoint; this is an UNBUILT SPEC.**

---

## 7. DUPLICATE / REDUNDANT computation — the DESTROY-pass dedup map

What the cascade UNIFIES (delete N paths → one accumulator). From the 2026-06-19 sweep.

### Cost / combat
- **`getModifiedIntValue` cost-combiner — ~23 call sites** (`CvCity.cpp` production-cost 3621/3626/3631, growth 6005-6008, hurry 6065-6068, maintenance 7616 + distance/numCities/scope deltas 7473/7494/7516/7540/7564/7566, corp 7862; `CvPlayer.cpp` civic-upkeep 14242/14254, free-unit 10220/10225, research 8214/15540/15546, warWeariness 10945-10959, production fallback 17305/17577). The cascade routes ALL cost mods through ONE combiner site.
- **Maintenance modifier triple-sum (city + player + area)** — `getEffectiveMaintenanceModifier` (`CvCity.cpp:7590`) sums city `getMaintenanceModifier` + player + `area()->getTotalAreaMaintenanceModifier`; **re-summed independently for UI** at `CvDLLWidgetData.cpp:5081` (a scope-mismatch duplicate). Resolve scope order ONCE/turn.
- **Unit extra-stat DUAL-FEED (8 stats)** — strength, strengthModifier, maxHP, attackCombatMod, defenseCombatMod, upkeep100, bombardRate, cargo are each fed from BOTH `processPromotion` (`CvUnit.cpp` ~18678+) AND `processUnitCombat` (~18283+) into the SAME `m_iExtra*` member. One unified deposit, not two pipelines.
- **Commander/commodore re-walk** — `getExtraAttackCombatModifier`/`getExtraDefenseCombatModifier` (`CvUnit.cpp` ~15606-15655) re-traverse commander+commodore pointers and re-sum on EVERY call (not cached).

### Economic (yields/commerce/health/happiness)
- **Commerce-modifier ADD-THEN-SUBTRACT dedup** — in `getTotalCommerceRateModifier` (`CvCity.cpp` ~12008-12021), `CommerceRateModifierfromEvents` and `…fromBuildings` are ADDED then SUBTRACTED (folded into the player's generic `getCommerceRateModifier` AND tracked separately for UI). The cascade keeps ONE accumulator, no reverse-subtraction.
- **Parallel city/area/player accumulators** — building **good/bad health** (city `getBuildingGoodHealth`:8294 + `area()`:659 + player:10798, summed in `goodHealth` ~5809) and building **happiness** (city:8449 + area:701 + player:10867, summed in happy/unhappy ~5644/5705). Three scopes summed at read time → the cascade resolves scope roll-up once.
- **x1 / x100 twins** — `getYieldRate`÷`getYieldRate100`, `getCommerceRate`÷`getCommerceRateTimes100`, `getBaseCommerceRate`÷`…Times100`, `getExtraYield`÷`getExtraYield100`, `getSpecialistCommerce`÷`m_aiSpecialistCommerce100`: the x1 is always `x100/100` (a derived twin, not a separate value). Cascade stores once.
- **`getTotalCommerceRateModifier` is the central hub** (`CvCity.cpp` ~11995, cached w/ dirty flag) assembling bonus+building+player-from-buildings+event+player(−dedup)+capital — the single point the commerce-modifier cascade replaces.
- **No-cache recompute HOTSPOTS** (recomputed every call, multi-source, called per turn by UI/AI): `goodHealth`/`badHealth` (~13 sources each), `happyLevel`/`unhappyLevel` (~20-25 sources each), `getBaseYieldRateModifier`, `getCommerceFromPercent`, `getBaseCommerceRateExtra`, and O(n) loops `getImprovement{Good,Bad}Health`/`getSpecialist{Good,Bad}Health`/`getSpecialist{,Un}Happiness`. The cascade's O(1) summed accumulator is the direct replacement (perf win + dedup).

---

## 8. THE COMPLETE FAMILY INVENTORY — "all the things that modify all the places" (checklist)

The definitive universe of modifier FAMILIES (the modifiable variables / "places"), from a 2026-06-19
inventory sweep. Status = the EMULATOR's coverage (dump + fidelity guard), NOT the cascade engine's. The
"things that modify" each = the source classes (buildings/civics/techs/traits/bonuses/religions/corps/
specialists/events/handicap/era/cultureLevel/…).

- **✅ LIVE (dump + guard verified):** `food`, `production`, `commerce` (city yields).
- **◐ DUMP built, guard pending:** the commerce split — `gold`, `research`, `culture`, `espionage` (commerce-derived).
- **📋 MAPPED (§1-5; dump + guard pending):** `health`, `happiness` (good/bad split), `defense` (amount/min/bombard/…), `maintenance`, `upkeep`.
- **📋 MAPPED (§6; needs the `unitInput` endpoint, aggregate-only):** unit-plane — `strength` (+ all members), `withdrawal`, `firstStrike`, `bombard`, `collateral`, `air`, `heal`, `capture`, and partials `movement`/`experience`/`workRate`/`cargo`/`vision`/`espionage`(unit).
- **✅ MAPPED (§9, wave-2):** each `PROPERTY_*` (solver, §9.1), `revolution` (Python-authoritative — only the C++ anger step is reproducible, §9.2), `growth`+`foodKept` (§9.3), `inflation`/`hurry`+`hurryAnger`/`freeExperience` (§9.4), `buildRate`/`greatPeopleRate`/`tradeRoutes` (§9.5). **NB `culture` RATE = `COMMERCE_CULTURE`** (already captured via commerce); only plot-culture *spread* is extra (spatial → #429).
- **✅ MAPPED (§10, wave-3) — with corrections:**
  - plot-substrate: `movement`, `cultureDistance` (spatial), `buildTime`, `vision` — §10.1. (Plot YIELD base decomposes the §1 yield `base`.)
  - cost/duration scaling: `costs`/`buildCost`/`techCost`, `durations`, `perEra`, `missionYieldMultiplier` — §10.2, all via the `getModifiedIntValue` hub (§7).
  - building-level: `cityCapture`, `occupationTime`, `espionageDefense`, `populationGrowthRate` (FLOAT log-space), `healing` — §10.3. **`pillageGold` is an ORPHANED/dead building field** (→ drop).
  - **CORRECTIONS:** ~~`celebrity happiness` does not exist~~ — **WRONG, retracted 2026-06-19: it DOES exist** (`CvCity::getCelebrityHappiness` 5599 → `happyLevel` 5715; unit-derived). `byCargo` does not exist (vestigial); `byOccupant` = military happiness + the celebrity term. `spawnRate` is a **stochastic per-plot event**, not a per-turn value-channel. `stateReligion` is a predicate gate on existing families (6 sub-modifiers), not its own channel. Inert/edge unit flags `poison`/`revoltProtection`/`survivor` stay out.
- **⛔ NOT modifier families (out of the emulator):** `grants`/`enables`/`obsoletes`/`replaces`/`requires`/`allowed`/`identity`/`ui`/`world`/`sound`/`cost`/`ai`; one-shot pulses (`goldenAge`, population/revolution bursts) live in `grants`.

## 9. Wave-2 channel maps

### 9.1 PROPERTY (each `PROPERTY_*`) — a STATEFUL SOLVER, not a sum

Realized value getter: `CvProperties::getValueByProperty(eProp)` (`CvProperties.cpp` ~101) — **RAW INT, no x100** (a CRIME value of 50 is 50). Per-turn value = `CvPropertySolver::gatherAndSolve` (`CvPropertySolver.cpp` ~421) — **3 phases, each predict→computePredict→correct→apply**: **(1) Propagators** (cross-OBJECT spread/gather/diffuse), **(2) Interactions** (cross-PROPERTY convert-constant/convert-percent/inhibited-growth), **(3) Sources**.
- **Sources (the yield-like deposits):** `CONSTANT` (`iAmountPerTurn`), `CONSTANT_LIMITED` (cap to `iLimit`), `DECAY` (`-(iPercent * max(|v| - iNoDecayAmount, 0)) / 100`, gated off below the no-decay threshold), `ATTRIBUTE_CONSTANT` (`object.getAttribute(eAttr) * iAmountPerTurn`, e.g. ×population). Authored on `CvPropertyInfo::m_PropertyManipulators` (+ per-object manipulators), NOT on the building directly.
- **`targetLevel` / `operationalRangeMin/Max`** are AI-need + UI/heuristic only — **NOT solver clamps** (no per-turn hard bound; the predict/correct split is what converges).
- **Effect bands** (`PropertyBuilding {iMinValue,iMaxValue,eBuilding}`) gate the effect-buildings each turn (`CvCity::checkPropertyBuildings` ~1507) — the dormancy the cascade models via `requires.operate`.
- **⚠ EMULATOR:** the per-turn VALUE is the solver, not a bare per-source sum. **Propagators = the #429-deferred SPATIAL leakage** — the property channel reproduces the **sources + interactions** delta (non-spatial) and flags propagators as #429. It is `value + Σ(source deltas via predict/correct) (+ interactions)`, spatial excluded.

### 9.2 REVOLUTION — PYTHON-authoritative index; only the C++ anger step is reproducible

**The index is computed entirely in PYTHON** (`Assets/Python/Revolution/Gameready/Revolution.py` `updateLocalRevIndices` ~974): per-turn Δ = `gameSpeedMod × revIdxModifier × Σ(localEffects) + revIdxOffset + feedbackDecay`, over ~15 grievances (happiness, distance-to-capital, colony, connectivity, religion, cultureRate, nationality, health, garrison, spirit, size, starvation, occupation, civics/traits/buildings via `RevUtils`, difficulty) — using `pow()` + float math. **C++ is READ-ONLY:** `m_iRevolutionIndex` is set by Python (`setRevolutionIndex`/`changeRevolutionIndex`, `CvCity.cpp` ~956-969); `getRevolutionIndex`/`getLocalRevIndex` expose it.
- **The one C++-reproducible part — anger conversion** (`CvCity.cpp:5509`): `getRevIndexPercentAnger = min(40, (125 + min(getLocalRevIndex()*5, 100)) * (getRevolutionIndex() - 325) / 7500)`, 0 below index 325; feeds `unhappyLevel` (already a §3 input).
- **⚠ EMULATOR / OOS:** the INDEX calc is Python-authoritative + float/`pow` → **NOT C++-reproducible, NOT OOS-deterministic** (pending Python→C++ port). The `revolution` channel reproduces ONLY the C++ anger step (dump `revolutionIndex` + `localRevIndex` → assert `getRevIndexPercentAnger`); the index computation is deferred Python-side. `m_iRevolutionIndex` saves as a plain int.

### 9.3 GROWTH + foodKept — cost-style, reproducible

`foodDifference()` (`CvCity.cpp:5980`) = `getYieldRate(YIELD_FOOD) - foodConsumption()` (disorder→0; foodProduction city→`min(0,·)`; pop1/food0→`max(0,·)`). `foodConsumption` = `getFoodConsumedByPopulation - (angryPop if noAngry) - healthRate + foodWastage`.
`growthThreshold()` (`CvCity.cpp:6003`) = `getModifiedIntValue(player.getGrowthThreshold(pop), city.getPopulationgrowthratepercentage() + player.getPopulationgrowthratepercentage())`, `×0.5` if hominid, `max(1,·)`. `player.getGrowthThreshold` (`CvPlayer.cpp:24435`) = `BASE_CITY_GROWTH_THRESHOLD + (pop-1)*CITY_GROWTH_MULTIPLIER`, `×gamespeed% ×era.growthPercent% ×(AI handicap) ×(goldenAge less-food)`.
`foodKept`: `getFoodKeptPercent` clamped **[0,99]** (per-source building `getFoodKept`); stored `m_iFoodKept` capped at `growthThreshold × pct/100`, refund-on-growth.
- **EMULATOR:** `growth` (threshold + foodDifference) + `foodKept` — reproducible from base-threshold + growth% + food produced/consumed. Uses `getModifiedIntValue` + gamespeed/era/handicap/goldenAge scalers (overlaps §7).

### 9.4 INFLATION / HURRY / FREE-XP / CULTURE

- **inflation** (`CvPlayer.cpp:7965/8008`): `getInflationMod10000` = base `100×getHurriedCount() × handicap.inflationPercent/100`, modified by `(m_iInflationModifier + civic+project+tech+building inflation − 100×isRebel)` via `getModifiedIntValue` (+AI handicap), returns `10000 + perTurn`. `getInflationCost = preInflated × mod/10000 − preInflated`; `preInflatedCosts` = treasuryUpkeep + totalMaintenance + civicUpkeep + finalUnitUpkeep + unitSupply + corpMaint. **Anarchy→0. x10000 scale.** Per-source: civic/project/tech/building inflation getters. **§12: only realized total + preInflatedCosts dumped — per-source NOT.**
- **hurry** (`CvCity.cpp:6094/6117`): gold = `hurryCost × hurry.goldPerProduction` (min 1); pop = `1+(cost-1)/prodPerPop` (min 1). **Anger** `getHurryPercentAnger` (`CvCity.cpp:5448`, timer-based, feeds `unhappyLevel` §3): `1+(1+(timer-1)/flatHurryAngerLength)×HURRY_POP_ANGER×PERCENT_ANGER_DIVISOR/pop`; `flatHurryAngerLength = HURRY_ANGER_DIVISOR ×gamespeed% ×(100+getHurryAngerModifier)%`. Per-source: building `m_iHurryAngerModifier` + player `getNationalHurryAngerModifier`. **§12: only realized hurryAnger dumped; the gold/pop COSTS NOT dumped.**
- **freeExperience** (`CvCity.cpp:3187`, at unit-build): `getFreeExperience(city) + player.getFreeExperience()` + (if `canAcquireExperience`) `getSpecialistFreeExperience` + `getUnitCombatFreeExperience(combat+sub)` (city+player) + `getDomainFreeExperience` (≥0). Per-source: building/civic/trait `changeFreeExperience`.
- **culture** — ⚠ city culture RATE **IS** `getCommerceRateTimes100(COMMERCE_CULTURE)` (`doCulture`, `CvCity.cpp:16386`) → **already captured via commerce** (no separate channel). The EXTRA is **plot-culture SPREAD** (`doPlotCulture` distance-dropoff, `CvCity.cpp:16393`) = **SPATIAL (#429-adjacent)** + plot decay — out of the containment model, like property propagators. Storage x100.

### 9.5 buildRate / greatPeopleRate / tradeRoutes

- **buildRate** (`getProductionModifier(item)`, `CvCity.cpp:3867/3911/3940`): summed signed-% from player+city `unit/building/project/domain/unitCombat/military/space/stateReligion/bonus` mods. Applied as a **DISCOUNT**: `effectiveCost = max(1, getModifiedIntValue(player.getProductionNeeded(item), −modifier))` ≈ `base×(100−mod)/100`. x1 signed %. **§12: only the UNIT overload dumped (behind `?type=UNIT_*`); building/project NOT.**
- **greatPeopleRate** (`getGreatPeopleRate`, `CvCity.cpp:7153`) = `getBaseGreatPeopleRate × getTotalGreatPeopleRateModifier / 100`; disorder→0. base = `max(0, m_iBaseGreatPeopleRate) + player.getNationalGreatPeopleRate`; modifier = `100 + city + player + (stateReligion) + (goldenAge)`, `max(0,·)`. Threshold = `player.greatPeopleThresholdNonMilitary` = `GREAT_PEOPLE_THRESHOLD ×era.greatPeoplePercent ×(GPThresholdMod) ×gamespeed /10000`, `max(1)`. Per-source: building `getGreatPeopleRateChange`/`getGreatPeopleRateModifier`(+global), specialist `getGreatPeopleRateChange`.
- **tradeRoutes** — count `getTradeRoutes` (`CvCity.cpp:15347`) = `game + player + (coastal) + extra`, clamped `[0, getMaxTradeRoutes]` (`MAX_TRADE_ROUTES + player adj`). Per-route profit `calculateTradeProfitTimes100` = `getBaseTradeProfit × totalTradeModifier / 100` (x100, base floored at 100); yield `calculateTradeYield` = `profit × player.getTradeYieldModifier(yield) / 100`. `totalTradeModifier` = `100 + route + pop + team + capital + overseas + foreign + peace + sharedCivics`.

## 10. Wave-3 channel maps — plot-substrate, scalers, building-level, crossovers

### 10.1 PLOT-SUBSTRATE (feeds the captured city-yield `base`)
- **Plot yield** (`CvPlot::getYield`/`calculateYield` ~8148/8320): `calculateNatureYield + extraYield + cityChange + popChange + terrainYieldChange + seaPlotYield + workingCityYieldChange + landmark + extra/lessYieldThreshold + goldenAge + improvementYieldChange + routeYieldChange`, `max(0,·)`; city plots `max(getMinCity,·)`. `calculateNatureYield` = `getBaseYield (terrain + feature + river + hills/peak) + bonus.getYieldChange`. **The city SUMS worked-plot yields → `m_aiBaseYieldRate` → `getBaseYieldRate`** — i.e. the §1 yield channel's `base` decomposes HERE.
- **movementCost** (`CvPlot::movementCost` ~4487): route-path (min of route costs) OR `terrain + feature + hills + riverCrossing + peak − extraMoveDiscount`, ×MOVE_DENOMINATOR, doubleMove ÷2/÷4, `max(90,·)`/min.
- **cultureDistance** (`CvCity::cultureDistance` ~6165): euclidean OR (REALISTIC_SPREAD) per-plot terrain/feature/route/bonus/hills culture-distance + shortest-neighbor path → **SPATIAL** (#429-adjacent).
- **buildTime** (`CvPlot::getBuildTime` ~3599): `build.getTime + featureTime`, ×peak%, − existing-route time, ×`terrain.buildModifier` ×gamespeed.hammerCostPercent ×era.buildPercent.
- **vision**: `seeFromLevel` = `improvement.seeFrom + (!water ? 1 + elevation : extraWaterSeeFrom)`; `seeThroughLevel` = `(!water ? 1+elev : 0) + feature.seeThroughChange`.

### 10.2 COST / DURATION SCALERS — all route through `getModifiedIntValue` (the §7 hub)
- **production cost** `getProductionNeeded(unit/building/project)` (`CvPlayer.cpp` ~7008+): `base×100 ×gamespeed.hammerCostPercent ×era.{train|construct|create}Percent ×global *_PRODUCTION_PERCENT`, ×`getBuildingCostModifier` (combiner), ×AI-handicap (perEra ramp + world/standard %), ÷100, ×AI-option discount; `max(1,·)`. Unit adds the `iInstanceCostModifier` count-ramp.
- **research cost** `CvTeam::getResearchCost` (~2581): `base×100 ×TECH_COST_MODIFIER ×gamespeed ×era.researchPercent ×(teamMember) + cuttingEdge + AI-handicap + upscaled`, ÷100. (Per-player `calculateResearchModifier` = diffusion/welfare, ≤+100% — a RATE modifier, not the cost.)
- **durations**: `getCivicAnarchyLength` (~8937) = `Σ civic.anarchyLength×100 − qtyDiscount ×gamespeed + worldSize ×anarchyMod ×civicAnarchyMod ×era.anarchyPercent`, ÷2 rebel, clamp `[minAnarchy, max]`. `getGoldenAgeLength` = `getModifiedIntValue(game.goldenAgeLength100, goldenAgeMod)/100`. religionAnarchy similar.
- **missionYieldMultiplier**: `adaptValueToGame(ADAPT_UNIT_YIELD)` = `value × gamespeed.getUnitYieldScalePercent / 100`. **hammerCostPercent** = `gamespeed.speedPercent ×(UPSCALED if option)`.
- GameSpeed has 3 distinct scalers by use: `hammerCostPercent` (production), `speedPercent` (research/anarchy), `unitYieldScalePercent` (mission yields).

### 10.3 BUILDING-LEVEL CITY families
- **cityCapture**: National (`player.getExtraNationalCaptureProbability/ResistanceModifier`) + Local (`city.getExtraLocalCaptureProbability/ResistanceModifier`), %; capturing a CITY (≠ unit `capture`).
- **pillageGold**: building `m_iPillageGoldModifier` is **ORPHANED — stored but NEVER aggregated** (no city/player path; the live one is unit-side `getPillageChange`). **Dead/unwired field → §8 dead-data candidate.**
- **occupationTime**: `occupationTimer = (BASE_OCCUPATION_TURNS + √pop) ×gamespeed ×cultureDamp`, then `getModifiedIntValue(·, occupationTimeModifier)` at capture (building source).
- **espionageDefense**: `city.getEspionageDefenseModifier` = `m_i + player.getNationalEspionageDefense` (building source).
- **populationGrowthRate**: ⚠ `getPopulationgrowthratepercentage` = `exp(m_fPopulationgrowthratepercentageLog)×100 − 100` — **FLOAT LOG-SPACE, multiplicative (OOS-relevant!)**; building source via log accumulation. Feeds the §9.3 growth threshold.
- **healing**: `getHealRate` (`m_iHealRate`, building `getHealRateChange`) + `getHealUnitCombatTypeTotal(UC)` (building `HealUnitCombatType` array).

### 10.4 CROSSOVERS / spawnRate / stateReligion
- **byOccupant**: **military happiness** (`getMilitaryHappiness` = `militaryHappinessUnits × player.getHappyPerMilitaryUnit`) AND **celebrity happiness**. **⚠ CORRECTION 2026-06-19 (5-minion dump audit): `celebrity happiness` DOES EXIST** — `CvCity::getCelebrityHappiness()` (`CvCity.cpp:5599`) sums `getCelebrityHappy()` over `plot()->units()` and is added into `happyLevel()` at line 5715. It is UNIT-derived (hence absent from the per-building decomposition, which is why it was mistaken for nonexistent). It must be DUMPED, not dropped — see §12.
- **byCargo**: **does NOT exist as an economic family** — cargo is transport (space/type) only. Drop from the inventory.
- **spawnRate**: **event-driven per-plot RNG** (`CvGame` ~6375) — civ `spawnRateModifier`/`npcPeaceModifier` + `CvSpawnInfo.turnRate` set a per-plot probability. **Not a per-turn value-channel** (a stochastic event, like a `grants.repeatable` chance).
- **stateReligion**: deterministic CONDITIONAL (gate `getStateReligion()!=NO_RELIGION ∧ isHasReligion`), **6 sub-modifiers routed through their host families**: unitProduction, buildingProduction, buildingCommerce, happiness, greatPeopleRate (holy-city), and **`HolyCityXPModifier` → feeds `getUnitCombatFreeExperience`** (a free-XP source beyond buildings/civics/traits). Not its own channel — a predicate gate on existing families.

## 11. EMPIRE / player-scope + unit-plane (wave-4)

### 11.1 Player economy net-rates (playerInput) — reproducible
- **gold/turn** `getGoldPerTurn` / `calculateGoldRate` (`CvPlayer.cpp` ~8224) = `getCommerceRate(GOLD) + getGoldPerTurn(deals) − getFinalExpense`. `getFinalExpense` = `isAnarchy()?0 : calculatePreInflatedCosts() × getInflationMod10000()/10000`; `preInflatedCosts` = `treasuryUpkeep + getTotalMaintenance + getCivicUpkeep + getFinalUnitUpkeep + calculateUnitSupply + getCorporateMaintenance`. **Reproducible from components.**
- **science/turn** `calculateResearchRate`/`calculateBaseNetResearch` (~8203) = `getModifiedIntValue(BASE_RESEARCH_RATE + getCommerceRate(RESEARCH), getNationalTechResearchModifier + calculateResearchModifier)`; the diffusion/welfare `calculateResearchModifier` (≤100) rides in as a dumped value. **Reproducible.**
- **culture/espionage per-turn** = the player commerce sums (`getCommerceRate(CULTURE/ESPIONAGE)`; espionage is team-pooled). **Roll-ups:** `getGold` (treasury), `getTotalMaintenance`.

### 11.2 Score / power / demographics
- **SCORE — OUT OF EMULATOR/CASCADE SCOPE (owner 2026-06-19: "not gameplay-affecting").** It's a display/demographic, so we DON'T reproduce it. (Technically Python-authoritative — `CvPlayer.cpp:4416` → `Assets/Python/CvGameUtils.calculateScore`, `Σ FACTOR×(component+free)/(free+max)` over pop/land/tech/wonders; components are C++ getters, combination is Python — but unlike the revolution index it doesn't matter, so it's skipped, not a dragon.)
- **power** (`getPower` ~11470) = `(m_iPower + m_iTechPower + m_iUnitPower)/100`; base = totalPopulation, unitPower = `Σ unit.getPowerValueTotal`. **Reproducible.** **assets** = `(10×(totalPopulation + totalLandScored) + Σ unit.assetValue)/100`.
- **demographics (readings):** `getTotalPopulation`, `getRealPopulation`, `getTotalLand`, `getTotalLandScored`, `getNumMilitaryUnits`.

### 11.3 Durations + war-weariness (live readings)
- **golden-age / anarchy:** decrementing timers (`getGoldenAgeTurns`/`getAnarchyTurns`, −1/turn; mutually exclusive) + the length formulas (§10.2). Live readings.
- **war-weariness:** ⚠ **STATEFUL + EVENT-DRIVEN** — the team `m_aiWarWearinessTimes100` accumulates per combat event (culture-scaled: kills/captures/nukes) and decays per turn (`WW_DECAY_RATE`/turn, ×0.99 on peace/enemy-weak). Player `getWarWearinessPercentAnger` is derived (feeds city happiness, already a §3 input). **A live reading**, not an offline reproduction (the accumulation needs the combat history) — like property/revolution.

### 11.4 Unit-plane (unitInput)
- **baseCombatStr** (`baseCombatStrPreCheck` `CvUnit.cpp:11341`) = `(m_iBaseCombat + getExtraStrength()) × (100 + getExtraStrengthModifier())/100` — **OFFLINE-REPRODUCIBLE.** **maxCombatStr/currCombatStr are CONTEXT-DEPENDENT** (~730-line situational calc, needs attacker/plot) → NOT offline; dump base-str + HP + the aggregate stat set instead.
- **Dump:** `m_iBaseCombat` + the full aggregate `getExtra*` set (combatPercent/city/hills/withdrawal/firstStrikes/collateral/bombard/air/heal/moves/visibility/workRate/capture + the terrain/feature/unitCombat/domain keyed maps) + HP. **Per-source = aggregate-only** → iterate `getPromotionKeyedInfo`/`getUnitCombatKeyedInfo` and sum from the Infos.
- **unit-build start-XP** (`CvCity::getProductionExperience` ~3187, per unit-TYPE) = `getFreeExperience(city)+player + (canAcquireExp ? specialistFreeExp + unitCombatFreeExp(combat+sub)(city+player) + domainFreeExp)` — **REPRODUCIBLE.** **buildRate** = `getProductionModifier(item)` — **REPRODUCIBLE.** Both are per-(city × unitType) `CvCity` methods → the city/query side, not the per-unit-instance dump.

---

**✅ THE CALC MAP IS COMPLETE (waves 1-4, 2026-06-19).** Every per-turn value calc — city, empire/player,
and unit-plane — is mapped to its realized getter + per-source + gotchas; the dedup map (§7) + the family
inventory (§8) round it out. **The CALC map being complete is NOT the same as the DUMP being complete** —
§12 is the audited gap list.

---

## 12. DUMP COVERAGE — the audited truth (VERIFIED 2026-06-19, 5-minion `CvHttpServer.cpp` vs legacy getters)

The calc map above says what legacy COMPUTES; this section is the audited truth of what the DIAGNOSTIC DUMP
actually EMITS (so StoneBase has the data). **Where a per-§ "Dump:" line conflicts with this
section, THIS section is ground truth.** Endpoints that exist: **`cityInput`** (city-scope), **`playerInput`**
(player-scope). **No `unitInput` endpoint exists.**

**✅ COMPLETE-for-reproduction (every formula input emitted):**
- City **yields** (§1) — incl. the full 7-way modifier breakdown `modBonus/modBuilding/modPlayer/modEvent/modPower/modArea/modCapital` (the §1 "Dump:" line UNDERSTATES this).
- **Commerce split** (§2) — slider/baseExtra100/totalModifier/prodToCommerce/realized + city-level consts. *(The §2 ⚠ "verify visibility" warning is STALE — all three getters are public, called directly.)*
- **Health** (§3) — all good/bad signed sources (improvement/specialist emitted raw ×100, emulator /100). *(building-health city/area/player scope SPLIT is aggregate-only — attribution gap, not reproduction.)*
- **Defense** (§4), city **maintenance** (§5 city half), **growth/foodKept** (§9.3), **property current values** (§9.1 `properties[]`), **greatPeopleRate** + **tradeRoutes** (§9.5), and the entire **playerInput** suite: gold/turn, science/turn, **power/techPower/unitPower/assets**, demographics (§11.1).

**❌ REAL GAPS — singular values legacy uses that the dump does NOT emit (the actionable add-list):**
- **HAPPINESS (`unhappyLevel`/`happyLevel`) — NOT fully reproducible:** missing anger% sources `Σ getCivicPercentAnger` (per-civic, the big one), `getDefyResolutionPercentAnger`, `getRevRequestPercentAnger`; unhappy-side `getEspionageHappinessCounter`, `getEventAnger`, `calculateTaxRateUnhappiness`, foreign-unhappy (`getForeignUnhappyPercent`+culture%), landmark anger/happiness (MAP_PERSONALIZED), cityOverLimit unhappy; happy-side `getCelebrityHappiness`, `getVassalUnhappiness`; and the zero-out gate flags `isNoUnhappiness`/`isNoUnhealthyPopulation`/`isBuildingOnlyHealthy`/player `isNoCapitalUnhappiness` (`state` emits only isPowered/isCapital/isGoldenAge).
- **UPKEEP (civic + unit, §5):** only realized totals emitted (`civicUpkeep`/`finalUnitUpkeep`, in cityInput) — NO decomposition (`getUpkeepModifier`, handicap %, AI mults, civilian/military net, per-unit `calcUpkeep100`). The §5 "(+ the handicap/AI/era mults as inputs)" claim is FALSE.
- **INFLATION (§9.4):** realized `inflationMod10000` + `preInflatedCosts` total only — NO `getHurriedCount`, handicap inflation %, `m_iInflationModifier`, the 4 source getters (`getCivicInflation`/`getProjectInflation`/`getTechInflation`/`getBuildingInflation`), rebel, AI. The §9.4 "Per-source: …" claim overstates.
- **HURRY (§9.4):** only the realized `hurryAnger` is emitted; the hurry **gold and population COSTS** (`getHurryGold`/`getHurryPopulation`/`hurryCost`) are NOT dumped at all.
- **buildRate (§9.5):** only the UNIT overload (and only behind `?type=UNIT_*`); `getProductionModifier(building)`/`(project)` not dumped.
- **UNIT-PLANE (§6/§11.4): ENTIRELY ABSENT** — no `unitInput` endpoint; `/units` emits roster fields only (no combat str, maxHP, the ~70 `getExtra*` set, nor the promotion/unitcombat lists). §6/§11.4's "Dump:" lines are an UNBUILT SPEC, not live output.
- Minor: commerce `baseExtra100`/`totalModifier` are aggregate-only (sub-source attribution), `foodWastage` float, `isDisorder` in the maintenance block, building-health scope split.

*(This section is the audited replacement for the per-§ "Dump:" optimism above. The add-list is the work
to reach "all singular values mapped".)*

---

**Build order off this map:** cityInput already does yields + commerce; add health/happiness → defense →
maintenance (city channels, one dump); add `unitInput` (aggregate stats + promotion/unitcombat lists);
then the in-flight + unmapped families as they land. Each channel gets a modcalc fidelity guard. One
compile, one live verify across all.

## See also
- [fixed-point & the scale registry](../../specs/curators/fixed-point-and-scales.md) — the single source of truth for every
  ×100 / per-100 / human scale this map names; consult it before de-scaling any field cited here.
- [decisions ledger](../../architecture/decisions.md) — the cross-cutting rulings this map operates under:
  [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete) (why this map must be
  complete before a legacy calc is cut), [DEC-no-guessing](../../architecture/decisions.md#dec-no-guessing)
  (a divergence is attributed to a named source via the dump, never hypothesized),
  [DEC-parity](../../architecture/decisions.md#dec-parity) (exact parity is the bar — a divergence is a
  data-collection gap, never a formula difference).
- [enabler](../../specs/enabler.md) / [modifier](../../specs/modifier.md) — the cascade design these accumulators replace;
  this map is the legacy side those summed accumulators are shadowed against.
- [observability surface](../../reference/observability.md) — the `/diagnostic/cityInput` / `playerInput`
  endpoints this dump spec extends, and how the shadow guards read them.
- [comprehension map](../../README.md) — the docs2 overview-of-overviews that orients this reference.
