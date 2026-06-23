# Observability map: Unit Upkeep, Supply & Food-for-Units — the per-unit cost side

> **Status:** reference · **Verified against:** live `Sources/Engine/CvUnit.cpp` / `CvPlayer.cpp`,
> `Sources/AI/CvPlayerAI.cpp`, `Sources/Tools/CvHttpServer.cpp` — 2026-06-20.
> **Grounding:** citations confirmed against the named functions in the live source (line numbers had
> already drifted in the old draft, e.g. `calcUpkeep100` 15797→15798) — confirm the function, not the
> integer.
>
> **BLUF:** every per-unit upkeep cost, the civilian/military split, the free-allowance system, supply
> (outside-territory units), food-for-units training, and the AI financial-trouble gates all fold into the
> single opaque `goldRate` on `/players`. That is **Tier 1** ([Telescreen](README.md)). This map covers the
> *per-unit mechanics* and the AI decision paths that consume them; how upkeep + supply aggregate into
> `getFinalExpense` is in [`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) §1-F / §1-G.

The observability scale (0–5), the Orwell reconstruction bar, and the three canonical hook shapes are
defined once in [`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]). The live-read rules
live in [`http-server.md`](http-server.md).

---

## 1. How it works

### 1-A. Per-unit accumulator — `CvUnit::calcUpkeep100` (`Sources/Engine/CvUnit.cpp`, ~15798)
Every non-NPC unit carries:

| Field | Meaning |
|---|---|
| `m_iUpkeep100` | Current upkeep × 100 (the live per-unit value) |
| `m_iUpkeepModifier` | Additive % modifier from unit-combat type + promotions |
| `m_iUpkeepMultiplierSM` | Size-Matters rank multiplier (×1.5/rank, compounding) |
| `m_iExtraUpkeep100` | Flat extra upkeep from unit-combat + promotion effects |

```
iCalc = 100 × UnitInfo.getBaseUpkeep() + m_iExtraUpkeep100
if iCalc > 0:
    iCalc = getModifiedIntValue(iCalc, m_iUpkeepModifier)      // unit-combat / promo modifier
    iCalc = getModifiedIntValue(iCalc, m_iUpkeepMultiplierSM)  // SM rank multiplier
m_iUpkeep100 = max(0, iCalc)
if changed: GET_PLAYER(owner).changeUnitUpkeep(m_iUpkeep100 − iOldUpkeep, isMilitaryBranch())
```
Triggers: creation, promotion gain/loss, unit-combat type change, SM rank change.

**Death/removal** (`CvUnit.cpp`, ~1513): `owner.changeUnitUpkeep(−getUpkeep100(), isMilitaryBranch())`,
before `changeNumMilitaryUnits`.

**`isMilitaryBranch()`** (`CvUnit.cpp`, ~11054) = `UnitInfo.isMilitarySupport()` — the XML flag that buckets
a unit into military vs civilian upkeep.

### 1-B. Player accumulators — `CvPlayer::changeUnitUpkeep` (`Sources/Engine/CvPlayer.cpp`, ~10254)
Two running 100× sums on `CvPlayer`:

| Member | Accessor | Holds |
|---|---|---|
| `m_iUnitUpkeepCivilian100` | `getUnitUpkeepCivilian100()` | Sum of `m_iUpkeep100` over non-military units |
| `m_iUnitUpkeepMilitary100` | `getUnitUpkeepMilitary100()` | Sum of `m_iUpkeep100` over military units |

Every call marks the cached `m_iFinalUnitUpkeep` dirty (`m_bUnitUpkeepDirty`).

### 1-C. Modifiers + free allowances (`CvPlayer.cpp`, ~10160–10326)
**Modifiers** (additive % on the gross civilian/military sums): `m_iCivilianUnitUpkeepMod`
(`getUnitUpkeepCivilian`, ~10278) and `m_iMilitaryUnitUpkeepMod` (`getUnitUpkeepMilitary`, ~10298) —
applied `× (100+mod)/100` when positive, `× 100/(100−mod)` when negative.

