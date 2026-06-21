# Unit Selection Mechanics (how a unit is actually picked for building)

**Purpose.** [`UnitAI_Selection.md`](UnitAI_Selection.md) traces the *whole*
pipeline (role choice → unit choice → creation). This companion zooms in on the
**selection step itself** — the loop and scoring that, given a role
(`UnitAITypes`), turn the catalog of trainable units into one chosen
`UnitTypes`. It is written as the **baseline for later evaluation**: it states
precisely what the code does today, what it costs, and where the selection
*profile* (which unit it ends up favouring) is shaped or distorted — so we can
later decide how to speed it up and improve the choices.

The selection step is two functions:

- **`CvCityAI::AI_bestUnitAI(eUnitAI, ...)`** — `Sources/CvCityAI.cpp:4141` — the
  scan/cache/compose loop.
- **`CvPlayerAI::AI_unitValue(eUnit, eUnitAI, pArea, criteria)`** —
  `Sources/CvPlayerAI.cpp:10313` — the raw per-unit score.

> Line numbers are anchors at time of writing; treat as "around line N".

---

## 1. The selection loop, exactly

`AI_bestUnitAI` (`CvCityAI.cpp:4141-4390`) does the following, in order:

1. **Normalize criteria** (`4170-4193`). Copies the incoming
   `CvUnitSelectionCriteria` into a local `tempCriteria` and stamps the role into
   it (`tempCriteria.m_eUnitAI = eUnitAI`).

2. **`bGrowMore` gate** (`4196-4203`). If the city should keep growing
   (positive food surplus, not angry, population below the player's per-city
   average, and more good tiles than population), `bGrowMore = true` — which will
   **exclude all `isFoodProduction` units** (settlers, workers) from the scan.
   This is the documented "small cities won't build settlers/workers" gate
   (comment at `4199-4202`).

3. **Cache check** (`4205-4230`). `m_bestUnits` is a
   `std::map<int, UnitValueInfo>` keyed by `tempCriteria.getHash()`. On a hit it
   still **re-validates** the cached unit (`!isFoodProduction` under `bGrowMore`,
   `AI_meetsUnitSelectionCriteria`, and `canTrain`) because unit limits or
   resources may have changed mid-turn, then returns the cached unit + value.

