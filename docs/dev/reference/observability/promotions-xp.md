# Observability — Unit promotions & XP — what's on the wire for unit progression

> **Status:** reference   ·   **Verified against:** 2026-06-20 (function sites re-confirmed in `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvGameCoreUtils.cpp`, `Sources/AI/CvUnitAI.cpp`; line numbers drift)
> **Grounding:** live `Sources/Engine/CvUnit.cpp` (`changeExperience100`, `experienceNeeded`, `setLevel`, `testPromotionReady`, `getExperiencePercent`, `maxXPValue`, `isInvisible`-adjacent promotion machinery), `Sources/Engine/CvGameCoreUtils.cpp` (`calcBaseExpNeeded`), `Sources/Engine/CvCity.cpp` (`addProductionExperience` / `getProductionExperience`), `Sources/AI/CvUnitAI.cpp` (`AI_promote`), `Sources/Tools/CvHttpServer.cpp` (`/units`, `/players`, `/cities`). Carried over from the old draft map; function sites re-confirmed, paths re-grounded to the reorganized `Sources/` tree.
> XP/promotion is **Tier 1 (Telescreen)** today: the `/units` snapshot exposes a unit's `level` and nothing else of its progression — not its XP, not its promotion-ready flag, not its promotion set. **Zero XP or promotion events reach any log or the `/events` stream**; every AI auto-promotion, XP gain, and level-up happens silently. This is the most observation-opaque per-unit system in S2S. This map walks XP storage → the level-up threshold → every XP source → the gain modifiers → caps → Great-General points → free promotions → AI auto-promotion, names what's dark, and proposes the snapshot fields / log tags / endpoint to climb to Tier 3.

The observability scale (0–5) and the three canonical hook shapes are defined once in
[`README.md`](README.md) ([DEC-obs-scale], [DEC-obs-hook-shapes]); the live surface and the rules for
reading it (logs held open mid-session, use `/events` + `/diagnostic`) live in
[`http-server.md`](http-server.md). This doc does not restate them.

> Line numbers below are anchors at time of writing and **drift** — confirm the named function, not the
> integer.

---

## 1. How it actually works

### 1a. XP storage

Each unit carries `m_iExperience` (stored ×100 internally — "Experience100") in `CvUnit`.
`getExperience()` divides by 100 for integer reads; `getExperience100()` is the raw 100-scale value.
**All** XP accumulation funnels through
`changeExperience100(iChange, iMax, bFromCombat, bInBorders, bUpdateGlobal)`
(CvUnit.cpp:14577).

### 1b. Level-up threshold

`CvUnit::experienceNeeded(iLvlOffset=0)` (CvUnit.cpp:12690) calls `calcBaseExpNeeded`
(CvGameCoreUtils.cpp:3572):

```
calcBaseExpNeeded(level, owner)  →  (99 + (level² + 1) × (100 + player.getLevelExperienceModifier())) / 100
```

Commander/Commodore units pay 3/2 × the base threshold (in `experienceNeeded`). The game option
`GAMEOPTION_UNIT_MORE_XP_TO_LEVEL` scales by `MORE_XP_TO_LEVEL_MODIFIER` (CvGameCoreUtils.cpp,
`calcBaseExpNeeded`).

`setLevel()` (CvUnit.cpp:14669) maintains `CvPlayer::m_iHighestUnitLevel` — a per-player stat that gates
building prerequisites (`getUnitLevelPrereq`).

`testPromotionReady()` (CvUnit.cpp:16709) sets `m_bPromotionReady = true` when
`getExperience() >= experienceNeeded() && canAcquirePromotionAny()` (also triggers on
`getRetrainsAvailable() > 0`).

### 1c. XP sources (exhaustive enumeration)

All cited to `Sources/Engine/CvUnit.cpp` unless noted; line numbers are from the old draft against the
pre-reorg tree and are **approximate** post-reorg — confirm the named code, not the integer.

