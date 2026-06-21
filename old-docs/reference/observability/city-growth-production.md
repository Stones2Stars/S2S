> DRAFT observability map (2026-06-18 by parent) — claims cited from code; verify before relying.

# Observability map: City growth, production, overflow & hurry

**System:** Per-city food / growth / starvation, hammer accrual, production overflow, overflow-to-gold conversion, hurry (buy + whip), hurry-anger, and production decay.

**Anchor entry points (live code):**
- `CvCity::doTurn` — `Sources/CvCity.cpp:1229` (calls `changeFood`, `doProduction`)
- `CvCity::changeFood` — `Sources/CvCity.cpp:9713` (food box delta → growth / starvation)
- `CvCity::doProduction` — `Sources/CvCity.cpp:16519` (hammer accrual, overflow, completion)
- `CvCity::hurry` — `Sources/CvCity.cpp:4079` (hurry-buy / whip)
- `CvCity::foodDifference` — `Sources/CvCity.cpp:5970`
- `CvCity::growthThreshold` — `Sources/CvCity.cpp:5993`
- `CvCity::getMaxProductionOverflow` — `Sources/CvCity.cpp:9798`
- `CvCity::popOrder` — `Sources/CvCity.cpp:~15750` (order completion, overflow write)

**Companion reference:** `docs/dev/reference/doProduction.md` (detailed walkthrough of `doProduction`).

---

## 1. How it actually works

### 1a. Per-turn doTurn sequence (food before production)

`CvCity::doTurn` (`CvCity.cpp:1229`) calls sub-steps in this order (relevant extract):

```
doCheckProduction()          // CvCity.cpp:1327 — validates queue, returns bAllowNoProduction
changeFood(foodDifference()) // CvCity.cpp:1329 — applies net food, fires growth/starvation
doCulture / doPlotCulture    // CvCity.cpp:1331-1335
CvPlot::setDeferredPlotGroupRecalculationMode(false) // CvCity.cpp:1338
doAutobuild()                // CvCity.cpp:1341 — adds free buildings first
doProduction(bAllowNoProduction) // CvCity.cpp:1343
```

Growth PRECEDES production; a growth event (pop change, food-box reset) is in effect before hammer
accrual this turn.

### 1b. Food / growth mechanics (`changeFood` + `foodDifference`)

**Net food delta** — `foodDifference()` (`CvCity.cpp:5970`):
```
foodDifference = getYieldRate(YIELD_FOOD) - foodConsumption(...)
```
Where `foodConsumption` (`CvCity.cpp:5912`) = `getFoodConsumedByPopulation(iExtra)` - `healthRate` + optional `foodWastage`.

`getFoodConsumedByPopulation` uses a **gradual-consumption** model (`CvCity.cpp:5894`): it interpolates
consumption as the food box fills, via `getPopulationPlusProgress100 × getFoodConsumedPerPopulation100 / 10000`
so consumption rises smoothly from pop N toward pop N+1 (Toffer 2019). This means even a city at the
same integer population consumes slightly more food the closer it is to growing — this is invisible to
the snapshot.

**`foodWastage`** (`CvCity.cpp:5923`): a float S-curve waste function keyed on `surplass` (food surplus
above `startWasteAtConsumptionPercent`×consumption). The default growth factor is 0.05 if
`GC.getWASTAGE_GROWTH_FACTOR()` is zero. This waste is NOT applied unless `bIncludeWastage=true`; the
default `foodDifference()` call at `CvCity.cpp:1329` passes the default (no wastage parameter = the
non-waste path). The wastage path IS exercised in `foodConsumption` when called as `foodConsumption(false, 0, true)`, but is not the main tick path. **This system is in §A of cascade-mapping-inventory.md as "food wastage — unmapped".**

