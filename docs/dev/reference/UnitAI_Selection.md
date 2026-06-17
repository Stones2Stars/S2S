# UNITAI Selection & Unit Production

**Scope:** how the AI decides *which UNITAI a unit is built as*, and — by
extension — *which concrete unit type it produces*, traced from the data model
through the production decision to the moment the unit is created.

This is a cross-cutting process that spans `CvEnums.h`, `CvUnitInfo`, `CvCityAI`,
`CvPlayerAI`, `CvGameAI`, `CvCity`, `CvPlayer`, and `CvUnit`/`CvUnitAI`. It is the
piece deliberately left out of [`doProduction.md`](doProduction.md), which
documents how an order is *advanced and completed* but explicitly *not* how the
AI *picks* the order.

> Line numbers are anchors against the source at time of writing; they drift, so
> treat them as "around line N" and confirm against the named function.

Related reference docs: [`CvCityAI`](CvCityAI.md),
[`CvPlayerAI`](CvPlayerAI.md),
[`CvUnitAI`](CvUnitAI.md),
[`CvContractBroker`](CvContractBroker.md),
[`CvGameAI`](CvGameAI.md).

---

## 1. The two questions

Producing a unit answers two distinct questions, in this order:

1. **Which UNITAI?** — the *role* the unit will fill: `UNITAI_ATTACK`,
   `UNITAI_SETTLE`, `UNITAI_WORKER`, `UNITAI_CITY_DEFENSE`, … This is a strategic
   choice about *what the empire needs right now*.
2. **Which concrete unit type for that role?** — e.g. a Warrior vs an Axeman for
   `UNITAI_ATTACK`. This is a tactical choice about *which trainable unit best
   serves the chosen role*, given tech, cost, and what the enemy is fielding.

The whole pipeline is just these two questions resolved in sequence and then
turned into a build order:

```
AI_chooseProduction()                 (which role? — strategic)
  -> AI_chooseUnit / AI_chooseUnitImmediate
       -> AI_bestUnit            (weigh ALL roles, pick one)   [implicit role path]
       -> AI_bestUnitAI(eUnitAI) (best concrete unit for role) [both paths]
            -> CvPlayerAI::AI_unitValue(eUnit, eUnitAI, ...)    (which type? — tactical)
  -> pushOrder(ORDER_TRAIN, eBestUnit, eUnitAI)
       ... later, on completion ...
  -> popOrder -> CvPlayer::initUnit -> CvUnit::init -> CvUnitAI::AI_init
       -> m_eUnitAIType = eUnitAI                               (role recorded on the instance)
```

**Key insight:** most branches in `AI_chooseProduction()` choose the UNITAI
*explicitly* — they already know they need a defender, a worker, a settler, etc.,
and pass a fixed `eUnitAI`. Only a handful of branches defer to `AI_bestUnit()`
(with `NO_UNITAI`) to let the engine weigh every role and pick one. So the
"strategic" decision is mostly encoded as the *order of priority branches*, not
as a single scoring function.

---

## 2. The data model

### 2.1 The `UnitAITypes` enum

The role list is a **hardcoded C++ enum** in `Sources/CvEnums.h:1447-1510`
(58 roles, from `NO_UNITAI = -1` through `NUM_UNITAI_TYPES`):

```cpp
enum UnitAITypes
{
    NO_UNITAI = -1,
    UNITAI_UNKNOWN,
    UNITAI_ANIMAL,
    UNITAI_SETTLE,
    UNITAI_WORKER,
    UNITAI_ATTACK,
    UNITAI_ATTACK_CITY,
    UNITAI_COLLATERAL,
    ... // land, then sea (UNITAI_*_SEA), then air (UNITAI_*_AIR), then specials
    UNITAI_ESCORT,
    NUM_UNITAI_TYPES
};
```

A comment at `CvEnums.h:1445` notes the order no longer matters and the XML for it
is not read — the enum registration (`registerUnitAIs`) is now the source of
truth, the enum body is documentary.

Two helper macros classify a role by domain (`CvEnums.h:1512-1531`):

| Macro | True for |
|---|---|
| `IS_NAVAL_AITYPE(e)` | all `UNITAI_*_SEA` roles |
| `IS_AIR_AITYPE(e)` | `UNITAI_ATTACK_AIR`, `UNITAI_DEFENSE_AIR`, `UNITAI_CARRIER_AIR`, `UNITAI_MISSILE_AIR` |

