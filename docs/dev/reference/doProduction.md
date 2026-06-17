# `CvCity::doProduction`

**File:** `Sources/CvCity.cpp:16538`
**Header:** `Sources/CvCity.h:1897`
**Signature:** `void CvCity::doProduction(bool bAllowNoProduction)`

`doProduction` is the per-turn production step for a single city. It is one of
the per-city subroutines run during `CvCity::doTurn`; it advances whatever the
city is currently building (unit / building / project / process), pops finished
orders, and converts leftover hammers into either overflow, gold (from the
"lost production" path) or — for AI cities — a request that the AI pick the
next order.

It is **not** the function that grants the unit / building / project itself.
Order completion is performed by `popOrder` (called from inside this function),
which in turn calls into `createUnit`, `addBuilding`, etc. `doProduction` is
the scheduler that drives `popOrder`.

---

## 1. Where it is called from

There is exactly one production call site in normal turn processing:

```cpp
// CvCity.cpp:1326
bool bAllowNoProduction = !doCheckProduction();
...
// CvCity.cpp:1340 — autobuild first
doAutobuild();

// CvCity.cpp:1342
doProduction(bAllowNoProduction);
```

The ordering inside `doTurn` matters; `doProduction` must run **after**:

| Prior step | Why it must run first |
|---|---|
| `doCheckProduction()` (1326) | Validates the queue, removes orders the city can no longer build, upgrades units in the queue, refunds gold from maxed wonders, and decides whether the city is allowed to coast on an empty queue this turn (the return value becomes `bAllowNoProduction`). |
| `changeFood(...)` (1328) | Food has to be applied before the food-producing-unit branch decides whether the city was fed this turn. |
| `doCulture` / `doPlotCulture` (1330–1334) | Plot ownership must be finalised so the production calculation reads the correct worked tiles. |
| `CvPlot::setDeferredPlotGroupRecalculationMode(false)` (1337) | Trade-network recalculation must be flushed so commerce / production bonuses from trade are current. |
| `doAutobuild()` (1340) | Free auto-built buildings (e.g. cottage-style automatic buildings) are added to the city before we compute hammers for the turn — they can change `getCurrentProductionDifference`. |

After `doProduction`, the city's contract broker advertises any new build
priority (`CvCity.cpp:1344`), then production decay (`doDecay`) ticks the
half-built work that was *not* the active order.

---

## 2. The `bAllowNoProduction` parameter

`bAllowNoProduction` is `true` when `doCheckProduction()` returned `false` — that
is, when the queue was disturbed during validation (`canContinueProduction`
failed and an order was popped, or the queue is empty for a human player with
no automation). In that case, `doProduction` is allowed to bail out early
rather than force-pick something to build.

Concretely, it is consulted at one spot:

```cpp
if (!bAllowNoProduction && !isProduction())
{
    return;
}
```

If the city has nothing queued *and* the caller did not grant permission to
skip, the function falls through and is expected to either pick something
(AI branch above) or leave the city idle.

---

## 3. Step-by-step walkthrough

### 3a. AI / automated production selection (lines 16541–16549)

```cpp
if (!isHuman() || isProductionAutomated())
{
    if (!isProduction() || isProductionProcess() || AI_isChooseProductionDirty() && !isProductionUnit())
    {
        AI_chooseProduction();
    }
}
```

For non-human or automated cities, the AI may choose a new order this turn if:

- the queue is empty (`!isProduction()`), **or**
- the city is on a *process* (research/gold/culture conversion), which is the
  AI's fallback; the AI re-checks every turn whether it has something better
  to build, **or**
- `AI_isChooseProductionDirty()` flagged that something changed (new tech,
  new bonus, conquered city, …) **and** the city is not already mid-train
  on a unit.

The unit-in-progress carve-out is the Koshling contracting comment: with the
contract broker system, units are built only to contractual demand, and we
must not abandon a half-built unit because a new tech became available — the
demand that placed the contract is what should drive the swap, not the tech
queue.

### 3b. Idle-city short-circuit (lines 16551–16554)

```cpp
if (!bAllowNoProduction && !isProduction())
{
    return;
}
```

Covers the human-with-empty-queue case described in §2.

### 3c. Process orders (lines 16556–16564)

```cpp
if (isProductionProcess())
{
    if (m_bPopProductionProcess)
    {
        popOrder(0, false, true);
        m_bPopProductionProcess = false;
    }
    return;
}
```