**`changeFood(iChange, bHandleGrowth=true)`** (`CvCity.cpp:9713`):
1. `m_iFood += iChange` — adds net food to the food box.
2. Also calls `changeFoodKept` to update the sub-ledger used after growth (food kept on growth = `getFoodKeptPercent()` / 100 of the threshold, clamped 0-99). (`CvCity.cpp:9723`)
3. **Starvation:** if `m_iFood < 0` and `pop > 1`, calls `changePopulation(-1)` and adds `growthThreshold()` back to food; repeats while food < 0 (`CvCity.cpp:9732-9739`). Pop-1 cities floor food at 0 (starvation stop).
4. **Growth:** if `m_iFood >= growthThreshold()`:
   - If `AI_avoidGrowth` (human) or `AI_isEmphasizeAvoidGrowth` (AI): food is capped at the threshold, no growth (`CvCity.cpp:9747-9749`).
   - Otherwise: `m_iFood -= threshold`, calls `changePopulation(1)`, recalculates threshold, repeats while food still ≥ threshold. (`CvCity.cpp:9753-9765`)
5. FoodKept is a sub-ledger preventing the food box from dropping below `FoodKeptPercent`% of the threshold on the next shrink. It is managed separately in `changeFoodKept` (`CvCity.cpp:9778`).

**`growthThreshold(iPopChange=0)`** (`CvCity.cpp:5993`):
```
threshold = getModifiedIntValue(
    GET_PLAYER(owner).getGrowthThreshold(getPopulation() + iPopChange),
    getPopulationgrowthratepercentage() + player.getPopulationgrowthratepercentage()
)
```
Hominid cities halve the threshold (`CvCity.cpp:6001`). The per-city `m_iPopulationgrowthratepercentage`
is modified by buildings (`changeFoodKept` etc.) and is NOT directly visible externally today.

**Disorder:** `foodDifference()` returns 0 when `isDisorder()` unless `bIgnoreFoodBuildOrRev=true`
(`CvCity.cpp:5972`). A city in civil disorder neither grows nor starves — this is invisible from the
endpoint.

**FoodProduction:** if `isFoodProduction()` (Settler/Worker being built), `foodDifference` uses
`min(0, yield - consumption)` — the city consumes food but surplus never contributes to growth
(`CvCity.cpp:5978-5980`). In `doProduction`, the surplus is instead routed to hammers.

### 1c. Production accrual (`doProduction`)

Full walkthrough in `docs/dev/reference/doProduction.md`. Summary of the state touched:

**Hammers per turn** — `getCurrentProductionDifference(FoodProduction | Overflow)` (`CvCity.cpp:3972`):
- `getProductionPerTurn` (`CvCity.cpp:3951`):
  ```
  max(1, getExtraYield(YIELD_PRODUCTION)
       + overflow (if flag set: getOverflowProduction() + getFeatureProduction())
       + foodSurplus (if FoodProduction flag and isFoodProduction())
       + getBaseYieldRate(YIELD_PRODUCTION) + getSpecialistYieldTotal(YIELD_PRODUCTION))
         × getBaseYieldRateModifier(YIELD_PRODUCTION) / 100
  ```
- The formula yields at least 1 hammer/turn for a producing city with no disorder.

**Overflow bucket** — `m_iOverflowProduction` (`CvCity.cpp:9807-9822`):
- Set to 0 each turn when production runs (`doProduction:16555`).
- Filled on order completion by `popOrder`: overflow = `min(iMaxOverflow, iProgress - iNeeded)` where
  `iMaxOverflow = getYieldRate(YIELD_PRODUCTION) × BugOptionINT("CityScreen__ProductionOverflowLimit", 2)` (`CvCity.cpp:9798`). Default cap = 2× base production per turn.
- Any raw overflow beyond `iMaxOverflow` is **converted to gold** at `GC.getMAXED_UNIT_GOLD_PERCENT()` / `GC.getMAXED_BUILDING_GOLD_PERCENT()` / `GC.getMAXED_PROJECT_GOLD_PERCENT()`. (`CvCity.cpp:15815-15816`, `15976-15977`, `16090-16091`)
- `[CIT/waste]` (`CvCity.cpp:16619`) logs gold-conversion of excess overflow (level 1).
- `[CIT/produced]` (`CvCity.cpp:15836`, `15980`, `16022`) logs `overflow=` and `lost=` fields on each completion.

