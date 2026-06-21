> DRAFT observability map (2026-06-18, parent cascade-mapping-inventory.md §A) — all mechanics claims
> cited from code as file:line; verify before relying on any detail.

# Observability map — Unit promotions and XP

Companion to `docs/dev/plans/cascade-mapping-inventory.md` (the §A opaque-system list and Tier 0–5
scale).  The goal for this system: reconstruct every unit's XP, level, and promotion set — for every
player, including AI — purely from the HTTP endpoints + `/events` + gated logs, without looking at the
screen.

---

## 1. How it actually works

### 1.1 XP storage

Each unit carries `m_iExperience` (stored × 100 internally — "Experience100") in `CvUnit`.
`getExperience()` divides by 100 for integer reads; `getExperience100()` is the raw 100-scale value
(`CvUnit.cpp:14647` / `CvUnit.cpp:14556`).  All XP accumulation goes through
`changeExperience100(iChange, iMax, bFromCombat, bInBorders, bUpdateGlobal)` (`CvUnit.cpp:14576`).

### 1.2 Level-up threshold

`CvUnit::experienceNeeded(iLvlOffset=0)` (`CvUnit.cpp:12689`) calls:

```
calcBaseExpNeeded(level, owner)  →  (99 + (level² + 1) × (100 + player.getLevelExperienceModifier())) / 100
```

(`CvGameCoreUtils.cpp:3582`).  Commander/Commodore units pay 3/2 × the base threshold
(`CvUnit.cpp:12693–12703`).  The game option `GAMEOPTION_UNIT_MORE_XP_TO_LEVEL` scales by
`MORE_XP_TO_LEVEL_MODIFIER` (`CvGameCoreUtils.cpp:3586`).

`setLevel()` (`CvUnit.cpp:14668`) maintains `CvPlayer::m_iHighestUnitLevel`
(`CvUnit.cpp:14676`) — a per-player stat that gates building prerequisites
(`CvPlayer.cpp:6764`, `CvGameTextMgr.cpp:18507`).

`testPromotionReady()` (`CvUnit.cpp:16708`) sets `m_bPromotionReady = true` when
`getExperience() >= experienceNeeded() && canAcquirePromotionAny()` (also triggers on
`getRetrainsAvailable() > 0`).

### 1.3 XP sources (exhaustive enumeration)

| Source | Call site | Notes |
|---|---|---|
| **Combat (attacker, lethal/withdrawal)** | `CvUnit.cpp:2284`, `2346` | `changeExperience100(withdrawalXP or defXP × str-ratio, 100 × maxXPValue, true, homeTerritory, updateGG)` |
| **Combat (combat-limit hit — attacker retreats defender to limit)** | `CvUnit.cpp:2346–2349` | Both attacker and defender get XP at limit-breach |
| **Dynamic XP (GAMEOPTION_UNIT_DYNAMIC_XP)** | `CvUnit.cpp:25152` (`doDynamicXP`) | XP proportional to odds and HP damage taken/dealt; applied via `applyDynamicXP` (`CvUnit.cpp:25232`); used instead of flat XP when that game option is on |
| **In-battle promotion (dynamic XP path, "occasional promotion")** | `CvUnit.cpp:25088–25148` | A probabilistic mid-combat promotion; resets XP to pre-combat value (`setExperience100(iInitialAttXP)`) |
| **Breakdown attack (RAM/siege)** | `CvUnit.cpp:2256` | `changeExperience100(10, MAX_INT, false, false, true)` — not flagged as combat, no GG credit |
| **Production (city buildings / civics / traits)** | `CvCity.cpp:3244` via `addProductionExperience` | `getProductionExperience(unitType)` = city `getFreeExperience` + player `getFreeExperience` + specialist XP + per-combat-type building XP + domain XP + state-religion XP, modified by capital/holy-city XP modifiers |
| **Conscript penalty** | `CvCity.cpp:3245` | Starting XP halved for conscripted units |
| **Goody hut** | `CvPlayer.cpp:6077` | `pUnit->changeExperience(GC.getGoodyInfo(eGoody).getExperience())` — no cap, not flagged combat |
| **Air bomb (improved XP game option)** | `CvUnit.cpp:7337–7339` | `setExperience100(xp + 25 + rand(26))` on successful non-suicide bomb if `MODDERGAMEOPTION_IMPROVED_XP` |
| **Mission performance (spy/criminal trade mission)** | `CvUnit.cpp:9172`, `9188` | `changeExperience100(10)` on a successful trade run |
| **Mission performance (destroy production / steal plans)** | `CvUnit.cpp:8178`, `8337` | `changeExperience100(100)` on success |
| **Ranged intercept / withdrawal** | `CvUnit.cpp:1934`, `1939` | Both interceptor and intercepted unit get XP from `getExperiencefromWithdrawal` |
| **Healing (healer unit treats another)** | `CvUnit.cpp:6200`, `6308` | `pHealUnit->changeExperience100(10)` or `10 / numHealAsTypes` — healer gains XP |
| **Worker build completion** | `CvUnit.cpp:10040` | `changeExperience100(buildTime / max(1, workRate/50))` |
| **Pillage (gold scaled to level × XP)** | `CvUnit.cpp:2966`, `3485` | Not an XP award — this uses `getLevel() * getExperience()` as pillage gold value |
| **Upgrade** | `CvUnit.cpp:10668` | `pUpgradeUnit->setExperience(xp * 3 / 5)` unless leader or `GAMEOPTION_UNIT_INFINITE_XP` |
| **Combat conversion (capture/convert path)** | `CvUnit.cpp:1233–1235` | XP is scaled by the ratio of the new vs old owner's `getLevelExperienceModifier` |
| **Commander / Commodore XP trickle** | `CvUnit.cpp:14601`, `14607` | Every time the attached commander's unit earns combat XP, the commander gets 0.6 XP |
| **Unit combat type apply/remove** | `CvUnit.cpp:18370` | `changeExperiencePercent(kUnitCombat.getExperiencePercent())` — modifies the XP-gain multiplier |
| **Promotion apply** | `CvUnit.cpp:18911` | `changeExperiencePercent(kPromotion.getExperiencePercent())` — some promotions accelerate XP gains |

