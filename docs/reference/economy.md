# Economy reference — maintenance, upkeep, happiness, health, war-weariness, pollution

> The per-subsystem **mechanics the validator re-derives**. Behaviour as-is today; the cascade
> ([modifier](../cascade.md)/[tally](../specs/tally.md)) replaces these maintainers (verified live in-game)
> ([spine.md § What to log](../spine.md)).

## Gold expense (player)

`getFinalExpense = calculatePreInflatedCosts() × getInflationMod10000()/10000` (suppressed during anarchy).
**Six additive pre-inflation components:** treasury upkeep + total maintenance + civic upkeep + unit upkeep + unit
supply + corporate maintenance.

- **Treasury tax** (anti-hoarding): `(gold + 250·√gold) / (25 · gameSpeedPercent)`.
- **City maintenance is ORDINARY CASCADE, end to end — no engine components left.** Distance, city count and
  colonial separation are AUTHORED DEPOSITS on `TECH_GAME_START`'s `maintenance` block (the universal start node
  every civ holds — the same baseline home `canTradeOn` uses, [capabilities.md](../specs/capabilities.md)):
  distance scales `per: DISTANCE_TO_GOVERNMENT_CENTER`,
  city count `per: {CITY, empire, above: 1}`, colony rides `enabled: "!IS_HOME_AREA"` — the predicate
  [json.md §3.5](../specs/json.md) minted for exactly this.
  ⚖ **TWO TIERS, and the order is the mechanic:** each component KIND resolves against its OWN modifiers (a
  handicap authoring `maintenance.empire.distance.percent` scales distance without touching corporation), and
  the TOTAL then takes the empire-wide `amount` stack. Flattening the two would cross-apply every kind's modifier.
  ⚑ **The rebel discount is ONE authored entry** (`maintenance.empire.percent −50, enabled: "IS_REBEL"`),
  replacing four separate hardcoded halvings.
  ⛔ **THE COMPONENTS ARE THOSE THREE — `MAINTENANCE_CORPORATION` IS NOT A CITY COMPONENT.** It is its own
  pre-inflation expense (the sixth component above, `calcCorporateMaintenance`), so the city total SKIPS it.
  ⚠ The trap: its deposit is a city-scope FLAT (the corp's own per-city gold amount), so it lands in the city's
  package exactly like the other three — a read that loops every maintenance kind double-counts it. Skip the
  kind explicitly; do not infer the component set from the enum.
  ⚠ **This EXPRESSES THE INTENT, it does not reproduce the legacy curve.** Size-scaling went
  multiplicative → additive; the colony quadratic and the corp handicap square went linear; the 2,000,000 cap
  and the vassal-cities term are gone — a behaviour change to STATE and weigh
  ([validation.md](../specs/validation.md): the spec leads), never to preserve for its own sake.
  The city's realized value is a BARE PACKAGE READ; nothing is cached, because no formula's result needs it.
  **The empire total is the Σ over its cities' realized values, re-summed at the read** — no stored receiver
  slot holds it ([cascade.md](../cascade.md) § A CROSS-SCOPE RECEIVER TOTAL) —
  the one non-commerce receiver with no cache of its own.
  > **⛔ THE ONE SPECIAL CASE MAINTENANCE HAS OVER ANY OTHER CASCADE CHANNEL: a city emits 0 instead of
  > its package while WE LOVE THE KING DAY or DISORDER holds** — sent to the rest of the cascade only if no
  > status negates it.
  > ⚑ **It suppresses CONSUMPTION of the value, never its contents — so neither is a cache input and neither
  > marks it.** WLTKD is a
  > ONE-TURN status re-applied every turn ([state.md](../specs/state.md)); marking on it would thrash the cache
  > every turn over a number that never moved — the stored value stays real, and the read just declines to
  > forward it.
  > ⚑ `isDisorder()` is the OR of two ticking counters — the city's occupation timer and the player's anarchy
  > turns, a CITY status and a PLAYER status composed into one verdict
  > ([CvStatus.h](../../Sources/Engine/CvStatus.h)). The legacy `population > 0` guard is dropped.
  > ⚖ **There is NO effective-modifier sum to maintain, and no area surface.** The percent stack IS the roll-up
  > over the chain the city sits under (team + empire + city), so the hand-summed city + player + area +
  > connected-city legs collapse into one read. Three of those legs were never kinds but CONDITIONS wearing a
  > member's name ([conditions are predicates, never bespoke members](../specs/json/03-the-shared-vocabulary.md#35-predicates--a-systems-runtime-state-query)):
  > `coastalDistance` is *while coastal*, `connectedCity` is *while connected to the capital*, and
  > `homeArea`/`otherArea` IS `IS_HOME_AREA` — *"maintenance increases in another area"* is literally "this
  > city's area is not the capital's" ([json.md §3.5](../specs/json.md)), which is why `CvArea` carries no
  > maintenance surface at all (a landmass is not an ownable scope,
  > [cascade.md](../cascade.md)).
  ⛔ **The COLONY CAP is GONE — data, kind and getter.** It bounded the colony component as a RATIO of the
  distance component, a cross-component bound the grammar cannot express, so it went with the quadratic:
  `MAINTENANCE_CAP`, `CvHandicapInfo::getColonyMaintenanceCap` and the `iMaxColonyMaintenance` curator row are
  all deleted, and the curator DROPS the legacy tag rather than parking it in `identity`, which carries no
  effects ([json.md §7](../specs/json.md)).
- **Civic upkeep** = `max(0,(pop+offset)·popPct/100) + max(0,(cities+offset)·cityPct/100)`, handicap-scaled.
- **Inflation** = `100 · hurriedCount · handicapInflationPct/100`, × civic/tech/building/event/rebel chain; decays per `HURRY_INFLATION_DECAY_RATE`.
  > **⛔ INFLATION IS NOT ACTUALLY USED IN THE GAME, AND #430 DOES NOT REMODEL IT — IT IS A CONSCIOUS DECISION
  > TO CUT AND LIVE WITH THE CONSEQUENCES.** The mechanic above is what the engine still COMPUTES; it is
  > not a model anyone is standing behind. ⇒ A gap found in it is **not** a defect to repair and **not** a
  > wiring job: the correct action is the ordinary cut, and the consequence is accepted.
  > **⚖ WHEN IT RETURNS IT IS A CASCADE CHANNEL DRIVEN BY ACTUAL EXPENDITURE** — *"we need to have a
  > real plan for how it is to be modelled based on actual expenditure."* That is the whole of the forward
  > direction, and it is the part that does not exist yet: today's formula keys on **`hurriedCount`** — how often
  > you RUSHED — which is a proxy for spending rather than spending, so no amount of re-wiring the present shape
  > reaches the intended one.
  > ⛔ **So do NOT re-point an inflation read onto a cascade kind to "finish" it.** Wiring a live read onto a
  > mechanic that is being replaced whole is the half-migration
  > ([build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)), and the plan the replacement
  > needs is a DESIGN decision the owner has not taken — inventing one is the rollerskate
  > ([the no-guessing rule](../../AGENTS.md#conduct)).
  > ⚠ **THREE UNRELATED ADDRESSES SHARE THE WORD, and that is worth knowing before touching any of them** — a
  > reader who checks one concludes the wrong thing about the others:
  >
  > | authored address | kind | what it actually is |
  > |---|---|---|
  > | `inflation.<scope>.percent` | `SCALAR_INFLATION` (memberless, empire) | the modifier CHAIN on the expense — live, read by `getInflationMod10000` |
  > | `upkeep.empire.inflation.percent` | `UPKEEP_INFLATION` | the handicap's inflation leg — live, read by the same function |
  > | `hurry.empire.inflation.percent` | `SCALAR_HURRY_INFLATION` | the modifier on the hurried-count DECAY rate — **now read by NOBODY** |
  >
  > ⚑ The third lost its only consumer when the stranded `hurryInflationModifier` accumulator was cut
  > ([the uniform legacy-accumulator cut](../cascade/03-no-staleness-no-selfheal.md#-the-legacy-accumulator-cut--every-accumulator-one-uniform-mechanism)); its kind and its
  > two civic authorings STAY, inert, because purging them is the remodel's call and not a tidy-up.
- **Per-turn order:** `verifyGoldCommercePercent` (silently raises the gold slider on deficit) → `doGold` (strike +
  forced-disband when gold < 0) → `doAdvancedEconomy` (inflation decay, unmodified).
- **⚑ Cascade fold:** negative-gold buildings route to **`maintenance.city.flat`** (NOT `gold.flat`) — this brought
  the maintenance divergence to 0. (Gap to close: crime/ordinance pseudobuildings — classify as maintenance vs negative commerce.)

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
  > **⛔ UPKEEP *IS* MAINTENANCE — THERE IS NO DIFFERENCE; IT ONLY COMES FROM UNITS INSTEAD OF CITIES.**
  > It is the same expense, subtracted from gold earned at the end, and it therefore rides the SAME empire
  > receiver rather than a parallel one. The receiver rule is unchanged — a cross-scope total is the Σ of its
  > MEMBERS' realized values ([cascade.md](../cascade.md)) — and the only
  > thing that varies is WHICH members: maintenance sums the player's cities, upkeep sums its units. So the
  > empire's Σ has two legs into one slot.
  > ⛔ Do NOT mint a second receiver for it. Reading "upkeep" as its own machine is what produces a parallel
  > expense plane, a second bit in the receiver region, and a double-count at `getFinalExpense` the moment both
  > are added — the components are ONE total, not two.
- **Supply** = `max(0, outsideUnits)·75/100·(era+1)` × `distantUnitSupportCostModifier` × AI-handicap; 0 in anarchy/NPC.

## Happiness + health (city)

- **Net happiness** = `happyLevel − unhappyLevel`; negative → `angryPopulation`. Bypass: `isNoUnhappiness` zeroes
  unhappy entirely — the city's `abolishedAnger` fold, so a capital-only grant is the grantor's own `IS_CAPITAL`
  condition resolving at fold time rather than a second test beside it ([modifier.md §2b](../cascade.md)).
- **Percent-anger** (scale with pop via `angerPct·pop / PERCENT_ANGER_DIVISOR(1000)`): overcrowding, no-military,
  foreign-culture, enemy-religion-war, hurry/conscript/defy/rev timers, war-weariness, rev-index (only when > 325),
  civic. **Flat anger** (additive): buildings, features, bonuses, religion, commerce, area/player buildings, extra,
  handicap, vassal, espionage, specialists, world, tax, corp, event, foreign, landmark, over-limit.
- **Health** = `min(0, goodHealth − badHealth)` (always ≤ 0); `unhealthyPopulation = max(0, pop − angryPop)` (unless
  `isNoUnhealthyPopulation`). `foodConsumption = consumed − angryPop − healthRate` (sick cities eat more).
- **WLTK ("We Love the King/Emperor Day", civic-named text)** — cleared on occupation / anger / sickness; else
  stochastic (pop ≥ min, `pop-rand < WE_LOVE_THE_KING_RAND`); a random event may set it via the Python setter.
  **Sole gameplay effect is maintenance suppression** (§ Gold expense above — the city emits 0 instead of its
  package while it holds). The "no anger" half of the folklore is the TRIGGER condition, not an effect; everything
  else is cosmetic (fireworks, celebrate text). *(A prior claim here that it "doubles GPP" was FALSE — the
  exhaustive consumer sweep finds no such site. Distinct from the trait-fed "free-city yield" accumulator
  `m_aiFreeCityYield` — [modifier.md §2a](../cascade.md).)*
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

- **The C++ Global Warming machinery is GONE** — the `GLOBAL_WARMING` feature macro, `CvGame::doGlobalWarming`
  and its `doTurn` call, and the five orphaned `GLOBAL_WARMING_*` global defines are all removed. The feature
  member it read (`iWarmingDefense`) was already an owner-ruled curator DROP, so nothing authored fed it.
  ⚠ **The NUKE COUNTER is NOT one of them and STAYS.** `CvGame::getNukesExploded`/`changeNukesExploded` is raised
  by a real detonation (`CvPlot`), is serialized and is published to Python; it merely happened to be READ by the
  warming calc. Owner-ruled KEEP even with no C++ consumer — *"it's worth having"* — so a removal that follows
  the NAME rather than the WRITER zeroes a live counter.
  > **⛔ THE PYTHON GLOBAL WARMING IS NOT ALIVE EITHER — "it just pretends to be".**
  > `CvRandomEventInterface.doGlobalWarming`, its `TXT_KEY_EVENT_GLOBAL_WARMING*` text, the
  > `BUILDING_POLLUTION_*_GLOBAL_WARMING` pseudo-buildings and the event trigger that requires one are all still
  > in the tree and all still reachable — which is exactly why it reads as a live mechanic on inspection.
  > ⚑ **It shares NOTHING with the C++ half**: it reads none of the removed defines (its weights are hardcoded
  > 100 / 10000 / 1000000) and never called `doGlobalWarming` in the DLL. That independence is what made the C++
  > removal safe, and it is the fact to re-check rather than re-derive.
  > ⛔ It is NOT removed here: random events are a permanent, owner-ruled Python-authoritative carve-out, out of
  > scope for this data model. ⚠ So do not read its survival as evidence the mechanic works, and do not "restore"
  > a C++ leg to serve it.
- **Pollution is LIVE** via the [property solver](engine.md#properties--the-generic-attribute-bag--its-legacy-auto-placement) (propagators → interactions → sources).
  Rates (`CIV4PropertyInfos.xml`): city decay ~6%/turn + 1/pop/turn; city→plot ~5%, plot→city ~12%, plot→plot ~4%;
  target 0. **24 band buildings** (12 air, `POLLUTION_LIGHT_SMOG`@≥400 … `BLACKENED_SKIES`@≥1950; 12 water …
  `TOXIC_HYDROSPHERE`@≥1800). Legacy added/removed them per turn; they are now placed once and gated by a
  `requires.operate` PROPERTY band ([engine.md](engine.md#properties--the-generic-attribute-bag--its-legacy-auto-placement)).

## See also

- [engine.md](engine.md) — the property solver + the save checksum these feed.
- [../specs/modifier.md](../cascade.md) / [../specs/tally.md](../specs/tally.md) — the machines that replace these maintainers.