**Feature production** (`m_iFeatureProduction`): added alongside overflow (cleared each turn at `doProduction:16556`). Comes from chopping forests. Not separately exposed.

**Disorder stops production:** `doProduction` returns early when `isDisorder()` (`CvCity.cpp:16547`).

**Process orders:** `doProduction` returns early after handling `m_bPopProductionProcess`
(`CvCity.cpp:16537-16544`). A process city accumulates no overflow.

**Decay of partial progress** — `doDecay` (`CvCity.cpp:16636`): human cities only; queued items NOT
currently at the head of the queue decay at `BUILDING_PRODUCTION_DECAY_PERCENT`% per
`BUILDING_PRODUCTION_DECAY_TIME` turns (game-speed scaled). Fires AFTER `doProduction`. No log tag
exists for decay.

**Completion spin guard** — `[CIT/spin]` (`CvCity.cpp:16571`, `16590`): fires at level 1 when
`iCompletionSafety > 50` (stale value loop) or when the AI failed to pick anything after completing an
order. This is an anomaly signal, not a normal-path log.

### 1d. Hurry mechanics (`canHurry`, `hurry`)

**Hurry types:** controlled by `GC.getHurryInfo(eHurry)`. Two main flavors in Civ4/C2C:
- **Buy** (`getGoldPerProduction() > 0`): spends gold to complete immediately.
- **Whip** (`getProductionPerPopulation() > 0`): sacrifices population for hammers.

**`canHurry(eHurry)`** (`CvCity.cpp:3998`): gate checks — `canHurryInternal` (player has hurry,
not in disorder, pop > hurryPopulation); production not already complete; only unit or building
orders; player has enough gold (buy path).

**`hurry(eHurry)`** (`CvCity.cpp:4079`):
1. If buy: `changeGold(-getHurryGold(eHurry))` on the player.
2. If whip: `changePopulation(-hurryPopulation(eHurry))`, `changeHurryAngerTimer(hurryAngerLength(eHurry))`.
3. Always: `changeProduction(hurryProduction(eHurry))`.
4. Fires `CvEventReporter::getInstance().cityHurry(this, eHurry)` → Python `onCityHurry`.

**Hurry anger** — `m_iHurryAngerTimer` (`CvCity.cpp:1374-1376`): decremented each `doTurn`. Length =
`flatHurryAngerLength()` = `GC.getHURRY_ANGER_DIVISOR() × gameSpeedPercent/100 × (100+hurryAngerModifier)/100`.
No log tag for hurry events.

**Overflow after hurry** (`hurryOverflow`, `CvCity.cpp:4119`): client-side calculation for the BUG UI
assist. Not applied in `hurry()` itself; `hurry()` just calls `changeProduction`, and the completion
loop in `doProduction` handles the overflow from the resulting over-completion.

**`maxHurryPopulation()`** (`CvCity.cpp:6148`): `getPopulation() / 2` — hard cap on pop consumed.

---

## 2. Current observability

**Tier: 1 — Telescreen** (coarse snapshot only; key transients invisible).

### 2a. What IS exposed today

**`/cities` endpoint** (`CvHttpServer.cpp:83-103`, rendered at `CvHttpServer.cpp:339-358`):

| JSON field | What it is | Source |
|---|---|---|
| `population` | Current city population | `getPopulation()` (`CvHttpServer.cpp:1549`) |
| `food` | YIELD_FOOD rate (gross food per turn from worked tiles + buildings) | `getYieldRate(YIELD_FOOD)` (`CvHttpServer.cpp:1550`) |
| `production` | YIELD_PRODUCTION rate (base hammers per turn) | `getYieldRate(YIELD_PRODUCTION)` (`CvHttpServer.cpp:1551`) |
| `producing` | XML key of current production head (or NONE) | `CvHttpServer.cpp:1565-1573` |
| `producingTurns` | Turns-to-complete estimate for current head (0 if idle/process) | `getProductionTurnsLeft()` (`CvHttpServer.cpp:1574`) |

**`/players` endpoint** (`CvHttpServer.cpp:298`):

