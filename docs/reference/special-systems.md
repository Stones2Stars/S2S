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

> **⚖ PROMOTION VALUE IS EVALUATED ON A REAL UNIT ONLY — THE PRODUCTION DECISION DOES NOT ASK (owner).** A city
> choosing what to build weighs the free XP AMOUNT and nothing about what those levels would buy. The unit
> evaluates its own promotions once it EXISTS, off its resolved cache (`CvUnitAI::AI_promote`).
> ⛔ **The cost is the reason, and a better walk does not rescue it (owner): *"even though our promotion walk
> would be significantly more efficient, it's still wildly expensive."*** The question is per
> (city × candidate unit × promotion) on the hottest loop of the turn, so it is unaffordable however
> efficiently it is written — this is [DEC-turn-time-is-king](../architecture/decisions.md#dec-turn-time-is-king)
> deciding a feature, not an optimization to attempt.
> ⛔ **Nor can it be cached around: *"caching a theoretical promotion setup for all the units is unrealistic at
> best of times"*** — what would be cached is a hypothetical promotion set for units that do not exist, keyed by
> a candidate the city may never build.
> ⚑ **It may return later, and there is exactly ONE shape it may return in:** the traversal `AI_promote` already
> uses — the player's maintained unlocked-promotion set → live per-unit applicability → the ONE
> `AI_promotionValue` ([enabler.md §7.1](../specs/enabler.md)'s promotions carve-out;
> [DEC-single-implementation](../architecture/decisions.md#dec-single-implementation)). ⛔ Never a second walk,
> and never a whole-registry sweep per candidate.
> ⚠ **The XP term itself is a cascade FLAT (`EXPERIENCE_AMOUNT` is `CASC_UNIT_FLAT` at city scope), so every
> reader reduces at its point of use** ([DEC-fixedpoint-x100](../architecture/decisions.md#dec-fixedpoint-x100)).
> A reader that omits the `÷100` inflates free XP 100× against everything it is weighed beside — silently, since
> the result stays plausible.

## Vision & visibility (plot, per team)

- Per-plot per-team: `m_aiVisibilityCount` (>0 = visible now), `m_abRevealed` (ever-seen, permanent), `…LastSeenTurn`,
  `…StolenVisibilityCount`, `…InvisibleVisibilityCount`. `isVisible = visibilityCount>0 || stolenVisibilityCount>0`.
- **Sight range** = `1 + plot.getTerrainElevation() + getExtraVisibilityRange() + improvement.getVisibilityChange()`,
  cap `MAX_UNIT_VISIBILITY_RANGE` (elevation 0/1/2 = flat/hills/peak; air sees all).
> **⚖ FOG DECAY IS A REAL, RECENT FEATURE — WANTED, AND NOT IN THE TREE (owner).** It gives a **map that goes
> fully dark again where you have not been for a while**: the revealed-but-unseen tier decays instead of being
> remembered forever, riding the per-plot `…LastSeenTurn` above plus its own `m_iVisibilityDecay` /
> `m_iDefaultDecay` and the `m_bPermanentMapLand` / `m_bPermanentMapSea` opt-outs. **It is off because it BROKE
> HOTSEAT**, never because it was abandoned.
> ⛔ **It lived behind an `ENABLE_FOGWAR_DECAY` guard, and feature `#ifdef`s are banned outright** (owner:
> *"an anachronism from when source control was not really a thing"* — [AGENTS.md](../../AGENTS.md)
> Conventions §Design), so the guard and its arms are gone from `Sources/`. **Git is the archive: the last
> commit carrying the implementation is `624803b16`** — `git show 624303b16 -- Sources/Engine/CvPlot.cpp` and
> its siblings recover it in full.
> ⚑ **Reviving it means writing it as ORDINARY CODE, not restoring the switch** — the hotseat break is the
> thing to solve, and a guard would only hide it again. This entry exists so the intent survives the guard
> ([DEC-keep-unkilled-ideas](../architecture/decisions.md#dec-keep-unkilled-ideas) is about the IDEA, which is
> kept here; it never required the dead branch to sit in the tree).
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