A *process* is a `ProcessTypes` conversion order (gold / science / culture /
espionage). It has no completion milestone; hammers are spent the turn they
are produced. So `doProduction` does no progress accounting for processes,
and only honours a deferred-cancel flag (`m_bPopProductionProcess`) that
another system set earlier in the turn. After that, it returns — `doDecay`
still runs separately.

### 3d. Disorder (lines 16566–16569)

```cpp
if (isDisorder()) { return; }
```

Cities in civil disorder produce no hammers; bail before applying any.

### 3e. Apply this turn's production and pop finished orders (lines 16571–16611)

```cpp
if (isProduction())
{
    changeProduction(getCurrentProductionDifference(
        ProductionCalc::FoodProduction | ProductionCalc::Overflow));
    setOverflowProduction(0);
    setFeatureProduction(0);

    setBuiltFoodProducedUnit(isFoodProduction());
    clearLostProduction();
    ...
}
```

This is the core of the function. Step by step:

1. **`getCurrentProductionDifference(FoodProduction | Overflow)`** asks the city
   for hammers earned this turn, with these flags:
   - `FoodProduction` — if the city is building a food-consuming unit
     (Settler, Worker variants), include surplus food as production input.
   - `Overflow` — include any banked overflow / feature-clear production.

2. **`changeProduction(...)`** adds those hammers to the active order's
   `m_iProduction` counter.

3. **`setOverflowProduction(0)` / `setFeatureProduction(0)`** zero the
   carry-over buckets because they have just been consumed.

4. **`setBuiltFoodProducedUnit(isFoodProduction())`** records that the unit
   we are about to finish (if any) consumed food. This drives a later check
   that prevents a food-build exploit (see step 7).

5. **`clearLostProduction()`** resets the per-turn lost-production accumulator
   that `popOrder` may write to if a wonder is finished but its commit is
   rejected.

6. **Completion loop** — `while (productionLeft() <= 0)`:

   ```cpp
   while (productionLeft() <= 0)
   {
       popOrder(0, true, true);
       if (!isProduction())
       {
           if (isHuman()) break;
           else AI_chooseProduction();
           FAssertMsg(isProduction(), "AI set city to produce nothing at all!")
       }
       if (isFoodProduction() && !isBuiltFoodProducedUnit())
       {
           break;
       }
       changeProduction(getOverflowProduction());
       setOverflowProduction(0);
   }
   ```

   - `productionLeft()` returns `getProductionNeeded() - getProductionProgress()`.
     When that drops to zero or below, the head order is complete.
   - `popOrder(0, /*bFinish=*/true, /*bChoose=*/true)` finalises the order
     (creates the unit / adds the building / commits the project) and shifts
     the queue. `bFinish=true` is the "the order was earned, do not refund"
     signal; `bChoose=true` lets the AI pick a follow-up if the queue empties.
   - If the queue drained, a human city stops looping (we don't auto-pick for
     them); an AI city calls `AI_chooseProduction()` and asserts that
     something was picked.
   - **Food-build exploit guard:** if the *new* head order is a food-producing
     unit (Settler/Worker) and we did **not** just finish a food-producing
     unit this turn, break out of the loop. This prevents stockpiling
     hammers on a cheap build and then swapping to a Settler to "pre-build"
     it instantly from overflow. The original comment in the file lists
     "Settlers and Workers" as the canonical case.
   - Otherwise, fold this turn's overflow back into the new head order and
     loop again — a single, huge production turn can in principle finish
     multiple chained cheap orders.

7. **Lost-production refund (16613–16620):**

   ```cpp
   if (m_iGoldFromLostProduction > 0)
   {
       ... AddDLLMessage ...
       GET_PLAYER(getOwner()).changeGold(m_iGoldFromLostProduction);
       clearLostProduction();
   }
   ```

   `popOrder` writes to `m_iGoldFromLostProduction` when a building / wonder
   that this city tried to finish was already completed elsewhere (or
   otherwise invalidated) and the spent hammers were converted to gold. The
   player gets paid here, gets the in-game message, and the buffer is
   cleared.

### 3f. Overflow accumulation when nothing is queued (lines 16622–16625)

```cpp
else
{
    changeOverflowProduction(getCurrentProductionDifference(ProductionCalc::FoodProduction));
}
```

