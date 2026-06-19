# Observability map: Unit Upkeep, Supply & Food-for-Units

> DRAFT observability map (2026-06-18, parent cascade-mapping-inventory sweep) — all
> mechanics claims cited from live source; verify before relying.
> Tier assessment: **1 (Telescreen)** — the aggregate net-gold number (`goldRate`) is
> exposed; every upkeep/supply component that feeds it is invisible.

**Cross-reference:** The aggregated expense side (how upkeep + supply fold into
`getFinalExpense` → `calculateGoldRate`) is fully covered in
[`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) §1-F and §1-G.  This
map covers the *per-unit mechanics*: how individual units accumulate their cost, the
free-allowance system, supply (outside-territory), food-for-units, and the AI
decision paths that depend on these values.

---

## 1. How it actually works

### 1-A. Per-unit upkeep accumulator — `CvUnit::calcUpkeep100` (CvUnit.cpp:15797)

Every non-NPC unit carries three fields:

| Field | Meaning |
|---|---|
| `m_iUpkeep100` | Current upkeep cost × 100 (the live per-unit value) |
| `m_iUpkeepModifier` | Additive % modifier from unit-combat type + promotions |
| `m_iUpkeepMultiplierSM` | Size-Matters rank multiplier (×1.5 per rank, compounding) |
| `m_iExtraUpkeep100` | Extra flat upkeep from unit-combat + promotion effects |

`calcUpkeep100()` (CvUnit.cpp:15797):
```
iCalc = 100 × UnitInfo.getBaseUpkeep() + m_iExtraUpkeep100
if iCalc > 0:
    iCalc = getModifiedIntValue(iCalc, m_iUpkeepModifier)     // unit-combat/promo modifier
    iCalc = getModifiedIntValue(iCalc, m_iUpkeepMultiplierSM) // SM rank multiplier
iOldUpkeep = m_iUpkeep100
m_iUpkeep100 = max(0, iCalc)
if changed: GET_PLAYER(owner).changeUnitUpkeep(m_iUpkeep100 - iOldUpkeep, isMilitaryBranch())
```

Triggers: unit creation, promotion gain/loss, unit-combat type change, Size-Matters
rank change (`calcUpkeepMultiplierSM`).

**Death/removal site** (CvUnit.cpp:1512):
```
owner.changeUnitUpkeep(-getUpkeep100(), isMilitaryBranch())
```
Called once during the unit-kill path, before `changeNumMilitaryUnits`.

**`isMilitaryBranch()`** (CvUnit.cpp:11053) = `UnitInfo.isMilitarySupport()` — the XML
flag that classifies units into the military vs. civilian upkeep bucket.

### 1-B. Player-level upkeep accumulators — `CvPlayer::changeUnitUpkeep` (CvPlayer.cpp:10254)

Two running 100× sums kept on `CvPlayer`:

| Member | Accessor | What it holds |
|---|---|---|
| `m_iUnitUpkeepCivilian100` | `getUnitUpkeepCivilian100()` | Sum of `m_iUpkeep100` for all non-military units |
| `m_iUnitUpkeepMilitary100` | `getUnitUpkeepMilitary100()` | Sum of `m_iUpkeep100` for all military units |

Every `changeUnitUpkeep` call marks the cached `m_iFinalUnitUpkeep` dirty.

### 1-C. Upkeep modifiers and free allowances (CvPlayer.cpp:10160-10326)

Before the dirty-cache is recomputed, modifiers and free quotas are applied:

**Modifiers** (additive % mod on gross civilian / military sums):
- `m_iCivilianUnitUpkeepMod` — buildings that reduce civilian upkeep; applied as
  `× (100 + mod) / 100` when positive, `× 100 / (100 - mod)` when negative
  (`getUnitUpkeepCivilian`, CvPlayer.cpp:10278)
- `m_iMilitaryUnitUpkeepMod` — same pattern for military upkeep
  (`getUnitUpkeepMilitary`, CvPlayer.cpp:10298)

**Free allowances** (subtract from gross AFTER modifiers; floor at 0 for net):
```
getFreeUnitUpkeepCivilian()  = max(0, getBaseFreeUnitUpkeepCivilian()
                                + getModifiedIntValue(getTotalPopulation(),
                                                      getFreeUnitUpkeepCivilianPopPercent()))
getFreeUnitUpkeepMilitary()  = max(0, getBaseFreeUnitUpkeepMilitary()
                                + getModifiedIntValue(getTotalPopulation(),
                                                      getFreeUnitUpkeepMilitaryPopPercent()))
```

Sources for the base free values (CvPlayer.cpp:388-390):
- `BASE_FREE_UNITS_UPKEEP_CIVILIAN` = **0** (GlobalDefines.xml:597)
- `BASE_FREE_UNITS_UPKEEP_MILITARY` = **0** (GlobalDefines.xml:601)
- `BASE_FREE_UNITS_UPKEEP_CIVILIAN_PER_100_POP` = **0** (GlobalDefines.xml:605)
- `BASE_FREE_UNITS_UPKEEP_MILITARY_PER_100_POP` = **0** (GlobalDefines.xml:609)

All base values are **currently 0** — free allowances come entirely from traits,
buildings, and civics that call `changeBaseFreeUnitUpkeepMilitary` /
`changeFreeUnitUpkeepMilitaryPopPercent` etc.

**Net amounts** (input to `calcFinalUnitUpkeep`):
```
getUnitUpkeepCivilianNet() = max(0, getUnitUpkeepCivilian() − getFreeUnitUpkeepCivilian())
getUnitUpkeepMilitaryNet() = max(0, getUnitUpkeepMilitary() − getFreeUnitUpkeepMilitary())
```

### 1-D. Final upkeep with handicap scaling — `calcFinalUnitUpkeep` (CvPlayer.cpp:10332)

```
iCalc = getUnitUpkeepCivilianNet() + getUnitUpkeepMilitaryNet()
if iCalc > 0:
    iCalc × handicap.getUnitUpkeepPercent() / 100     // per-player difficulty modifier
    if !isHumanPlayer():                               // AI additional scaling
        iCalc × handicap.getAIUnitUpkeepPercent() / 100
        iCalc × max(0, 100 + handicap.getAIPerEraModifier() × era) / 100
return max(0, iCalc)
```

Returns 0 for NPCs (`isNPC()`).  Cached as `m_iFinalUnitUpkeep`; dirty-flag
`m_bUnitUpkeepDirty` is set on every `changeUnitUpkeep` call.

The `getFinalUnitUpkeepChange(iExtra, bMilitary)` (CvPlayer.cpp:10388) helper
temporarily mutates the accumulators and re-runs `calcFinalUnitUpkeep` to get a
marginal cost — used by AI unit-training valuation without permanently dirtying state.

### 1-E. Supply (outside-territory units) — `calculateUnitSupply` (CvPlayer.cpp:7909)

A separate cost channel for units operating outside the player's own territory:

```
getNumOutsideUnits()  // m_iNumOutsideUnits: incremented on move to non-owned/non-vassal
                      //    tile (CvUnit.cpp:13879); decremented on move away (CvUnit.cpp:13910)

paidUnits = max(0, getNumOutsideUnits() − INITIAL_FREE_OUTSIDE_UNITS)
                                                // INITIAL_FREE_OUTSIDE_UNITS = 0
baseCost  = paidUnits × INITIAL_OUTSIDE_UNIT_GOLD_PERCENT / 100 × (era + 1)
                                                // INITIAL_OUTSIDE_UNIT_GOLD_PERCENT = 75
iMod      = getDistantUnitSupportCostModifier()  // from civics (CvPlayer.cpp:18117)
if iMod != 0: baseCost modified
if isNormalAI():
    iMod += handicap.AIUnitSupplyPercent − 100 + handicap.AIPerEraModifier × era
if iMod != 0: supply = getModifiedIntValue(supply, iMod)
```

Returns 0 during anarchy or for NPCs.  Era multiplier means supply costs grow
progressively: the same count of foreign-territory units costs more in later eras.

`getNumOutsideUnits()` counts any unit on a plot whose team does not own it AND is not
a vassal of the player's team (checked on every `setXY` call in CvUnit.cpp:13877-13910).

### 1-F. Food-for-units — `CvCity::isFoodProduction` (CvCity.cpp:3487)

Not a per-turn supply cost — this is about how units are *trained* (production
converted from food rather than hammers). Per city:

```
bool CvCity::isFoodProduction(UnitTypes eUnit):
    return GC.getUnitInfo(eUnit).isFoodProduction()     // unit XML flag
        || (GET_PLAYER(owner).isMilitaryFoodProduction() // player has trait/civic
            && GC.getUnitInfo(eUnit).isMilitaryProduction())  // unit XML flag
```

`isMilitaryFoodProduction()` on the player (CvPlayer.cpp:10455) = true when
`getMilitaryFoodProductionCount() > 0` — a counter bumped by:
- civic `isMilitaryFoodProduction()` (CvPlayer.cpp:18208)
- leader trait `isMilitaryFoodProduction()` (CvPlayer.cpp:28536)

When `isFoodProduction(eUnit)` is true for the current production head, the city's
`doProduction` pathway subtracts from the food store instead of the hammer
stockpile to complete the unit.  This is a build-mode classification only — it does
not create any ongoing per-turn food cost.  The city's food yield and food rate are
not modified by units once trained.

### 1-G. Per-turn call order in `CvPlayer::doTurn` (CvPlayer.cpp:3683)

Upkeep and supply are charged implicitly through `doGold` → `calculateGoldRate`:

```
~3807  verifyGoldCommercePercent()    // auto-raise gold slider if deficit
~3809  doGold():
           gold += calculateGoldRate()    // = income − getFinalExpense()
           if gold < 0:
               setStrike(true); changeStrikeTurns(+1)
               if strikeTurns > 1:
                   disband floor(strikeTurns/2) units    // forced disbanding
```

There is no separate "charge upkeep" step — it is rolled into the single `changeGold`
call via `calculateGoldRate` → `calculateBaseNetGold` → `getFinalExpense`.

### 1-H. AI disband-on-financial-trouble — `AI_doTurnPre` / `AI_fundingHealth` (CvPlayerAI.cpp:16459)

The AI proactively disbands units when financially stretched, independently of the
forced-disbanding mechanism above:

```
AI_fundingHealth < AI_safeFunding()  ⟹  AI_isFinancialTrouble() = true
```

`AI_safeFunding()` (CvPlayerAI.cpp:3774) — a per-player safe-margin percent (default
`SAFE_PROFIT_MARGIN_BASE_PERCENT`) adjusted for rank, war count, repeat research, etc.

`AI_fundingHealth()` (CvPlayerAI.cpp:3830) — expressed as 0-100+ percent:
- Returns 100 for anarchy/NPC
- Returns 10000 if min-tax income covers full expenses (no tax needed)
- Returns 200 if `profitMargin > 25`
- Otherwise computes from treasury prognosis vs `AI_goldTarget()`, or falls back to
  `profitMargin × 2`

When in financial trouble, the AI iterates four passes (by experience threshold 1/6/12/-1)
calling `AI_disbandUnit` while `getUnitUpkeepMilitaryNet() > 0` and income < expenses
(CvPlayerAI.cpp:16467).

`[CIT/begin]` (CityAI.cpp:966) logs `finTrouble=1` when the city's production decision
runs under financial trouble — this is the **only existing log exposure** of the
financial-trouble state.

---

## 2. Current observability — tier and surface

**Tier: 1 (Telescreen).** `goldRate` (the aggregate net result) is in the `/players`
snapshot.  Every per-unit and per-player component feeding it is opaque from outside.

### 2-A. What IS already exposed

| Endpoint / log | Field | What it tells you | Limitation |
|---|---|---|---|
| `/players` | `gold` | Treasury balance | Not a cost breakdown |
| `/players` | `goldRate` | Net gold per turn (income − ALL expenses) | Aggregate only; upkeep not isolated |
| `/players` | `units` | Total unit count | No cost per unit, no civilian/military split |
| `/events` `log` | `[CIT/begin] finTrouble=` | Boolean financial-trouble flag per city production decision | Only fires during AI production choice; boolean only |
| `/units` | `type`, `unitAI`, `damage`, `level` | Unit identity and state | No upkeep field; no military-branch flag |

### 2-B. What is NOT exposed (the gap)

The following unit upkeep and supply values cannot be reconstructed from outside today:

**Per-unit:**
- `getUpkeep100()` — the current upkeep cost of each unit (×100) — not in `/units`
- `isMilitaryBranch()` — whether a unit is in the military or civilian bucket — not in `/units`
- `m_iUpkeepModifier` / `m_iUpkeepMultiplierSM` — modifier and SM-rank multiplier on
  this unit's cost — not exposed

**Per-player aggregates (none in any endpoint):**
- `getUnitUpkeepCivilian100()` — raw gross civilian upkeep × 100
- `getUnitUpkeepMilitary100()` — raw gross military upkeep × 100
- `getUnitUpkeepCivilian()` — gross after civilian modifier
- `getUnitUpkeepMilitary()` — gross after military modifier
- `getFreeUnitUpkeepCivilian()` / `getFreeUnitUpkeepMilitary()` — free quota (base + pop-percent)
- `getBaseFreeUnitUpkeepCivilian()` / `getBaseFreeUnitUpkeepMilitary()` — base free (from traits/buildings)
- `getFreeUnitUpkeepCivilianPopPercent()` / `getFreeUnitUpkeepMilitaryPopPercent()` — pop-scaled free
- `getCivilianUnitUpkeepMod()` / `getMilitaryUnitUpkeepMod()` — cost-reduction modifiers
- `getUnitUpkeepCivilianNet()` / `getUnitUpkeepMilitaryNet()` — net amounts after free quota
- `getFinalUnitUpkeep()` — the number that actually goes into `calculatePreInflatedCosts`

**Supply:**
- `getNumOutsideUnits()` — count of units on non-own-territory — not in any endpoint
- `calculateUnitSupply()` — the resulting supply cost — not in any endpoint
- `getDistantUnitSupportCostModifier()` — civic/building modifier on supply cost — not exposed

**Food-for-units:**
- `isMilitaryFoodProduction()` — player-level flag from trait/civic — not in `/players`
- No event fires when a unit is trained via food rather than hammers

**AI financial state:**
- `AI_fundingHealth()` — the 0-10000 funding-health score — never logged
- `AI_safeFunding()` — the per-player safe-margin threshold — never logged
- `AI_isFinancialTrouble()` — the boolean gate used by virtually every AI production/
  tech/diplomacy decision — only exposed as `finTrouble=` in `[CIT/begin]` (boolean, city-scope)
- `getProfitMargin()` — the 0-100 profit-margin percent used by financial health — not logged
- Disband-loop iterations (CvPlayerAI.cpp:16467) — units silently removed; no log line

**Dead code note:** `calculateUnitCost(int& iFreeUnits, ...)` (CvPlayer.h:350) is declared
but has **no implementation and no callers** — it is dead BTS-era code.  Do not attempt to
use it.

---

## 3. The gap — what cannot be reconstructed from outside today

An agent monitoring HTTP + `/events` + gated logs today **cannot**:

1. **Know the per-unit cost of any unit.** A unit at level 5 with SM-rank upkeep multiplier
   and a promotion modifier has a materially different cost than a freshly trained unit of
   the same type; neither cost is in `/units`.

2. **Know the civilian/military split.** Whether an AI is paying mostly civilian upkeep (many
   workers, settlers, spies) vs. military upkeep (army) is invisible — the split drives the
   AI's disband logic (`getUnitUpkeepMilitaryNet() > 0` guard).

3. **Know the free-allowance state.** If traits/civics grant a large free military upkeep
   quota, the player can field many units at no net cost; this cannot be seen from outside.

4. **Observe supply pressure.** `getNumOutsideUnits()` and the supply cost are not in any
   endpoint; an agent watching a player wage a foreign war has no way to quantify the
   associated supply drag.

5. **Watch the AI disband loop.** When the AI sheds units for financial reasons at
   `AI_doTurnPre`, no log line fires. The only observable artifact is a count drop in
   `/players.units` (≤5s stale) — indistinguishable from combat attrition.

6. **Determine the AI financial health score.** `AI_fundingHealth()` is the primary gate for
   a huge fraction of AI decisions (production, civics, tech weighting, diplomacy gold,
   unit training). Its numeric value — and whether it is above or below `AI_safeFunding()`
   — is never logged.

7. **Confirm food-for-units is active.** Whether a player's current trait/civic enables
   `isMilitaryFoodProduction()` (which silently reroutes unit training from hammers to food)
   is not exposed.

Consequence for the Orwell bar: the unit-upkeep and supply system is a significant
driver of AI behaviour (disband decisions, production gating, financial-trouble diagnosis)
that is completely opaque. Any cascade replacement of upkeep-related `requires.operate`
conditions (e.g. a building that requires "military upkeep below X") cannot be verified
without these surfaces.

---

## 4. Proposed hooks — concrete additions to reach Tier 3-4

All hooks follow established patterns (`gPlayerLogLevel` gate, `streamLogTee` for `/events`,
new fields in `PlayerSnap` / `UnitSnap` in `CvHttpServer.cpp`).

### 4-A. `/units` snapshot — add per-unit upkeep fields

Add to `UnitSnap` (`CvHttpServer.cpp:46`):
```cpp
int iUpkeep100;        // CvUnit::getUpkeep100()   — upkeep × 100 for precision
int iMilitary;         // CvUnit::isMilitaryBranch() ? 1 : 0
```

Populated in the unit loop of `publishIfDue` alongside the existing `snap.iDamage` etc.
These are `O(1)` reads per unit. Gives full per-unit cost reconstruction.

### 4-B. `/players` snapshot — add upkeep/supply breakdown fields

Add to `PlayerSnap` (`CvHttpServer.cpp:61`):
```cpp
int64_t iUnitUpkeepCivilian100;         // getUnitUpkeepCivilian100()
int64_t iUnitUpkeepMilitary100;         // getUnitUpkeepMilitary100()
int iCivilianUnitUpkeepMod;             // getCivilianUnitUpkeepMod()
int iMilitaryUnitUpkeepMod;             // getMilitaryUnitUpkeepMod()
int iFreeUnitUpkeepCivilian;            // getFreeUnitUpkeepCivilian()
int iFreeUnitUpkeepMilitary;            // getFreeUnitUpkeepMilitary()
int64_t iUnitUpkeepCivilianNet;         // getUnitUpkeepCivilianNet()
int64_t iUnitUpkeepMilitaryNet;         // getUnitUpkeepMilitaryNet()
int64_t iFinalUnitUpkeep;               // getFinalUnitUpkeep()
int iNumOutsideUnits;                   // getNumOutsideUnits()
int iUnitSupply;                        // calculateUnitSupply()
int iDistantUnitSupportCostModifier;    // getDistantUnitSupportCostModifier()
int iMilitaryFoodProduction;            // isMilitaryFoodProduction() ? 1 : 0
```

These are all `O(1)` reads or cheap dirty-cache reads; no search required.  With these
fields an agent can fully reconstruct the upkeep/supply split in `getFinalExpense`.

### 4-C. `/players` snapshot — AI financial health fields

For non-NPC players, also add:
```cpp
int iAIFundingHealth;    // isNormalAI() ? AI_fundingHealth() : -1
int iAISafeFunding;      // isNormalAI() ? AI_safeFunding()   : -1
int iProfitMargin;       // getProfitMargin()
int iIsFinancialTrouble; // AI_isFinancialTrouble() ? 1 : 0
int iIsStrike;           // isStrike() ? 1 : 0
int iStrikeTurns;        // getStrikeTurns()
```

`AI_fundingHealth()` is not trivially cheap (it calls `getProfitMargin` which calls
`getFinalExpense`), but it is called from the snapshot publish once per player and
`publishIfDue` runs on the game thread — safe. Guard: call only for `isNormalAI()`.

### 4-D. Log tag `[UPK]` — upkeep events (new tag, `gPlayerLogLevel` gate)

Register a new `[UPK]` (upkeep) tag in `BetterBTSAI.{h,cpp}`, log file `UpkeepAI.log`,
scope global `gPlayerLogLevel`:

**Unit creation / promotion change (CvUnit.cpp:15797, at `calcUpkeep100` when value changes):**
```
[UPK/unit] turn=N player=P unit=U type=UNIT_X base=BB extraUpkeep100=EE
           modifier=MM smMultiplier=SS upkeep100=UU military=1|0
```
Level 2 (fires per unit per modifier change; potentially high volume on mass promotions).
Use `if (gPlayerLogLevel >= 2)` guard around the format.

**Unit removal (CvUnit.cpp:1512, at `changeUnitUpkeep(-getUpkeep100(), ...)`)**:
```
[UPK/remove] turn=N player=P unit=U type=UNIT_X upkeep100=UU military=1|0
```
Level 2.

**Financial trouble transition (CvPlayerAI.cpp:3923, at `AI_isFinancialTrouble()` FIRST
time it transitions — add a player-level dirty-flag for the transition):**
```
[UPK/trouble] turn=N player=P fundingHealth=FF safeFunding=SS profitMargin=MM
              civilianNet=CC militaryNet=MM supply=SS
```
Level 1 (state transition event only, not every call).

**AI disband step (CvPlayerAI.cpp:16477, after `AI_disbandUnit` returns true):**
```
[UPK/disband] turn=N player=P pass=P militaryNet=MM profitMargin=MM
```
Level 1.

**Strike entry (CvPlayer.cpp:15487, inside `doGold` when strike begins):**
```
[UPK/strike] turn=N player=P strikeTurns=S goldRate=GG
```
Level 1.

**Per-turn headline summary (once per player per turn, gated at level 1, in `doGold` or as
a new `doUpkeepSummary` call from `doTurn`):**
```
[UPK/turn] turn=N player=P civilian100=CC military100=MM freeCiv=FC freeMil=FM
           finalUpkeep=FU supply=SU outside=OO
```
Level 1 — feeds `/events` stream at `gStreamLogLevel ≥ 1`.

### 4-E. No new diagnostic endpoint needed

Unit upkeep is a pure accumulator (no gate function to shadow-test). The `/players`
snapshot additions (§4-B) give the complete state snapshot; the `[UPK/turn]` per-turn
stream gives the time series.

---

## 5. Tier assessment

| Tier | Description | Met after proposed hooks? |
|---|---|---|
| 1 | Coarse snapshots | Yes (current) — but only aggregate `goldRate` |
| 2 | + buildability shadows | Orthogonal to upkeep |
| 3 | + live event stream + per-turn state | With `[UPK/turn]` + `[UPK/trouble]` + `[UPK/disband]` (§4-D): upkeep/supply changes are live in `/events`; AI financial-trouble transitions are visible |
| 4 | + full state reconstructible for all players | With §4-B + §4-C snapshot fields + §4-D events: full upkeep split (civilian/military/supply) reconstructible per player/AI, per turn |
| 5 | Total — opaque systems included | Food-for-units training mode (§4-B `isMilitaryFoodProduction`) would also be covered; nothing deeper is hidden once §4-A through §4-D land |

**Current tier: 1.**  With §4-B player snapshot fields and `[UPK/turn]` + `[UPK/trouble]`
events, the system reaches **Tier 3** immediately. Full §4-A through §4-D reaches
**Tier 4/5** for this system.

---

## 6. Priority — HIGH

Unit upkeep drives AI disband decisions, AI production gating, AI financial-trouble
diagnosis, and handicap-difficulty scaling — all of which are invisible from outside.
The AI financial-trouble state (`AI_isFinancialTrouble()`) gates decisions in at least
fifteen call-sites across `CvCityAI.cpp`, `CvPlayerAI.cpp`, and `CvUnitAI.cpp`; it is
the single most impactful invisible AI-state variable after individual unit evaluation
scores.

Minimum viable for cascade verification: add `iFinalUnitUpkeep`, `iUnitSupply`,
`iNumOutsideUnits`, `iIsFinancialTrouble`, and `iIsStrike` to `PlayerSnap` (§4-B + §4-C),
and add the `[UPK/turn]` per-turn headline (§4-D). Those additions alone let an agent
track whether the cascade's modelled upkeep/supply agrees with the engine's computed
values, player by player, turn by turn, for both human and AI players.
