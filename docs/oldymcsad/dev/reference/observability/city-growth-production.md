# Observability — City production, overflow & hurry — what's on the wire for the hammer pipeline

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites re-confirmed in `Sources/Engine/CvCity.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvCity.cpp` (production tick / overflow / hurry / decay), `Sources/Tools/CvHttpServer.cpp` (`/cities`), the `[CIT]` tags in `Sources/AI/`. Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree, and the food/growth half split out to its canonical home.
> A city's production pipeline is **Tier 1 (Telescreen)** today: only the gross production rate (`/cities.production`) and a derived turns-to-complete estimate (`/cities.producingTurns`) are on the wire. The banked overflow bucket, partial progress, the overflow→gold conversion, hurry side-effects, and decay are all computed live and emitted nowhere except sparse `[CIT/*]` completion lines. This map walks the per-turn production mechanics, names exactly what is and isn't observable, and proposes the snapshot fields / log tags / endpoint that climb it.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> **Scope split:** food, growth/starvation, the food bar and granary are mapped in
> [`food-yields-wastage.md`](food-yields-wastage.md) — this doc covers the **production/hammer** half
> only (overflow, hurry, decay). `doTurn` runs food *before* production, so a growth event is already in
> effect when hammers accrue this turn.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer. (The old draft's numbers were already ~30–90 lines stale when re-confirmed against the
> reorganized tree; the functions are stable.)

---

## 1. How it actually works — per-turn mechanics

### 1.1 Per-turn order (`CvCity::doTurn`, CvCity.cpp:1234)

The relevant sub-steps fire in this order:

```
doCheckProduction()                 // validates the queue, returns bAllowNoProduction
changeFood(foodDifference(), true)  // food tick — see food-yields-wastage.md (precedes production)
doCulture / doPlotCulture
doAutobuild()                       // free buildings added before production
doProduction(bAllowNoProduction)    // hammer accrual, overflow, completion
```

Growth precedes production: a pop change or food-box reset is already applied before hammers accrue.

### 1.2 Production accrual (`CvCity::doProduction`, CvCity.cpp:16604)

**Hammers per turn** — `getProductionDifference` (CvCity.cpp:3974) over `getProductionPerTurn`
(CvCity.cpp:3961):
```
max(1, getExtraYield(YIELD_PRODUCTION)
     + overflow      (if flag set: getOverflowProduction() + getFeatureProduction())
     + foodSurplus   (if FoodProduction flag and isFoodProduction())
     + getBaseYieldRate(YIELD_PRODUCTION) + getSpecialistYieldTotal(YIELD_PRODUCTION))
       * getBaseYieldRateModifier(YIELD_PRODUCTION) / 100
```
A producing city with no disorder yields at least 1 hammer/turn.

**Early exits in `doProduction`:**
- `isDisorder()` → returns early; no hammers accrue this turn.
- Process-mode order (`m_bPopProductionProcess`) → handled and returns; a process city accumulates
  **no overflow** (its production converts to gold/science/culture instead).

### 1.3 Overflow bucket (`m_iOverflowProduction`, `getMaxProductionOverflow` CvCity.cpp:9808)

- The overflow + feature-production buckets are **cleared each turn** when production runs (inside
  `doProduction`), then the per-turn hammers are added on top.
- Filled on order completion by `popOrder` (CvCity.cpp:15769): overflow =
  `min(iMaxOverflow, iProgress - iNeeded)` where
  `iMaxOverflow = getYieldRate(YIELD_PRODUCTION) * BugOptionINT("CityScreen__ProductionOverflowLimit", 2)`
  (CvCity.cpp:9808). **Default cap = 2× base production per turn.**
- Any raw overflow beyond `iMaxOverflow` is **converted to gold** at
  `GC.getMAXED_UNIT_GOLD_PERCENT()` / `MAXED_BUILDING_GOLD_PERCENT()` / `MAXED_PROJECT_GOLD_PERCENT()`.
- **Feature production** (`m_iFeatureProduction`): hammers from chopping forests, banked alongside
  overflow and consumed with it; cleared each turn in `doProduction`. Not separately exposed.

### 1.4 Hurry (`canHurry` CvCity.cpp:4008, `hurry` CvCity.cpp:4089)

