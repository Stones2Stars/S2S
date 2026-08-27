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

- ⛔ **THE FOOD STORE EMITS NO SPINE FACT, AND THAT IS CORRECT:** *"changing food in and of itself does
  not actually alter any other state anywhere"* — it is an inert accumulator. The fact belongs where the store
  CROSSES into state something else depends on, which is the population step, and `setPopulationInternal` already
  emits `SEVT_CITY_POPULATION_ADDED`/`_REMOVED` carrying the delta as a magnitude. ⚑ So a food mutator reaching
  `m_iFood` without an emit is **not** a missing-fact defect and must not be "fixed" by adding one: a
  `SEVT_CITY_FOOD_*` would fire on every tick of an accumulator no consumer can act on, and it would double-report
  the only transition that matters. ⚠ The shape misleads because `m_iFood` is written in ~11 scattered places with
  no `setFoodInternal` choke point, so it reads like population's commit-point pattern with the emit forgotten.
  It is not — population needs one because a pop change moves dependent state; food does not.

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
  as a live rate (no accrual); a NON-converting process is idle and banks overflow instead (§ The order queue).
- **Overflow cap** = `getYieldRate(PRODUCTION) × CityScreen__ProductionOverflowLimit` (default **2** — 2× base/turn);
  beyond cap → gold at `MAXED_{UNIT,BUILDING,PROJECT}_GOLD_PERCENT`. Feature production (chop hammers) banks
  alongside; both cleared each turn.
### The order queue — a PROCESS may only ever stand ALONE

The queue holds `ORDER_TRAIN` / `ORDER_CONSTRUCT` / `ORDER_CREATE` / `ORDER_MAINTAIN` (a process) / `ORDER_LIST`.
The AI **queues rather than re-deciding per completion**
([ai-build-queue-parity.md](../plans/parked/ai-build-queue-parity.md)): `AI_chooseBuilding` appends construct
orders up to `AI_BUILDING_SHORTLIST_DEPTH`, and `doProduction` re-enters `AI_chooseProduction` when the queue
empties. **A process is the exception at every point below, because it is the one order that NEVER COMPLETES.**

- ⛔ **A PROCESS MAY ONLY EVER STAND ALONE IN AN AI QUEUE: *"idle should not be possible to add unless
  there is literally nothing else."*** Enforced at both ends of `pushOrder` — the `ORDER_MAINTAIN` case REFUSES
  the push when an AI/automated queue already holds any non-process order (`[CIT/push/reject]
  reason=queueNotEmpty`), and the tail purge pops EVERY queued process wherever it sits the moment a real order
  joins. Humans keep the free hand and manage their own queue.
  ⚑ **The failure it closes is the SANDWICHED process:** one sitting BETWEEN real orders is never reached at the
  head, and because the re-decide fires on the queue EMPTYING, nothing ever strips it — the city locks on it
  permanently, for the rest of the game.
  ⚠ **Whether the push LANDED is read off the QUEUE LENGTH, never assumed from having asked** —
  `AI_chooseProcess` returns the queue delta, because a decision-cascade rung that returns on a bare `true` ends
  the entire decision having set nothing.
- ⛔ **A PROCESS HAS A FAKE ONE-TURN COMPLETION TIME AT ALL TIMES.** `getOrderProductionTurnsLeft`
  returns **1** for `ORDER_MAINTAIN`, and `getTotalProductionQueueTurnsLeft` counts it as one turn instead of
  reading `getProductionNeeded` (`MAX_INT` for a process). ⚑ The reason is that both feed SUMS shared with real
  orders: the queue total bails to **999** for any order needing >999 hammers, so a single queued process made
  the contract broker read `turns=1000` for a unit costing six hammers, collapsing that city's bid.
- ⛔ **A PROCESS NEVER EATS THE COMPLETION OVERFLOW (owner: *"idle eats the remaining production, and then the
  cycle repeats"*).** `doProduction`'s completion loop breaks before `changeProduction(getOverflowProduction())`
  when the new head is a process. ⚑ `changeProduction` routes the hammers through the process's
  `productionToCommerce` rates — and `PROCESS_IDLE` declares NONE — so the overflow was converted at zero and
  then cleared: destroyed outright. Breaking leaves it BANKED, and a process head returns from `doProduction`
  before the per-turn overflow clear, so it survives to the next real order.
- ⛔ **AN IDLE ORDER BEHAVES AS NO ORDER.** A process whose `conversion` block is empty converts
  nothing (`CvProcessInfo::convertsProduction()` is false), so `doProduction` **banks the city's hammers as
  overflow** exactly as the no-order path does, rather than discarding them. ⚑ **The rule is about IDLE
  itself, not about who selected it** — a human parking a city and an AI falling back to it get the same
  answer, and the production survives to whatever the city can finally build. ⚠ A **converting** process
  accrues nothing here by design: its conversion is a live rate read off the city, never an accrual.
  ⚑ **Why this is load-bearing rather than tidiness:** `PROCESS_IDLE` is the only process
  `TECH_GAME_START` carries, and every real one is unlocked by a tech (`PROCESS_WEALTH_MEAGER` ←
  `TECH_COOPERATION`, `PROCESS_RESEARCH_MEAGER` ← `TECH_ORAL_TRADITION`, both ← `TECH_LANGUAGE`) — the domain
  is built purely from `enables` edges, so a low-tech player's ONLY reachable process is idle. Discarding
  production there is a trap with no exit: no hammers, so no building, so no economy, so no tech.
- ⚖ **A QUEUED PROCESS IS RE-DECIDED EVERY TURN: *"if PROCESS is being run, recalc has to happen for
  that city every turn as long as process is run."*** `doProduction` re-enters `AI_chooseProduction` when a
  process sits ANYWHERE in the queue, and `AI_chooseProduction` strips every `ORDER_MAINTAIN` before deciding
  (`[CIT/dropProcess]`) — **a process is a fallback, never a commitment.** The end-of-cascade fallback re-adds
  one only when there is still nothing better this turn. ⚠ The strip pops with `bChoose = false`, so it never
  re-enters the chooser — derived data must never drive a loop
  ([ai-architecture-north-star.md](../plans/parked/ai-architecture-north-star.md) §2.4).

- **Hurry:** Buy (gold, `getGoldPerProduction>0`) or Whip (pop + `m_iHurryAngerTimer`, `getProductionPerPopulation>0`);
  `maxHurryPopulation = pop/2`. **Decay** (`doDecay`, human cities only): non-head queued items bleed
  `BUILDING_PRODUCTION_DECAY_PERCENT`% per `BUILDING_PRODUCTION_DECAY_TIME` turns (speed-scaled).

## Golden ages & era

- **Golden age** — full reference: **[golden-age.md](golden-age.md)**. Yield-relevant summary: it adds yield in
  **three** places, all in `base` (so all `× modifier`): the **per-plot** threshold bonus (`calculateYield` — tested on
  the **PRE-improvement/route** running yield), the **player** golden-age yield, and golden-age **commerce**. Plus
  anarchy → 0, `+GOLDEN_AGE_GREAT_PEOPLE_MODIFIER` GP rate. Growth is UNAFFECTED by golden age (no food discount —
  golden-age.md §2). `m_iGoldenAgeTurns` (−1/turn),
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
  the property solver. [../specs/modifier.md](../cascade.md) — the yield modifier families that replace the stacks above.