### 1.4 XP gain modifiers (per-unit multiplier applied during combat)

`getExperiencePercent()` (`CvUnit.cpp:16451`): sums `m_iExperiencePercent` (accumulated from unit-combat
type and promotion `getExperiencePercent()` contributions) plus the commander's `getExperiencePercent()`
or commodore's equivalent.  Combined with `kPlayer.getExpInBorderModifier()` (from civics/traits,
`CvPlayer.cpp:10561`) in `changeExperience100` when `bFromCombat && bInBorders` (`CvUnit.cpp:14587–14590`).

### 1.5 XP caps

`maxXPValue(pVictim)` (`CvUnit.cpp:12720`) gates combat XP from weak opponents:
- vs. animals: `GC.getANIMAL_MAX_XP_VALUE()` unless the unit has `UNITCOMBAT_EXPLORER` or
  `PROMOTION_ANIMAL_HUNTER`.
- vs. hominids/barbarians: `GC.getBARBARIAN_MAX_XP_VALUE()` unless `UNITCOMBAT_RECON` or
  `PROMOTION_BARBARIAN_HUNTER`.
- Game option `GAMEOPTION_UNIT_INFINITE_XP` or NPC units: no cap (returns -1).

### 1.6 Great General points

When `changeExperience100(…, bUpdateGlobal=true)` runs, it also calls
`kPlayer.changeFractionalCombatExperience(modifiedChange, getGGExperienceEarnedTowardsType())`
(`CvUnit.cpp:14596`).  Player accumulates GG points per-unit-type; the type reaching threshold spawns
a Great General (`CvPlayer.cpp:11629`).

### 1.7 Free / starting promotions

Free promotions for a unit come from three sources (applied during `addProductionExperience` →
`assignPromotionsFromBuildingChecked`, and in unit `init()`):

1. **Unit info (`CvUnitInfo::getFreePromotions`)** — defined in XML per unit type
   (`CvUnit.cpp:17932`, `CvUnit.cpp:26252`).
2. **Player free promotions (`CvPlayer::isFreePromotion(unitType, promo)`)** — granted by civics,
   traits, or buildings.
3. **Building-granted promotions** (`assignPromotionsFromBuildingChecked` in `CvCity.cpp:3252`) —
   buildings in the training city.

`isPromotionValid()` (`CvUnit.cpp:17870+`) gates whether a promotion can be taken; it checks tech
prereqs, obsolete tech, unit-combat type qualifications, game-option gating
(`PromotionLine.getNotOnGameOption`), and explicitly allows promotions that are "free" for the unit
even when prereqs would otherwise block (`CvUnit.cpp:17932`).

### 1.8 AI auto-promotion

AI units self-promote via `AI_promote()` (`CvUnitAI.cpp:856`): called from `setPromotionReady(true)`
when `isAutoPromoting()` is set (`CvUnit.cpp:16669–16674`).  It scores all valid promotions via
`AI_promotionValue`, picks the best, and calls `promote()` — silently, with no log line emitted.
`AI_promote()` recurses until no further promotions are available (`CvUnitAI.cpp:883`).

---

## 2. Current observability

**Current tier: 1 (Telescreen).**  The snapshot exposes per-unit level but nothing else in the
XP/promotion system.

### What IS observable today