| JSON field | What it is | Source |
|---|---|---|
| `population` | Total player population (sum of all city pops) | `getTotalPopulation()` (`CvHttpServer.cpp:1533`) |
| `production` | Total production rate (sum of `YIELD_PRODUCTION` across all cities) | `CvHttpServer.cpp:1580-1582` |

**`[CIT]` log tags** (via `CityAI.log` + `/events` log tee, gated by `gCityLogLevel`):

| Tag | Level | What it carries | Site |
|---|---|---|---|
| `[CIT/produced] … UNIT` | 1 | Unit completion: `overflow=` banked hammers, `lost=` hammers burned to gold | `CvCity.cpp:15836` |
| `[CIT/produced] … BUILDING` | 1 | Building completion: `overflow=`, `lost=` | `CvCity.cpp:15980` |
| `[CIT/produced] … PROJECT` | 1 | Project completion | `CvCity.cpp:16022` |
| `[CIT/waste]` | 1 | Overflow exceeds cap → `lostProd=` hammers burned, `gold=` gold gained | `CvCity.cpp:16619` |
| `[CIT/cancel]` | 1 | Order cancelled (abandoned/obsoleted): `progressLost=` | `CvCity.cpp:15770` |
| `[CIT/push]` | 2 | Order enqueued | `CvCity.cpp:15662` |
| `[CIT/push/reject]` | 2 | Anti-spam guard blocked a duplicate | `CvCity.cpp:15554`, `15587` |
| `[CIT/spin]` | 1 | Completion loop hit the 50-iteration safety cap | `CvCity.cpp:16571`, `16590` |
| `[CIT/proplevel]` | 1 | Per-city property snapshot (crime/education/disease) — NOT food/production | `CvCity.cpp:1244` |
| `[CIT/begin]` | 1 | AI production-choice context (pop, danger, finances) | `CvCityAI.cpp:966` |
| `[CIT/order]` | 1 | AI production decision (what was chosen and why) | `CvCityAI.cpp:8851`, `9120`, `9167`, `9194` |

**Python events** (via `CvEventReporter`):
- `onCityHurry` — fired on every hurry action; carries `(city, hurryType)`. No payload beyond that.
- `onCityBuildingUnit` / `onCityBuildingBuilding` / `onCityBuildingProcess` — queue push notifications.

---

### 2b. What is NOT exposed today (the gaps)

| State | Why it matters | Currently observable? |
|---|---|---|
| **Food box level** (`getFood()` / `m_iFood`) | The current fill of the food box; without it you cannot reconstruct turns-to-grow or turns-to-starve | NOT exposed in any endpoint or log |
| **Growth threshold** (`growthThreshold()`) | The target food value for the next pop growth — varies with buildings/civics/game speed | NOT exposed |
| **Net food per turn** (`foodDifference()`) | What is actually added to the box each turn (after consumption, health, wastage, disorder zero) | NOT exposed; `/cities.food` is gross yield, not net |
| **Food consumption** (`foodConsumption()`) | What the city eats (population + health + optional wastage) | NOT exposed |
| **Food wastage** (`foodWastage()`) | The S-curve waste amount — the §A "food wastage especially" mystery | NOT exposed |
| **Starvation event** | Whether a city lost population this turn due to starvation (no growth event either) | No event, no log tag, no endpoint signal |
| **Growth event** | Whether a city gained population this turn (normal growth) | No event, no log tag beyond population changing in the next snapshot |
| **Avoid-growth flag** | Whether AI/human has `avoidGrowth` set (growth suppressed) | NOT exposed |
| **Food-kept sub-ledger** (`getFoodKept()` / `m_iFoodKept`, `getFoodKeptPercent()`) | Food preserved across a growth event (building-based mechanic) | NOT exposed |
| **Overflow production bucket** (`getOverflowProduction()`) | Banked hammers from last completion, consumed next turn | NOT exposed in endpoint (only appears in `[CIT/produced]` at completion) |
| **Feature production** (`getFeatureProduction()`) | Hammers from chop events, consumed with overflow | NOT exposed |
| **Overflow cap** (`getMaxProductionOverflow()`) | The BUG-option-configurable cap (default 2× base production/turn) | NOT exposed |
| **Current production progress** (`getProductionProgress()`) | Hammers banked so far toward the current head order | NOT exposed (only `producingTurns` is exposed, which is derived) |
| **Production needed** (`getProductionNeeded()`) | Total hammers required for the current order | NOT exposed |
| **Per-item partial progress** (`m_progressOnBuilding`, `m_progressOnUnit`) | Partial hammer investment in queued items (non-head) — the decay ledger | NOT exposed |
| **Production decay** | Whether a non-head item lost hammers this turn via `doDecay` | No log tag, no event |
| **Hurry event** (hammer + pop side effects) | That `hurry()` fired, how much production was injected, how much pop was consumed | Only Python `onCityHurry`; no log tag in `[CIT]`; no endpoint field; the event fires but carries only `(city, hurryType)` — no `productionAdded`, no `popConsumed`, no `goldSpent` |
| **Hurry-anger timer** (`m_iHurryAngerTimer`) | Per-city countdown of whip-anger duration | NOT exposed |
| **Hurry-anger modifier** | Per-city modifier that scales anger duration | NOT exposed |
| **Disorder state** (`isDisorder()`) | Halts food delta + production + effectively zeroes all yields for the turn | NOT exposed |
| **isFoodProduction** | Whether the current order consumes food surplus as hammers (Settler/Worker) | NOT exposed |
| **PopulationgrowthratePercentage** | Per-city growth-rate modifier (affects threshold) | NOT exposed |
| **Process conversion rates** | What a process-mode city converts production into (gold/science/culture) | NOT directly; `producing` gives the process key but no per-turn output |