### 2.2 Per-unit XML

Each unit declares its role data in `Assets/XML/Units/CIV4UnitInfos.xml`:

```xml
<UnitInfo>
    <Type>UNIT_AXEMAN</Type>
    ...
    <DefaultUnitAI>UNITAI_ATTACK</DefaultUnitAI>   <!-- fallback role at creation -->
    <UnitAIs>                                       <!-- roles this unit MAY fill -->
        <UnitAI><UnitAIType>UNITAI_ATTACK</UnitAIType><bUnitAI>1</bUnitAI></UnitAI>
        <UnitAI><UnitAIType>UNITAI_COUNTER</UnitAIType><bUnitAI>1</bUnitAI></UnitAI>
    </UnitAIs>
    <NotUnitAIs>                                     <!-- roles this unit may NOT fill -->
        <NotUnitAI>UNITAI_CITY_DEFENSE</NotUnitAI>
    </NotUnitAIs>
</UnitInfo>
```

### 2.3 `CvUnitInfo` storage

The XML is parsed into `CvUnitInfo` (`Sources/Infos/CvUnitInfo.h`):

| Member | Meaning |
|---|---|
| `m_iDefaultUnitAIType` (`CvUnitInfo.h:718`) | the `<DefaultUnitAI>` fallback role |
| `m_pbUnitAIType[]` (`CvUnitInfo.h:824`) | bool array, size `NUM_UNITAI_TYPES`: may this unit fill role `i`? |
| `m_pbNotUnitAIType[]` (`CvUnitInfo.h:825`) | bool array: is this unit explicitly *barred* from role `i`? |

Accessors: `getDefaultUnitAIType()` / `getUnitAIType(i)` / `getNotUnitAIType(i)`
(`CvUnitInfo.cpp:799-802, 1358-1362`). XML loading uses `SetVariableListTagPair`
(`CvUnitInfo.cpp:4068-4069` for the default, `4131-4132` for the two lists).

### 2.4 Per-instance state

Once built, the chosen role lives on the unit instance as
`m_eUnitAIType` (`Sources/CvUnitAI.h:471`). This is what `AI_getUnitAIType()`
returns and what the unit's tactical AI dispatches on every turn.

---

## 3. The production decision: `AI_chooseProduction()`