**Two flavours**, by `GC.getHurryInfo(eHurry)`:
- **Buy** (`getGoldPerProduction() > 0`): spends gold to complete immediately.
- **Whip** (`getProductionPerPopulation() > 0`): sacrifices population for hammers.

`canHurry` gates (via `canHurryInternal`, CvCity.cpp:3989): player has the hurry, not in disorder,
pop > hurryPopulation; production not already complete; unit or building orders only; player has the gold
(buy path).

`hurry(eHurry)` applies, in order:
1. Buy: `changeGold(-getHurryGold(eHurry))` on the player.
2. Whip: `changePopulation(-hurryPopulation(eHurry))`, `changeHurryAngerTimer(hurryAngerLength(eHurry))`.
3. Always: `changeProduction(hurryProduction(eHurry))`.
4. Fires `CvEventReporter::cityHurry(this, eHurry)` → Python `onCityHurry`.

The injected hammers over-complete the order; the completion loop in `doProduction` then banks the
resulting overflow per §1.3. `maxHurryPopulation()` caps whip pop consumed at `getPopulation() / 2`.

**Hurry anger** — `m_iHurryAngerTimer`: decremented each `doTurn`; length =
`flatHurryAngerLength()` = `HURRY_ANGER_DIVISOR * gameSpeedPercent/100 * (100+hurryAngerModifier)/100`.

### 1.5 Production decay (`CvCity::doDecay`, CvCity.cpp:16728)

Human cities only; fires **after** `doProduction`. Queued items **not at the head** decay at
`BUILDING_PRODUCTION_DECAY_PERCENT`% per `BUILDING_PRODUCTION_DECAY_TIME` turns (game-speed scaled). The
partial-progress ledgers (`m_progressOnBuilding` / `m_progressOnUnit`) are the values that bleed.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### 2.1 What IS observable

| Source | Field | What it is |
|---|---|---|
| `GET /cities` | `production` | `getYieldRate(YIELD_PRODUCTION)` — gross base hammers/turn (excludes banked overflow) |
| `GET /cities` | `producing` | XML key of the current head order (or NONE) |
| `GET /cities` | `producingTurns` | `getProductionTurnsLeft()` — rounded turns-to-complete estimate (0 if idle/process) |
| `GET /players` | `production` | sum of `YIELD_PRODUCTION` across the player's cities |
| `[CIT/produced] … UNIT/BUILDING/PROJECT` (level 1) | `overflow=` banked, `lost=` burned-to-gold | completion line per order (`Sources/AI/`) |
| `[CIT/waste]` (level 1) | `lostProd=` hammers burned, `gold=` gained | overflow exceeded cap → converted to gold |
| `[CIT/cancel]` (level 1) | `progressLost=` | order cancelled (abandoned/obsoleted) |
| `[CIT/push]` / `[CIT/push/reject]` (level 2) | order enqueued / anti-spam guard fired | queue mutation |
| `[CIT/spin]` (level 1) | completion loop hit the 50-iteration safety cap | anomaly signal, not normal path |
| `[CIT/begin]` / `[CIT/order]` (level 1) | AI production-choice context + decision | `Sources/AI/CvCityAI.cpp` |
| Python `onCityHurry` | `(city, hurryType)` | hurry fired — **no** prodAdded/popConsumed/goldSpent payload |

### 2.2 What is NOT observable

Every field below is computed live but emitted nowhere (or only at completion):

| State | Function | Significance |
|---|---|---|
| Overflow bucket | `getOverflowProduction()` | Banked hammers consumed next turn; only surfaces at completion in `[CIT/produced]` |
| Feature production | `getFeatureProduction()` | Chop hammers banked with overflow |
| Overflow cap | `getMaxProductionOverflow()` (CvCity.cpp:9808) | BUG-option cap (default 2× base/turn); explains why overflow was burned to gold |
| Current progress | `getProductionProgress()` | Hammers banked toward the head order; only `producingTurns` (derived) is exposed |
| Production needed | `getProductionNeeded()` | Total cost of the head order; with progress → true fraction complete |
| Per-item partial progress | `m_progressOnBuilding` / `m_progressOnUnit` | The decay ledger for queued non-head items |
| Decay event | `doDecay()` (CvCity.cpp:16728) | Whether a non-head item lost hammers this turn; no log, no event |
| Hurry side-effects | `hurryProduction` / `hurryPopulation` / `getHurryGold` | How much production/pop/gold the hurry moved; `onCityHurry` carries none of it |
| Hurry-anger timer | `m_iHurryAngerTimer` | Per-city whip-anger countdown |
| Disorder state | `isDisorder()` | Halts production for the turn; not a snapshot field |
| isFoodProduction | `isFoodProduction()` | Whether food surplus is routed to hammers (Settler/Worker); changes effective production |
| Process conversion | (process-mode output) | What a process city converts production into per turn |

