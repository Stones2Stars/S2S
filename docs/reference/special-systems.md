# Special systems reference — espionage, great people, promotions/XP, vision, trade, diplomacy, victory

> Lifted + condensed mechanics (the formulas the validator re-derives). Behaviour as-is; the cascade replaces these
> (verified live in-game).

## Espionage

- **EP accrual:** `doEspionagePoints` → `doEspionageOneOffPoints(getCommerceRate(ESPIONAGE))`; off until ≥1 team met.
- **EP split:** divided per turn proportionally by `m_aiEspionageSpendingWeightAgainstTeam[]` (0–99 per target;
  unmet/dead/self excluded). **Mission cost** = `baseCost × costModifier/100 × numTeamMembers` (chain: pop, trade
  routes, shared religion/holy city, culture ratio, `getEspionageDefenseModifier`, distance, spy fortify, EP-ever
  ratio `ESPIONAGE_SPENDING_MULTIPLIER·(2·theirEver+ourEver)/(theirEver+2·ourEver)`, counterespionage, discounts).
  On success: deduct from `m_aiEspionagePointsAgainstTeam[target]`.
- Counterespionage `mod = mission.getCounterespionageMod() + 5·spy.currInterceptionProbability()`, timer −1/turn.
  Four per-city effect timers (health/happiness/disabled-power/war-weariness) all −1/turn.

## Great people (city)

- **GPP:** `doGreatPeople` per city (disorder guard); adds `getGreatPeopleRate()` to `m_iGreatPeopleProgress` +
  per-type. **Rate** = `isDisorder()?0 : baseGreatPeopleRate × totalGreatPeopleRateModifier/100`; base =
  `baseGreatPeopleRate + nationalGreatPeopleRate`; modifier = city + player + global + state-religion + golden-age.
- **Spawn:** at `progress ≥ greatPeopleThresholdNonMilitary`, draw `rand(totalUnitProgress)` to pick the type by
  accumulated weight; deduct the full threshold; zero per-type. **Threshold** = `GREAT_PEOPLE_THRESHOLD ×
  era.getGreatPeoplePercent()[START era] × getModifiedIntValue64(.., thresholdModifier) × speedPercent/10000`, min 1;
  the threshold modifier ramps each spawn by `GREAT_PEOPLE_THRESHOLD_INCREASE·(created/5 + 2)` (non-linear).

## Promotions & XP (unit)

- XP stored **×100** (`m_iExperience`), all via `changeExperience100(iChange, iMax, bFromCombat, bInBorders, bUpdateGlobal)`.
- **Level threshold** `calcBaseExpNeeded(level, owner) = (99 + (level²+1)·(100 + getLevelExperienceModifier()))/100`;
  Commander/Commodore × 3/2. **Caps:** vs animals → `ANIMAL_MAX_XP_VALUE` (unless EXPLORER/ANIMAL_HUNTER); vs
  barbarians → `BARBARIAN_MAX_XP_VALUE` (unless RECON/BARBARIAN_HUNTER); `UNIT_INFINITE_XP` removes caps.
- **Sources:** combat, production (`getProductionExperience`), goody, air-bomb, spy/trade (10), intercept, healer (10
  or `10/numHealAsTypes`), worker-build (`buildTime/max(1, workRate/50)`), upgrade (×3/5). Breakdown (RAM/siege) +10,
  NOT combat-flagged (no GG credit). **Pillage is NOT XP.** Commander +0.6 XP when its attached unit earns combat XP;
  `bUpdateGlobal=true` feeds GG points. Free promotions bypass tech prereqs (`isPromotionValid`). AI `AI_promote()`
  recurses silently. **No log anywhere in the XP/promo system** beyond `level` in `/units`.

## Vision & visibility (plot, per team)

- Per-plot per-team: `m_aiVisibilityCount` (>0 = visible now), `m_abRevealed` (ever-seen, permanent), `…LastSeenTurn`,
  `…StolenVisibilityCount`, `…InvisibleVisibilityCount`. `isVisible = visibilityCount>0 || stolenVisibilityCount>0`.
- **Sight range** = `1 + plot.getTerrainElevation() + getExtraVisibilityRange() + improvement.getVisibilityChange()`,
  cap `MAX_UNIT_VISIBILITY_RANGE` (elevation 0/1/2 = flat/hills/peak; air sees all).