---

## 3. The gap — what cannot be reconstructed from outside today

**You cannot reconstruct turns-to-grow, turns-to-starve, or the current growth state** from the API.
The endpoint gives you `food` (gross yield rate) but not net food, not the box level, not the
threshold. A city going from pop 5 to pop 6 is visible only when the next `/cities` snapshot shows
`population=6` — you cannot see *when* it will happen or *why* (growth vs hurry vs event).

**You cannot reconstruct turns-to-complete beyond the estimate.** `producingTurns` is exposed, but it
is a rounded estimate; you cannot verify it because `getProductionProgress()` (the actual hammer
investment) and `getProductionNeeded()` (the total cost) are hidden. If a hurry fired mid-turn, the
estimate resets but there is no corresponding event visible externally except the next snapshot change.

**You cannot distinguish growth from hurry from an event gift.** All three change `population` and
`food`/production state, but only hurry fires `onCityHurry` (Python only). A city that grows
naturally from 3 to 4 looks identical to one that received a random event `+1 pop` from outside — both
just update `population` in the next snapshot.

**Food wastage is completely dark.** The S-curve waste mechanism (`foodWastage`) is in §A of
`cascade-mapping-inventory.md` as flagged-unmapped. It affects `foodConsumption` and therefore net
food, but neither the waste amount nor the surplus that triggers it is exposed.

**The overflow pipeline is opaque between completions.** `[CIT/produced]` and `[CIT/waste]` tell
you what happened *when an order completed*, but between completions the banked overflow, feature
production, and their combined effect on `producingTurns` are invisible.

**Hurry side effects are under-reported.** `onCityHurry(city, hurryType)` confirms that hurrying
happened but reports no production gain, no pop consumed, no gold spent, no anger-timer value. You
cannot reconstruct the state change from the event alone.

**Decay is entirely invisible.** Human cities losing hammers via `doDecay` leave no trace in any log
or endpoint.

---

## 4. Proposed hooks — concrete additions to climb tiers

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### 4a. New log tags (cheapest — no endpoint change required)