---

## 3. The gap

An external observer seeing only the API today gets the gross production rate and a rounded
turns-to-complete estimate. It **cannot**:

1. **Verify turns-to-complete.** `producingTurns` is a rounded estimate; `getProductionProgress()`
   (hammers banked) and `getProductionNeeded()` (total cost) are both hidden, so it cannot be checked.
   A mid-turn hurry resets the estimate with no visible cause except the next snapshot change.
2. **See the overflow pipeline between completions.** `[CIT/produced]` and `[CIT/waste]` describe what
   happened *at completion*; between completions the banked overflow, feature production, and their
   effect on the next turn's effective hammers are invisible.
3. **Reconstruct a hurry's effect.** `onCityHurry(city, hurryType)` (Python only) confirms a hurry
   happened but reports no production gained, no pop consumed, no gold spent, no anger timer — none of
   the state change.
4. **See decay at all.** Human cities losing hammers via `doDecay` leave no trace in any log or endpoint.
5. **Distinguish growth from hurry from an event gift.** All three change `population`/production state,
   but only hurry fires `onCityHurry`; a natural growth and an event `+1 pop` are indistinguishable in
   the next snapshot. (The growth half is mapped in [`food-yields-wastage.md`](food-yields-wastage.md).)

**Orwell-bar consequence:** the production state of an AI city is opaque — a city one hammer from
completing a wonder and one that just started it can give identical `production` + `producingTurns`
snapshots if the estimate rounds the same.

---

## 4. Proposed hooks (concrete additions)

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes].

### 4.1 Extend `/cities` snapshot with production internals (CvHttpServer.cpp)

Add to `CitySnap` + `renderCities` (cheap scalar reads, game-thread-safe under the snapshot contract):

| JSON field | C++ source | Notes |
|---|---|---|
| `overflowProduction` | `getOverflowProduction()` | Banked overflow; with `production` → true per-turn hammers for the head order |
| `featureProduction` | `getFeatureProduction()` | Chop hammers banked with overflow |
| `productionProgress` | `getProductionProgress()` | Hammers banked toward head; with `productionNeed` → verify `producingTurns` |
| `productionNeed` | `getProductionNeeded()` | Total cost of the head order |
| `overflowCap` | `getMaxProductionOverflow()` | Why overflow was capped to gold |
| `hurryAngerTimer` | `getHurryAngerTimer()` | Whip-anger countdown; needed to reconstruct happiness |
| `disorder` | `isDisorder() ? 1 : 0` | Production halter for the turn |
| `foodProd` | `isFoodProduction() ? 1 : 0` | Food-surplus-to-hammers redirect; changes effective production |

### 4.2 Per-turn production log tag: `[CIT/overflow]` (level 2)

Emit in `doProduction` before the per-turn `changeProduction`, one line per producing city:
```
[CIT/overflow] turn=N city=NAME owner=P overflow=N feature=N cap=N progress=N need=N
```
Level 2 — every producing city every turn, moderate volume; the per-turn production audit trail.

### 4.3 Hurry side-effects log tag + event: `[CIT/hurry]` (level 1)

Emit in `CvCity::hurry` (CvCity.cpp:4089) after the side-effects are applied:
```
[CIT/hurry] turn=N city=NAME owner=P type=HURRY_WHIP|HURRY_GOLD
            prodAdded=hurryProduction popConsumed=hurryPopulation goldSpent=getHurryGold
            angerTimer=hurryAngerLength overflowAfter=getOverflowProduction
```
Low-frequency, high-signal. Pair with an `/events` SSE `cityHurry` event carrying the same fields —
replacing the payload-free Python `onCityHurry` with a first-class, self-describing wire event.

