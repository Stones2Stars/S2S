# Economy reference — maintenance, upkeep, happiness, health, war-weariness, pollution

> Lifted + condensed from the old observability docs. The per-subsystem **mechanics the validator re-derives**.
> Behaviour as-is today; the cascade ([modifier](../specs/modifier.md)/[tally](../specs/tally.md)) shadows then
> replaces these maintainers ([logging](../specs/logging.md) §6).

## Gold expense (player)

`getFinalExpense = calculatePreInflatedCosts() × getInflationMod10000()/10000` (suppressed during anarchy).
**Six additive pre-inflation components:** treasury upkeep + total maintenance + civic upkeep + unit upkeep + unit
supply + corporate maintenance.
- **Treasury tax** (anti-hoarding): `(gold + 250·√gold) / (25 · gameSpeedPercent)`.
- **City maintenance** = era floor + modified base; suppressed on disorder/WLTK. Base = distance (distance×pop, 0
  at the government center) + numCities `((n−1)·72·(pop+13)/13`, vassal-divided) + colony + corporation +
  building-gold (when `TREAT_NEGATIVE_GOLD_AS_MAINTENANCE`). Effective modifier = city + player + area +
  (connected & ¬capital ? connectedMod : 0).
- **Civic upkeep** = `max(0,(pop+offset)·popPct/100) + max(0,(cities+offset)·cityPct/100)`, handicap-scaled.
- **Inflation** = `100 · hurriedCount · handicapInflationPct/100`, × civic/tech/building/event/rebel chain; decays per `HURRY_INFLATION_DECAY_RATE`.
- **Per-turn order:** `verifyGoldCommercePercent` (silently raises the gold slider on deficit) → `doGold` (strike +
  forced-disband when gold < 0) → `doAdvancedEconomy` (inflation decay).
- **⚑ Cascade fold:** negative-gold buildings route to **`maintenance.city.flat`** (NOT `gold.flat`) — this brought
  the maintenance divergence to 0. (Open post-cutover: crime/ordinance pseudobuildings — maintenance vs negative commerce.)

## Unit upkeep + supply (player)

- Per-unit upkeep `max(0, 100·base + extraUpkeep100)` × `m_iUpkeepModifier` (unit-combat + promo, additive %) ×
  `m_iUpkeepMultiplierSM` (Size-Matters rank). Stored `m_iUpkeep100`; the delta is written to player accumulators on
  every change (create / promote / remove).
- **Two player accumulators — `m_iUnitUpkeepCivilian100` / `m_iUnitUpkeepMilitary100`, bucketed by
  `UnitInfo.isMilitarySupport()`** — this is the engine side of the `military` [tag](../specs/tags.md) /
  `militarySupport` [skill](../specs/skills.md) reclassification. Modifier: gross × `(100+mod)/100` if +,
  × `100/(100−mod)` if −; free allowances subtracted after, floor 0. `getFinalUnitUpkeepChange(iExtra, bMilitary)`
  *temporarily* mutates the accumulators for marginal-cost AI valuation.
- Final = `(civilianNet + militaryNet) × handicapPct/100 × AI-handicap × era-scale`, 0 for NPCs.
- **Supply** = `max(0, outsideUnits)·75/100·(era+1)` × `distantUnitSupportCostModifier` × AI-handicap; 0 in anarchy/NPC.

## Happiness + health (city)

- **Net happiness** = `happyLevel − unhappyLevel`; negative → `angryPopulation`. Bypass: `isNoUnhappiness` zeroes
  unhappy entirely; `isCapital && isNoCapitalUnhappiness` too.
- **Percent-anger** (scale with pop via `angerPct·pop / PERCENT_ANGER_DIVISOR(1000)`): overcrowding, no-military,
  foreign-culture, enemy-religion-war, hurry/conscript/defy/rev timers, war-weariness, rev-index (only when > 325),
  civic. **Flat anger** (additive): buildings, features, bonuses, religion, commerce, area/player buildings, extra,
  handicap, vassal, espionage, specialists, world, tax, corp, event, foreign, landmark, over-limit.
- **Health** = `min(0, goodHealth − badHealth)` (always ≤ 0); `unhealthyPopulation = max(0, pop − angryPop)` (unless
  `isNoUnhealthyPopulation`). `foodConsumption = consumed − angryPop − healthRate` (sick cities eat more).
- **WLTK** cleared on occupation / anger / sickness; else stochastic (`pop-rand < WE_LOVE_THE_KING_RAND`). It waives
  distance + numCities maintenance AND doubles GPP.
- **Decaying timers** (−1/turn): hurry, conscript, defy, happiness, rev-request, rev-success, landmark; the WW city
  timer −20/turn; event anger −1 per `10·speedPct/100` turns; espionage counters −1/turn.

## War-weariness

- Stored `m_aiWarWearinessTimes100[MAX_TEAMS]` on `CvTeam` (per-enemy, ×100). Accrual `iRatio = 100·theirCulture /
  (ours+theirs)` × factor (event constants: attacker-killed 3, defender-killed 5, city-captured 6 *no ratio*,
  nuke-hit 20, nuke-use 10, …). Caps: rebel-vs-parent ≤ 40, raw rebel ≤ 60.
- Decay −1/turn always; at peace / dead-enemy additionally × `WW_DECAY_PEACE_PERCENT(99)/100` (fast melt).
- Player anger = `Σ getWarWeariness(e)·(100+mod)/1e6 × BASE_WAR_WEARINESS_MULTIPLIER(5)` × world-size × AI-handicap.
  City final = `player.WWanger × max(0,cityMod+playerMod+100)/100 × max(0,cityTimer+100)/100`.
- Espionage WW is a separate channel (city timer only, −20/turn). Alliance averages WW; vassal max-propagates.

## Pollution (live) — Global Warming (dead)

- **Global Warming is compiled OUT** (`// #define GLOBAL_WARMING`, `CvGameCoreDLL.h:232`) — `doGlobalWarming`,
  `getWarmingDefense`/`getNukesExploded`, all `GLOBAL_WARMING_*` defines are **dead vestiges** (the Pedia even shows a
  zero-effect stat). Removal is tracked (the old global-warming-mod plan).
- **Pollution is LIVE** via the [property solver](engine.md#properties--the-generic-attribute-bag--its-legacy-auto-placement) (propagators → interactions → sources).
  Rates (`CIV4PropertyInfos.xml`): city decay ~6%/turn + 1/pop/turn; city→plot ~5%, plot→city ~12%, plot→plot ~4%;
  target 0. **24 band buildings** (12 air, `POLLUTION_LIGHT_SMOG`@≥400 … `BLACKENED_SKIES`@≥1950; 12 water …
  `TOXIC_HYDROSPHERE`@≥1800), placed/removed by `checkPropertyBuildings` — a property-band maintainer the cascade replaces.

## See also
- [engine.md](engine.md) — the property solver + the save checksum these feed.
- [../specs/modifier.md](../specs/modifier.md) / [../specs/tally.md](../specs/tally.md) — the machines that replace these maintainers.