**`[CIT/food]` (level 1) — net food + growth state, per city per turn**
Emit from `CvCity::doTurn` immediately after `changeFood(foodDifference(), true)` (`CvCity.cpp:1329`).
One line per city per turn:
```
[CIT/food] turn=N city=NAME owner=P pop=X food=BOX threshold=THRESH netFood=DIFF
           gross=YIELD_FOOD consumed=CONSUME waste=WASTE foodProd=0|1 disorder=0|1
```
Fields: `pop` = pre-change pop (or post — specify consistently); `food` = `m_iFood` after `changeFood`;
`threshold` = `growthThreshold()`; `netFood` = `foodDifference()` (the value passed to `changeFood`);
`gross` = `getYieldRate(YIELD_FOOD)`; `consumed` = `foodConsumption(false, 0, false)` (no wastage);
`waste` = `(int)foodWastage()` (maps the §A mystery to a logged value);
`foodProd` = `isFoodProduction() ? 1 : 0`; `disorder` = `isDisorder() ? 1 : 0`.
**Tier climb:** Tier 1 → Tier 3 for food (net food + threshold visible in the event stream).

**`[CIT/grow]` (level 1) — growth / starvation events**
Emit from `CvCity::setPopulation` when `iChange != 0` (or inline in `changeFood`'s growth/starvation
blocks at `CvCity.cpp:9736` and `9763`). One line per growth or starvation event:
```
[CIT/grow] turn=N city=NAME owner=P pop=NEWPOP delta=+1|-1 cause=growth|starvation
           foodAfter=BOX threshold=THRESH
```
`cause` distinguishes `changePopulation(-1)` from food (`starvation`) vs whip, vs conscript (could use
`cause=hurry`/`cause=conscript` at those call sites). At level 1 this is a rare event (one per growth
cycle, not every turn). **Tier climb:** events stream shows growth/starvation per city per turn; the
event spine can count growth events per player.

**`[CIT/hurry]` (level 1) — hurry side effects**
Emit from `CvCity::hurry` (`CvCity.cpp:4079`) after the side effects are applied:
```
[CIT/hurry] turn=N city=NAME owner=P type=HURRY_WHIP|HURRY_GOLD
            prodAdded=HAMMERS popConsumed=N goldSpent=G angerTimer=T overflowAfter=OVERFLOW
```
`prodAdded` = `hurryProduction(eHurry)`; `popConsumed` = `hurryPopulation(eHurry)` (0 for buy);
`goldSpent` = `getHurryGold(eHurry)` (0 for whip); `angerTimer` = `hurryAngerLength(eHurry)` (0 for buy);
`overflowAfter` = `getOverflowProduction()` after `changeProduction`.
**Tier climb:** hurry events become self-describing in the wire stream; Python `onCityHurry` no longer
needed for reconstruction.

**`[CIT/decay]` (level 2) — production decay on queued items (human cities)**
Emit from `CvCity::doDecay` (`CvCity.cpp:16636`) when a non-head item actually loses progress:
```
[CIT/decay] turn=N city=NAME owner=P kind=UNIT|BUILDING type=TYPE_KEY progressLost=N
```
Level 2 because it is routine for human queues; level 1 would flood CityAI.log.

**`[CIT/overflow]` (level 2) — overflow bucket state at start of production**
Emit from `CvCity::doProduction` before `changeProduction` (`CvCity.cpp:16554`):
```
[CIT/overflow] turn=N city=NAME owner=P overflow=N feature=N cap=N
```
`overflow` = `getOverflowProduction()`; `feature` = `getFeatureProduction()`; `cap` = `getMaxProductionOverflow()`.
Level 2 — every producing city every turn, moderate volume.

### 4b. `/cities` endpoint additions

The following fields should be added to `CitySnap` (`CvHttpServer.cpp:83`) and the render block
(`CvHttpServer.cpp:339`), published from `publishIfDue` (`CvHttpServer.cpp:1549` block):