| Source | Call site (approx.) | Notes |
|---|---|---|
| **Combat (attacker, lethal/withdrawal)** | CvUnit.cpp ~2284, ~2346 | `changeExperience100(withdrawalXP or defXP × str-ratio, 100 × maxXPValue, true, homeTerritory, updateGG)` |
| **Combat (combat-limit hit)** | CvUnit.cpp ~2346–2349 | both attacker and defender get XP at limit-breach |
| **Dynamic XP (`GAMEOPTION_UNIT_DYNAMIC_XP`)** | CvUnit.cpp `doDynamicXP` / `applyDynamicXP` | XP proportional to odds + HP damage; replaces flat XP when the option is on |
| **In-battle promotion (dynamic-XP "occasional promotion")** | CvUnit.cpp dynamic-XP block | probabilistic mid-combat promotion; resets XP to pre-combat (`setExperience100(iInitialAttXP)`) |
| **Breakdown attack (RAM/siege)** | CvUnit.cpp ~2256 | `changeExperience100(10, MAX_INT, false, false, true)` — not flagged combat, no GG credit |
| **Production (city buildings / civics / traits)** | CvCity.cpp `addProductionExperience` | `getProductionExperience(unitType)` = city + player `getFreeExperience` + specialist XP + per-combat-type building XP + domain XP + state-religion XP, modified by capital/holy-city XP modifiers |
| **Conscript penalty** | CvCity.cpp `addProductionExperience` | starting XP halved for conscripted units |
| **Goody hut** | CvPlayer.cpp (goody handler) | `pUnit->changeExperience(GC.getGoodyInfo(eGoody).getExperience())` — no cap, not flagged combat |
| **Air bomb (`MODDERGAMEOPTION_IMPROVED_XP`)** | CvUnit.cpp ~7337–7339 | `setExperience100(xp + 25 + rand(26))` on a successful non-suicide bomb |
| **Mission (spy/criminal trade mission)** | CvUnit.cpp ~9172, ~9188 | `changeExperience100(10)` on a successful trade run |
| **Mission (destroy production / steal plans)** | CvUnit.cpp ~8178, ~8337 | `changeExperience100(100)` on success |
| **Ranged intercept / withdrawal** | CvUnit.cpp ~1934, ~1939 | both interceptor and intercepted unit gain `getExperiencefromWithdrawal` |
| **Healing (healer treats another)** | CvUnit.cpp ~6200, ~6308 | healer gains `changeExperience100(10)` or `10 / numHealAsTypes` |
| **Worker build completion** | CvUnit.cpp ~10040 | `changeExperience100(buildTime / max(1, workRate/50))` |
| **Upgrade** | CvUnit.cpp ~10668 | `pUpgradeUnit->setExperience(xp × 3 / 5)` unless leader or `GAMEOPTION_UNIT_INFINITE_XP` |
| **Combat conversion (capture/convert path)** | CvUnit.cpp ~1233–1235 | XP scaled by the ratio of new vs old owner's `getLevelExperienceModifier` |
| **Commander / Commodore trickle** | CvUnit.cpp ~14601, ~14607 | when the attached unit earns combat XP, the commander gets 0.6 XP |

> NOTE (from old draft, carried as a caveat): "Pillage" was listed as an XP source but is **not** one —
> `getLevel() × getExperience()` there is the pillage *gold* value, not an XP award. Dropped from the table
> above to avoid the head-bite.

XP-rate modifiers (not direct awards) also flow through `changeExperiencePercent`: unit-combat-type
apply/remove (`kUnitCombat.getExperiencePercent()`) and promotion apply
(`kPromotion.getExperiencePercent()`) adjust the per-unit XP-gain multiplier (§1d).

### 1d. XP-gain modifier (per-unit multiplier applied during combat)

`getExperiencePercent()` (CvUnit.cpp:16452) sums `m_iExperiencePercent` (accumulated from unit-combat-type
+ promotion `getExperiencePercent()` contributions) plus the commander's / commodore's contribution.
During `changeExperience100`, when `bFromCombat && bInBorders`, it is further combined with
`kPlayer.getExpInBorderModifier()` (from civics/traits).

### 1e. XP caps

`maxXPValue(pVictim, bBarb)` (CvUnit.cpp:12721) caps combat XP from weak opponents:
- vs animals: `GC.getANIMAL_MAX_XP_VALUE()` unless the unit has `UNITCOMBAT_EXPLORER` or
  `PROMOTION_ANIMAL_HUNTER`.
