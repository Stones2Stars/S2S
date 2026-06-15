# #428 migration — RENAME REGISTRY (canonical old→new mapping)

**Every MANUAL/semantic rename during the XML→JSON migration is logged HERE (owner ruling 2026-06-15).** The
primary purpose is to keep the old↔new mapping **unambiguous for the pass when we update the C++ `readJson`
readers** — a reader must be able to look up exactly which old XML tag a new JSON key came from. A manual rename
must never live only in a curator's head or a single docstring. Add the entity's rows the moment you author its
curator (the cold-modder rule means new names are chosen for clarity, so the old↔new trail is exactly what the
readers pass — and modders/pedia — need).

**Two kinds of rename:**
- **Mechanical de-Hungarianization** (`iGridX`→`gridX`, `bTrade`→`tradeable`, `ArtDefineTag`→`icon`) is applied
  uniformly by `engine.de_i` / `engine.FIELD_RENAME` / `curate_common.{B_FLAG_NAMES,ART_RENAME,AI_BEHAVIOUR}`.
  Those shared maps ARE the documentation for the mechanical class — not re-logged per field here.
- **Semantic / structural renames** (the JSON key means something different, or a field is re-homed to a new
  family/section) are logged per entity below. These are the ones a reader can't infer mechanically.

---

## GameSpeed  (`curate_gamespeed.py`)

| old XML tag | new JSON path | note |
|---|---|---|
| `iSpeedPercent` | `speed.world.percent` | The master game-pace percentage (Normal=100 → 100%, Eternity=1000 → 1000%). Authored as the single value it is; the engine applies it across costs / durations / growth / culture (engine job, not data). An earlier pass fanned it into `costs`/`growth`/`durations`/`cultureThreshold` members — collapsed (cold-modder ruling). |
| `iUnitYieldScalePercent` | `missionYieldMultiplier.world.percent` | The multiplier (as a %) on yields a unit MISSION produces — a merchant's trade mission boosting another city, a subdued animal slaughtered for food/production (the `<AdaptUnitYield>` channel, ~sqrt of speed; Normal=500, Eternity=1575). Renamed from the non-descriptive `unitYieldScale` (owner, 2026-06-15). |

## Handicap  (`curate_handicap.py`)

