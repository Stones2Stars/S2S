# Yields & growth reference — civics, food, plots, city production, golden ages & era

> Lifted + condensed mechanics (the formulas the validator re-derives). The old docs' "what's dark on the wire /
> proposed hooks" sections are deliberately **not** here — that's a build tracker, not mechanics. Behaviour as-is;
> the cascade replaces these (verified live in-game).

## Civics

- State: `CvPlayer::m_paeCivics[]` (one `CivicTypes` per `CivicOptionTypes`); NPCs hold civics but `processCivics`
  is a no-op for them. `canDoCivics` = tech prereq met OR option already unlocked, AND city count within limit.
- **Anarchy length:** `Σ(anarchyLength·100 per changed civic)` → qty discount `−= total·N·CIVIC_ANARCHY_QTY_DISCOUNT/100`
  → ×gameSpeed → `+ numCities·worldNumCitiesAnarchyPercent` → ×anarchyModifier ×civicAnarchyModifier → era factor →
  rebel `/=2` → `/=100` → clamp `[min,max]`. Golden age → 0. `isPolicy()` civics are **zero-cost** (excluded).
- `verifyCivics` (each doTurn) **silently** switches any ineligible civic to the first eligible one in the slot —
  no log, no event. AI re-evaluates on a 25-turn throttle (`CIVIC_CHANGE_DELAY`).

## Food & growth (city)

- Tick `changeFood(foodDifference(), true)` at `doTurn`. **Gross food** = `min(CITY_MAX_YIELD_RATE, max(100,
  (baseYieldRate + specialistYield)·baseYieldRateModifier + 100·extraYield))`.
- **Consumption** = `getFoodConsumedByPopulation − healthRate − angryPop + foodWastage`, using
  `getPopulationPlusProgress100 = 100·pop + 100·food/growthThreshold` (fractional, scales as the food bar fills).
  `FOOD_CONSUMPTION_PER_POPULATION = 4`. `foodDifference()` carries NO wastage; `isFoodProduction()` caps surplus
  (→ hammers); `isDisorder()` → 0.
- **Wastage** (when `WASTAGE_START_CONSUMPTION_PERCENT(50) ≥ 0` and surplus > consumption·50/100): memoised
  `waste[N] = waste[N-1] + 1 − (0.05 + 0.95/(1+0.05·N))` — ~logarithmic, truncated to int (a static memo cache,
  safe only because the game thread is serial).
- **Granary:** +`max(1, delta·foodKeptPct/100)` on a positive delta, −`min(-1, delta/2)` on negative. **Growth:**
  subtract threshold, pull from granary if `food < foodKept`, then `+1 pop`. `growthThreshold = getModifiedIntValue(
  player.threshold(pop), cityGrowthRatePercent + playerGrowthRatePercent)`, halved for barbarian.

## Improvements & plot yields

- Per-plot per-turn order: ownership → bonus discover/deplete → improvement **upgrade** (only if worked OR
  fortify-upgrade) → feature growth/disappear → culture diffusion.
- **Upgrade:** +`getImprovementUpgradeProgressRate` (base 100 + civic/trait/tech) each qualifying turn; threshold
  `100·getImprovementUpgradeTime` (XML time ×speed ×era). AI picks the best target **silently** (`AI_getImprovementValue`).
- **Yield** `calculateImprovementYieldChange` is a **7+-term additive stack:** XML base + river + irrigation (if
  available) + route + tech + civic + player (`getImprovementYieldChange` = trait + civic + building) + team +
  bonus-resource bonus. Floored (cannot drive total plot yield negative). `m_aiYield[eYield]` is the live cached
  value, refreshed by `updateYield` on any input change.

## City production

- `doTurn` order: `doCheckProduction` → food → culture → `doAutobuild` → `doProduction` (growth precedes hammers).
- **Projects build exactly like units/buildings/wonders** — one city's queue, hammers in, order completes (the
  effect lands team-wide). ⚠ The engine *looks* like it supports multiple cities working on the same project
  (per-city `m_paiProjectProduction`), but that multi-city production feed **does not actually work** — a
  project is effectively single-city built. Changing that is a post-migration redesign, not a #430 item.
- **Hammers/turn** = `max(1, extraYield + overflow(if flag) + foodSurplus(if FoodProduction) + (baseYieldRate +
  specialistYield)·baseYieldRateModifier/100)`. `isDisorder()` → 0. Process-mode converts to gold/science/culture
  (no overflow).
- **Overflow cap** = `getYieldRate(PRODUCTION) × CityScreen__ProductionOverflowLimit` (default **2** — 2× base/turn);
  beyond cap → gold at `MAXED_{UNIT,BUILDING,PROJECT}_GOLD_PERCENT`. Feature production (chop hammers) banks
  alongside; both cleared each turn.
- **Hurry:** Buy (gold, `getGoldPerProduction>0`) or Whip (pop + `m_iHurryAngerTimer`, `getProductionPerPopulation>0`);
  `maxHurryPopulation = pop/2`. **Decay** (`doDecay`, human cities only): non-head queued items bleed
  `BUILDING_PRODUCTION_DECAY_PERCENT`% per `BUILDING_PRODUCTION_DECAY_TIME` turns (speed-scaled).

## Golden ages & era

- **Golden age** — full reference: **[golden-age.md](golden-age.md)**. Yield-relevant summary: it adds yield in
  **three** places, all in `base` (so all `× modifier`): the **per-plot** threshold bonus (`calculateYield` — tested on
  the **PRE-improvement/route** running yield), the **player** golden-age yield, and golden-age **commerce**. Plus
  anarchy → 0, `+GOLDEN_AGE_GREAT_PEOPLE_MODIFIER` GP rate, less food for growth. `m_iGoldenAgeTurns` (−1/turn),
  length `max(1, GOLDEN_AGE_LENGTH·speedPercent·(1+goldenAgeModifier/100)/100)`.
- **Era:** `m_eCurrentEra` advances **only** in `CvTeam::setHasTech` when `player era < tech.getEra()` (only
  increases) — so era advance is TECH-DRIVEN, and `setCurrentEra` is where the fact is announced
  (`emitEraChanged`), never a place a consumer's effect is inlined. Its own side-effects are heritage commerce
  deltas and graphics. As a cascade input it gates `requires` atoms and scales anarchy/growth/event-prob + the
  AI per-era handicap bonus.
  ⚑ **An era-advance HANDOUT is a trigger-plane consumer of that fact, not a side-effect of the setter** — the
  trait's era-advance free specialist authors `onEraChanged` → `action.grant.specialists`
  ([json.md §5](../specs/json.md)) and lands in the city's UNATTRIBUTED typed-free ledger, so it outlives the
  trait ([superseded-ideas #10](../architecture/superseded-ideas.md): the discriminator is whether removing the
  source removes the specialist).

## See also

- [economy.md](economy.md) — maintenance/upkeep/happiness feed off these. [engine.md](engine.md) — gamespeed/era +
  the property solver. [../specs/modifier.md](../specs/modifier.md) — the yield modifier families that replace the stacks above.