- vs hominids/barbarians: `GC.getBARBARIAN_MAX_XP_VALUE()` unless `UNITCOMBAT_RECON` or
  `PROMOTION_BARBARIAN_HUNTER`.
- `GAMEOPTION_UNIT_INFINITE_XP` or NPC units: no cap (returns -1).

### 1f. Great-General points

When `changeExperience100(…, bUpdateGlobal=true)` runs, it also calls
`kPlayer.changeFractionalCombatExperience(modifiedChange, getGGExperienceEarnedTowardsType())`. The player
accumulates GG points per-unit-type; the type reaching threshold spawns a Great General.

### 1g. Free / starting promotions

Free promotions reach a unit from three sources (applied during `addProductionExperience` →
`assignPromotionsFromBuildingChecked`, and in unit `init()`):

1. **Unit info (`CvUnitInfo::getFreePromotions`)** — XML per unit type.
2. **Player free promotions (`CvPlayer::isFreePromotion(unitType, promo)`)** — civics, traits, buildings.
3. **Building-granted promotions** (`assignPromotionsFromBuildingChecked` in `CvCity.cpp`) — buildings in
   the training city.

`isPromotionValid()` gates whether a promotion can be taken (tech prereqs, obsolete tech, unit-combat-type
qualifications, game-option gating via `PromotionLine.getNotOnGameOption`), and explicitly allows
promotions that are "free" for the unit even when prereqs would otherwise block.

### 1h. AI auto-promotion

AI units self-promote via `AI_promote()` (CvUnitAI.cpp:1036): invoked from `setPromotionReady(true)` when
`isAutoPromoting()` is set. It scores all valid promotions via `AI_promotionValue`, picks the best, and
calls `promote()` — **silently, with no log line emitted** — then recurses (CvUnitAI.cpp:1063) until no
further promotions are available.

---

## 2. What's on the wire today — **Tier 1 (Telescreen)**

### What exists

| What | Where | How | Precision |
|---|---|---|---|
| Unit **level** | `/units` → `"level"` | `UnitSnap.iLevel` (`CvHttpServer.cpp`) | exact integer per unit |
| Unit **type** | `/units` → `"type"` | XML key | enables static lookup of `getFreePromotions` — what a *fresh* unit of that type should have |
| Unit **damage** | `/units` → `"damage"` | `UnitSnap.iDamage` | current HP loss (indirect: lower HP → less XP this fight) |
| Unit **completion log** | `[CIT/produced]` in `CityAI.log` / `/events` | `logCityAI(1, …)` in `CvCity.cpp` | logs unit type/owner/city — **not** starting XP or promotions |
| Player **era** | `/players` → `"era"` | | coarse tech-level proxy (some promotions are tech-gated) |

### What is NOT exposed (the gap)

| Gap | Impact |
|---|---|
| Unit **current XP** (`experience100`) | can't compute progress-to-next-level, can't detect XP-cap hits or stalled veterans |
| Unit **promotionReady** flag | can't tell if a unit is waiting on a promotion pick |
| Unit **promotion set** (`hasPromotion[]`) | entire promotion state invisible; AI picks chosen silently |
| Unit **experiencePercent** modifier | XP-rate multiplier unknown |
| **XP-gain events** | no log/event when a unit gains XP (combat or non-combat) |
| **Promotion-gained events** | `setHasPromotion` and `AI_promote` emit nothing — AI promotions wholly silent |
| **Level-up events** | `setLevel` emits nothing |
| **Free promotions on production** | `addProductionExperience` logs nothing — can't tell what a fresh unit received |
| Player **combatExperience** (GG points) | not in `/players`; can't reconstruct a GG approaching threshold |
| Player **highestUnitLevel** | not in `/players`; silently gates building `getUnitLevelPrereq` |
| Player **freeExperience / levelExperienceModifier** | not in any endpoint; needed to reconstruct `experienceNeeded` per player |
| City **productionExperience** | not in `/cities`; can't predict starting XP for units a city produces |
| **In-battle promotion events** (dynamic-XP path) | emit a player popup but no log line |

---

## 3. The gap