### 4.4 Decay log tag: `[CIT/decay]` (level 2)

Emit in `CvCity::doDecay` (CvCity.cpp:16728) when a non-head item actually loses progress:
```
[CIT/decay] turn=N city=NAME owner=P kind=UNIT|BUILDING type=TYPE_KEY progressLost=N
```
Level 2 (routine for human queues; level 1 would flood the log). Closes the only entirely-dark path.

### 4.5 `/diagnostic/production?player=N` endpoint

A mailbox endpoint (same model as `canConstruct`) returning the full production balance for a player's
cities on demand:
```json
{ "player": 1, "cities": [
  { "id": 42, "name": "London", "producing": "BUILDING_GRANARY",
    "perTurn": 14, "overflow": 6, "feature": 0, "progress": 88, "need": 120,
    "turnsLeft": 3, "overflowCap": 28, "hurryAnger": 0, "disorder": 0, "foodProd": 0 } ] }
```
The screen-free production read for the Orwell bar — every AI city's hammer pipeline in one call.

---

## 5. Tier assessment

| Tier | Name | Production system status |
|---|---|---|
| 0 | Oblivious | (below current) |
| **1** | **Telescreen** | **Current state.** Gross production rate + rounded `producingTurns` only. Overflow, progress, need, hurry effects, decay: all invisible. |
| 2 | Informant | + `/diagnostic/production` snapshot per player (§4.5): full pipeline on demand. |
| 3 | Big Brother | + `[CIT/overflow]` / `[CIT/hurry]` / `[CIT/decay]` log + the `cityHurry` event in `/events` (§4.2–4.4). |
| 4 | Thought Police | + `/cities` extended with `overflowProduction`/`productionProgress`/`productionNeed`/`hurryAngerTimer` etc. (§4.1) — pipeline reconstructible from the polling snapshot for all players. |
| 5 | Meta | + level-3 per-order progress trace + process-mode per-turn conversion output. |

**To climb 1 → 4 (the Orwell bar):** implement §4.1 (extend `/cities`) and §4.3 (`[CIT/hurry]` log +
event). §4.2/§4.4 are the per-turn audit trail. All cheap, gated, off by default — no game-logic side
effects.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine classes → `Sources/Engine/`,
> `Cv*AI` → `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| Per-turn order site | `CvCity::doTurn` — `Sources/Engine/CvCity.cpp:1234` |
| Production accrual + early exits | `CvCity::doProduction` — `Sources/Engine/CvCity.cpp:16604` |
| Hammers/turn formula | `CvCity::getProductionPerTurn` — `Sources/Engine/CvCity.cpp:3961`; `getProductionDifference` `:3974` |
| Overflow cap | `CvCity::getMaxProductionOverflow` — `Sources/Engine/CvCity.cpp:9808` |
| Overflow fill / gold conversion on completion | `CvCity::popOrder` — `Sources/Engine/CvCity.cpp:15769` |
| Hurry gate | `CvCity::canHurry` — `Sources/Engine/CvCity.cpp:4008`; `canHurryInternal` `:3989` |
| Hurry application | `CvCity::hurry` — `Sources/Engine/CvCity.cpp:4089` |
| Production decay | `CvCity::doDecay` — `Sources/Engine/CvCity.cpp:16728` |
| `isFoodProduction` redirect | `CvCity::foodDifference` — `Sources/Engine/CvCity.cpp:5980` |
| `/cities` snapshot publish + render | `Sources/Tools/CvHttpServer.cpp` (`publishIfDue` city walk / `renderCities`) |
| `[CIT/*]` completion/queue tags | `Sources/AI/` (`CvCityLogTags.h`) + `Sources/Engine/CvCity.cpp` emit sites |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/cities`, `/events`, `/diagnostic`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`food-yields-wastage.md`](food-yields-wastage.md) — the food/growth half: food bar, threshold, granary,
  growth/starvation events. `doTurn` runs food before production, so growth is already applied when
  hammers accrue here.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing for map-before-delete.
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