| What | Where | How | Precision |
|---|---|---|---|
| Unit **level** | `GET /units` → `"level"` field | `UnitSnap.iLevel` (`CvHttpServer.cpp:56`, `1482`, `1250`) | Exact integer per unit |
| Unit **type** | `GET /units` → `"type"` | XML key | Enables static-data lookup of `getFreePromotions` per unit type — tells you what a fresh unit *should* have |
| Unit **damage** | `GET /units` → `"damage"` | `UnitSnap.iDamage` | Current HP loss (indirectly relevant: lower HP → less XP earned this fight) |
| Unit **completion log** | `[CIT/produced]` in `CityAI.log` / `/events` | `logCityAI(1, ...)` at `CvCity.cpp:15836` | Logs unit type, owner, city — but NOT starting XP or starting promotions |
| Player **era** | `GET /players` → `"era"` | | Coarse proxy for tech level (hence some promotions gated by tech) |

### What is NOT observable today

| Gap | Impact |
|---|---|
| Unit **current XP (experience100)** | Cannot compute progress-to-next-level, cannot detect XP-cap-hits, cannot detect stalled veterans |
| Unit **promotionReady flag** | Cannot tell whether a unit is waiting for a promotion pick |
| Unit **promotion set** (`hasPromotion[]`) | Entire promotion state is invisible from outside; AI promotions chosen silently |
| Unit **experiencePercent modifier** | XP-rate multiplier (from unit-combat type + promotion bonuses) unknown |
| **XP gain events** | No log line or event is emitted when a unit gains XP (combat or non-combat) |
| **Promotion gained events** | `setHasPromotion` emits no log line; `AI_promote` emits no log line — AI promotions are completely silent |
| **Level-up events** | `setLevel` emits no log line |
| **Free promotions on production** | `addProductionExperience` emits no log — cannot tell what promotions a freshly-produced unit received |
| Player **combatExperience** (GG points) | Not in `/players`; cannot reconstruct when a GG is approaching threshold |
| Player **highestUnitLevel** | Not in `/players`; gates building `getUnitLevelPrereq()` prerequisites silently |
| Player **freeExperience / levelExperienceModifier** | Not in any endpoint; needed to reconstruct `experienceNeeded` threshold per player |
| City **productionExperience** | Not in `/cities`; cannot predict starting XP for units produced in a city |
| **In-battle promotion events** (dynamic-XP path) | Probabilistic mid-combat promotions (`CvUnit.cpp:25088`) emit a DLL message (player popup) but no log line |

---

## 3. The gap

Everything below level is invisible from outside:

- The XP value itself (cannot compute level-threshold progress or detect when a unit just levelled up)
- The full promotion set per unit (cannot reconstruct effective stats or validate cascade promo requirements)
- All XP events: combat gains, goody huts, mission performance, healing, breakdown, worker builds — none emit a tag
- All promotion events: AI auto-promotion, free-on-production, in-combat promotions — none emit a tag
- The player-level inputs needed to reconstruct XP thresholds (`levelExperienceModifier`,
  `highestUnitLevel`, `combatExperience`/GG points, `freeExperience`, city `productionExperience`)

This means: given only the live wire, you cannot tell whether an AI unit is a fresh-off-the-line
level-1 unpromotedwarrior or a hardened veteran with six promotions nearing another level-up.  The
cascade's `requires.operate` checks on promotions (if/when they exist) would be completely unverifiable
from outside.

---

## 4. Proposed hooks to reach Tier 2 / Tier 3