| Proposed JSON field | Source | Why needed |
|---|---|---|
| `foodBox` | `pLoopCity->getFood()` | The current fill of the food box — without this, turns-to-grow is unreconstructable from the snapshot |
| `foodThreshold` | `pLoopCity->growthThreshold()` | The target; together with `foodBox` + `food` rate → turns-to-grow is fully computable |
| `foodNet` | `pLoopCity->foodDifference()` | Net food (not gross yield); the single number that drives growth/starvation per turn |
| `overflowProduction` | `pLoopCity->getOverflowProduction()` | Banked overflow hammers; with `production` + `overflowProduction` → true per-turn yield for the current order |
| `productionProgress` | `pLoopCity->getProductionProgress()` | Hammers banked so far; with `productionNeed` → verify `producingTurns` externally |
| `productionNeed` | `pLoopCity->getProductionNeeded()` | Total cost of current head; with `productionProgress` → fraction complete |
| `hurryAngerTimer` | `pLoopCity->getHurryAngerTimer()` | Countdown of whip anger; required to reconstruct happiness state after a hurry |
| `disorder` | `pLoopCity->isDisorder() ? 1 : 0` | Disorder state halts food + production; reconstructing turns-to-complete requires knowing if a city is disordered |
| `foodProd` | `pLoopCity->isFoodProduction() ? 1 : 0` | Whether food surplus routes to hammers (Settler/Worker build); changes both net food and effective production |

**Note:** `foodNet`, `foodThreshold`, and `foodBox` together close the §A "food wastage" gap at the
snapshot level: `foodNet = foodDifference()` already incorporates wastage (when called with the
correct flag), so the wastage effect IS visible in `foodNet` even without a separate `waste` field.
For the full wastage breakdown a separate `foodWaste` field could be added, but is lower priority.

### 4c. Proposed event spine emission (future, once spine covers city domain)

When the event spine gains a `CITY` DOMAIN kind (currently only BUILDING/UNIT domains exist):

- **`CITY_GROW`** event: `{city, owner, pop, delta}` — emitted when `changePopulation` fires from food
  context. This would let the tally count growth events per player.
- **`CITY_HURRY`** event: `{city, owner, hurryType, prodAdded, popConsumed, goldSpent}` — replaces the
  Python `onCityHurry` signal with a first-class event in the OOS-safe DOMAIN stream.

---

## 5. Priority ranking

| Priority | Hook | Gap it closes |
|---|---|---|
| **HIGH** | `foodBox` + `foodThreshold` + `foodNet` to `/cities` | Turns-to-grow/starve fully reconstructable from snapshot; §A food mystery grounded |
| **HIGH** | `[CIT/food]` log tag (per-turn net food + wastage) | Food wastage observable in the event stream for the first time |
| **HIGH** | `overflowProduction` + `productionProgress` + `productionNeed` to `/cities` | Production pipeline state reconstructable; `producingTurns` verifiable |
| **HIGH** | `disorder` + `foodProd` to `/cities` | Two silent halters of food/production now explicit in snapshot |
| **MEDIUM** | `[CIT/grow]` log tag (growth/starvation events) | Growth events visible in `/events` stream without polling snapshots |
| **MEDIUM** | `[CIT/hurry]` log tag (hurry side effects) | Hurry pipeline fully self-describing on the wire |
| **MEDIUM** | `hurryAngerTimer` to `/cities` | Happiness accounting reconstructable |
| **LOW** | `[CIT/overflow]` log tag | Overflow pipeline visible between completions |
| **LOW** | `[CIT/decay]` log tag | Decay losses visible (human cities only) |
| **LOW** | `productionOverflowCap` to `/cities` (or BUG option value in `/players`) | Reconstructing why overflow was capped to gold |

---

## 6. Observability tier self-assessment (2026-06-18)

**Current tier for this system: 1 (Telescreen).**

- Snapshot (`/cities`): population, gross food rate, production rate, production head + turns estimate. ✓ T1
- No net food, no food box, no threshold, no growth/starvation events. ✗ T2
- No overflow state, no production progress, no hurry state. ✗ T2
- No wastage, no disorder signal, no foodProd signal. ✗ T2

With the **HIGH-priority hooks above**: the system reaches **Tier 3 (Big Brother)** — food and production
state changes visible in the event stream + snapshots, growth/starvation/hurry events reconstructable
per-turn from `/events`. The food-wastage §A gap is covered by `foodNet` (which already incorporates
it) + `[CIT/food]` waste breakdown.

To reach **Tier 4 (Thought Police)**: also add `[CIT/grow]` + `[CIT/hurry]` + `[CIT/overflow]` so
every city-turn state change is covered for ALL players (AI included) — no screen needed.