4. **Full catalog scan** (`4232-4368`). Iterates **every** unit info
   (`for iI = GC.getNumUnitInfos()-1 .. 0`). For each `eUnitX`:
   - **Cheap rejects first** (`4235-4247`): `bGrowMore && isFoodProduction`,
     `!AI_meetsUnitSelectionCriteria`, `getNotUnitAIType(eUnitAI)`, advisor
     filter, `!canTrain(eUnitX)`.
   - **Score** (`4248`): `iValue = player.AI_unitValue(eUnitX, eUnitAI, area(),
     &tempCriteria)` (§2).
   - **Post-process** (`4251-4331`) when `iValue > 0`:
     - clamp to `MAX_INT/100`, then `iValue *= 100` (headroom so later integer
       divisions don't truncate to zero);
     - `iValue += getProductionExperience(eUnitX)` (city XP buildings favour the
       units they boost);
     - **±50 % random multiplier** (`4314-4326`):
       `iValue = iValue * (rand(50) + 100) / 100` — i.e. ×1.00…×1.50;
     - `if (unit.isSuicide()) iValue /= 3`.
   - **Keep the max** (`4333-4336`).

5. **Cache the winner** (`4370-4378`) — but **only when `!bGrowMore && !bAsync`**.
   So growth-gated scans and async (UI/preview) scans are never memoized.

6. **Return** the best `UnitTypes` (or `NO_UNIT`).

**Cache lifetime.** `m_bestUnits.clear()` runs in `AI_preUnitTurn`
(`CvCityAI.cpp:226`) — the cache is **turn-scoped per city**.

---

## 2. What `AI_unitValue` actually scores

`CvPlayerAI::AI_unitValue` (`CvPlayerAI.cpp:10313-11811`) is the substance of the
selection *profile*. Structure:

1. **Eligibility gates → return 0** (`10321-10335` + the `bValid` switch
   `10347-10812`): domain mismatch (`AI_unitAIDomainType`), `getNotUnitAIType`,
   the settler-only rule, and role-specific minimums (e.g. WORKER needs builds,
   ATTACK needs combat & `!isOnlyDefensive`).
2. **Base seed**: `1 + getAIWeight()` (XML per-unit knob).
3. **Combat normalization**: `CvGameAI::AI_combatValue` (`CvGameAI.cpp:83-114`) —
   `100 * combat`, first-strike bonus, divided by `getBestLandUnitCombat()`. So
   strength is measured **relative to the strongest land unit in the game**, which
   gives obsolete units a naturally shrinking score as the tech frontier advances.
4. **Per-role term** (`10838-11776`): each role adds its own mix — moves, work
   rate, withdrawal, collateral, bombard, city-defense/attack modifiers,
   interception, cargo, religion/corp spread, impassable-terrain penalties, etc.
   (full table in [`UnitAI_Selection.md`](UnitAI_Selection.md#6-choosing-the-concrete-unit-ai_bestunitai--ai_unitvalue)).
5. **Enemy-adaptive weights**: counter/reserve/defense terms scale anti-class
   bonuses by `AI_getUnitWeight` / `AI_getUnitCombatWeight`, refreshed once a turn
   by `AI_doEnemyUnitData` (`CvPlayerAI.cpp:25365`).
6. **Helper add-ons**: `AI_unitHealerValue`, `AI_unitPropertyValue`,
   `AI_unitImpassableCount`.

---

## 3. Performance characteristics (the "speed it up" baseline)

| Property | Reality today |
|---|---|
| **Scan complexity** | `AI_bestUnitAI` is **O(N) over `GC.getNumUnitInfos()`** per call. In a C2C-derived ruleset N is large (thousands of unit infos), so each role scan touches the whole catalog. |
| **Per-unit cost** | Dominated by `canTrain(eUnitX)` (prereq tech/building/resource/limit checks) **and** `AI_unitValue` (itself `PROFILE_FUNC`, iterating unit-combat infos, attack modifiers, etc.). Both run for every non-rejected unit. |
| **Roles per production decision** | The explicit cascade usually calls `AI_bestUnitAI` for **one** role. The implicit `AI_bestUnit` path (`CvCityAI.cpp:3846`) calls it for **every viable role**, so that path is **O(roles × N)**. |
| **Caching** | `m_bestUnits` memoizes per `criteria.getHash()` within a turn, cleared in `AI_preUnitTurn`. Mitigates repeated same-role scans, but **not** the first scan of each (role, criteria) per city per turn. |
| **Cache holes** | `bGrowMore` scans and `bAsync` scans are **never cached** (`4370`). Small growing cities therefore re-scan every time. |
| **Redundant work across cities** | The cache is **per city**; every city re-scans the same catalog with near-identical inputs. There is no player-level memo of "best unit for role R given my tech" shared across cities. |
| **Double `canTrain`** | On a cache hit, `canTrain` runs again during re-validation (`4213`) — cheap relative to a full scan, but a per-hit cost. |
| **No per-unit value cache across roles** | `AI_unitValue` recomputes the combat normalization and modifier loops every call; a unit evaluated for ATTACK and again for COUNTER does the shared sub-work twice. |

**Speed levers to evaluate later** (not yet implemented):

- Precompute, per player per turn, the set of `canTrain`-able units (and their
  `AI_combatValue`) once, instead of per city per role.
- Share a player-level `(role) → best unit` memo, invalidated on tech/civic/limit
  changes, so satellite cities skip the scan.
- Cache `bGrowMore` results too (or compute the food-unit answer separately) so
  growing cities aren't perpetual cache misses.
- Cap or early-exit the catalog scan (units are roughly era-ordered; most of the
  catalog is obsolete or untrainable).

---

## 4. Selection-profile characteristics (the "better choices" baseline)

What actually decides *which* unit wins — and where that is weak:

1. **±50 % random multiplier dominates fine distinctions** (`4314-4326`). Two
   units within ~33 % of each other's score can swap places purely on the roll.
   The cache then **freezes that random outcome for the whole turn**, so the
   noise is sticky, not averaged out. This is deliberate variety, but it also
   means a clearly-better-but-only-slightly unit is not reliably chosen.

2. **Promotions are not valued.** The block at `CvCityAI.cpp:4263-4312` that would
   add value for free promotions (from the unit, from buildings, from traits) is
   **commented out** with `"Super slow evaluation, needs smart caching to work"`
   and `"this need rework to take actual promotion values *** TODO ***"`. So a
   city that grants strong free promotions to a unit class does **not** prefer
   units of that class beyond raw stats. Large, known quality gap.

3. **Production/gold cost is not in `AI_unitValue`.** The scorer rewards
   capability but does **not** divide by `getProductionCost` (confirmed: no
   `getProductionCost` reference in `AI_unitValue`'s range). Cost only enters
   later, via the cascade's turns-to-build and process fallback logic. So
   selection can favour an expensive unit that is only marginally better per
   hammer. (Contrast: building scoring *does* fold cost in, e.g.
   `CvPlayerAI.cpp:16108`.)

4. **Enemy-adaptive weighting is uneven.** Counter/reserve/city-defense branches
   react to observed enemy composition; the plain `UNITAI_ATTACK` branch largely
   does not. Offensive stack composition is therefore less responsive to what the
   enemy actually fields than defensive composition is.

5. **Role weighting (implicit path) self-balances by count, not by quality.**
   `AI_bestUnit` subtracts existing unit counts per role (`CvCityAI.cpp:3985`) and
   applies fixed multipliers + leader skew + a `getSorenRandNum(iMilitaryWeight)`
   jitter. Composition is driven by *how many* of a role exist, not by whether the
   existing ones are obsolete — an army of stale units still suppresses the role's
   weight.

6. **`bGrowMore` can starve expansion.** Because growth blocks all food-production
   units, a city that "should grow" will not produce settlers/workers even when
   the empire badly needs them — the decision is local to the city, not weighed
   against empire-wide need.

7. **Suicide penalty is a blunt `/3`** (`4328-4331`) — flagged in-code as
   "much of this is compensated", i.e. an acknowledged approximation.

---

## 5. Evaluation hooks already in the code

When measuring current behaviour before changing it, these are the instrumented
points:

- **`LOG_CITY_BLOCK` / `logAiEvaluations`** inside `AI_bestUnitAI`
  (`4147-4366`) log each candidate's base vs final value, cache hits, and
  "not chosen (not better)" rejections — a ready-made trace of the selection
  profile per city per role.
- **`PROFILE_FUNC` / `PROFILE_EXTRA_FUNC`** on `AI_bestUnitAI`, `AI_unitValue`,
  and `AI_bestUnit` — these already feed the profiler, so hotspot share is
  measurable without new instrumentation.
- The "No buildable defender!!" / "No Buildable Unit for selected AI!!" debug
  strings (`4380-4387`) flag roles that resolved to `NO_UNIT`.

---

## 6. Reference map

| Concern | File:line | Symbol |
|---|---|---|
| Selection loop | `CvCityAI.cpp:4141` | `AI_bestUnitAI` |
| Growth gate | `CvCityAI.cpp:4196` | `bGrowMore` |
| Cache check / store | `CvCityAI.cpp:4205,4370` | `m_bestUnits` |
| Cache clear (per turn) | `CvCityAI.cpp:226` | `AI_preUnitTurn` |
| ±50 % jitter | `CvCityAI.cpp:4314` | `getSorenRandNum(50)` |
| Disabled promotion valuation | `CvCityAI.cpp:4263-4312` | commented TODO |
| Per-unit scorer | `CvPlayerAI.cpp:10313` | `AI_unitValue` |
| Combat normalization | `CvGameAI.cpp:83` | `AI_combatValue` |
| Role weighting (implicit) | `CvCityAI.cpp:3846` | `AI_bestUnit` |
| Existing-count subtraction | `CvCityAI.cpp:3985` | `AI_total*UnitAIs` |
| Enemy observation | `CvPlayerAI.cpp:25365` | `AI_doEnemyUnitData` |
| Cross-city ranking wrappers | `CvPlayerAI.cpp:25214,25267,26782` | `AI_bestAreaUnitAIValue` / `AI_bestCityUnitAIValue` / `bestBuildableUnitForAIType` |

---

## 7. Open questions for the improvement pass

1. Can the catalog scan be bounded (era window / trainable-set precompute) without
   losing valid late-game or special units?
2. Should the per-unit value cache be lifted to the **player** level (keyed by
   tech/civic state) so cities share it?
3. Is the ±50 % jitter still earning its variety, given the per-turn cache freezes
   it anyway? Would a smaller jitter (or jitter only among near-ties) give a
   sharper profile?
4. What is the right way to (cheaply) reintroduce promotion value — the explicit
   TODO at `CvCityAI.cpp:4263`?
5. Should `AI_unitValue` (or `AI_bestUnitAI`'s post-processing) fold in
   production/gold cost, as building selection does?
6. Should the `UNITAI_ATTACK` branch consume the enemy-composition weights the way
   the counter/defense branches do?

*Companion:* [`UnitAI_Selection.md`](UnitAI_Selection.md),
[`doProduction.md`](doProduction.md),
[`CvCityAI`](CvCityAI.md),
[`CvPlayerAI`](CvPlayerAI.md).