> **⚖ FOG DECAY IS A REAL, RECENT FEATURE THAT IS SWITCHED OFF — NOT DEAD CODE (owner).** `ENABLE_FOGWAR_DECAY`
> (the commented `#define` at `CvGameCoreDLL.h`) gates a **map that goes fully dark again where you have not
> been for a while**: the revealed-but-unseen tier decays instead of being remembered forever, riding the
> per-plot `…LastSeenTurn` above plus its own `m_iVisibilityDecay` / `m_iDefaultDecay` and the
> `m_bPermanentMapLand` / `m_bPermanentMapSea` opt-outs. **It is OFF because it BROKE HOTSEAT**, not because it
> was abandoned.
> ⛔ So it is un-killed forward intent ([DEC-keep-unkilled-ideas](../architecture/decisions.md#dec-keep-unkilled-ideas))
> and its code STAYS. ⚠ It is the standing exhibit for why the `#ifdef`-attic sweep
> ([AGENTS.md](../../AGENTS.md) Conventions §Design) is not a blanket delete: the guard is defined nowhere and
> every member it names appears nowhere else, so the mechanical test flags it exactly like an abandoned
> alternate — and deleting it would silently discard a wanted feature. **What separates them is whether a
> commented-out `#define` exists**; this one has one.
- **Per-turn full scratch rebuild** (`doTurn`, `CvGame.cpp:6002`) zeroes ALL counts then replays every sight source —
  only the post-rebuild state is authoritative ("a stickytape"). **Invisibility:** `alwaysInvisible = info.isInvisible()
  || alwaysInvisibleCount>0`; without Hide-and-Seek, invisible if `invisibleType != NONE && !spotterInSight`; with
  `COMBAT_HIDE_SEEK`, intensity-based. Stolen visibility = `stolenVisibilityTimer > 0`. Gotcha: `/units` exposes ALL
  units with no per-viewer invisibility filter.

## Trade routes (city)

- **Slot budget** = `game + player + (coastal? player.coastal : 0) + city.extra`, capped `[0, getMaxTradeRoutes]`.
  `updateTradeRoutes` runs **eagerly on every modifier change, not in doTurn**; candidates gate on
  `canHaveTradeRoutesWith` (diplomacy) + same `CvPlotGroup` (connectivity).
- **Profit** = `min(theirPop·POPULATION_TRADE_PERCENT, dist·world_TradeProfitPercent)·TRADE_PROFIT_PERCENT/100`, floor
  100, × `totalTradeModifier` (100 + route/pop/team + `CAPITAL_TRADE_MODIFIER` if connected-to-capital +
  `OVERSEAS_TRADE_MODIFIER` if different area + foreign/peace/civic).
- **`CvPlotGroup` is the bonus-connection oracle:** `hasBonus(eBonus)` = the city's plot-group (**per-player**, not
  per-team) holds ≥1 — this **is** the `requires.operate` resource-dormancy gate. `isConnectedToCapital` = same plot
  group as the capital.

## Diplomacy / attitude (AI)

- `AI_getAttitudeVal(player)` ∈ [−100, 100], lazily cached, thresholded to `AttitudeTypes` (0 Furious … 4 Friendly);
  short-circuits: NPC → −100, same team / uncapitulated vassal → 100. **~26 additive components** (peace-weight match
  `4 − |Δ|`, warmonger-respect min, war/peace/religion/civic/trade counters each `counter/divisor`).
- **Memory ledger** `memoryCount[P][type]` (44 types), each `count × leaderPercent/100`, probabilistic per-turn decay
  (`AI_RUTHLESS` → `iRand/=3`; `REALISTIC_DIPLOMACY` → `iRand/=1+count`). Team counters tick in `AI_doCounter`, player
  counters in `AI_doDiploCounters`. **Attitude is the master variable routing every AI diplomatic decision.**

## Victory

- `CvGame::testVictory()` every turn (grace: bail if `< speedPercent/10` turns elapsed). Checks: Time/Score, Conquest
  (no other alive non-vassal team has cities), Domination (X% pop AND Y% land), Religious, Cultural (N cities at a
  level), Scientific/Building (≥ threshold of a building), Space (every project ≥ `getVictoryMinThreshold`).
- **`starshipLaunched[team]` is a one-way latch** — set on the first Space pass; later passes skip the project check.
  A cascade re-deriving project counts **must preserve this latch** or the launch is wrongly rescinded.
- **Countdown** = `VictoryInfo.getVictoryDelayTurns × speedPercent/100` (+ extension for partial space projects), −1/turn
  while the condition holds; at 0, `rand(100) < getLaunchSuccessRate` gates the win (failure → `resetVictoryProgress`).
  Mastery/Total clears all other winners (by `getTotalVictoryScore`); Mercy Rule: a team > half the global total wins after a countdown.

## See also

- [engine.md](engine.md) (pathfinding/plot-groups, the save latches) · [economy.md](economy.md) (war-weariness/espionage
  timers) · [../specs/tally.md](../specs/tally.md) (counters/EP are tally domains) · [../specs/enabler.md](../specs/enabler.md) (plot-group dormancy gate).