Everything beyond `level` is invisible from outside: the XP value itself (so no threshold-progress and no
level-up detection), the full per-unit promotion set (so no reconstruction of effective stats or validation
of cascade promotion requirements), every XP event (combat, goody, mission, healing, breakdown, worker
builds — none tagged), every promotion event (AI auto-promotion, free-on-production, in-combat — none
tagged), and the player-level inputs needed to reconstruct thresholds (`levelExperienceModifier`,
`highestUnitLevel`, `combatExperience`/GG points, `freeExperience`, city `productionExperience`).

Given only the live wire, you **cannot** tell a fresh-off-the-line level-1 warrior from a six-promotion
veteran nearing another level-up. Any cascade `requires` atom that gates on a promotion (if/when they exist)
would be completely unverifiable from outside ([DEC-map-before-delete]).

---

## 4. Proposed hooks — climbing from Tier 1 to Tier 3

All hooks follow the three canonical hook shapes — see [DEC-obs-hook-shapes]. Snapshot fields are copied on
the game thread, so the server-thread-never-touches-game-objects constraint (see [`http-server.md`](http-server.md))
holds.

### 4a. `/units` snapshot additions (Tier 1 → 2)

Add to `UnitSnap` (`CvHttpServer.cpp`) and populate in the unit snapshot walk:

| JSON key | Source call | Type |
|---|---|---|
| `"xp"` | `pLoopUnit->getExperience()` | int — current XP (integer scale, not ×100) |
| `"xpNeeded"` | `pLoopUnit->experienceNeeded()` | int — threshold to next level |
| `"promotionReady"` | `pLoopUnit->isPromotionReady()` | int (0/1) |
| `"xpPct"` | `pLoopUnit->getExperiencePercent()` | int — combined XP-rate modifier |

The full `promotions[]` bool array (~300 entries/unit) would be expensive and noisy in the snapshot; the
event stream (§4c) carries promotion *changes* instead. The snapshot needs only "where is this unit in its
progression".

### 4b. `/players` + `/cities` snapshot additions (Tier 1 → 2)

`/players` (add to `PlayerSnap`):

| JSON key | Source call | Type |
|---|---|---|
| `"combatXP"` | `kPlayer.getCombatExperience()` | int — accumulated GG points |
| `"highestUnitLevel"` | `kPlayer.getHighestUnitLevel()` | int |
| `"freeXP"` | `kPlayer.getFreeExperience()` | int — player-level starting-XP bonus |
| `"levelXPMod"` | `kPlayer.getLevelExperienceModifier()` | int — threshold scaling modifier |

`/cities` (add to `CitySnap`): `"productionXP"` ← `pLoopCity->getProductionExperience(NO_UNIT)` (base XP
before unit-type-specific bonuses) — lets tooling predict starting XP for any unit the city produces.

### 4c. `[UNT]` log-tag additions (Tier 2 → 3)

These emit the **event**, not just a snapshot — the highest-value hooks, since **zero** XP/promotion events
reach the wire today. All call `logUnitAI(level, …)` at the commit point; `streamLogTee` sends level-1 lines
to `/events`.

| Tag | Level | Site | Line |
|---|---|---|---|
| `[UNT/xp]` | 1 | `changeExperience100` (CvUnit.cpp:14577) end, or `setExperience100` when value changes | `owner= unit= type= xp= prev= src=` |
| `[UNT/promo]` | 1 | `setHasPromotion`, at the `m_bHasPromotion`-set point, for gain and loss | `owner= unit= promo= action=gained\|lost free=0\|1` |
| `[UNT/level]` | 1 | `setLevel` (CvUnit.cpp:14669) when `m_iLevel != iNewValue` | `owner= unit= level= prev=` |
| `[UNT/promote/ready]` | 2 | `setPromotionReady` when the flag flips | `owner= unit= ready=1\|0` |

For `[UNT/xp]`, the `src` enum (`combat\|goody\|build\|mission\|worker\|breakdown\|healer\|upgrade`) requires
adding a `src` parameter to `changeExperience100` (default `xp_src_unknown`), with each call site passing its
tag. A minimal first cut omits `src`: emit from `setExperience100` when `m_iExperience != iNewValue`, fields
`owner= unit= xp= prev=`, guarded by one gated `if (gUnitLogLevel >= 1)` (cost when off: one int compare).

