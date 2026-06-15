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

## Civilization  (`curate_civilization.py`)

A source entity: game-start `grants` + per-civ `policies` + one modifier. Most fields are natural (policies
`bPlayable`→`playable`; art/identity de-Hungarianized). Notable mappings:

| old XML | new JSON path | note |
|---|---|---|
| `FreeTechs` / `FreeBuildings` / `InitialCivics` | `grants.{techs,buildings,civics}` | One-shot game-start provision (capital buildings, one starting civic per slot, free techs). FreeTechs is Neanderthal-only. |
| `iSpawnRateModifier` / `iSpawnRateNPCPeaceModifier` | `spawnRate.empire.{general,npcPeace}.percent` | The one cascade modifier (barb/NPC civs only). |
| `DisableTechs` | `disables.techs` | Per-civ REVERSIBLE research ban (a v0.3 `disables`, NOT a permanent removal — owner; no in-game reverse/apply logic exists, it's ONE hardcoded case: Neanderthal barbarians can't research `TECH_SEDENTARY_LIFESTYLE`). `disables` mirrors `grants`, extensible to other kinds. |
| `Cities` / `Leaders` / `DerivativeCiv` | `identity.{cityNames,leaders,derivativeCiv}` | City-name auto-naming pool (STAYS — integral to a city getting a name on founding), civ↔leader eligibility, civ-split lineage. Individual city names whose special characters break the game-matching encoding are DROPPED (they're broken in-game too — if the toolkit can't encode it, the loader can't load it): 1 dropped (`KōZUKE`). |

## Tech  (`curate_tech.py`)  — Tier B #13, the SPINE ROOT (943 records)

Tech is the constructive generator: nearly every cross-entity `enables`/`obsoletes` edge is store-inverted ONTO
the tech (keyed by the thing you HAVE), so most fields don't appear on the tech file at all — they live on the
targets' generation. The tech's OWN fields split into: the **`requires.build` CONFIRM** (its prereqs, read back
off the child — §13.8); **downward modifier deposits** (`TECH_BOOSTS`, the entity-targeted `Tech*Changes` inverted
onto the tech, kept-on-source per CREST §6); de-Hungarianized `cost`/`art`/`ai`/`identity`. **EXE-link: 0
`DllExport` on `CvTechInfo` — unconstrained** (data is free; the engine readJson-maps it). The trade-enabler
bool→channel renames (`bTechTrading`→`techTrading`, `bIrrigation`→`irrigation`, …) are mechanical mapping
`channel` entries (mapping/TechInfo.json), not re-logged. Structural / manual mappings:

| old XML | new JSON path | note |
|---|---|---|
| `AndPreReqs/PrereqTech` | `requires.build.all[].{type,scope:team}` | The tech-tree multi-parent AND (enabler-spec §3/§13.8). Authored from the tech's OWN child record (the store flattens these into OTHER techs' `enables.techs` for generation, but does NOT retain the child's grouping — so the curator reads them back). Tech presence is TEAM-scope, binary → atom carries NO `min`. The flat `enables.techs` STAYS (generation); `requires.build` is the per-candidate CONFIRM (the two coexist). |
| `OrPreReqs/PrereqTech` | `requires.build.any[][]` OR-group **OR** folded into `.all` | An OR-group (at-least-one). **A single-member OR-group FOLDS into `all`** ("at least one of {X}" ≡ "X required") — lossless, and 934 of 939 techs have a 1-member `OrPreReqs`, so the fold keeps the output clean; only 5 genuine multi-way ORs stay in `any`. `any` is a LIST OF OR-GROUPS (each AND-ed with the rest), so a tech OR-group and a building OR-group remain distinct requirements (modifier-spec §3 nested form). |
| `PrereqOrBuildings/PrereqOrBuilding` {`BuildingType`,`iNumBuildingNeeded`} | `requires.build.any[][].{type,scope:empire,min}` | **Was DROPPED — now captured.** A LIVE research gate (`CvPlayer::hasValidBuildings`→`canResearch`): need ≥`iNumBuildingNeeded` of one of these buildings. `getBuildingCount` is empire-wide → `scope:empire`; buildings ARE count-capable → explicit `min`. (Only 2 techs: waterproof-concrete / lead-glass.) The AND form `PrereqBuildings`→`requires.build.all` is handled too (no data today). |
| `FreeSpecialistCounts` {`SpecialistType`,`iFreeSpecialistCount`} | `freeSpecialists.empire.specialists.{SPECIALIST}.flat` | **Was SILENTLY DROPPED** (mapped as a scalar channel, but it's a Type-keyed container). An UNWIRED MODIFIER (modifier-spec §8-ii): read by AI valuation (`CvPlayerAI:4628`) + pedia (`CvGameTextMgr:12098`) but never granted in `processTech` → REVIVED as a modifier. Emitted via new generic keyed-container support in `curate_common.apply_channel` (mapping `targetType` hint). 1 tech (TECH_DERIVATIVES). |
| `Tech{Yield,Commerce,Happiness,Health,Specialist}Changes` etc. (on Building/Specialist/Improvement/Route) | `<family>.<scope>.<targetType>.{TARGET}.…` ON THE TECH | DOWNWARD deposits — the entity-targeted `Tech*Changes` are inverted onto the tech (the conditioner/source), keyed by the target they boost (kept-on-source, CREST §6 / §0.4). `RouteInfo.TechMovementChanges`→`movement` is the modifier-spec §8 "→ tech (inverted)" case. `iCost`→`cost.research`. |
| `FirstFreeUnit` / `FirstFreeProphet` / `iFirstFreeTechs` | `grants.{firstFreeUnit,firstFreeProphet,freeTechs}` | FIRST-TO-RESEARCH race rewards (not universal grants) — bespoke keys preserve the "first" semantics pending the §10-banked `firstToResearch` predicate. ⚑ FLAG: differs from the enabler-spec §6 plain `grants.{units,techs}` lists; owner call whether to restructure later. |
| `iAsset` / `iPower` | `identity.{worth,militaryWorth}` | Applied on research as player assets/tech-power (`processTech changeAssets/changeTechPower`), but score-like → identity (engine.FIELD_RENAME). |
| `bDisable` | `identity.disable` | ⚑ FLAG: a LIVE unconditional research kill (`canEverResearch` hard-false); only `TECH_DUMMY`. Load-stable disable → arguably `loadPrune`-like, but doesn't fit its `{onGameOptions}` shape; kept faithfully in identity pending an owner home decision. |
| `bGlobal` | `identity.global` | ⚑ FLAG: a LIVE global-uniqueness research RACE gate (religion techs — `canEverResearch`: only if no other team has it & not NPC, `CvPlayer:8268`). 29 techs. Doesn't fit `enables`/`requires` (needs a world-scope "no other team has it" NOT); kept faithfully in identity, flagged as not-fitting-the-locked-vocabulary. |
| `bRepeat` | `identity.repeat` | Repeatable tech (Future Tech). Faithful flag. |