⚠ The handicap STRUCTURE needs a future rework (out of scope for #428); the curator docstring is deliberately
verbose about each field's CURRENT meaning so that pass has the full picture. Names below marked PROVISIONAL may
change in the rework. The maintenance/upkeep MEMBER names (`distance`/`numCities`/`unit`/`civic`/… from
`iDistanceMaintenancePercent`/`iUnitUpkeepPercent`/…) are natural field→family.member splits, not re-logged.

| old XML tag | new JSON path | note |
|---|---|---|
| `iAIResearchPercent` | `techCost.empire.ai.percent` | **Manual semantic rename** — AI tech-research COST %. Renamed off `research` so it can't read as the research COMMERCE. PROVISIONAL. |
| `iAITrainPercent` / `iAIWorldTrainPercent` / `iAIConstructPercent` / `iAIWorldConstructPercent` / `iAICreatePercent` / `iAIWorldCreatePercent` | `buildCost.empire.{train,worldTrain,construct,worldConstruct,create,worldCreate}.ai.percent` | AI build-cost % per produced kind, grouped under a new `buildCost` family. PROVISIONAL family name. |
| `iAIPerEraModifier` | `perEra.empire.ai.percent` | META: a per-era ramp on the WHOLE AI-economy family (modifier-of-modifiers). PROVISIONAL — flagged as the least-natural field for the rework. |
| `iAIGrowthPercent` | `growth.empire.ai.percent` | AI city food-to-grow %. PROVISIONAL family name. |
| `iAIWorkRateModifier` | `workRate.empire.ai.percent` | AI worker build-rate %. PROVISIONAL family name. |
| `iHappyBonus` / `iHealthBonus` | `happiness.empire.flat` / `health.empire.flat` | Flat happy/health bonus in every city (dropped the `Bonus` suffix; `happy`→`happiness`). |
| `iMaxColonyMaintenance` | `maintenance.empire.colony.cap` | A hard CAP on the colony-maintenance component, carried as a `cap` member in the family structure (NOT a percent/unit). |
| `iRevolutionIndexPercent` | `revolution.empire.percent` | % into the Revolution index. INCOMPLETE mechanic (WIP, tracked), kept — NOT dead. |
| `iAttitudeChange` | `diplomacy.empire.attitude.flat` | Flat AI diplomatic-attitude shift (applied via the TARGET player's handicap). |
| `iNoTechTradeModifier` / `iTechTradeKnownModifier` | `diplomacy.team.noTechTrade.percent` / `diplomacy.team.techTradeKnown.percent` | Tech-trade availability thresholds (team scope). |
| `iSubdueAnimalBonusAI` | `combat.world.subdueAnimal.ai.percent` | AI subdue-animal odds bonus (game-global → `world`; AI audience). |
| `iFreeWinsVsBarbs` | `combat.empire.freeWinsVsBarbs.flat` | Per-player free combat wins vs barbarians. |
| `iAnimalBonus`/`iAIAnimalBonus`, `iBarbarianBonus`/`iAIBarbarianBonus` | `combat.world.{animal,barbarian}[.ai].percent` | Wildlife/barbarian combat-odds modifiers (game-global; base = vs-human, `ai` = vs-AI). |
| `iGold`, `iStarting{Defense,Worker,Explore}Units` (+ `iAIStarting…`) | `grants[.ai].{startingGold,startingDefenseUnits,startingWorkerUnits,startingExploreUnits}` | One-shot game-start provisioning → `grants` (humans/AIs split via own vs game handicap). |
| `iAdvancedStartPointsMod` / `iAIAdvancedStartPercent` | `identity.advancedStart.{pointsMod,aiPercent}` | Pre-game points-budget mod — NOT a modifier; parked in identity pending an advanced-start review. |

## Era  (`curate_era.py`)

The per-cost-type members (`train`/`construct`/`create`/`research`/`build` from `iTrainPercent`/… — each a
DISTINCT per-era field) are natural field→member splits, not re-logged. Manual/semantic renames:

| old XML tag | new JSON path | note |
|---|---|---|
| `iCuttingEdgeCutsTechCostModifier` | `costs.world.researchCutBelowEra.percent` | A summed-across-era-bands tech-cost CUT applied at the additive-mod stage (CvTeam:2627/2648), kept a DISTINCT member so the reader doesn't fold it into the research base. |
| `iImprovementPercent` | `costs.world.improvementUpgrade.percent` | Improvement UPGRADE-time scale (clarified from the bare `Improvement`). |
| `iGreatPeoplePercent` | `greatPeopleRate.world.percent` | Great-people RATE scale (`greatPeople`→`greatPeopleRate`). |
| `iAnarchyPercent` | `durations.world.anger.percent` | Anarchy DURATION scale — re-homed into the shared `durations` family (member `anger`), matching GameSpeed/the duration concept. |
| `iEventChancePerTurn` | `eventChance.world.flat` | Per-turn random-event chance. |
| `bNoAnimals` | — (DROPPED) | Dead as an era field; relocating to a game/BUG option (existing issue). See curate_era docstring for the likely era-gated intent. |
| `bNoGoodies` / `bNoBarbUnits` / `bNoBarbCities` | — (deferred) | LIVE C++ world-state gates, unset in all eras → not emitted; world-state-section home deferred to the Vote pass. |

## Victory  (`curate_victory.py`)

Victory is a PURE-config entity (no modifiers, no enables). Its win-condition fields are gathered under a
bespoke **`condition`** section — kept as-is; there is NO formal "cascading config section" concept and adding
one is a SEPARATE planning effort (owner 2026-06-15), so `condition` stays a bespoke section, not a spec change.
The condition keys are natural de-Hungarianizations (`bConquest`→`conquest`, `iLandPercent`→`landPercent`, …),
not re-logged. One non-obvious rename:

| old XML tag | new JSON path | note |
|---|---|---|
| `iVictoryDelayTurns` | `condition.delayTurns` | Space-race travel delay before the win triggers (dropped the redundant `Victory` prefix inside a victory file). |
| `CityCulture` | `condition.cityCulture` | A `CULTURELEVEL_*` ref (the culture level each of `numCultureCities` must reach) — forward ref to CultureLevel. |

## Vote  (`curate_vote.py`)

Vote (a diplomatic PROPOSAL for the UN + Apostolic Palace + Congress of Vienna) is self-contained config for the
EXISTING vote subsystem — NEITHER cascade (its `effect` bools are on-pass OUTCOMES handled by `processVote`, not
modifiers/enablers). Its keys are bespoke config groupings, not modifier families. Condition/effect bool keys are
natural de-Hungarianizations (`bFreeTrade`→`effect.freeTrade`, `iPopulationThreshold`→`threshold.population`, …),
not re-logged. Structural section choices:

| old XML | new JSON path | note |
|---|---|---|
| `DiploVotes`/`DiploVote` | `voteSource` | Which council may raise it: `DIPLOVOTE_UN`/`POPE`/`CVIENNA` = UN / Apostolic Palace / Congress of Vienna. |
| `bSecretaryGeneral`/`bVictory` | `role` (`secretaryGeneral`/`victory`) | The resolution CLASS (election / diplo-victory) — XOR with `effect`. |
| the on-pass toggles + `iTradeRoutes` + `ForceCivics` | `effect.{…}` | The OUTCOME payload applied by `processVote` (not a cascade). |
| (entity name `Vote`) | — | A future rename to `DiplomaticProposal` is DEFERRED (owner 2026-06-14); not done now. |

## CultureLevel  (`curate_culturelevel.py`)

A per-city-level conditioner (enables buildings via `PrereqCultureLevel` → `enables.buildings`, store-inverted).
Wonder caps (`iMaxWorldWonders`→`identity.maxWorldWonders`, …) and `iCityRadius`→`identity.cityRadius` are natural
de-Hungarianizations into identity (caps/overrides, not modifiers). Structural / manual renames:

| old XML | new JSON path | note |
|---|---|---|
| `iCityDefenseModifier` | `defense.city.amount.percent` | The one live modifier — extra city-defense % at this level. Re-homed into the `defense` family, member `amount` (the sibling `min` floor is authored at the Building pass). |
| `SpeedThresholds` | `identity.cultureThreshold` (Normal base only) | COLLAPSED: the per-speed table was redundant `base × GameSpeed.iSpeedPercent/100`. Keep only the Normal base; reader derives per-speed via **`GameSpeed.speed.world.percent`** (note: GameSpeed was collapsed to `speed` in info #1 — the old `cultureThreshold` member it referenced is gone). Lossless (0 non-geometric overrides). |
| `PrereqGameOption` | `loadPrune.onGameOptions` | Load-stable game-option availability gate → the enabler-spec §6 `loadPrune` section (NOT parked in identity). |
| `ReplacementID` / `ReplacementCondition` | `replacedBy.{cultureLevel,onGameOption}` | The CvInfoReplacements conditional whole-Info swap (CULTURELEVEL_POOR → ALT_POOR under a game option), store-detected. Distinct from the `replaces` enables-family member. |

## Hurry  (`curate_hurry.py`)

Tiny config entity (the 2 production-RUSH mechanics). The rush BASE rates are intrinsic config (the
`BuildingInfo.iHurryCostModifier` percent acts on them — they are NOT deposited modifiers), gathered under a
bespoke `conversion` section.

| old XML | new JSON path | note |
|---|---|---|
| `iGoldPerProduction` | `conversion.goldPerProduction` | Gold spent per hammer rushed (the gold-rush rate; HURRY_GOLD). Mutually exclusive with the pop rate per hurry. |
| `iProductionPerPopulation` | `conversion.productionPerPopulation` | Hammers gained per population sacrificed (the pop/slavery-rush rate; HURRY_POPULATION). |
| `bAnger` | `causesAnger` | Using this hurry inflicts temporary hurry-anger (manual rename for clarity). |

## BonusClass  (`curate_bonusclass.py`)

A pure structural bonus-CATEGORY axis (the categorization is consumed bonus-side via `bonusClassType`). The class
entity carries exactly ONE field; classes with no constraint emit a bare `{type}` (faithful — the category exists).

| old XML | new JSON path | note |
|---|---|---|
| `iUniqueRange` | `mapGeneration.uniqueRange` | Min map-gen spacing preventing same-class bonuses stacking (a C2C_World mapscript feature, `CvMapGenerator:60-101`). Re-homed from a parked `identity` into the `mapGeneration` group (parallels the bonus's own uniqueRange). 0 = no constraint, dropped. |

## Property  (`curate_property.py`)  — FIRST PASS, second pass expected

PropertyInfo defines the channels; the goal is to make properties FIRST-CLASS. Full decomposition in the curator
docstring. Structural mappings (not simple renames):

| old XML | new JSON | note |
|---|---|---|
| `PropertyManipulators` `PropertySource` (DECAY) | `<PROPERTY>.<scope>.percent` | The "decay" sources are MODIFIERS (poorly named) — per-turn change toward `targetLevel`. Self-deposit into the channel's own family. |
| `PropertyBuildings` {building,min,max} | `grants.buildings` [plain building-type list] (here) ; building-side `requires` (Building pass) | The effect-buildings are GRANTED (not enabled) — `grants.buildings` is a PURE LIST. `grants` and `requires` are SEPARATE reserved sections: the `requires` (WHEN active — the value-band atom `{type:PROPERTY_X, scope:city, min?,max?}`) belongs to the BUILDING's own `requires` section, authored at the Building pass (min/max from PropertyBuildings, store-accessible), NOT mixed into grants, NOT on the property. Every `requires` atom is full/explicit/self-describing. Pattern + the UNIFORMITY LAW in enabler-spec §6.1. |
| `iTargetLevel` / `TargetLevelbyEraTypes` | `targetLevel` (+`.byEra`) | A GENUINE ISOLATED field, outside enabler/modifier (the decay equilibrium). |
| `iOperationalRangeMin/Max`, `PropertyPropagator` (DIFFUSE), `ChangePropagators` | — (DROPPED → #429) | The obsolete LEAKING mechanic. The unit→city emission re-homes as a containment deposit on the unit/building. |
| `bSourceDrain` / `bOAType` | `identity.{sourceDrain,oaType}` | Property-system behaviour flags (don't fit enabler/modifier) — parked pending the rework; `bOAType` likely near-dead (getter only). |