**Entry:** `CvCityAI::AI_chooseProduction()`, `Sources/CvCityAI.cpp:776`, called
from `AI_doTurn` (see [`CvCityAI`](CvCityAI.md) "Turn Entry
Point").

The function is a **priority cascade**: ~85 numbered decision branches, each
guarded by a situational condition. The first branch whose condition fires queues
an order and `return`s — so ordering *is* the strategy. Early branches handle
emergencies; later branches handle opportunistic/economic builds; the final
branch is a wealth/research fallback.

Before the cascade (≈ lines 776-1139) it gathers the inputs every branch reads:
anarchy/danger checks, war status, financial trouble, food state, military
weight, worker/settler needs, area AI type, and whether the current order should
simply be continued.

Representative ordering of the unit-producing branches (conditions abbreviated):

| ~Priority | Situation | UNITAI(s) chosen |
|---|---|---|
| #1–#3 | City has **no defender** (`getNumDefenders == 0`) | CITY_DEFENSE, CITY_COUNTER, CITY_SPECIAL |
| #8 | **No / too few workers** in area | WORKER |
| #10 | Settler on plot needs an **escort** | CITY_DEFENSE, RESERVE, ATTACK … |
| #13–#14 | Room to **expand**, sites exist | SETTLE, SETTLER_SEA |
| #15 | Below see-invisible cap | SEE_INVISIBLE |
| #18 | Below **min defenders** (`AI_minDefenders`) | CITY_DEFENSE, CITY_COUNTER, CITY_SPECIAL |
| #23 | **Danger** and too few attackers | ATTACK |
| #27 | Too few **floating defenders** | CITY_DEFENSE, RESERVE, COLLATERAL |
| #28 | **War going badly** (`iWarSuccessRatio < -30`, enemy power high) | COUNTER, ATTACK, RESERVE, COLLATERAL |
| #32 / #84 | Building an **attack stack** / assault | ATTACK_CITY, ATTACK |
| #56 | **Aircraft** shortfall | ATTACK_AIR, DEFENSE_AIR, MISSILE_AIR, ICBM |
| #57 | **Sea assault** capacity shortfall | ASSAULT_SEA, ESCORT_SEA, ATTACK_SEA |
| #85 | nothing else chosen → `AI_chooseProcess()` | (wealth/research/culture process) |

A real call site (defenseless-city branch, `CvCityAI.cpp:1553`):

```cpp
if (AI_chooseUnitImmediate("defenseless city", UNITAI_CITY_DEFENSE, &DefaultCriteria))
{
    return;
}
```

Most branches name the UNITAI directly like this — that *is* the strategic role
choice. The `AI_bestUnit()` path (§4.2) is the exception.

---

## 4. Choosing the UNITAI role

### 4.1 Explicit path (the common case)

A branch already knows the role and passes a fixed `eUnitAI` into the
`AI_chooseUnit*` family (§5). No role scoring happens — the situation *is* the
justification.

### 4.2 Implicit path: `AI_bestUnit()`

When a branch wants "the most useful military unit right now" rather than a
specific role, it calls `AI_chooseUnit*` with `NO_UNITAI`, which routes to
`CvCityAI::AI_bestUnit()` (`CvCityAI.cpp:3846-4138`). Its real signature is:

```cpp
UnitTypes CvCityAI::AI_bestUnit(
    int& iBestUnitValue, int iNumSelectableTypes, UnitAITypes* pSelectableTypes,
    bool bAsync, UnitAITypes* peBestUnitAI, bool bNoRand, bool bNoWeighting,
    const CvUnitSelectionCriteria* criteria);
```

It fills `aiUnitAIVal[NUM_UNITAI_TYPES]` — a desirability score per role — in four
steps:

**(1) Base weights per role** (≈ 3877-3985). Each role gets a weight derived from
the empire's situation. Examples (`CvCityAI.cpp:3882-3901`):

```cpp
// settle only if not broke and a site exists
if (!bFinancialTrouble && bestFoundValue > 0) aiUnitAIVal[UNITAI_SETTLE]++;

aiUnitAIVal[UNITAI_WORKER]      += AI_neededWorkers(area());
aiUnitAIVal[UNITAI_ATTACK]      += iMilitaryWeight / ((bWarPlan||bLandWar||bAssault) ? 7 : 12) + (bPrimaryArea ? 2 : 0) + 1;
aiUnitAIVal[UNITAI_CITY_DEFENSE]+= iNumCitiesInArea + 1;
aiUnitAIVal[UNITAI_CITY_COUNTER]+= 5 * (iNumCitiesInArea + 1) / 8;
aiUnitAIVal[UNITAI_ATTACK_CITY] += iMilitaryWeight / ((bWarPlan||bLandWar||bAssault) ? 10 : 17);
aiUnitAIVal[UNITAI_DEFENSE_AIR] += getNumCities() + 1;
// ... etc for paradrop, counter, sea & air roles
```

The strategic context comes from (`CvCityAI.cpp:3862-3875`):

| Signal | Source |
|---|---|
| Area AI type (DEFENSIVE / OFFENSIVE / MASSING / ASSAULT) | `area()->getAreaAIType(getTeam())` |
| Military weight (how much army the empire wants) | `AI_militaryWeight(area())` |
| Worker need | `AI_neededWorkers(area())` |
| Is this the player's primary area | `AI_isPrimaryArea(area())` |
| War plan / danger / financial trouble | `hasWarPlan`, `AI_isDanger`, `AI_isFinancialTrouble` |
| Cities & coastal cities in area | `getCitiesPerPlayer`, `countNumCoastalCitiesByArea` |

**(2) Subtract what already exists** (≈ 3985-3996). Each role's weight is reduced
by the count of units already serving it, so force composition self-balances:

```cpp
if (IS_NAVAL_AITYPE(i))   aiUnitAIVal[i] -= AI_totalWaterAreaUnitAIs(pWaterArea, i);
else if (IS_AIR_AITYPE(i))aiUnitAIVal[i] -= AI_totalUnitAIs(i);          // air counted globally
else                      aiUnitAIVal[i] -= AI_totalAreaUnitAIs(area(), i); // land/sea per-area
```

**(3) Role multipliers + leader personality** (`CvCityAI.cpp:3998-4036`). Fixed
multipliers express baseline preference, then the leader's personality skews them:

```cpp
aiUnitAIVal[UNITAI_SETTLE] *= (bDanger ? 8 : 20);
aiUnitAIVal[UNITAI_WORKER] *= (bDanger ? 2 : 7);
aiUnitAIVal[UNITAI_ATTACK] *= 3;
aiUnitAIVal[UNITAI_ATTACK_CITY] *= 4;
aiUnitAIVal[UNITAI_COLLATERAL] *= 5;
aiUnitAIVal[UNITAI_EXPLORE] *= (bDanger ? 6 : 15);
// ... sea/air roles weighted higher (ESCORT_SEA *20, EXPLORE_SEA *18, ...)

for (int i = 0; i < NUM_UNITAI_TYPES; i++) {                 // leader skew
    aiUnitAIVal[i] *= std::max(0, getUnitAIWeightModifier(i) + 100);
    aiUnitAIVal[i] /= 100;
}
```

**(4) Random jitter + pick** (≈ 4061-4138). A random term scaled by
`iMilitaryWeight` is added per role (`getSorenRandNum`/`getASyncRand`,
`CvCityAI.cpp:4069-4073`), low-value roles are pruned, and for each surviving role
the best concrete unit is found via `AI_bestUnitAI` (§6); the role/unit pair with
the highest final value wins. The chosen role is returned through `peBestUnitAI`.

---

## 5. The glue: the `AI_chooseUnit*` family

These functions sit between "I want role X" and "queue a build order"
(`CvCityAI.cpp:8748-8869`).

`AI_chooseUnit(reason, eUnitAI, iOdds, ...)` (`CvCityAI.cpp:8748`):

1. **Odds gate** (`8773`): if `iOdds >= 0`, a `getSorenRandNum(100) < iOdds` roll
   can skip the build this turn.
2. **Contract tender** (`8791-8816`, guarded by `USE_UNIT_TENDERING`): for non-NPC
   players, rather than always building locally, the city *advertises the work* to
   the empire via `getContractBroker().advertiseWork(priority, ..., eUnitAI,
   iUnitStrength, criteria)`. Another city may win the contract and build it. This
   is gated by a per-city cap (`m_iRequestedUnit`) and a feasibility check
   (`bestBuildableUnitForAIType != NO_UNIT`). See
   [`CvContractBroker`](CvContractBroker.md). If tendering is off
   or the unit can only be built here, it falls through.
3. **Fall through** (`8819`): `AI_chooseUnitImmediate`.

`AI_chooseUnitImmediate(reason, eUnitAI, criteria, eUnitType)`
(`CvCityAI.cpp:8825`) resolves the concrete unit and queues it:

```cpp
if (eUnitType == NO_UNIT) {
    if (eUnitAI != NO_UNITAI)
        eBestUnit = AI_bestUnitAI(eUnitAI, iDummyValue, false, false, criteria); // role known
    else
        eBestUnit = AI_bestUnit(iDummyValue, -1, NULL, false, &eUnitAI, ...);     // weigh roles
}
if (eBestUnit != NO_UNIT) {
    pushOrder(ORDER_TRAIN, eBestUnit, eUnitAI, false, false, false);
    return true;
}
```

There is also a direct overload `AI_chooseUnit(UnitTypes eUnit, UnitAITypes
eUnitAI)` (`CvCityAI.cpp:8861`) that skips scoring and queues a specific unit.

---

## 6. Choosing the concrete unit: `AI_bestUnitAI()` + `AI_unitValue()`

### 6.1 `CvCityAI::AI_bestUnitAI()` — rank trainable units for a role

`CvCityAI.cpp:4141`. Given a fixed role, find the best buildable unit:

1. **Grow-more check** — if the city should grow, food-producing units (settlers)
   are skipped (`isFoodProduction`, `CvCityAI.cpp:4235`).
2. **Cache** — results are memoized in `m_bestUnits` keyed by role + criteria; a
   valid cached, still-trainable unit short-circuits the scan.
3. **Scan all unit infos** (`CvCityAI.cpp:4232`), skipping a unit when
   (`4235-4247`): blocked by growth, fails `AI_meetsUnitSelectionCriteria`,
   `getNotUnitAIType(eUnitAI)` is set, the advisor is filtered out, or `canTrain`
   is false.
4. **Score** each survivor:

```cpp
int iValue = player.AI_unitValue(eUnitX, eUnitAI, area(), &tempCriteria); // §6.2
if (iValue > 0) {
    iValue *= 100;                          // headroom against truncation
    iValue += getProductionExperience(eUnitX);   // local XP buildings help
    iValue *= getSorenRandNum(50) + 100;    // ±50% jitter, then /100
    iValue /= 100;
    if (unit.isSuicide()) iValue /= 3;      // penalize one-shot units
    // track best
}
```

5. **Cache and return** the best `UnitTypes`.

### 6.2 `CvPlayerAI::AI_unitValue()` — the core scorer

`Sources/CvPlayerAI.cpp:10313-11811`. Signature:

```cpp
int CvPlayerAI::AI_unitValue(UnitTypes eUnit, UnitAITypes eUnitAI,
                             const CvArea* pArea, const CvUnitSelectionCriteria* criteria) const;
```

**Eligibility gates** (return `0` = "cannot serve this role"):

| Gate | Code | Rule |
|---|---|---|
| Domain match | `10321` | `kUnitInfo.getDomainType() == AI_unitAIDomainType(eUnitAI)` (except ICBM). `AI_unitAIDomainType` maps each role to LAND/SEA/AIR/IMMOBILE (`CvPlayerAI.cpp:1956-2038`). |
| Not-AI bar | `10326` | `getNotUnitAIType(eUnitAI)` blocks it (unless `criteria->m_bIgnoreNotUnitAIs`). |
| Settler rule | `10332` | a `isFound()` (settler) unit is only valid for `UNITAI_SETTLE`. |
| Per-role `bValid` switch | `10347-10812` | role-specific requirements, e.g. `UNITAI_WORKER` needs `getNumBuilds() > 0`; `UNITAI_ATTACK` needs `getCombat() > 0 && !isOnlyDefensive()`; `UNITAI_COLLATERAL` additionally needs collateral/breakdown; `UNITAI_ICBM` needs `getNukeRange() != -1`; etc. |

**Combat normalization.** Most military scoring is driven by a normalized combat
figure from `CvGameAI::AI_combatValue` (`Sources/CvGameAI.cpp:83-114`):

```cpp
iValue = 100 * combat;                     // (air uses airCombat)
iValue *= 100 + (2*firstStrikes + chanceFS) * COMBAT_DAMAGE / 5;  iValue /= 100;
iValue /= getBestLandUnitCombat();         // as a % of the strongest land unit in the game
```

**Per-role scoring branches** (`CvPlayerAI.cpp:10838-11776`). Each role rewards
different attributes. Highlights:

| Role | What it rewards / penalizes |
|---|---|
| `UNITAI_SETTLE` | moves (`+ moves*100`) |
| `UNITAI_WORKER` | `numBuilds`, extra moves, `workRate` |
| `UNITAI_ATTACK` | combat, moves (×3 under FASTMOVERS), withdrawal, +unit/combat modifiers, overrun/knockback; **penalties** for `combatLimit < 100` and `noDefensiveBonus` |
| `UNITAI_ATTACK_CITY` | combat² term, collateral, **bombard/siege**, breakthrough, city-attack modifier, moves |
| `UNITAI_COLLATERAL` | combat + collateral + moves + withdrawal + overrun/knockback |
| `UNITAI_PILLAGE` | combat + moves + repel; *negative* properties are a plus |
| `UNITAI_RESERVE` | combat + light collateral + combat modifiers + moves |
| `UNITAI_COUNTER` | combat/2 + **anti-unit/anti-combat modifiers weighted by `AI_getUnitWeight`/`AI_getUnitCombatWeight`** + interception |
| `UNITAI_CITY_DEFENSE` | combat×1.5 + city-defense modifier + combat modifiers + pursuit/repel/unyielding; penalty if `isOnlyDefensive` |
| `UNITAI_EXPLORE` | moves² × (100+combat); ×2 if `isNoBadGoodies` |
| `UNITAI_MISSIONARY` | moves + religion/corporation spread (holy-city/HQ bonuses, area saturation) |
| sea (`ATTACK_SEA`, `ASSAULT_SEA`, `EXPLORE_SEA`, …) | combat/bombard/cargo, divided by `AI_unitImpassableCount` so units that can't reach water regions score lower |
| air (`ATTACK_AIR`, `DEFENSE_AIR`, `CARRIER_AIR`) | air combat, collateral, bomb rate, air range, interception |
| `UNITAI_ICBM` | nuke range, evasion |

Base value seeds from the unit's XML `AIWeight` (`1 + getAIWeight()`). Helper
contributions: `AI_unitHealerValue`, `AI_unitPropertyValue`,
`AI_unitImpassableCount`, and viability via `AI_calculateUnitAIViability`
(`CvPlayerAI.cpp:25499`).

**Enemy-adaptive weighting.** The counter/reserve/defense branches scale
anti-combat-class bonuses by what the AI has actually *seen* the enemy field.
`AI_doEnemyUnitData` (`CvPlayerAI.cpp:25365-25497`, called from `AI_doTurnPost`)
observes visible enemy units, decays and updates `m_aiUnitWeights` /
`m_aiUnitCombatWeights` (discounting obsolete eras), and those feed
`AI_getUnitWeight` / `AI_getUnitCombatWeight` inside the scoring branches. Net
effect: the AI biases toward units that counter the enemy composition in front of
it.

### 6.3 Related ranking wrappers

Used outside the city cascade (e.g. by the contract broker / capital selection):

| Function | File | Role |
|---|---|---|
| `AI_bestCityUnitAIValue(eUnitAI, pCity, ...)` | `CvPlayerAI.cpp:25267` | best unit + value for a role at a city (human players: filtered to default-AI units only) |
| `AI_bestAreaUnitAIValue(eUnitAI, pArea, ...)` | `CvPlayerAI.cpp:25214` | picks a city in the area, delegates to the above |
| `bestBuildableUnitForAIType(eDomain, eUnitAI, ...)` | `CvPlayerAI.cpp:26782` | picks an appropriate city (capital/coastal) and calls `AI_bestUnitAI` |

---

## 7. From order to unit (end of the pipeline)

Once a role and unit are chosen, the order carries **both**:

1. **`pushOrder(ORDER_TRAIN, eUnit, eUnitAI, ...)`** (`CvCity.cpp:15508`) stores
   the unit type and UNITAI in `OrderData` via `createUnitOrder`
   (`CvStructs.h:408-418`). A `0xFFFF` sentinel for the AI type means "use the
   unit's `getDefaultUnitAIType()`".
2. The order is advanced each turn by `doProduction` (see
   [`doProduction.md`](doProduction.md)); this doc is only about how it was
   *chosen*.
3. **`popOrder(..., bFinish=true)`** (`CvCity.cpp:15755-15822`) on completion reads
   the stored role back (`order.getUnitAIType()`), calls `AI_trained`, then
   creates the unit:

   ```cpp
   CvUnit* pUnit = owner.initUnit(eTrainUnit, getX(), getY(), eTrainAIUnit, ...);
   ```
4. **`CvPlayer::initUnit`** (`CvPlayer.cpp:2988-3001`) — if the role is
   `NO_UNITAI`, it falls back to `getUnitInfo(eUnit).getDefaultUnitAIType()`.
5. **`CvUnit::init`** (`CvUnit.cpp:323-402`, line 341) calls
   **`CvUnitAI::AI_init`** (`CvUnitAI.cpp:149-159`), which calls `AI_reset` —
   `m_eUnitAIType = eUnitAI` (`CvUnitAI.cpp:175`) — and bumps the player's
   per-role counter (`AI_changeNumAIUnits(role, +1)`). The role is now permanent
   instance state.

**Runtime reassignment.** A unit's role can change later via
`AI_setUnitAIType` (`CvUnitAI.cpp:1568-1602`), which decrements the old role's
area/player counters, sets the new role, increments the new counters, and
re-evaluates group membership (`joinGroup(NULL)`). `AI_getUnitAIType`
(`CvUnitAI.cpp:1568`) self-heals a stray `NO_UNITAI` back to the unit's default.

---

## 8. Worked examples

### 8.1 Explicit path — a settler

1. Cascade branch (~#13) fires: room to expand, a site exists, not losing a war.
2. `AI_chooseUnit("settler needed", UNITAI_SETTLE)` →
   `AI_chooseUnitImmediate(UNITAI_SETTLE)` (the role is fixed, no `AI_bestUnit`).
3. `AI_bestUnitAI(UNITAI_SETTLE)` scans units; `AI_unitValue(..., UNITAI_SETTLE)`
   passes only `isFound()` settlers and rewards moves → picks `UNIT_SETTLER`.
4. `pushOrder(ORDER_TRAIN, UNIT_SETTLER, UNITAI_SETTLE)`.
5. On completion: `popOrder` → `initUnit` → `AI_init` → `m_eUnitAIType =
   UNITAI_SETTLE`.

### 8.2 Implicit path — "build the best military unit"

1. A military branch calls `AI_chooseUnit(reason, NO_UNITAI, ...)`.
2. `AI_chooseUnitImmediate` sees `NO_UNITAI` → `AI_bestUnit()`.
3. `AI_bestUnit` weights every role from the empire's situation (area AI type,
   military weight, existing counts), applies multipliers + leader skew + jitter,
   and for each viable role calls `AI_bestUnitAI` → `AI_unitValue`. Say
   `UNITAI_ATTACK` + `UNIT_AXEMAN` scores highest.
4. The chosen role comes back via `peBestUnitAI`; `pushOrder(ORDER_TRAIN,
   UNIT_AXEMAN, UNITAI_ATTACK)`.
5. Completion records `m_eUnitAIType = UNITAI_ATTACK` on the new Axeman.

---

## 9. Caching, performance & gotchas

- **`m_bestUnits`** (`CvCityAI`) memoizes `AI_bestUnitAI` per role + criteria;
  invalidated when growth/criteria change. **`BuildingValueCache`** does the same
  for buildings. Both keep the cascade cheap within a turn.
- **`m_aiUnitWeights` / `m_aiUnitCombatWeights`** are refreshed once per turn in
  `AI_doTurnPost` via `AI_doEnemyUnitData`; everything reading them sees a stable
  snapshot during production.
- **`AI_unitValue` is profiled** (`PROFILE_FUNC`) and is hot — it is called for
  every unit × every viable role during a scan; the cache and the `bValid` early
  exits exist to keep it affordable.
- **Force composition self-balances** because role weights subtract existing unit
  counts (§4.2 step 2) — land/sea per area, air globally.
- **Humans bypass most of this.** `AI_bestCityUnitAIValue` only considers units
  whose *default* UNITAI matches for human players, and the cascade is an AI-only
  decision path; a human's manual builds still get a UNITAI assigned at
  `initUnit` (default-AI fallback) but skip the strategic weighting.
- **Tendering can move the build.** With `USE_UNIT_TENDERING`, the city that
  *decides* it needs a unit is not always the city that *builds* it — the
  contract broker may place it elsewhere (§5).

---

## 10. Reference: file / function map

| Concern | File | Key symbols (≈ line) |
|---|---|---|
| Role enum & domain macros | `Sources/CvEnums.h` | `UnitAITypes` (1447), `IS_NAVAL_AITYPE`/`IS_AIR_AITYPE` (1512) |
| Per-unit role data | `Sources/Infos/CvUnitInfo.h/.cpp` | `m_iDefaultUnitAIType` (h:718), `m_pbUnitAIType`/`m_pbNotUnitAIType` (h:824), accessors (cpp:799,1358), XML load (cpp:4068,4131) |
| Production cascade | `Sources/CvCityAI.cpp` | `AI_chooseProduction` (776) |
| Role weighting | `Sources/CvCityAI.cpp` | `AI_bestUnit` (3846) |
| Choose-unit glue | `Sources/CvCityAI.cpp` | `AI_chooseUnit` (8748), `AI_chooseUnitImmediate` (8825) |
| Concrete-unit ranking | `Sources/CvCityAI.cpp` | `AI_bestUnitAI` (4141) |
| Core unit scorer | `Sources/CvPlayerAI.cpp` | `AI_unitValue` (10313), `AI_unitAIDomainType` (1956), enemy data `AI_doEnemyUnitData` (25365), wrappers (25214/25267/26782) |
| Combat normalization | `Sources/CvGameAI.cpp` | `AI_combatValue` (83) |
| Order storage | `Sources/CvCity.cpp` / `Sources/CvStructs.h` | `pushOrder` (cpp:15508), `popOrder` (cpp:15755), `createUnitOrder` (h:408) |
| Unit creation & role assignment | `Sources/CvPlayer.cpp` / `Sources/CvUnit.cpp` / `Sources/CvUnitAI.cpp` | `initUnit` (player:2988), `init` (unit:323), `AI_init`/`AI_reset` (unitAI:149/167), `AI_setUnitAIType`/`AI_getUnitAIType` (unitAI:1568) |

---

*Companion docs:* [`doProduction.md`](doProduction.md) (advancing/completing an
order), [`CvCityAI`](CvCityAI.md),
[`CvPlayerAI`](CvPlayerAI.md),
[`CvUnitAI`](CvUnitAI.md).