**Free allowances** (subtracted AFTER modifiers; floor at 0):
```
getFreeUnitUpkeepCivilian() = max(0, getBaseFreeUnitUpkeepCivilian()
                               + getModifiedIntValue(getTotalPopulation(), getFreeUnitUpkeepCivilianPopPercent()))
getFreeUnitUpkeepMilitary() = max(0, getBaseFreeUnitUpkeepMilitary()
                               + getModifiedIntValue(getTotalPopulation(), getFreeUnitUpkeepMilitaryPopPercent()))
```
All four base globals are **currently 0** (`BASE_FREE_UNITS_UPKEEP_{CIVILIAN,MILITARY}` and the
`_PER_100_POP` pair, GlobalDefines.xml) — free quotas come entirely from traits/buildings/civics that call
`changeBaseFreeUnitUpkeep*` / `changeFreeUnitUpkeep*PopPercent`.

**Net** (input to `calcFinalUnitUpkeep`): `getUnitUpkeep{Civilian,Military}Net() =
max(0, gross_after_modifier − free)`.

### 1-D. Final upkeep + handicap — `calcFinalUnitUpkeep` (`CvPlayer.cpp`, ~10332)
```
iCalc = getUnitUpkeepCivilianNet() + getUnitUpkeepMilitaryNet()
if iCalc > 0:
    iCalc × handicap.getUnitUpkeepPercent() / 100
    if !isHumanPlayer():
        iCalc × handicap.getAIUnitUpkeepPercent() / 100
        iCalc × max(0, 100 + handicap.getAIPerEraModifier() × era) / 100
return max(0, iCalc)        // 0 for NPCs
```
Cached as `m_iFinalUnitUpkeep`. `getFinalUnitUpkeepChange(iExtra, bMilitary)` (~10397) temporarily mutates
the accumulators and re-runs the calc for a *marginal* cost — used by AI unit-training valuation without
permanently dirtying state.

### 1-E. Supply (outside-territory) — `calculateUnitSupply` (`CvPlayer.cpp`, ~7899/7911)
```
paidUnits = max(0, getNumOutsideUnits() − INITIAL_FREE_OUTSIDE_UNITS)   // INITIAL_FREE_OUTSIDE_UNITS = 0
baseCost  = paidUnits × INITIAL_OUTSIDE_UNIT_GOLD_PERCENT / 100 × (era + 1)  // PERCENT = 75
iMod      = getDistantUnitSupportCostModifier()    // from civics
if isNormalAI(): iMod += handicap.AIUnitSupplyPercent − 100 + handicap.AIPerEraModifier × era
if iMod != 0: supply = getModifiedIntValue(baseCost, iMod)
```
Returns 0 during anarchy or for NPCs. The era multiplier makes the same count of foreign-territory units
cost more in later eras. `getNumOutsideUnits()` (`m_iNumOutsideUnits`) counts any unit on a plot whose team
neither owns nor vassals — incremented/decremented on every `setXY` (`CvUnit.cpp`, ~13877–13910).

### 1-F. Food-for-units — `CvCity::isFoodProduction` (`Sources/Engine/CvCity.cpp`, ~3487)
A *training-mode* classification, **not** a per-turn cost:
```
isFoodProduction(eUnit) = UnitInfo.isFoodProduction()
                        || (player.isMilitaryFoodProduction() && UnitInfo.isMilitaryProduction())
```
`isMilitaryFoodProduction()` (`CvPlayer.cpp`, ~10455) is true when `getMilitaryFoodProductionCount() > 0`,
bumped by a civic or leader-trait `isMilitaryFoodProduction()`. When true for the production head, the
city's `doProduction` subtracts from the food store instead of hammers to complete the unit. It does **not**
create any ongoing per-turn food cost — once trained, the unit doesn't alter food yield/rate.

### 1-G. Per-turn call order (`CvPlayer::doTurn`, `CvPlayer.cpp`, ~3683)
Upkeep + supply are charged implicitly through `doGold` → `calculateGoldRate` → `calculateBaseNetGold` →
`getFinalExpense`. There is **no** separate "charge upkeep" step — it rolls into the single `changeGold`
call. (`verifyGoldCommercePercent` runs first; `doGold` triggers strike + forced disbanding when gold < 0.)

### 1-H. AI disband-on-trouble — `AI_doTurnPre` / `AI_fundingHealth` (`Sources/AI/CvPlayerAI.cpp`, ~16459)
The AI proactively disbands when stretched, independent of forced disbanding:
```
AI_fundingHealth() < AI_safeFunding()  ⟹  AI_isFinancialTrouble() = true
```
- `AI_safeFunding()` (~3774) — per-player safe-margin percent (default `SAFE_PROFIT_MARGIN_BASE_PERCENT`),
  adjusted for rank, war count, repeat research.