If the city had no active order but was still allowed to run (i.e.
`bAllowNoProduction == true` and we reached this point), the hammers earned
this turn are *not* lost — they go into the overflow bucket so they're
available the next time an order is queued. Note that this branch passes
only the `FoodProduction` flag; we deliberately do **not** pass `Overflow`
here, because that would double-count any overflow already banked.

---

## 4. State touched

| State | Read | Written |
|---|---|---|
| Order queue (`m_orderQueue`) | ✓ | via `popOrder` |
| `m_iProduction` (head order progress) | ✓ | `changeProduction` |
| `m_iOverflowProduction` | ✓ | `setOverflowProduction`, `changeOverflowProduction` |
| `m_iFeatureProduction` | — | `setFeatureProduction(0)` |
| `m_bBuiltFoodProducedUnit` | ✓ | `setBuiltFoodProducedUnit` |
| `m_iLostProduction`, `m_iLostProductionModified`, `m_iGoldFromLostProduction` | ✓ | `clearLostProduction` |
| `m_bPopProductionProcess` | ✓ | cleared after honouring |
| Player gold | — | `changeGold` (lost-production refund only) |
| AI choose-production-dirty flag | ✓ | indirectly (via `AI_chooseProduction`) |

It does **not** touch: production decay buckets (`m_progressOnBuilding`,
`m_progressOnUnit` — that is `doDecay`), the contract broker, religion,
great people, espionage visibility, or property unit spawning. Those are
sibling steps in `doTurn`.

---

## 5. Important invariants and gotchas

- **`doCheckProduction` must run first.** Without it, an order that has become
  invalid (lost prerequisite, completed elsewhere) could be popped as
  "finished" here, granting a wonder the player no longer qualifies for.
  `bAllowNoProduction` is the back-channel from that step.

- **Plot-group recalculation must be flushed before this runs**
  (`CvPlot::setDeferredPlotGroupRecalculationMode(false)` at `CvCity.cpp:1337`).
  `getCurrentProductionDifference` reads trade-network state; if it is stale,
  bonuses from newly-connected resources will be miscounted by one turn.

- **AI cities must never end this function with an empty queue.** The
  `FAssertMsg(isProduction(), "AI set city to produce nothing at all!")`
  inside the completion loop encodes that. Human cities are allowed to be
  idle.

- **Process orders are intentionally invisible to the completion loop.** They
  return early at §3c; `productionLeft()` does not apply.

- **Disorder zeros hammers without zeroing overflow.** The §3d early return
  skips both the apply step and the no-order overflow accumulation — a city
  in disorder neither spends nor banks hammers this turn.

- **Food-production guard is one-shot per turn.** `setBuiltFoodProducedUnit`
  is set *before* the loop based on what was at the head of the queue when
  the turn started. That snapshot is what gates the guard on subsequent
  iterations of the completion loop. Anything queued *behind* a non-food
  unit that gets swapped to a Settler mid-loop will be caught and bail out.

- **Lost-production gold is paid out only here.** `popOrder` writes the
  buffer; `doProduction` reads, awards, messages, and clears it. If another
  call site ever invokes `popOrder` with the lost-production path active
  (e.g. an out-of-turn rebuild), the gold will sit in the buffer until the
  next `doProduction` reads it.

---

## 6. Related functions

| Function | Role |
|---|---|
| `CvCity::doCheckProduction` (`CvCity.cpp:16384`) | Pre-flight: validates the queue, refunds maxed wonders, upgrades unit orders, decides whether the empty-queue path is permitted. |
| `CvCity::popOrder` (`CvCity.cpp:15770`) | Commits the head order (create unit / add building / finish project), advances the queue, writes lost-production gold when applicable. |
| `CvCity::getCurrentProductionDifference` (`CvCity.cpp:4011`) | Computes hammers per turn including modifiers, food-into-production conversion, and overflow inclusion based on the `ProductionCalc::flags` mask. |
| `CvCity::productionLeft` (`CvCity.cpp:6115`) | `getProductionNeeded() - getProductionProgress()` — drives the completion loop. |
| `CvCity::doAutobuild` | Adds free auto-buildings before production is computed; can change hammer output for this turn. |
| `CvCity::doDecay` (`CvCity.cpp:16629`) | Decays partially-built items that are **not** the current head order. Sibling step, runs immediately after `doProduction`. |
| `CvCityAI::AI_chooseProduction` | Selects what an AI / automated city should build next. Invoked from §3a and from the completion loop. |