All hooks follow the three canonical observability hook shapes — see [DEC-obs-hook-shapes](../../decisions.md#dec-obs-hook-shapes).

### 4.1 `/units` snapshot additions (Tier 1 → Tier 2)

Add the following fields to `UnitSnap` (`CvHttpServer.cpp:46`) and populate them in `publishIfDue`
(`CvHttpServer.cpp:1476`).  These are read-only snapshot fields; the hard constraint (server thread
never touches game objects) is satisfied because they are copied into the snapshot on the game thread.

| JSON key | Source call | Type |
|---|---|---|
| `"xp"` | `pLoopUnit->getExperience()` | int — current XP (integer scale, not ×100) |
| `"xpNeeded"` | `pLoopUnit->experienceNeeded()` | int — XP threshold to next level |
| `"promotionReady"` | `pLoopUnit->isPromotionReady()` | int (0/1) |
| `"xpPct"` | `pLoopUnit->getExperiencePercent()` | int — combined XP-rate modifier |

Adding `promotions[]` (the full bool array, ~300 entries) would be expensive and noisy per unit.
The better path is the event stream (§4.2): snapshot just needs xp + xpNeeded + promotionReady for
"where is this unit in its progression" without listing every promotion.

### 4.2 `/players` snapshot additions

| JSON key | Source call | Type |
|---|---|---|
| `"combatXP"` | `kPlayer.getCombatExperience()` | int — accumulated GG points |
| `"highestUnitLevel"` | `kPlayer.getHighestUnitLevel()` | int |
| `"freeXP"` | `kPlayer.getFreeExperience()` | int — player-level starting-XP bonus |
| `"levelXPMod"` | `kPlayer.getLevelExperienceModifier()` | int — threshold scaling modifier |

### 4.3 `[UNT]` log tag additions (Tier 2 → Tier 3)

These emit the **event**, not just a snapshot.  All call `logUnitAI(level, ...)` at the commit point;
`streamLogTee` sends level-1 lines to `/events`.

**`[UNT/xp]` (level 1) — XP gain event.**
Emit from `setExperience100` (`CvUnit.cpp:14561`) when the new value differs from the old, or
alternatively from the end of `changeExperience100` (`CvUnit.cpp:14644`) after the update.
Fields: `owner= unit= type= xp= prev= src=` where `src` is an enum tag
(`combat|goody|build|mission|worker|breakdown|healer|upgrade`).  To distinguish sources, add a
`src` parameter to `changeExperience100` (default = `xp_src_unknown`); each call site passes the
appropriate tag.  This is the highest-value hook for AI observability — currently ZERO XP events
reach the wire.

Minimal viable version without source tagging: emit from `setExperience100` when
`m_iExperience != iNewValue`, fields `owner= unit= xp= prev=`.  One gated `if
(gUnitLogLevel >= 1)` guard before the format string — cost when off: one int compare.

**`[UNT/promo]` (level 1) — promotion gained/lost event.**
Emit from `setHasPromotion` (`CvUnit.cpp:19134`) at the point where `info->m_bHasPromotion`
is set (`CvUnit.cpp:19196`), for both gain and loss.
Fields: `owner= unit= promo= action=gained|lost free=0|1`.  This covers AI auto-promotions,
free-on-production, in-combat promotions, and civic/tech-driven grant/revoke.

**`[UNT/level]` (level 1) — level change event.**
Emit from `setLevel` (`CvUnit.cpp:14668`) when `m_iLevel != iNewValue`.
Fields: `owner= unit= level= prev=`.

**`[UNT/promote/ready]` (level 2) — promotion-ready state change.**
Emit from `setPromotionReady` (`CvUnit.cpp:16636+`) when the flag flips.
Fields: `owner= unit= ready=1|0`.  Primarily useful for detecting AI units that have
been "promotion-ready but not yet promoted" for many turns (a bug signal).

### 4.4 `[CIT/produced]` extension for free promotions (level 2)

In `addProductionExperience` (`CvCity.cpp:3239`), after `assignPromotionsFromBuildingChecked`,
emit a `[CIT/promos]` line listing the promotions just assigned:
`[CIT/promos] city=… owner=… unit=… unitId=… xp=… promos=PROMO_X,PROMO_Y`.
This closes the "what did this unit start with" gap for the production path.

### 4.5 City `/cities` snapshot addition

Add `"productionXP"` to `CitySnap` (`CvHttpServer.cpp:83`): `pLoopCity->getProductionExperience(NO_UNIT)`
(the base XP before unit-type-specific bonuses).  Lets external tooling predict starting XP for any
unit the city produces, without the per-unit-type breakdown.

---

## 5. Summary

| Tier | Status |
|---|---|
| **Tier 1 — Telescreen** | CURRENT: only `level` is exposed per unit |
| **Tier 2 — Informant** | Reachable by: adding `xp`/`xpNeeded`/`promotionReady`/`xpPct` to `/units` + `combatXP`/`highestUnitLevel`/`freeXP`/`levelXPMod` to `/players` + `productionXP` to `/cities` |
| **Tier 3 — Big Brother** | Reachable by: `[UNT/xp]` + `[UNT/promo]` + `[UNT/level]` event hooks in `/events` stream |

The XP/promotion system is the most observation-opaque per-unit system in S2S: **zero events today**.
Every AI promotion, every XP gain, every level-up happens silently.  This is the dominant gap between
the current Tier 1 state and a promotions-aware cascade verification substrate.

---

*Cross-references:*
- `docs/dev/plans/cascade-mapping-inventory.md` — the §A opaque-system list and Tier 0–5 scale
- `docs/dev/reference/http-server.md` — the live endpoint surface
- `docs/dev/reference/ai-logging-reference.md` — the tag taxonomy (`[UNT/*]` section)
- `Sources/CvUnit.cpp` — XP mechanics ground truth
- `Sources/CvUnitAI.cpp:856` — `AI_promote()` (silent AI promotion path)
- `Sources/CvCity.cpp:3172` — `getProductionExperience` / `addProductionExperience`
- `Sources/CvGameCoreUtils.cpp:3582` — `calcBaseExpNeeded` threshold formula