- `AI_fundingHealth()` (~3830) — 0-100+ percent: 100 for anarchy/NPC; 10000 if min-tax covers expenses;
  200 if `profitMargin > 25`; else from treasury prognosis vs `AI_goldTarget()` (fallback `profitMargin × 2`).
- In trouble, the AI iterates four passes (experience thresholds 1/6/12/−1) calling `AI_disbandUnit` while
  `getUnitUpkeepMilitaryNet() > 0` and income < expenses (~16467).

`[CIT/begin]` (`CvCityAI.cpp`, ~966) logs `finTrouble=1` when a city's production decision runs under
financial trouble — the **only existing** log exposure of the financial-trouble state.

---

## 2. What's on the wire today — Tier 1 (Telescreen)

`goldRate` (the aggregate net) is in `/players`; every per-unit and per-player component feeding it is opaque.

| Endpoint / log | Field | Limitation |
|---|---|---|
| `/players` | `gold` | Treasury balance, not a cost breakdown |
| `/players` | `goldRate` | Net gold/turn — aggregate only; upkeep not isolated |
| `/players` | `units` | Total count — no per-unit cost, no civilian/military split |
| `/events` log | `[CIT/begin] finTrouble=` | Boolean, per AI production decision only |
| `/units` | `type`, `unitAI`, `damage`, `level` | No upkeep field, no military-branch flag |

---

## 3. The gap — what cannot be reconstructed from outside

An agent on HTTP + `/events` + gated logs **cannot**:

1. **Know any unit's cost.** A level-5 unit with SM-rank multiplier + promo modifier costs materially more
   than a fresh unit of the same type; neither `getUpkeep100()` nor `isMilitaryBranch()` is in `/units`.
2. **Know the civilian/military split** — `getUnitUpkeep{Civilian,Military}100()` / `*Net()` are absent.
   The split drives the AI disband guard (`getUnitUpkeepMilitaryNet() > 0`).
3. **Know the free-allowance state** — trait/civic-granted free military quota (units at no net cost) is
   invisible (`getFreeUnitUpkeep*`, `getBaseFreeUnitUpkeep*`, `*PopPercent`).
4. **Observe supply pressure** — `getNumOutsideUnits()`, `calculateUnitSupply()`,
   `getDistantUnitSupportCostModifier()` are in no endpoint; foreign-war supply drag isn't quantifiable.
5. **Watch the AI disband loop** — no log line fires; only a `/players.units` drop (≤5s stale),
   indistinguishable from combat attrition.
6. **Determine the AI financial-health score** — `AI_fundingHealth()` / `AI_safeFunding()` /
   `getProfitMargin()` are never logged, yet gate a huge fraction of AI decisions.
7. **Confirm food-for-units is active** — `isMilitaryFoodProduction()` (silently reroutes training from
   hammers to food) is not exposed.

Consequence for the Orwell bar: a major driver of AI behaviour (disband, production gating, financial-trouble
diagnosis) is fully opaque. Any cascade replacement of an upkeep-related `requires.operate` condition (e.g.
"military upkeep below X") cannot be verified without these surfaces.

> **Dead-code note:** `calculateUnitCost(int& iFreeUnits, …)` (`CvPlayer.h`) is declared but has **no
> implementation and no callers** — dead BTS-era code; do not use it.

---

## 4. Proposed hooks — reach Tier 3/4

All hooks follow the three canonical shapes ([DEC-obs-hook-shapes]).

### 4-A. `/units` — per-unit upkeep fields
Add to `UnitSnap` (`Sources/Tools/CvHttpServer.cpp`): `iUpkeep100` (`getUpkeep100()`), `iMilitary`
(`isMilitaryBranch() ? 1 : 0`). `O(1)` per unit; gives full per-unit cost reconstruction.

### 4-B. `/players` — upkeep/supply breakdown
Add to `PlayerSnap`: `iUnitUpkeep{Civilian,Military}100`, `iCivilianUnitUpkeepMod`,
`iMilitaryUnitUpkeepMod`, `iFreeUnitUpkeep{Civilian,Military}`, `iUnitUpkeep{Civilian,Military}Net`,
`iFinalUnitUpkeep`, `iNumOutsideUnits`, `iUnitSupply`, `iDistantUnitSupportCostModifier`,
`iMilitaryFoodProduction`. All `O(1)` / cheap dirty-cache reads — full reconstruction of the upkeep/supply
split in `getFinalExpense`.