`[UNT/promo]` covers AI auto-promotions, free-on-production, in-combat promotions, and civic/tech grant/revoke
in one place. `[UNT/promote/ready]` is primarily a bug signal — an AI unit promotion-ready-but-not-promoted
for many turns.

### 4d. `[CIT/promos]` extension for free-on-production promotions (Tier 2)

In `addProductionExperience` (`CvCity.cpp`), after `assignPromotionsFromBuildingChecked`, emit:

```
[CIT/promos] city= owner= unit= unitId= xp= promos=PROMO_X,PROMO_Y
```

This closes the "what did this unit start with" gap for the production path.

---

## 5. Cost & priority

| Hook | Tier gain | Effort | Rationale |
|---|---|---|---|
| `/units` fields (§4a) | 1→2 | 4 struct fields + walk reads | "where is this unit in its progression" for ALL units |
| `/players` + `/cities` fields (§4b) | →2 | struct + walk reads | per-player threshold inputs + per-city starting-XP predictor |
| `[UNT/xp]`/`[UNT/promo]`/`[UNT/level]` (§4c L1) | →3 | gated, ~zero cost off; `src` needs a param thread-through | **the** dominant gap — every XP gain, promotion, and level-up becomes visible on `/events` |
| `[UNT/promote/ready]` (§4c L2), `[CIT/promos]` (§4d) | 3 | gated | stalled-promotion bug signal + free-on-production set |

The XP/promotion system is the most observation-opaque per-unit system in S2S: **zero events today**. The
event hooks (§4c) are the dominant lever between the current Tier 1 and a promotions-aware cascade
verification substrate.

---

## 6. Code cross-reference

> Paths re-grounded to the reorganized `Sources/` tree (`Cv*` engine → `Sources/Engine/`, `Cv*AI` →
> `Sources/AI/`, `CvHttpServer` → `Sources/Tools/`). Line numbers drift — confirm the function.

| Claim | Source |
|---|---|
| `changeExperience100` (all XP funnels here) | `Sources/Engine/CvUnit.cpp:14577` |
| `experienceNeeded` (level threshold) | `Sources/Engine/CvUnit.cpp:12690` |
| `calcBaseExpNeeded` (threshold formula) | `Sources/Engine/CvGameCoreUtils.cpp:3572` |
| `setLevel` (maintains `m_iHighestUnitLevel`) | `Sources/Engine/CvUnit.cpp:14669` |
| `testPromotionReady` | `Sources/Engine/CvUnit.cpp:16709` |
| `getExperiencePercent` (XP-rate modifier) | `Sources/Engine/CvUnit.cpp:16452` |
| `maxXPValue` (combat XP cap) | `Sources/Engine/CvUnit.cpp:12721` |
| `getProductionExperience` / `addProductionExperience` (free promos + starting XP) | `Sources/Engine/CvCity.cpp` |
| `AI_promote` (silent AI auto-promotion) | `Sources/AI/CvUnitAI.cpp:1036` |
| `UnitSnap` / `PlayerSnap` / `CitySnap` (add fields here) | `Sources/Tools/CvHttpServer.cpp` |

---

## See also
- [`README.md`](README.md) — the observability scaffold: the 0–5 scale ([DEC-obs-scale]), the Orwell bar,
  and the three canonical hook shapes ([DEC-obs-hook-shapes]) this map's hooks instantiate.
- [`http-server.md`](http-server.md) — the live surface (`/units`, `/players`, `/cities`) these hooks
  extend, and the live-read rules (logs held open mid-session).
- [`vision-visibility.md`](vision-visibility.md) — the sibling per-unit map; `getExtraVisibilityRange` is a
  promotion-driven bonus, so promotion state also feeds a unit's sight range.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — why total
  observability is load-bearing: a promotion-gated cascade atom cannot be shadowed/cut until the promotion
  set is on the wire ([DEC-map-before-delete]).
- [`../../README.md`](../../README.md) — the comprehension map / overview-of-overviews.

[DEC-obs-scale]: ../../architecture/decisions.md#dec-obs-scale
[DEC-obs-hook-shapes]: ../../architecture/decisions.md#dec-obs-hook-shapes
[DEC-map-before-delete]: ../../architecture/decisions.md#dec-map-before-delete