### 4-C. `/players` — AI financial-health fields (non-NPC)
`iAIFundingHealth` (`isNormalAI() ? AI_fundingHealth() : −1`), `iAISafeFunding`, `iProfitMargin`,
`iIsFinancialTrouble`, `iIsStrike`, `iStrikeTurns`. `AI_fundingHealth()` is not trivially cheap (calls
`getProfitMargin` → `getFinalExpense`) but runs once per player at publish on the game thread — safe; guard
with `isNormalAI()`.

### 4-D. `[UPK]` log — `UpkeepAI.log`, `gPlayerLogLevel`
Register `[UPK]` in `Sources/AI/BetterBTSAI.{h,cpp}`:
- `[UPK/unit]` (lvl 2, at `calcUpkeep100` on change): `type/base/extraUpkeep100/modifier/smMultiplier/
  upkeep100/military` — high volume on mass promotions; guard `gPlayerLogLevel >= 2`.
- `[UPK/remove]` (lvl 2, at the removal `changeUnitUpkeep`): `type/upkeep100/military`.
- `[UPK/trouble]` (lvl 1, **transition only** — add a player-level dirty-flag): `fundingHealth/safeFunding/
  profitMargin/civilianNet/militaryNet/supply`.
- `[UPK/disband]` (lvl 1, after `AI_disbandUnit` returns true, `CvPlayerAI.cpp` ~16477): `pass/militaryNet/profitMargin`.
- `[UPK/strike]` (lvl 1, in `doGold` when strike begins): `strikeTurns/goldRate`.
- `[UPK/turn]` (lvl 1, once per player per turn): `civilian100/military100/freeCiv/freeMil/finalUpkeep/
  supply/outside` — feeds `/events` at `gStreamLogLevel ≥ 1`.

### 4-E. No diagnostic endpoint needed
Unit upkeep is a pure accumulator (no gate function to shadow-test). §4-B gives the full state snapshot;
`[UPK/turn]` gives the time series.

> **Note:** finance overlaps the gold map. `[FIN/turn]`/`[FIN/aitroubled]` ([`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) §4-C) and `[UPK/turn]`/`[UPK/trouble]` cover adjacent ground; when implementing, pick one home per field to avoid double-emitting the AI financial-trouble transition.

---

## 5. Tier assessment

| Tier | Met after proposed hooks? |
|---|---|
| 1 Coarse snapshots | Yes (current) — only aggregate `goldRate` |
| 2 + buildability shadows | Orthogonal to upkeep |
| 3 + live stream + per-turn state | With `[UPK/turn]` + `[UPK/trouble]` + `[UPK/disband]` (§4-D) |
| 4 Full state per player | With §4-B + §4-C snapshot fields + §4-D events |
| 5 Total | + food-for-units mode (§4-B `isMilitaryFoodProduction`); nothing deeper hidden once §4-A–§4-D land |

**Current tier: 1.** §4-B + `[UPK/turn]`/`[UPK/trouble]` reach **Tier 3** immediately; full §4-A–§4-D reach
**Tier 4/5**.

**Priority: HIGH.** Unit upkeep drives AI disband, production gating, financial-trouble diagnosis, and
handicap scaling — all invisible. `AI_isFinancialTrouble()` gates decisions at many call-sites across
`CvCityAI.cpp` / `CvPlayerAI.cpp` / `CvUnitAI.cpp`; it is the single most impactful invisible AI-state
variable after individual unit evaluation scores. **Minimum viable for cascade verification:** add
`iFinalUnitUpkeep`, `iUnitSupply`, `iNumOutsideUnits`, `iIsFinancialTrouble`, `iIsStrike` to `PlayerSnap`
(§4-B/§4-C) + the `[UPK/turn]` headline (§4-D) — enough to track the cascade's modelled upkeep/supply
against the engine, player by player, turn by turn.

---

## See also
- [`gold-maintenance-inflation.md`](gold-maintenance-inflation.md) — the aggregate expense side: how this map's `getFinalUnitUpkeep` (§1-F there) and `calculateUnitSupply` (§1-G there) fold into `getFinalExpense` → `calculateGoldRate`. Read it for the gold/inflation picture; read here for the per-unit mechanics.
- [`README.md`](README.md) — the observability scale + the three canonical hook shapes ([DEC-obs-scale], [DEC-obs-hook-shapes]) this map assesses against.
- [`http-server.md`](http-server.md) — the live surface these snapshot fields/log tags attach to, and the live-read rules.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total observability is load-bearing ([DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete)).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
