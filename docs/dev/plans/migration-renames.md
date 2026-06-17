# #428 migration — RENAME REGISTRY (canonical old→new mapping)

**Every MANUAL/semantic rename during the XML→JSON migration is logged HERE (owner ruling 2026-06-15).** The
primary purpose is to keep the old↔new mapping **unambiguous for the pass when we update the C++ `readJson`
readers** — a reader must be able to look up exactly which old XML tag a new JSON key came from. A manual rename
must never live only in a curator's head or a single docstring. Add the entity's rows the moment you author its
curator (the cold-modder rule means new names are chosen for clarity, so the old↔new trail is exactly what the
readers pass — and modders/pedia — need).

**Two kinds of rename:**
- **Mechanical de-Hungarianization** (`iGridX`→`gridX`, `bTrade`→`tradeable`, `Button`→`ui.art.icon`) is applied
  uniformly by `engine.de_i` / `engine.FIELD_RENAME` / `curate_common.{B_FLAG_NAMES,ART_BLOCK,AI_BEHAVIOUR}`.
  Those shared maps ARE the documentation for the mechanical class — not re-logged per field here.
- **Semantic / structural renames** (the JSON key means something different, or a field is re-homed to a new
  family/section) are logged per entity below. These are the ones a reader can't infer mechanically.

---

## Art blocks — `ui` / `world` / `sound` (#428 restructure, cross-cutting; DONE 2026-06-16)

All art/audio maps to the THREE **top-level** blocks (NOT a single `art` section; `art` is a SUB-block within
`ui`/`world` so non-art members sit beside it — modifier-spec §0.8). The **canonical tag→dotted-path map is
`curate_common.ART_BLOCK`** (it IS the documentation; the per-entity rows below no longer re-log art). cc-curated
entities route art through `curate()`; bespoke curators through the shared `curate_common.put_art`/`emit_art`
helpers — one shape everywhere. Empty/`NONE` audio dropped (`drop_empty_audio`). The block split resolves the
"icon headache" (UI `ui.art.icon` ← `Button` vs on-map `world.art.icon` ← `ArtDefineTag`), retiring the old
per-entity `art_rename` hack. The non-mechanical (semantic) art tokens worth logging:

| old XML tag | new JSON path | note |
|---|---|---|
| `Texture` | `ui.art.texture` | A 2nd DISTINCT UI icon (Specialist city-screen citizen icon), kept apart from `Button`→`ui.art.icon` — both are UI (interface `.dds`, neither on-map; verified consumers), and 2 specialists carry differing values so they must not collapse. |
| `DefaultPlayerColor` | `world.art.playerColor` | Civ render colour (an EXE-bound `int` FK), beside the civ's other `world.art` (icon/style/unitStyle). |
| `CreateSound` | `sound.onCompletion` | Project audio played when the project is COMPLETED (`CvCity.cpp:16205`) — named by what it is, not the misleading "create". (No project populates it today.) |
| `bFirstSoundtrackFirst` | `sound.introSoundtrack` | Era flag: lead with the era's first soundtrack on entry (`CvGameInterface.cpp:2766`). A sibling flag over `sound.soundtracks`; owner's better future shape is a soundtracks OBJECT with a per-track `firstPlayed` (out of scope). |
| `VictoryMovie` | `ui.art.movie.file` | Grouped under `movie` (file + sound together). |

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
`iCityRadius`→`identity.cityRadius` (a workable-radius override). The wonder caps are part of the ENABLING (the cap
gates how many of a category a city may hold), so they move into the declarative `allowed` ceiling — NOT identity (owner
2026-06-17). Structural / manual renames:

| old XML | new JSON path | note |
|---|---|---|
| `iMaxWorldWonders` / `iMaxTeamWonders` / `iMaxNationalWonders` | `allowed.{worldWonders,teamWonders,nationalWonders}` | **MOVED out of `identity` into `allowed`** (owner 2026-06-17): a culture level grants a city its per-city COUNT allowance of each wonder CATEGORY (the discriminator keys; `totalWonders` reserved, no source today). City scope implicit. The new canDoStuff gate enforces it (a category building drops from the city frontier when its count hits the allowance), reading the **tally**; engine owns ignoring it under `NO_WONDER_LIMIT`/`CHALLENGE_ONE_CITY`. enabler-spec §5a/§13.7. |
| `iMaxNationalWondersOCC` | — (DROPPED) | OCC forces wonder limits OFF in-engine (owner 2026-06-17); it carries no separate cap. Any future OCC-specific limit lives in the **game-option's own definition** / a game-option-specific JSON the engine loads on option-enable (override-by-design), never on every CultureLevel (out of #428 scope). |
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
onto the tech, kept-on-source per CREST §6); de-Hungarianized `cost`/`ui`(art)/`ai`/`identity`. **EXE-link: 0
`DllExport` on `CvTechInfo` — unconstrained** (data is free; the engine readJson-maps it). The trade-enabler
bool→channel renames (`bTechTrading`→`techTrading`, `bIrrigation`→`irrigation`, …) are mechanical mapping
`channel` entries (mapping/TechInfo.json), not re-logged. Structural / manual mappings:

| old XML | new JSON path | note |
|---|---|---|
| `AndPreReqs/PrereqTech` | `requires.build.all[].{type,scope:team}` | The tech-tree multi-parent AND (enabler-spec §3/§13.8). Authored from the tech's OWN child record (the store flattens these into OTHER techs' `enables.techs` for generation, but does NOT retain the child's grouping — so the curator reads them back). Tech presence is TEAM-scope, binary → atom carries NO `min`. The flat `enables.techs` STAYS (generation); `requires.build` is the per-candidate CONFIRM (the two coexist). |
| `OrPreReqs/PrereqTech` | `requires.build.any[][]` OR-group **OR** folded into `.all` | An OR-group (at-least-one). **A single-member OR-group FOLDS into `all`** ("at least one of {X}" ≡ "X required") — lossless, and 934 of 939 techs have a 1-member `OrPreReqs`, so the fold keeps the output clean; only 5 genuine multi-way ORs stay in `any`. `any` is a LIST OF OR-GROUPS (each AND-ed with the rest), so a tech OR-group and a building OR-group remain distinct requirements (modifier-spec §3 nested form). |
| `PrereqOrBuildings/PrereqOrBuilding` {`BuildingType`,`iNumBuildingNeeded`} | `requires.build.any[][].{type,scope:empire,min}` | **Was DROPPED — now captured.** A LIVE research gate (`CvPlayer::hasValidBuildings`→`canResearch`): need ≥`iNumBuildingNeeded` of one of these buildings. `getBuildingCount` is empire-wide → `scope:empire`; buildings ARE count-capable → explicit `min`. (Only 2 techs: waterproof-concrete / lead-glass.) The AND form `PrereqBuildings`→`requires.build.all` is handled too (no data today). |
| `FreeSpecialistCounts` {`SpecialistType`,`iFreeSpecialistCount`} | `freeSpecialists.empire.specialists.{SPECIALIST}.flat` | **Was SILENTLY DROPPED** (mapped as a scalar channel, but it's a Type-keyed container). An UNWIRED MODIFIER (modifier-spec §8-ii): read by AI valuation (`CvPlayerAI:4628`) + pedia (`CvGameTextMgr:12098`) but never granted in `processTech` → REVIVED as a modifier. Emitted via new generic keyed-container support in `curate_common.apply_channel` (mapping `targetType` hint). 1 tech (TECH_DERIVATIVES). |
| `Tech{Yield,Commerce,Happiness,Health,Specialist}Changes` etc. (on Building/Specialist/Improvement/Route) | `<family>.<scope>.<targetType>.{TARGET}.…` ON THE TECH | DOWNWARD deposits — the entity-targeted `Tech*Changes` are inverted onto the tech (the conditioner/source), keyed by the target they boost (kept-on-source, CREST §6 / §0.4). `RouteInfo.TechMovementChanges`→`movement` is the modifier-spec §8 "→ tech (inverted)" case. `iCost`→`cost.research`. **`BuildingInfo.TechSpecialistChanges` is 2D-KEYED** (building × specialist via `SpecialistCounts`) → `freeSpecialists.city.buildings.{BUILDING}.specialists.{SPECIALIST}.flat` (hole #4, owner 2026-06-16): the specialist is a SECOND target dimension BEFORE the unit, not a `{SPEC:N}` map under `flat`. Enabled by an optional 8th `subTargetType` field on the `TECH_BOOSTS` tuple → `curate_common.accumulate_boosts` nests `targetType.{TARGET}.subt.{KEY}.unit` (config-gated, backward-compatible). Cleared the last 12 flags → real data 0. |
| `FirstFreeUnit` / `FirstFreeProphet` / `iFirstFreeTechs` | `grants.{firstFreeUnit,firstFreeProphet,freeTechs}` | FIRST-TO-RESEARCH race rewards (not universal grants) — bespoke keys preserve the "first" semantics pending the §10-banked `firstToResearch` predicate. ⚑ FLAG: differs from the enabler-spec §6 plain `grants.{units,techs}` lists; owner call whether to restructure later. |
| `iAsset` / `iPower` | `identity.{worth,militaryWorth}` | Applied on research as player assets/tech-power (`processTech changeAssets/changeTechPower`), but score-like → identity (engine.FIELD_RENAME). |
| `bDisable` | `identity.disable` | ⚑ FLAG: a LIVE unconditional research kill (`canEverResearch` hard-false); only `TECH_DUMMY`. Load-stable disable → arguably `loadPrune`-like, but doesn't fit its `{onGameOptions}` shape; kept faithfully in identity pending an owner home decision. |
| `bGlobal` | `allowed.world` (`=1`) | The religion-uniqueness research RACE gate — **now the unified `allowed` CAP** (owner 2026-06-17): `allowed:{world:1}` = "at most ONE of this tech researched anywhere in the world" (`canEverResearch` bars a global tech once `countKnownTechNumTeams>0`, `CvPlayer:8268`). **SUPERSEDES** the earlier `requires.build.noneOf:[{type:SELF,scope:world}]` spelling — instance caps are `allowed`, not a `requires` SELF-atom (which conflated *needed* with *allowed* and forced an off-by-one); `SELF` left `requires` entirely. The 29 `bGlobal` techs are EXACTLY the 29 religion-prereq techs — one idiom covers the religion-founding-once rule. Coexists with the positive `requires.build.all`/`.any` prereqs. (enabler-spec §5a/§13.7.) |
| `bRepeat` | `identity.repeat` | Repeatable tech (Future Tech). Faithful flag. |

## Civic  (`curate_civic.py`)  — Tier B #14, "first heavy" (175 records, BESPOKE)

An EMPIRE-wide source: nearly every modifier deposits through `CvPlayer::processCivics` → player accumulators, so
scope is `empire` (the bespoke curator carries the per-field classification from the classify-civic workflow,
verified vs `CvCivicInfo.{h,cpp}` + the CvCity/CvPlayer/CvTeam consumers). EXE-link: **0 `DllExport` on
`CvCivicInfo`** (unconstrained). Every XML tag classified (no leftovers). **No `requires`** (only `TechPrereq`
gates civics → store inverts to `tech.enables.civics`); civics are never an obsolete/replace target. The bulk
(yield/commerce split families, maintenance/upkeep cost-style, happiness/health/combat/experience/great-people,
`enables` = capability lists [`SpecialistValids`/`Hurrys`/`SpecialBuildingNotRequireds`] + store-derived
`PrereqCivic` buildings/units) is de-Hungarianized field→family.member, not re-logged. Structural / manual:

| old XML | new JSON path | note |
|---|---|---|
| `iRevIdxSwitchTo` | `grants.revolution` (bare signed value) | **MOVED from the `revolution` family to `grants`** (owner 2026-06-15: "grants feels like the natural place to put those pulses"). It's a ONE-TIME revolution-index BURST applied on *switching to* the civic (revolting to a civic raises future revolution chance), unlike the other ~13 *continuous* `revolution` fields which stay. Establishes `grants` as the home for non-entity one-time pulses (the same shape the outcomes system will use for one-time yields). Bare value (signed; e.g. `-100` Despotism) to match existing grants (`freeTechs:1`, `freeSpecialists:{…}`). |
| `PropertyManipulators`/`PropertySource` | `<PROPERTY>.<scope>.<unit>` (v3) | Converted via the shared **`engine.property_source_v3`** (the STANDARD, owner 2026-06-15: "no standard setup yet, make it the new version at once") — NOT the old `perTurn`/`mult` shape. `CONSTANT`→`flat`, `DECAY`→`percent`, attribute-scaled (`ATTRIBUTE_CONSTANT` or `<Mult>`)→`flat:{value,per:{type:POPULATION,scope}}`. scope from `GameObjectType` (`city`/`plot`, **not** the old hardcoded `empire`); `RELATION_ASSOCIATED` (the civic acting on its own cities) is the cascade default, dropped. Uniform with the Property pass. |
| revolution `iRev*`/`fRev*` (the rest) | `revolution.empire.{member}.{flat\|percent}` | RevolutionDCM — KEPT faithful (logic lives in Python only because the original modder didn't know C++; values feed a Python→C++ port). `f*` are floats, carried verbatim. |
| state-religion `iStateReligion*` | `stateReligion.empire.{member}.{unit}` | Grouped family, gated on having a state religion (gate carried by the family name — no city/player-state predicate in the enabler vocab yet; civic-OWNED, not inverted onto any ReligionInfo). |
| policy bools (`bStateReligion`/`bNoForeignTrade`/`bAllowInquisitions`/…) | `policies.{name}` | ~17 player-STATE toggles (not additive numbers, not availability). ⚑ shares the section name with Civilization's `policies` (playable/aiPlayable) — different concept, same keyword. |
| `BonusCommerceModifiers` | — (DROPPED) | EMPTY on the only civic that has the tag (CIVIC_BANDITRY `<BonusCommerceModifiers/>`); zero data → the §6 keep-on-source-vs-invert question is moot, no double-author. |
| `FreeSpecialistCounts` | `grants.freeSpecialists.{SPECIALIST}` | Keyed free-specialist counts → grants (one-shot provision). |
| capital-restricted (`CapitalYieldModifiers`/`CapitalCommerceModifiers`) | `<family>.empire.capital.{unit}` | Modeled as a `capital` member (no `isCapital` predicate in the enabler vocab yet — accept, future refinement). |
| `Upkeep`/`CivicOptionType`/`iAnarchyLength`/`WeLoveTheKing` | `identity.{upkeepLevel,civicOption,anarchyLength,weLoveTheKing}` | Config/identity. Folder = the `CivicOptionType` short name (category split). |

## Religion  (`curate_religion.py`)  — Tier B #15 (29 records, BESPOKE)

A foundable faith / source-conditioner. EXE-link **0 `DllExport`**; every XML tag classified, no leftovers; no
`requires` (only `TechPrereq`, store-inverted to `tech.enables.religions`); never an obsolete/replace target.
`enables` = store-derived `PrereqReligion` buildings/units (the religion is the CONDITIONER). `religionInfluence`
inbound boost (Building `ReligionChanges`) kept target-keyed. `Adjective`→text; `iSpreadFactor`→identity;
`FreeUnit`+`iFreeUnits`→`grants` (only when count>0 — the award is count-gated, CvPlayer:8838). Structural:

| old XML | new JSON path | note |
|---|---|---|
| `StateReligionCommerces` | `<commerce>.city.flat[].{value, enabled:{STATE_RELIGION:RELIGION_X}}` | Conditional commerce → an `enabled` deposit with the OBJECT-EVALUATED predicate `STATE_RELIGION` (owner 2026-06-15: "STATE_RELIGION: religion"). The engine's predicate owns the C++ compound (present AND (is-state-religion OR no-state-religion OR non-state-commerce), CvCity~12498) — data just names predicate + the specific religion; the city/player self-reports. Was condition-as-member. |
| `HolyCityCommerces` | `<commerce>.city.flat[].{value, enabled:{HOLY_CITY:RELIGION_X}}` | Same: `HOLY_CITY` predicate (the city self-reports `isHolyCity`, CvCity~12507). Multiple conditioned deposits to one commerce/scope/unit → a LIST of entries (modifier-spec §1.3). |
| `GlobalReligionCommerces` | `shrine.{commerce}` (raw values) — PARKED to Building | NOT a city gate: `value × countReligionLevels(religion)` WORLD-scaled, consumed through a *shrine building* (CvCity~12185). Owner: "shrine is building, deal with it then." Raw per-commerce values parked in a `shrine` section; the world-scaling + religion↔shrine-building routing (+ the world religion-levels count token) are authored at the Building pass. |
| `PropertyManipulators` | `<PROPERTY>.<scope>.<unit>` (v3) | Via the shared `engine.property_source_v3` (no-op — religions carry no PropertySources; kept uniform). |

*Predicate vocabulary established here (enabler-spec §3 object-evaluated predicates): `HAS_RELIGION:religion`,
`STATE_RELIGION:religion`, `HOLY_CITY:religion`, `IS_CAPITAL:true` — engine/object-resolved, reused by Civic/Trait
(capital bonuses) and the later civic `capital`/`stateReligion` retrofits.*

## Corporation  (`curate_corporation.py`)  — Tier B #16 (23 records, BESPOKE, FIRST PASS)

Follows the **Religion model** (owner 2026-06-15): founding creates the HQ building (`FoundsCorporation`), then the
corp SPREADS like a religion (`isHasCorporation` city flag) and its per-city effects apply only where active. EXE-link
0 `DllExport`; no leftovers. ⚠ FIRST PASS — corps need a dedicated rework pass (HQ-revenue + spread mechanics).

| old XML | new JSON path | note |
|---|---|---|
| `YieldChanges`/`CommerceChanges` | `<family>.city.flat[].{value, enabled:{HAS_CORPORATION:SELF}}` | Per-city flat add, gated by the spread-presence predicate `HAS_CORPORATION` (parallel to `HAS_RELIGION`; CvCity returns 0 unless `isActiveCorporation`). |
| `YieldsProduced`/`CommercesProduced` | `<family>.city.flat[].{value, enabled:{HAS_CORPORATION:SELF}, per:{anyOf:[prereqBonuses], scope:city}}` | Per-bonus output: `YieldProduced × Σ getNumBonuses(prereqBonus)` → a `per` over the **set** of PrereqBonuses (`anyOf`, new modifier-spec §4 form). Replaces the opaque `produced.perBonus`. |
| `iMaintenance` | `maintenance.city.corporation.flat[].{value, enabled, per:{anyOf:…}}` | Per-bonus maintenance, same gating + per-set. |
| `PrereqBonuses` | (read into the `per:{anyOf}` set) + **HQ building `requires.build`** (Building pass) | NOT a corp `requires` and NOT an enabler edge (store rows removed): the corp is generated by its tech; PrereqBonuses are the per-bonus scaling basis here + the FOUND requirement authored on the HQ `FoundsCorporation` building. |
| `PrereqBuildings` | — (store row removed; → Building/HQ) | Not an enabler edge for the corp (means, not generator). |
| `TechPrereq` | (store) `tech.enables.corporations` | The generator — kept. |
| `HeadquarterCommerces` | `<commerce>.empire.headquarters.perCorporationLevel` (UNCHANGED) | DEFERRED to the corp rework pass: HQ revenue scaled by `countCorporationLevels`; kept in its current form pending a per-corp-level token + HQ-city modeling. |
| `BonusProduced`/`FreeUnit` | `grants.{bonusProduced,freeUnit}` · `iSpread*` → identity/`cost.spread` | Unchanged. HQ-founding (`FoundsCorporation`), `CorporationSpreads` (units), `GlobalCorporationCommerce` (HQ amplifier) edges deferred to Building/Unit. |

## Trait  (`curate_trait.py`)  — Tier B #17, "Mount Doom" (390 records, BESPOKE)

ONE `CvTraitInfo` serving TWO systems. EXE-link 0 `DllExport`; no leftovers. Empire-wide source/enabler; the
per-field classification mirrors Civic (verified vs `CvTraitInfo`+`processTrait`). **SIMPLE vs COMPLEX SPLIT
INTO TWO FOLDERS** (owner 2026-06-15): `Assets/Data/traits/simple/` (88) + `…/complex/` (302). The complex
system is entirely the **Thunderbrd** module (302 complex all Thunderbrd; simple = 87 BASE + 1 Thunderbrd) — the
only module touching traits, owner-confirmed in.

| old XML | new JSON path | note |
|---|---|---|
| (split classifier) | folder `complex/` vs `simple/` | **complex** iff a `CvInfoReplacements` complex variant (`ReplacementID`) OR gated by `GAMEOPTION_LEADER_COMPLEX_TRAITS` (`OnGameOptions`); else **simple** (incl. the 64 vanilla bases that have a complex counterpart). "extra care" semantic split, not naive-by-module (catches the 1 Thunderbrd-simple trait). |
| `ReplacementID`/`ReplacementCondition` | — (DROPPED, no `replacedBy`) | Simple & complex are "2 completely different traits hacked on top of each other" (TB), NOT base+variant — so the cross-link is dropped; each folder holds independent, full traits. END-STATE: complex traits become their **own Info type behind a shared interface** (coding pass #430; most loading stays shared). Despair Index #11 status updated. |
| `OnGameOptions`/`NotOnGameOptions` | `loadPrune.{onGameOptions,notOnGameOptions}` | Complex traits carry `onGameOptions:[GAMEOPTION_LEADER_COMPLEX_TRAITS]` (materialize only with complex on). Set selection by game option (+ leaders' `DefaultTraits`/`DefaultComplexTraits` lists, LeaderHead pass), not the dropped hack-link. |
| `TraitPrereq`/`TraitPrereqOr1/2`/`PrereqTech` | (store) `enables.traits` on the prereq trait/tech | Dev-leaders prereqs inverted top-down. |
| `DisallowedTraitTypes` | `excludes` | Same-tier mutual exclusion (one end authored; reverse derived cold-path). |
| `PromotionLine`/`iLinePriority` | `succession.{promotionLine,priority}` | Developing-leaders line ordering. |
| `PropertyManipulators` | `<PROPERTY>.<scope>.<unit>` (v3) | Via shared `engine.property_source_v3` (274 sources, all CONSTANT/RELATION_ASSOCIATED → `<PROPERTY>.city.flat`). |
| `BonusHappinessChanges` | `happiness.empire.flat[].{value, enabled:{type:BONUS_X, scope:empire, min:1}}` | **AUTHORED ON THE TRAIT (keep-on-source, modifier-spec §6 — UPDATED 2026-06-15: was "folds onto the bonus", that was the old pre-v3 invert rule; a resource is NEVER a target).** The trait grants +N happiness while a bonus is present: deposit `happiness.empire.flat` (scope = the modifier's — benefits the player's cities), condition = the agreed enabler atom `{type, scope, min}` (§6.1/§13.7), `min:1` = "we have ≥1" (a tally count). Coexists with unconditional happiness members via the §1.5 mixed list. 26 traits. `trait-classification.json` confirms: CREST conditioner, do-NOT-invert. |
| `GreatPeopleUnitType`+`iGreatPeopleRateChange`, `CityStartCulture`/`BonusPopulationinNewCities`, state-religion/policies/grants/etc. | (per the curator docstring) | Mirror Civic conventions; unchanged this pass. |

## Bonus  (`curate_bonus.py`)  — Tier C #18 (902 records: cultures 410 / map 118 / manufactured 374)

A SOURCE/CONDITIONER positioned **above** plot/feature/improvement/building in the containment spine
(`building-cascade-conversion.md:281`); it cascades DOWN and is **NEVER a target** (the "coal test"). So it
carries only its OWN amplification + `enables`; the prior `BONUS_BOOSTS` inversion table (building/civic/trait
effects conditioned on the bonus folded ONTO it) was the old pre-v3 invert rule and is REMOVED — those are
modifiers on the TARGET they boost (keep-on-source, modifier-spec §6), authored at the source's pass. EXE-link:
0 `DllExport` on `CvBonusInfo`. No inbound boosts. Sub-foldered `cultures`/`map`/`manufactured` (`bonus_folder`).

| old XML | new JSON path | note |
|---|---|---|
| `YieldChanges` | `<yield>.plot.flat` (food/production/commerce) | The on-map resource buffs the TILE it sits on, downward — **plot** scope (owner 2026-06-15: "the actual map bonus buffs the plot downwards"). Split into per-yield families. |
| `iHealth` / `iHappiness` | `health.empire.flat` / `happiness.empire.flat` | The trade-connected resource's benefit to the player's cities — **empire** scope (connected bonus amplifies empire-wide, `building-cascade-conversion.md:1205-1207`). |
| `TechReveal` / `TechCityTrade` | (store) `tech.enables.bonuses` | The tech reveals / enables-trade of the bonus → inverted onto the TECH (store PREREQ_FIELDS); dropped here. |
| `TechObsolete` | (store) `tech.obsoletes.bonuses` | The obsoleting tech's edge → inverted onto the TECH (store OBSOLETE_FIELDS); dropped here (`extra_drop`). |
| `PrereqAndBonus`/`PrereqOrBonuses`/vicinity (on Building/Unit/Route) | (store) `bonus.enables.{buildings,units,routes}` | The bonus is the CONDITIONER those targets require → surfaces as the bonus's `enables` edge (store-indexed). The Culture chain (Culture wonder grants a `BONUS_*` that gates the punk buildings) appears here. |
| map-gen fields (`iPlacementOrder`/`iConstAppearance`/`Rands`/`iTilesPer`/`bHills`/`TerrainBooleans`/…) | `mapGeneration.{…}` | Placement/spawn config grouped out of the identity catch-all (de-Hungarianized). |
| Building/Unit/Project/Civic/Trait `Bonus*Changes`/`Bonus*Modifiers` | — (NOT on the bonus) | Conditioned-on-bonus modifiers stay on the SOURCE (keep-on-source, §6), gated by the bonus; authored at the Building/Unit/Project pass (Civic empty/moot; Trait `BonusHappinessChanges` authored on the trait — see Trait above). The old `BONUS_BOOSTS` fold is REMOVED. |

## Terrain  (`curate_terrain.py`)  — Tier C #20 (102 records: 42 base + 60 module space/planet terrains)

A plot-leaf **TARGET** that CARRIES ITS OWN modifiers onto the plot it forms (owner 2026-06-16: the hill delivers
hammers; a feature then modifies the plot on top). Every effect is a terrain-OWN deposit at **`plot` scope**; **NO
inbound boost folds onto the terrain** — per the **deliveryguy ownership rule** (modifier-spec §6.1) a
`Building.TerrainYieldChanges` stays on the building (it delivers the buff), a `Civic.TerrainYieldChanges` on the
civic, an improvement/feature/route on itself. EXE-link: **1 `DllExport` (`getArtDefineTag`, an art FK) —
unconstrained for data**. Enables nothing (`store.enabled_by(TERRAIN_*)` empty → no `enables`/`requires`/`grants`).
`Categories`/`PropertyManipulators`/`bImpassable` are authored on **0/42** terrains → never emitted (consumers exist
but no terrain populates them; `Categories` is also a dead TB-combat list). Per-field dispositions:
`classifications/terrain-classification.json`. Mechanical de-Hungarian (`Description`→`description`) not re-logged.

| old XML | new JSON path | note |
|---|---|---|
| `Yields` | `<yield>.plot.flat` (food/production/commerce) | The terrain's OWN base tile yield (grassland food, ocean food+commerce), summed into the plot's base-yield accumulator (`CvPlot.cpp:8077`). SPLIT per-yield families. The documented buried-in-identity trap (the first-pass mapping parked it in identity). |
| `iMovement` | `movement.plot.flat` | The terrain's base per-plot move cost — **a `movement` FAMILY** (owner 2026-06-16: "movement is a family"). Terrain seeds the per-plot cost (`CvPlot.cpp:4555`); features/hills add on top. *(Distinct from Route #19, whose own move cost was authored `identity.movementCost` — routes combine by override/min; flagged for possible alignment.)* |
| `iDefense` | `defense.plot.amount.percent` | Terrain's intrinsic tile-defense % — **a `defense` FAMILY** (owner 2026-06-16: a family, "improvable by a fort"; `member: amount`, the same member CultureLevel's `defense.city.amount.percent` uses). Terrain seeds the per-plot defense (`CvPlot.cpp:4400`); feature/hills/peak add. |
| `iCultureDistance` | `cultureDistance.plot.flat` | **NEW `cultureDistance` family** (owner 2026-06-16: a summable family for the `REALISTIC_CULTURE` game option; "the base for the later culture-equilibrium structure — a sensible start"). `CvCity.cpp:6302+` accumulates `getCultureDistance()` across worked plots into a city total. |
| `iBuildModifier` | `buildTime.plot.percent` | **NEW `buildTime` family** (owner 2026-06-16: ties to `workRate`/work-capacity — *"buildTime directly influences the turns a worker uses on a plot… work capacity and buildTime should be in the same family"*; the `buildTime`↔`workRate` unification is later structural work, flagged). `CvPlot.cpp:3607` multiplies the build time of work done on this terrain. |
| `iHealthPercent` | — (DROPPED, cat i dead) | Dead on terrain: every `getHealthPercent` reader is Feature/Specialist/Improvement/Building, none against a terrain (pre-existing note `modifier-cascade-mapping.json:2667`); no Python reader. The capability lives on those entities. 9/42 populated but inert. |
| `ArtDefineTag` | `world.art.icon` | The **ON-MAP terrain graphics** FK → `CIV4ArtDefines_Terrain.xml`. Lands in the `world` block (on-map/3D art); the icon-headache is resolved by the BLOCK split, not the old per-entity `art_rename` (now obsolete): `world.art.icon` (on-map) vs `ui.art.icon` (UI `Button`) can no longer collide. Art-block restructure (DONE 2026-06-16; see the Art-blocks note). |
| `Button` | `ui.art.icon` | The UI icon (`.dds`). |
| `FootstepSounds` / `WorldSoundscapeAudioScript` | `sound.footsteps` / `sound.soundscape` | Audio → the flat `sound` block (audio is itself the asset; canonical names via `curate_common.ART_BLOCK`). The earlier "defer audio reorg" note is RESOLVED — the `sound` block IS the organization. |
| `ClimateZoneType` / `iDistanceToLand` / `bFound` / `bFoundCoast` / `bFoundFreshWater` / `bFreshWaterTerrain` | `identity.{climateZoneType, distanceToLand, found, foundCoast, foundFreshWater, freshWaterTerrain}` | World-gen classification, city-found gates, fresh-water provision — read directly, never summed. |
| `MapCategoryTypes` | `identity.mapCategories` | Live placement-gate classification array (`CvPlot::getMapCategories` → `isMapCategory<>` set-membership); matched by attribute, not summed → identity. Renamed for clarity. |
| `Categories` / `PropertyManipulators` / `bImpassable` | — (not emitted) | 0/42 authored (see intro). |

**SYNTHETIC plot-yield deposits (no terrain XML field; owner 2026-06-16) — injected via a `post_process` hook,
read from `YieldInfo` (no magic numbers):**

| source | new JSON path | note |
|---|---|---|
| `YieldInfo.YIELD_COMMERCE.iRiverChange` (=1) | `commerce.plot.flat[].{value:1, enabled:"HAS_RIVER"}` on the **19 river-capable land terrains** | The RIVER add-on. River is a plot edge-attribute "just added on" to a terrain (no terrain field); the Info data is only the **conditioner** on `CvPlot`'s live river boolean (owner: "the live data defines if there is a river, the info data is just the conditioner"). Lands on the terrain because every river plot has a terrain (the always-present plot owner); compounds with a feature's `RiverYieldChange` on top. River-capable = LAND: `MAPCATEGORY_EARTH ∧ ¬AQUATIC` (the 17 earth-land) **+** `TERRAIN_HILL`/`TERRAIN_PEAK` (owner: include them); excludes water (AQUATIC) + all space terrains (no EARTH) + `TERRAIN_NONE`. The first real `HAS_RIVER` use (enabler-spec §3). |
| `YieldInfo.iHillsChange` / `iPeakChange` | `TERRAIN_HILL` `food −1`/`prod +1`; `TERRAIN_PEAK` `food −2`/`prod +3`/`commerce −1` (`.plot.flat`) | The HILLS/PEAK plot-yield deltas, moved off `YieldInfo`'s plot-type changes onto their own terrains (owner 2026-06-16: "hills and peaks are their own terrain, so they should handle their own yield modifiers"). `TERRAIN_HILL`/`PEAK` are dual plot-type + terrain entities; the cascade engine (#430) reads these terrain yields for hills/peak plots instead of `YieldInfo.getHillsChange/getPeakChange` (which retire). PEAK's commerce is a mixed list `[−1, {value:1, enabled:"HAS_RIVER"}]` (own peak −1 + the river add-on). |

*Toolkit: registered `TerrainInfo` + `YieldInfo` in `store.ENTITIES`; added `defense`/`cultureDistance`/`buildTime`
to `FAMILY_ORDER`, an `art_rename` hook, and a `post_process` hook to `EntityConfig`/`curate_common` (for synthetic
deposits not sourced from the entity's own XML). **YieldInfo migration owes nothing for hills/peak/river yields —
already on the terrains;** its remaining fields (min-city/city-change/golden-age/trade-modifier/colour/symbols) are
a later `YieldInfo` curation. Other river-side yields (`FeatureInfo.RiverYieldChange`,
`ImprovementInfo.RiverSideYieldChange`, `BuildingInfo.RiverPlotYieldChanges`) stay on their deliveryguy at those
passes (enabler-spec §3, modifier-spec §6.1).*

## Feature  (`curate_feature.py`)  — Tier C #21 (101 records: 76 base + 25 module)

A plot-leaf **TARGET / deliveryguy** that MODIFIES the plot it sits on. Unlike Terrain (which SEEDS the plot base), a
feature **ADDS** its values onto the terrain-seeded accumulator (`CvPlot.cpp` movement 4559 / defense 4404 / yield
8081, all `+=`) — a genuine per-plot DELTA, so its own effects are feature-owned `plot`-scope modifier families.
Enables nothing; **all inbound feature-keyed modifiers stay keep-on-source** (the civic/unit/promotion/unit-combat
that delivers them owns them, conditioned on the feature — Feature carries no inbound boost). EXE-link: **1
`DllExport` (`getArtInfo`, an art FK) — unconstrained.** Classification: `classifications/feature-classification.json`
(adversarially verified). Mechanical de-Hungarian not re-logged.

| old XML | new JSON path | note |
|---|---|---|
| `YieldChanges` | `<yield>.plot.flat` (food/production/commerce) | The feature's own per-plot yield DELTA (forest −food/+hammers, jungle −food, oasis +food/+commerce). Mapping said `city` scope — WRONG → `plot`. |
| `RiverYieldChange` | `<yield>.plot.flat[].{value, enabled:"HAS_RIVER"}` | The feature's EXTRA river-side yield (forest-on-river), `HAS_RIVER`-gated (bare predicate), compounding with the terrain's river bonus. First real `HAS_RIVER` use alongside the terrain (enabler-spec §3). Injected via `post_process` (apply_channel can't express the conditional). |
| `iHealthPercent` | `health.plot.percent` | Feature OWNS health (Terrain dropped it precisely because it lives here). |
| `iDefense` | `defense.plot.amount.percent` | Feature defense %, additive onto the plot (same `amount` member as Terrain/CultureLevel). |
| `iMovement` | `movement.plot.flat` | Extra move cost, additive onto the terrain-seeded cost. |
| `iCultureDistance` | `cultureDistance.plot.flat` | Summed into the city culture-distance total. |
| `iSeeThrough` | `vision.plot.seeThrough.flat` | Line-of-sight see-through → a **`vision` block** (owner 2026-06-16: dedicated-block rule, modifier-spec §0.8), grouped so the coming vision-behaviour rework lands beside it. |
| `iWarmingDefense` | — (DROPPED, cat i dead) | `GLOBAL_WARMING` is `// #define`d out → the mechanic is compiled out. A future global-warming system gets its OWN base object, not a feature field (owner). Issue **#436** + `global-warming-mod.md`. |
| `PropertyManipulators` | `properties[]` (PARKED raw) | `RELATION_NEAR` pollution = **spatial leakage → #429**; the atomic cutover replaces the XML, so the sources are preserved verbatim (via `clean_property_source`), not dropped, pending the #429 redesign. ⚑ FLAG. |
| `OnUnitChangeTo` | `grants.onUnitChangeTo` | A feature that transforms a unit on entry (module-only, 0 in base XML). ⚑ FLAG: `grants` vs a dedicated transform edge — owner pass if it ever carries data. |
| `iAdvancedStartRemoveCost` | `cost.advancedStartRemoveCost` | Gold cost to remove the feature in advanced start. |
| `ArtDefineTag` / `EffectType` / `iEffectProbability` / `GrowthSound` / `FootstepSounds` / `WorldSoundscapeAudioScript` | `world.art.icon` / `world.art.effect.{type,probability}` / `sound.{growth,footsteps,soundscape}` | Art + audio split into the `world`/`sound` blocks (`ArtDefineTag` → `world.art.icon`, the on-map graphic; feature carries no separate UI `Button`). Canonical map `curate_common.ART_BLOCK`; the "defer audio reorg" note is RESOLVED by the `sound` block. Art-block restructure DONE 2026-06-16. |
| `iAppearance` / `iDisappearance` / `iGrowth` / `iSpread` / `iPopDestroys` / `bCanGrowAnywhere` / `bRequires*` / `bNo*` / placement flags / `bGraphicalOnly` / `TerrainBooleans` | `identity.*` | World-gen RNG / lifecycle / placement config (read directly). `bGraphicalOnly` is a base-class flag (moved out of the mapping's `art` → identity). |

*Toolkit: registered `FeatureInfo` in `store.ENTITIES`; `curate_feature.py` (bespoke) reuses the `post_process` hook
for the `HAS_RIVER` river yield + the `properties` parking; fixed `mapping/FeatureInfo.json` (`bGraphicalOnly` art→flag).*

## Route  (`curate_route.py`)  — Tier C #19 (21 records: 12 base + 9 module-added space routes)

A small plot-leaf entity: a route lays movement cost + (optionally) its own tile yields onto the plot it
occupies, ranks itself by `iValue`, and is GATED by a prerequisite bonus — so it is a DEPENDENT in the
bonus→route enabler chain (`store.enabled_by(ROUTE_*)` is empty → **no `enables` block**, no `requires`).
EXE-link: **0 `DllExport` on `CvRouteInfo`** (unconstrained; readJson-mapped). `Categories` (getCategory trio
compiles but no route consumer reads it via `getRouteInfo`, and no route populates `<Categories>`) and
`PropertyManipulators` (empty on every route) are dead/empty → dropped. Mechanical de-Hungarian
(`Button`→`ui.art.icon` via the art-block restructure, `Description`→`description`) not re-logged. Structural / manual:

| old XML | new JSON path | note |
|---|---|---|
| `Yields` | `<yield>.plot.flat` (food/production/commerce) | The route's OWN base-yield deposit onto the tile it occupies, downward — **plot** scope (a tile yield summed alongside terrain/improvement yields in `CvPlot::calculateYield`). Split into per-yield families. |
| `ImprovementInfo.RouteYieldChanges` (INBOUND) | `<yield>.plot.improvements.{IMPROVEMENT}.flat` | The improvement's per-route yield bonus (`getRouteYieldChanges(route,yield)`, applied to a plot when that improvement sits on a tile carrying THIS route, `CvPlayer::calculatePlotRouteYieldDifference`) — **FOLDED ONTO THE ROUTE**, keyed by the source improvement, at plot scope. **Owner ruling 2026-06-16: KEPT on the route** (NOT moved keep-on-source like Bonus #18's `BONUS_BOOSTS` removal). Rationale (owner): **routes only ever boost plots/improvements, and we want all of a route's boosts in ONE place so a modder reading the route file understands its full effect** (the cold-modder rule, governing ruling #3). Distinct from the bonus case — a bonus is never a target so nothing folds onto it; a route's narrow, uniform effect surface makes co-locating its boosts the clearer home. (Honors the older 2026-06-13 ruling #4 "RouteYieldChanges owned by `CvRouteInfo`".) |
| `iMovement` / `iFlatMovement` | `identity.movementCost` / `identity.flatMovementCost` | Base per-plot traversal cost — an INTRINSIC own-stat read directly by pathfinding (`CvPlot::movementCost`), never summed onto a scope accumulator → `identity`, NOT a `movement` family (owner ruling: base stats are identity; only summed contributions are modifier families). The cascading movement modifier is the per-TECH route delta, folded onto the tech (`movement.team.routes.{ROUTE}.flat`). Renamed for clarity off `iMovement`/`iFlatMovement`. |
| `iAdvancedStartCost` | `identity.advancedStart.cost` | Pre-game advanced-start placement points — NOT a modifier; parked in identity pending the advanced-start review (matches Handicap/Era). |
| `iValue` | `identity.value` | Route TIER/ranking comparator (trail=1 … gravityTrain=10); a pure ranking scalar, not a deposited modifier. |
| `bSeaTunnel` | `identity.seaTunnel` | Capability flag: route lets LAND units cross water tiles (`CvPlot::isSeaTunnel`). Boolean (true emitted, false omitted); only `ROUTE_TUNNEL` sets it. |
| `BonusType` / `PrereqOrBonuses` | (store) `bonus.enables.routes` | The prereq bonus(es) gating the route — the bonus is the CONDITIONER, store-inverted to its `enables.routes` (dropped here; the route is a DEPENDENT). Live cold-path consumer `SevoPediaRoute.py` re-points to the derived reverse index at the reader phase. |
| `TechMovementChanges` | (curate_tech) `tech` `movement.team.routes.{ROUTE}.flat` | Per-tech route-movement improvement → inverts onto the TECH (`CvTeam::processTech` accumulates it team-wide); dropped here. Live consumers reworked at the reader phase: C++ `CvTeam::processTech`, Python `Revolution.py` / `CvTechChooser.py`. Only `ROUTE_VACTRAIN` sets one (`TECH_SKYROADS`, −4). |
| `Categories` / `PropertyManipulators` | — (DROPPED) | Dead/empty on every route (see intro). |
| `m_zobristValue` | — (drop) | Runtime-only (seeded from `getSorenRand()` in the ctor), not XML-backed. |

## Improvement  (`curate_improvement.py`)  — Tier C #22 (266 records: 122 base + 144 module-added; 101 graphical-only)

A plot-leaf **TARGET** (Terrain/Feature peer): carries its OWN `plot`-scope modifiers + a placement `requires.build`;
never a source (`store.enabled_by` empty → no `enables`). Curated from the adversarially-verified
`classifications/improvement-classification.json` (classify-improvement workflow) + owner rulings 2026-06-16; full
per-field rationale + the placement-gate INVARIANT live there. EXE-link: **3 DllExport** (`isGoody`,
`isRequiresRiverSide`, `getArtInfo`) → `bGoody`+`bRequiresRiverSide` EXE-constrained. ≡ its **Build** (#23): the worker
Build is the action that PLACES it (button/time/cost on Build); the improvement carries NO `<Button>`. Mechanical
de-Hungarian + the ui/world/sound art blocks not re-logged. Structural / manual:

| old XML | new JSON path | note |
|---|---|---|
| `YieldChanges` | `<yield>.plot.flat` (food/production/commerce) | The improvement's OWN tile yield. MAPPING ERROR: buried in identity + scope:city → `plot` (the Terrain/Feature trap). |
| `RiverSideYieldChange` | `<yield>.plot.flat[].{value, enabled:"HAS_RIVER"}` | Extra river-plot yield, `HAS_RIVER`-gated (post_process). |
| `IrrigatedYieldChange` | `<yield>.plot.flat[].{value, enabled:"HAS_IRRIGATION"}` | Extra yield when irrigated; new `HAS_IRRIGATION` plot predicate. |
| `iDefenseModifier` | `defense.plot.amount.percent` | Fort tile-defense % (Terrain/Feature `amount` member). MAPPING ERROR scope:city → plot. |
| `iCulture` / `iCultureRange` | `culture.plot.flat` / `identity.cultureRange` | Plot culture deposit + its radius parameter (identity, paired). |
| `iVisibilityChange` / `iSeeFrom` | `vision.plot.visibilityRange.flat` / `vision.plot.seeFrom.flat` | Dedicated `vision` block (§0.8); the improvement's first vision members. |
| `PrereqTech` + placement bools/lists | `requires.build` (`all`/`any`/`noneOf`) | **The placement gate** (owner double-mapping; consumed by `CvPlot::canHaveImprovement`). `all`: `{type:TECH,scope:team}` + bare `IS_WATER`/`IS_PEAK` (domain), `HAS_IRRIGATION`(`bRequiresIrrigation`), `IS_FLATLANDS`, `HAS_FEATURE`, `HAS_RIVER`(`bRequiresRiverSide`), `COASTAL_LAND`(`bCanMoveSeaUnits`), `{natureYield:{…}}`(`PrereqNatureYields`). `any` (one OR-group, the make-valid set): `{terrain:[…]}`/`{feature:[…]}`/`{bonus:[…]}` + `IS_HILLS`/`IS_FRESHWATER`/`HAS_RIVER`/`IS_PEAK`. `noneOf`: `IS_FRESHWATER`(`bNoFreshWater`). NEW vocab: bare plot predicates + `{terrain\|feature\|bonus:[…]}` membership + `HAS_BONUS:{}` + `{natureYield}`. `PrereqTech` ALSO store-inverts to `tech.enables.improvements` (coexist). |
| `BonusTypeStructs.YieldChanges` | `<yield>.plot.flat[].{value, enabled:{HAS_BONUS:BONUS_X}}` | Per-bonus extra yield (mine-on-coal), `HAS_BONUS`-gated (post_process). |
| `BonusTypeStructs.bBonusMakesValid` | `requires.build.any […].{bonus:[…]}` | Per-bonus placement validity (folded into the make-valid OR-set). |
| `BonusTypeStructs.bBonusTrade` / `iDiscoverRand` / `iDepletionRand` | `identity.bonuses.{BONUS}.{trade,discoverRand,depletionRand}` | Per-bonus capability + RNG (DepletionRand = live gated `MODDERGAMEOPTION_RESOURCE_DEPLETION`). |
| `bCarriesIrrigation` | `identity.carriesIrrigation` | **KEPT** — the improvement must retain its carry-irrigation ability (propagation is live `updateIrrigated`; team-tech-gated by `TECH_CANAL_SYSTEMS`, engine-ANDed; no tech clause here). Carriers: ORCHARD/PLANTATION/VERTICAL_FARM/FARM/GROUNDWATER_WELL. |
| `bIsUniversalTradeBonusProvider` | `identity.universalBonusTrade` | LIVE capability ("trades ALL bonuses on its plot"), via `isImprovementBonusTrade(-1)` OR-fold — NOT unwired (the lynchpin). |
| `bActsAsCity`/`bIsZOCSource`/`bMilitaryStructure`/`bGraphicalOnly`/`bExtraterresial`/`bOutsideBorders` | `identity.{actsAsCity,zoneOfControl,militaryStructure,graphicalOnly,extraterrestrial,outsideBorders}` | Persistent plot-capability flags (NOT `grants`). `bExtraterresial`→`extraterrestrial` (typo fix). |
| `ImprovementUpgrade`/`iUpgradeTime`/`AlternativeImprovementUpgradeTypes`/`bUpgradeRequiresFortify`/`ImprovementPillage`/`iPillageGold`/`bBombardable` | `identity.{upgradesTo,upgradeTime,alternativeUpgrades,upgradeRequiresFortify,pillageTo,pillageGold,bombardable}` | Upgrade + pillage LIFECYCLE (deferred outcome system, not modifiers). |
| `bPlacesBonus`/`bPlacesFeature`/`bPlacesTerrain`/`FeatureChangeTypes`/`bChangeRemove` | `identity.{placesBonus,placesFeature,placesTerrain,featureChanges,changeRemove}` | The "random spawn"/placement-transform mechanic (e.g. apple bonus under a lumberjack) — KEPT on identity; needs its OWN pass, OUT OF SCOPE #428. |
| `iAdvancedStartCost` | `identity.advancedStart.cost` | Advanced-start placement gold (Route/Handicap/Era precedent), NOT a production cost. (⚑ `-1` sentinel = not advanced-start-placeable; carried faithfully.) |
| `iUniqueRange` / `iGoodyRange` / `iTilesPerGoody` / `bGoody` | `mapGeneration.{uniqueRange,goodyRange,tilesPerGoody,goody}` | Map-gen spacing/goody config. `bGoody` EXE-constrained (`isGoody`). |
| `iAirBombDefense` / `iFeatureGrowth` | `identity.{airBombDefense,featureGrowth}` | RNG / world-state odds (engine mechanic is the roll), out of cascade. |
| `TechYieldChanges` | (curate_tech) `yield.team.improvements.{IMPROVEMENT}.flat` | Inverts ONTO the tech (downward, `curate_tech` TECH_BOOSTS). DROP here. **PROVISIONAL** pending the Phase-F modifier-ownership review (owner 2026-06-16). |
| `RouteYieldChanges` | (curate_route) `yield.plot.improvements.{IMPROVEMENT}.flat` | Already folded onto the ROUTE (`curate_route` ROUTE_BOOSTS, owner ruling). DROP here. |
| `iHealthPercent` | — (DROPPED, balance-cut) | BALANCE-CUT as a source from improvements (capability kept globally; §8-iv). |
| `PropertyManipulators` | `properties[]` (PARKED raw) | Spatial leakage → #429; preserved verbatim (cutover replaces XML). |
| `Categories` / root `iDepletionRand` / `Button` / `MapCategoryTypes` | — (DROPPED) | Dead: no consumer / no member / no improvement button / 0-of-266. |

**Placement-gate INVARIANT (owner-LOCKED 2026-06-16):** `requires.build` gates the BUILD + `canHaveImprovement`-checked
paths (worker build, map-gen, advanced-start, AI, upgrade) **and generic event placement** (events check-then-set, so
they respect it = the "double mapping"). `setImprovementType()` stays a **separate non-checking actor** (the doer); the
CHECK is the caller's job, run BEFORE it in the normal path; **direct ungated calls are the sanctioned exception** for
engine outcomes that must always place (`raze`→`IMPROVEMENT_CITY_RUINS`, `found`→`IMPROVEMENT_CITY`). Breaks nothing
(setImprovementType doesn't re-validate). `IMPROVEMENT_CITY`'s `TECH_DUMMY` correctly makes it never worker-buildable;
city-ruins has no Build (gate moot). Future merge of check+set with an explicit skip-check flag → **#437**.

## Build  (`curate_build.py`)  — Tier C #23 (304 records: 151 base + 153 module-added)

The worker ACTION that PLACES improvements / lays routes / adds-removes features / terraforms terrain — CLEANLY
SEPARATED (owner 2026-06-16): it references each outcome by **FK** in a `produces` section, never embeds their data.
A self-contained leaf action OVER the done Improvement #22 + Feature #21; **enables nothing** (`store.enabled_by(BUILD_*)`
empty → no `enables`). **The DOUBLE-MAPPING:** plot-VALIDITY (terrain/feature/etc.) lives on the IMPROVEMENT
(`canHaveImprovement`, #22); the build's `requires.build` is its OWN tech/bonus MEANS gate (`CvPlayer::canBuild`). Not
redundant — two placement paths (worker build vs direct event). **EXE-link: 2 `DllExport`** — `getEntityEvent`
(XML → `world.art.entityEvent`) + `getMissionType` (RUNTIME-assigned, not XML → dropped). Bespoke curator; no modifier
families (Build is an action, never a cascade source/target). Mechanical de-Hungarian + the ui/world art blocks not re-logged.

**FOLDERED by PRIMARY outcome (owner 2026-06-16)** into `Assets/Data/builds/<folder>/`, priority
**bonus > forts > routes > features > terraform > improvements > clearing** (`curate_build.folder`): `bonus` (65, the
improvement PLACES a bonus — `bPlacesBonus`/`BonusChange`), `forts` (28, improvement `bActsAsCity`/`bMilitaryStructure`),
`routes` (21, `RouteType`), `features` (12, ADD a feature `FeatureChange`), `terraform` (18, `TerrainChange`),
`improvements` (150, the bulk), `clearing` (10, NO improvement — REMOVE feature/terrain/pollution: drain swamp / clear
fallout / burn vegetation / break ice / reclaim land / remove rock). A multi-outcome build (e.g. `BUILD_ROAD` clears
features but lays a route) lands by the priority above (→ `routes`).

| old XML | new JSON path | note |
|---|---|---|
| `PrereqTech` | `requires.build.all[].{type,scope:team}` | The build's OWN tech gate (`CvPlayer::canBuild:7540`). ALSO store-inverts to `tech.enables.builds` (generation) — this clause is the per-candidate CONFIRM, the two coexist (Improvement/Tech precedent). |
| `PrereqBonusTypes`/`PrereqBonusType` | `requires.build.all[].{type:BONUS_X,scope:plot,connection:trade}` | ALL listed bonuses must be adjacent-plot-group-connected (`CvPlot::canBuild:3398`). ⚑ scope: the gate is plot-group-local trade reach; exact scope (plot vs city vs team) pins at #430. 3 builds (MOAI_STATUES/GEOGLYPH/MACHU_PICCHU). |
| `ImprovementType` / `RouteType` / `TerrainChange` / `FeatureChange` | `produces.{improvement,route,terrainChange,featureChange}` | Single-FK outcomes (improvement laid / route laid / terraform-to terrain / feature planted-or-changed-to). FK only — never embeds the target's data (owner clean-separation ruling). |
| `FeatureStructs`/`FeatureStruct` `{FeatureType,PrereqTech,iTime,iProduction,bRemove}` | `produces.features[].{feature, tech?, time?, production?, remove?}` | Per-feature add/REMOVE during the build: `remove:true` = the CHOP (clears the feature → `production` hammers + `time`); a `remove`-absent entry with only `tech` is the per-feature TECH GATE ("road on a swamp needs Canal Systems"). Per-feature PrereqTech STAYS here — it is CONDITIONAL on the plot already having that feature, NOT an unconditional `requires.build` (`CvPlayer::canBuild:7550`); it ALSO store-inverts to `tech.enables.builds`. Zero `time`/`production` + `remove:false` omitted. Edits → post-migration phase (owner). |
| `TerrainStructs`/`TerrainStruct` `{TerrainType,PrereqTech,iTime}` | `produces.terraform[].{terrain, tech?, time?}` | Per-terrain terraform time + tech gate (`CvPlayer::canBuild:7556`). 9 builds. |
| `iCost` | `cost.gold` | Gold spent on the build (`CvPlayer::getBuildCost:7609`, × gamespeed). Cost-vs-costs convention (a base intrinsic cost, not a multiplier family). Becomes important for BuildingInfo/UnitInfo later (owner). |
| `iTime` | `cost.time` | Base worker-turns (`CvPlot::getBuildTime:7568`); always emitted (incl. 0). **`iTime < 0` is the build-DISABLED sentinel** (`CvPlot::canBuild:3345`) — carried faithfully. |
| `Button` | `ui.art.icon` | The build's action button (the IMPROVEMENT it lays carries NO `<Button>` — it lives on the build). |
| `HotKey` / `iHotKeyPriority` / `bShiftDown` / `bCtrlDown` / `bAltDown` | `ui.{hotkey,hotKeyPriority,shiftDown,ctrlDown,altDown}` | `CvHotkeyInfo` base-class key triggers (Build is the FIRST hotkey-bearing entity migrated, sets the clean shape per `ART_BLOCK`: bools only when true, priority only when non-zero, **`HotKey "0"` = no-hotkey placeholder → dropped**, 65 builds). |
| `EntityEvent` | `world.art.entityEvent` | The on-map worker ANIMATION (`ENTITY_EVENT_SHOVEL/IRRIGATE/BUILD/…`), EXE-bound (`getEntityEvent` DllExport). Added to `curate_common.ART_BLOCK`. |
| `bKill` | `identity.consumesUnit` | The build CONSUMES the worker on completion (`isKill`; owner: "isConsumed" — gatherers / great farmers / worker ships). 94 builds. ⚑ **FUTURE (owner 2026-06-16):** floated moving this to the worker UNIT as an `isConsumed` variable — BUT that would break correct current behaviour, since the SAME unit does both consuming and NON-consuming builds (a boat is NOT consumed when placing a seaweed gatherer, + a few others — and getting that wrong is "mightily annoying for AI"). So consume-or-not is genuinely a **per-BUILD** decision, which is exactly the build-keyed model here. Any unit-side relocation must preserve the per-build distinction — DEFERRED to the Unit pass / a separate ticket, decided then; kept build-keyed (`consumesUnit`) until then. |
| `ObsoleteTech` | (store) `tech.obsoletes.builds` | Drop from the build (store-inverted; tech obsoletes the build). |
| `MissionType` | — (DROP) | RUNTIME-assigned (`m_iMissionType`, `NO_MISSION` default, `setMissionType`), NOT XML-backed — never on the XML record. `getMissionType` is DllExport but reads the runtime value. |
| `MapCategoryTypes` | — (DROP, 0/304) | A live placement gate (`CvPlot::canBuild:3406` `isMapCategory(info)`) but **SPACEMAP-related** (owner 2026-06-16) → handled via existing tooling when spacemap is fixed properly. No build data today. |
| `PlaceBonusTypes` | — (DROP, 0/304) | A SPECIAL place-a-bonus struct, **dropped** (owner 2026-06-16). The place-a-bonus capability belongs to the BUILD as canonical tooling — concretely the `BUILD_BONUS_*` builds lay an `IMPROVEMENT_BONUS_*` whose own `bPlacesBonus`/`BonusChange` spawns the bonus (the improvement-side mechanic, parked on improvement identity, its own pass). The special struct + the **Great Farmer's unit-side `UnitInfo.PlaceBonusTypes`** hijack are the hacks to retire; the Great Farmer is to be reworked to ADHERE to the build tooling (Unit pass / post-migration). |
| `Categories` | — (DROP, 0/304) | Dead: no `getBuildInfo(...).getCategory` consumer (same as Terrain/Improvement's dead `Categories`). |

*Toolkit: `curate_build.py` (bespoke, modeled on `curate_victory.py`); added `EntityEvent → world.art.entityEvent` to
`curate_common.ART_BLOCK`. `BuildInfo` was already registered in `store.ENTITIES` + its edges in `PREREQ_FIELDS`
(PrereqTech, PrereqBonusTypes, per-feature/terrain PrereqTech → builds) + `OBSOLETE_FIELDS` (ObsoleteTech). The
`produces` section is a NEW reserved section (owner-approved 2026-06-16) — the build-OUTCOME home; #430 reads it as the
worker-action outcome system.*

## PromotionLine  (`curate_promotionline.py`)  — Tier D #27 (333 records: 262 base + 72 module − 1 dropped)

A GROUPING/HIERARCHY axis for promotions (the "promotion line" chains), **units-only**. NOT a modifier source (the
individual PROMOTION owns yield/commerce/property modifiers — audit 2026-06-14) and **enables nothing**
(`store.enabled_by(PROMOTIONLINE_*)` empty → no `enables`). It "rides Promotion" (#28): its applicability gates are
unit-plane vocabulary the Promotion pass defines, so they are PARKED in `identity` here and re-homed there (owner
2026-06-16: "we need a broader picture on the enabling"). **EXE-link 0 `DllExport`** (unconstrained). Bespoke curator,
no cascade families. Consumers: the line's gates are read by `CvPromotionInfo` (cpp:2080) + `CvUnit` (17664/25585) to
decide which units may take the line's promotions.

| old XML | new JSON path | note |
|---|---|---|
| `OnGameOptions` | `loadPrune.onGameOptions` | Load-stable game-option availability gate (enabler-spec §6/§10; CultureLevel/Trait precedent). Owner 2026-06-16: WorldBuilder toggling game options is the enable/disable mechanism (engine removes disabled promotions; WB "supported but use at own risk"). `NotOnGameOptions` → `loadPrune.notOnGameOptions` (0 populated). |
| `bBuildUp` | `buildUp.active: true` | **Dedicated OBJECT MODULE** (owner 2026-06-16: "treat buildup as a module, create a home for it" — disliked but kept, so isolated + purgeable; modifier-spec §0.8). The 28 build-up lines. Object-module activation convention: the block's presence is the signal; `active:true` is the INTERIM marker because the line record holds no other build-up data — WHAT a line builds up (defense/chasedown/…) lives on its **promotions**, enriched at the Promotion pass, at which point `active` becomes redundant and drops. Buildups have NO property (owner). |
| `UnitCombatPrereqTypes` / `NotOnDomainTypes` / `NotOnUnitCombatTypes` | `identity.{unitCombats, notOnDomains, notOnUnitCombats}` | Unit-plane applicability/exclusion gates (which units may take the line's promotions). PARKED in identity, deferred to the Promotion pass (the unit-plane enabling vocabulary isn't defined yet); not store-inverted, so kept here faithfully (provisional names). |
| `Button` | `ui.art.icon` | The promotion-line button. |
| `PrereqTech` | (store) `tech.enables.promotionLines` | DROP (1 line, TECH_PIERCING). Single tech → **no `requires`** (owner #4: a lone tech needs no requires; only a multi-tech AND would be "murky", which doesn't arise — PromotionLine has one `PrereqTech`). |
| `PropertyType` / `bPoison` | — (DROP, gone with the affliction line) | The SOLE `PropertyType` carrier was the dropped affliction line; `bPoison` 0. Removed from the model entirely → `buildUp` is only ever `{active:true}`. |
| `ObsoleteTech` / `Categories` / `UnitCombat`+`Tech ContractChanceChanges` | — (DROP) | 0-populated. ObsoleteTech (not store-registered, moot); Categories (checksum-excluded, dead); `*ContractChanceChanges` (dead unimplemented system — ranking #27). |
| `Promotions` / `Buildings` (post-load) | — (not XML) | Derived reverse indexes (`doPostLoadCaching` from `CvPromotionInfo.getPromotionLine` / `CvBuildingInfo.getPromotionLineType`) — emit nothing. |

**DROPPED ENTITY (owner 2026-06-16):** `PROMOTIONLINE_AFFLICTION_DISEASE_COMMON_COLD` — a stone-dead vestige of the
purged **Outbreaks-and-Afflictions** mod (a TRUE orphan: 0 promotions/buildings/traits reference it, GameText keys
only). Dropped during migration as a targeted owner-directed exception to the §0 "purge entities in a separate pass"
rule, because it was the **sole `PropertyType` carrier** — dropping it kills the PropertyType/bPoison special case
(`buildUp` collapses to one shape). The small-pass also found 4 other true orphans (BARBARIAN/MARAUDER/MEDIC/SNEAK)
that carry NO special fields → left for the post-migration content-purge pass (not dropped here).

*Object-module convention (top-level keys = category objects; presence-and-non-empty = active; empty/absent = false)
recorded in modifier-spec §0.8. `PromotionLineInfo` already registered in `store.ENTITIES` + `PREREQ_FIELDS`
(PrereqTech → promotionLines).*

⚑ **Promotion-pass baseline (owner 2026-06-16):** WHAT each build-up line builds up is its baseline identifier, to be
derived at the Promotion pass from the line's promotions — surveyed: only 4 of 28 build-ups target a PROPERTY
(CRIME_FIGHTING/SCHEMES→`PROPERTY_CRIME`, DISEASE_CONTROL→`PROPERTY_DISEASE`, TEACH→`PROPERTY_EDUCATION`, via their
`PropertyManipulators`/`PropertySource`); the other 24 build up combat/unit STATS (defense/range/concealment/…). So
`buildUp.property` (the 4) + a stat identifier (the 24) get filled there, and `buildUp.active` (the interim marker)
drops. ⚑ **WATCH (owner, unverified):** the C++ that matches the correct promotion line to a unit MAY be complex —
not confirmed, just anticipated at this stage; approach the unit-plane enabling carefully (safe rather than sorry) and
verify it at the Promotion pass rather than assuming either way.

## LeaderHead  (`curate_leaderhead.py`)  — Tier D #30 (119 records, base only)

The AI PERSONALITY entity — ~90 AI/diplomacy/strategy params defining how an AI player behaves all game. NOT a
cascade source/target (zero per-turn-effect fields): virtually everything lands in the unified **`ai`** group,
subgrouped. This is the densest `ai` entity, so it shapes the `ai` sub-group vocabulary — but that vocabulary is
PROVISIONAL (expected to be reworked at the load-writing/readJson phase, owner 2026-06-16), so it is NOT logged as a
locked spec; the subgroup→field map below IS the reader's lookup. EXE-link: **1 `DllExport`** (`getArtInfo` →
`ArtDefineTag`). Bespoke curator; `LeaderHeadInfo` newly registered in `store.ENTITIES` (no enabler edges). Mechanical
de-Hungarian within each subgroup not re-logged.

| old XML | new JSON path | note |
|---|---|---|
| `Flavors` | `ai.flavours` | FLAVOR_* weights. |
| personality scalars (`iBaseAttitude`/`iBasePeaceWeight`/`iPeaceWeightRand`/`iWarmongerRespect`/`iEspionageWeight`/`iWonderConstructRand`/`iBuildUnitProb`/`iFreedomAppreciation`/`iVassalPowerModifier`) | `ai.personality.{…}` | The misc personality knobs. |
| war/peace rands (`iMaxWar*`/`iLimitedWar*`/`iDogpileWarRand`/`iMakePeaceRand`/`iDeclareWarTradeRand`/`iDemandRebuked*`/`iRefuseToTalkWarThreshold`/`iBaseAttackOddsChange`/`iAttackOddsChangeRand`/`iRazeCityProb`) | `ai.war.{…}` | AI war/peace decision parameters. |
| `i{Culture,Space,Conquest,Domination,Diplomacy}VictoryWeight` | `ai.victory.{culture,space,conquest,domination,diplomacy}` | AI victory-pursuit weights. |
| `iMaxGoldTradePercent`/`iMaxGoldPerTurnTradePercent`/`iNoTechTradeThreshold`/`iTechTradeKnownPercent` | `ai.trade.{maxGoldPercent,maxGoldPerTurnPercent,noTechTradeThreshold,techTradeKnownPercent}` | AI trade thresholds. |
| the attitude relation families (`i{CloseBorders,LostWar,AtWar,AtPeace,SameReligion,DifferentReligion,BonusTrade,OpenBorders,DefensivePact,ShareWar,FavoriteCivic}Attitude{Change,Divisor,ChangeLimit}` + `i{Better,Worse}RankDifferenceAttitudeChange`) | `ai.attitude.<relation>.{change,divisor,changeLimit}` | Relation-driven attitude deltas, grouped per relation. |
| the `*RefuseAttitudeThreshold` + `DemandTribute`/`NoGiveHelp AttitudeThreshold` strings | `ai.refuse.<deal>: ATTITUDE_*` | Min attitude to agree to each deal/action. |
| `MemoryDecays` / `MemoryAttitudePercents` | `ai.memory.{decay,attitudePercent}: {MEMORY_*: n}` | Per-memory decay rand + attitude impact. |
| `ContactRands` / `ContactDelays` | `ai.contact.{rand,delay}: {CONTACT_*: n}` | Per-contact-type initiation odds + cooldown. |
| `NoWarAttitudeProbs` | `ai.noWarProb: {ATTITUDE_*: n}` | Per-attitude no-war probability. |
| `UnitAIWeightModifiers` / `ImprovementWeightModifiers` | `ai.unitWeights: {UNITAI_*: n}` / `ai.improvementWeights: {IMPROVEMENT_*: n}` | AI build/value weightings. |
| `FavoriteCivic` / `FavoriteReligion` | `ai.favorites.{civic,religion}` | AI attitude drivers (owner-agreed → ai, not identity). |
| `bNPC` | `ai.npc: true` | AI-only (barbarian/NPC) leader — an AI classification (owner 2026-06-16: fits `ai`, not identity; 3 leaders). Empties `identity` entirely. |
| `Traits` / `DefaultTraits` / `DefaultComplexTraits` | `grants.{traits, developingTraits, complexTraits}` | The leader grants its traits at game start. FAITHFUL lists; the option-driven SELECTION/filtering (`GAMEOPTION_LEADER_{COMPLEX_TRAITS,DEVELOPING,START_NO_POSITIVE_TRAITS,PURE_TRAITS}` + `isValidTrait` + LinePriority) is the create-player subroutine = engine machinery (§0.6), not data conditions. ⚑ **DEFERRED:** the simple→complex MIRRORING (deriving `complexTraits` for the 117 base-only leaders by `TRAIT_X`↔`TRAIT_COMPLEX_X` name-match; "developing" = an original with no complex twin) is a derivation depending on the simple↔complex correspondence the Trait pass dropped → resolve at the trait-system coding pass (#430). Mid-game trait-type swaps are catastrophic (WB-safe-swap ticket #438). |
| `ArtDefineTag` | `world.art.icon` | On-map leaderhead portrait FK (EXE-bound `getArtInfo`). |
| `Diplomacy{Intro,}Music{Peace,War}` | `sound.diplo{Intro,}Music{Peace,War}` | Diplo-screen audio. Full music = `{era: DiploScriptId}` map; intro music = era LIST (its entries carry only `EraType`, no script). Kept faithful (4 maps × ~14 eras/leader); run-collapse trimming deferred to a later audio pass (owner 2026-06-16). |

*Toolkit: `curate_leaderhead.py` (bespoke); `LeaderHeadInfo` added to `store.ENTITIES`. The `ai` subgroup vocabulary
is provisional (reworked at the load-writing phase) — captured here as the reader's old→new lookup, not a spec lock.*

## Promotion  (`curate_promotion.py`)  — Tier D #28 (1229 records, BESPOKE)

A **unit-plane stat SOURCE**: every effect is a **`unit`-scope SELF-ACCUMULATOR** deposit (source==target via
`CvUnit::processPromotion`'s `changeExtra*`/`change*Count` stack, modifier-spec §5), verified field-by-field against
that consumer + `processUnitCombat` + the capture/hidden-nationality Python. EXE-link **0 `DllExport`** on
`CvPromotionInfo` → unconstrained. Shares the unit-stat vocabulary with **UnitCombat #29** + SpecialUnit (Unit pass).
**This JSON is the STATIC DEFINITION of the promotion, NOT the promotion-as-on-a-unit** — but every shape is
**accumulator-shaped so adding a promotion to a unit is clean O(1) CONCATENATION** (sum `unit`-scope deposits, union
capabilities, merge the `promotionLine` map), never apply-time post-processing (owner 2026-06-16). The first-pass
mapping dumped ~all fields into `identity`/`prereqs` (the documented under-classification trap) — the bespoke curator
carries the real vocabulary. Coverage-checked: every XML tag handled.

**THE UNIT-STAT FAMILY VOCABULARY (owner-blessed 2026-06-16; names PROVISIONAL — a reader-pass refines them).** All
at **`unit` scope**.
- **`strength`** — THE combat family: *"the strength of something, or weakness ON / INTO / AGAINST something"* (owner).
  Members: `percent`←`iCombatPercent` · `flat`←`iStrengthChange` · `sizeModifier`←`iStrengthModifier`(SM) ·
  `attack`/`defense`←`i{Attack,Defense}CombatModifierChange` · `vsBarbs` · `religious` · `stealth` · `damageModifier`
  · `maxHP`(SM, post-migration review) · `quality`/`group`(SM) · `perSize{More,Less}`/`perVolume{More,Less}`(SM) ·
  `unnerve`/`enclose`/`lunge`/`dynamicDefense`(S&D `SURROUND_DESTROY`, live/deferred) · `endurance`/`taunt`/
  `breakdownChance`/`breakdownDamage` · `kamikaze`/`combatLimit`/`stealthStrikes` · **situational** `cityAttack`/
  `cityDefense`/`hillsAttack`/`hillsDefense` · **vs-keyed** `terrain.{T}.{attack,defense}`,`feature.{F}.{attack,defense}`,
  `unitCombat.{UC}`,`domain.{D}`,`flanking.{UC}`.
- **`withdrawal`** `firstStrike`{strikes,chance} `bombard`{rate,rangedDamage[/Limit/MaxUnits],dcmRange,dcmAccuracy}
  `collateral`{damage,limit,maxUnits,protection} `air`{range,intercept,evasion,combatLimit}
  `heal`{enemy,neutral,friendly,sameTile,adjacentTile,selfModifier,support,victory[/Adjacent/Stack],unitCombat.{UC}}
  `movement`{moves,moveDiscount,dropRange} `experience` `workRate`{rate,hills,peaks,terrain.{T},feature.{F},build.{B}}
  `cargo`{space,smSpace,volume,volumeModifier} `upkeep`{modifier,extra100,upgradeDiscount} `vision`{range + the LOS
  resolver, below}.
- **`capture`** {probability,resistance} — captive-taking of units (+subdued animals); a **GRADIENT** (numeric: the
  engine nets `captureProbabilityTotal - captureResistanceTotal` into a 0-99% chance, summed across unit/promotion/
  commander/national/local), its OWN family (owner). ⚑ owner design intent: capture (military captives) and **subdue**
  (animals) *should* be separate mechanics later — a hunter good at trapping animals shouldn't freely take military
  captives; not changed in #428 (faithful), recorded for a future pass.
- **`poison`** {probability} — the (inert) **afflictions** mechanic ("nothing can be poisoned"); kept-for-now.
- **`espionage`** {insidiousness (= revolutions, owner), investigation (= crime, owner)}. **`trap`** {damageMax,
  damageMin,complexity,numTriggers,disable.{UC},avoidance.{UC},trigger.{UC}} (dedicated sub-system block).
  Singletons: **`revoltProtection`** `pillage` `survivor`. per-`PROPERTY_*` (below).

**CAPABILITIES — a SEPARATE group of BOOLEANS (owner): grant=`true` / revoke=`false`** (the `Add`/`Subtract` & `Remove`
pairs). `change*Count`-verified. Plain bools (blitz/amphib/river/…); pair grant-or-revoke (stampede±, attackOnlyCities±,
ignoreNoEntryLevel±, ignoreZoneofControl±, fliesToMove±); the count-int abilities (excile, **passage** [non-combat
units enter foreign land w/o granting military passage], noNonOwnedCityEntry, barbCoExist, blendIntoCity,
upgradeAnywhere, hiddenNationality, assassin, stealthDefense, defenseOnly, noInvisibility, triggerBeforeAttack,
noDefensiveBonus, gatherHerd, animalIgnoresBorders — `>0` ⇒ has it); per-type ability maps (terrainDoubleMove.{T},
featureDoubleMove.{F}, trapImmunity.{UC}, trapTarget.{UC}, trapSetWith.{PROMOTION}).

| old XML | new JSON path | note |
|---|---|---|
| `PropertyManipulators`/`PropertySource` | `<PROPERTY>.<scope>.flat` (v3) | The live crime/disease/education **unit→city/plot emission** translated to the SCOPED MODIFIER system (owner 2026-06-16: "scoped like other property yields, like a modifier"). `PROPERTYSOURCE_CONSTANT`→flat; scope from `GameObjectType` (`GAMEOBJECT_PLOT`→`plot`, `GAMEOBJECT_CITY`→`city`); a promotion emitting to BOTH = **two modifiers** (`PROPERTY_CRIME.{city,plot}.flat`). `RELATION_SAME_PLOT` is the containment default → **`engine.property_source_v3` extended** to drop it (like `ASSOCIATED`). Feeds the PromotionLine `buildUp.property` baselines (EDUCATE→EDUCATION, CRIME_FIGHTING→CRIME, DISEASE_CONTROL→DISEASE). |
| `PromotionLine` + `iLinePriority` | `promotionLine: {PROMOTIONLINE_X: rank}` | Line membership as a `{LINE: rank}` **OBJECT** (owner 2026-06-16). `iLinePriority` is a **RANK** within the line (COMBAT1=1, COMBAT2=2, …), NOT a priority. Object (keyed by line) is accumulator-friendly: a unit's promotions MERGE their maps into one `{LINE: rank, …}` → a clean **bottom→up rank overwrite** + line-hierarchy check (owner). ~261 of 1229 promotions have NO line → omitted. |
| `OnGameOptions`/`NotOnGameOptions` | `loadPrune.{onGameOptions,notOnGameOptions}` | Load-stable game-option gate. The 192 option-gated promotions link to **LIVE** optional mechanics (SIZE_MATTERS 83 / HIDE_SEEK 70 / WITHOUT_WARNING 39) — conditionally active, **none dead-by-dead-option**. |
| `SubCombatChangeTypes`/`RemovesUnitCombatTypes`/`FreetoUnitCombats`/`AddsBuildTypes`/`SetSpecialUnit` | `grants.{unitCombats,removesUnitCombats,freeToUnitCombats,builds,specialUnit}` | What the promotion CONFERS on the unit. |
| `VisibilityIntensity*`/`InvisibilityIntensity*` + `Invisible/Visible{Terrain,Feature,Improvement}[Range]Changes` + `NegatesInvisibilityTypes` + `iVisibilityChange` | `vision.{visibilityIntensity,…, invisibleTerrain[],…, negates, range}` | The hide-&-seek **LOS RESOLVER** — a non-cascade structured `vision` block (modifier-spec §7/§0.6), read by the visibility resolver, NOT additive families. |
| `AIWeightbyUnitCombatTypes` | `ai.unitCombatWeights: {UC: weight}` | AI weighting (how the AI values this promotion per unitcombat) → the `ai` group. |
| `TechPrereq`/`PrereqBonusTypes`/`PrereqPlotBonusTypes` | (store) `tech`/`bonus.enables.promotions` | The promotion is a generated candidate of its tech/bonus → store-inverted, DROPPED here. |
| `PromotionPrereq`/`PromotionPrereqOr1`/`Or2` | — (DROPPED) | Promotion-on-promotion prereq → the enabling lives on the **tech it requires**; the chain ORDER is carried by `promotionLine`+rank (owner #4 2026-06-16). |
| `ObsoleteTech` | (store) `tech.obsoletes.promotions` | A tech can obsolete a promotion → **new `OBSOLETE_FIELDS` edge** (owner-approved), DROPPED here. |
| `UnitCombats`/`NotOnDomainTypes`/`NotOnUnitCombatTypes`/`Prereq{Terrain,Feature,Improvement,PlotBonus,LocalBuilding}Types`/`StateReligionPrereq`/`Min`/`MaxEraType`/`iLevelPrereq`/cargo·rBombard·normInvisible prereqs/command/leader/renames/replacesUnitCombat/celebrityHappy/zeroesXP/status flags | `identity.*` | **PARKED**: the promotion's applicability/plot/state gates + config flags, deferred to the unit-plane enabling pass (as PromotionLine #27 parked its gates — the broader enabling picture is still being shaped). `celebrityHappy` is really a city-happiness modifier (byOccupant, §5) → structured properly at the Unit pass. `zeroesXP` = the XP-reset cost of a quality upgrade (engine acts at acquire-time). |
| `iDamageperTurn`/`iStrAdjperTurn`/`iWeakenperTurn` | — (DROPPED) | **BATTLEWORN, nuked** (owner): `#define`d-out mechanic, not even applied in `processPromotion` (pedia-render only). |
| `Categories` / `iStealthCombatModifier` / `Qualified`/`DisqualifiedUnitCombatTypes` | — (DROPPED) | Dead (`Categories`); XML **typo** (`iStealthCombatModifier` — engine reads `…Change`, ignored in-game, 2 recs); pedia-derived (`doPostLoadCaching`). |

*Toolkit: `curate_promotion.py` (bespoke; modeled on `curate_build.py`) with a **coverage check** (reports any XML tag
handled by no table — caught 3 misses + the typo). `store.py` gained the `PromotionInfo.ObsoleteTech`→`promotions`
`OBSOLETE_FIELDS` edge; `engine.property_source_v3` extended to accept `RELATION_SAME_PLOT` as the containment default
(alongside `ASSOCIATED`). `PromotionInfo` was already registered + its prereq edges in `PREREQ_FIELDS`. ⚑ NEXT:
UnitCombat #29 reuses this vocabulary (the designated definer of the §5 banked gap) + holds the SM `*Base` ranks (kept
as-is, §0.6) + the `CvOutcomeList` kill/action system; SpecialUnit shares it at the Unit pass.*

## UnitCombat  (`curate_unitcombat.py`)  — Tier D #29 (814 records, BESPOKE)

The unit-COMBAT-CLASS (`UNITCOMBAT_MELEE`/`ARCHER`/…) — a unit-plane stat SOURCE that deposits onto a unit via
`CvUnit::processUnitCombat` (the §5 self-accumulator). **REUSES the Promotion #28 unit-stat vocabulary VERBATIM** —
the curator IMPORTS Promotion's `STRENGTH`/`FAMILIES`/`CAP_*`/`VISION_*` tables (the two entities jointly DEFINE the
§5 vocabulary; importing enforces they can't drift). EXE-link **0 `DllExport`** → unconstrained. Newly registered in
`store.ENTITIES`. Coverage-checked. Same as Promotion: `*Change` stats → the same families (strength/…/capture/poison/
espionage/trap), CAPABILITIES boolean group, the `vision` LOS resolver, properties → scoped `PROPERTY_X.{plot,city}.flat`
deposits (`property_source_v3`). UnitCombat-specific:

| old XML | new JSON path | note |
|---|---|---|
| `iQualityBase`/`iGroupBase`/`iSizeBase` (`-10` sentinel) + `iRBombardDamage*Base`/`iDCMBomb*Base` | `identity.base.{quality,group,size,rangedBombard*,dcm*}` | The CLASS's intrinsic **create-unit base stats** (§0.6 "create-unit-subroutine data, kept as-is") — NOT cascade modifiers, NOT the `*Change` deltas Promotion carries. ⚑ **SIZE-MATTERS clusters here** (owner 2026-06-16: "size matters seems to be mostly governed in unitcombat") → flagged for a **dedicated cross-entity Size-Matters pass** (a `sizeMatters` module §0.8 gathering the base ranks + the SM-gated `strength.sizeModifier`/`perSize`/`perVolume`/`maxHP` + the `INVISIBLE_SIZE` vision + SM cargo, across Promotion+UnitCombat+Unit); kept faithful for #428. |
| `KillOutcomes` / `Actions` | `outcomes.{kill,actions}` | The `CvOutcomeList` mission-outcome system (subdue-animal record-tale, animal-combat reward, …), carried faithfully (`engine.generic`). **An `outcome` is a DEFERRED, MISSION-TRIGGERED, unit-ACCUMULATED grant** — distinct from `grants` (owner 2026-06-16): `grants` = **NOW** (fires on its event if the enabler/tech is held, §6.1); `outcomes` = **FUTURE** (parked on the unit, accumulated, fires only when the mission is pushed). Same one-time-yield *shape* (the commerce burst is wrapped in `AdaptUnitYield` = GameSpeed `missionYieldMultiplier`, the SAME scale a merchant trade-mission uses), different *lifecycle* → its own section, NOT folded into `grants`. ⚑ The opaque positional `Commerces` `[gold,research,culture,espionage]` (each a flat or an `AdaptUnitYield(Constant N)`) renders grant-shaped (`{gold,culture}` + the scale) at the **dedicated outcome-system pass** (#430), which also brings in the merchant `UnitInfo` trade-mission. No promotion/unitcombat grants a gold *trade*-mission; only `UNITCOMBAT_MAMMAL_RHINO`/`URSINE` carry gold via `MISSION_ANIMAL_COMBAT`. |
| `ReligionType`/`CultureType`/`EraType` · `bForMilitary`/`bForNavalMilitary` · `GGptsforUnitTypes` · `DefaultStatusTypes` | `identity.{religion,culture,era, forMilitary,forNavalMilitary, ggPointsForUnits, defaultStatuses}` | Refs + AI tags + great-general-points-per-killed-type + auto-applied statuses — PARKED (their proper homes — enabler? grants? — settle at the unit-plane enabling / Unit pass). |
| vs-keyed combat/work modifiers (`TerrainAttackChangeModifiers`/`FeatureAttackChangeModifiers`/`UnitCombatChangeModifiers`/`FlankingStrengthbyUnitCombatTypeChange`/`*WorkChangeModifiers`/`TrapAvoidanceUnitCombatTypes`/`DomainMods`) | `strength`/`workRate`/`trap`.unit.…  | Same homes as Promotion's `VS_KEYED`, under UnitCombat's `*ChangeModifiers` container names (`{Type, iModifier}` shape). |
| extra capability bools (`bSpy`/`bCannotMergeSplit`/`bRBombardDirect`/`bRBombardForceAbility`/`bInvisible`/`bHealsAs`) + `iNoCaptureChange` | `capabilities.{spy,cannotMergeSplit,rBombardDirect,rBombardForceAbility,alwaysInvisible,healsAs,noCapture}` | UnitCombat-only capabilities. |
| `VisibilityIntensitySameTileChangeTypes` | `vision.visibilityIntensitySameTile` | The extra same-tile LOS pair-list (UnitCombat-only). |
| `Button` | `ui.art.icon` | The combat-class icon (UnitCombat extends `CvInfoBase` but carries a `Button`; all 814). |
| `Categories` / `FeatureAttacks` / `FeatureDefenses` / `iWithdrawalProb` | — (DROPPED) | `Categories` dead; the others are WRONG-TAG entries the engine ignores (it reads `FeatureAttackChangeModifiers` / `iWithdrawalChange`) — dead in-game (the Promotion `iStealthCombatModifier`-typo pattern). |

*Toolkit: `curate_unitcombat.py` (bespoke) imports the shared tables + helpers from `curate_promotion`. `UnitCombatInfo`
registered in `store.ENTITIES`. NEXT: SpecialUnit shares this vocabulary at the Unit pass (depositing onto the loaded
unit); the SM-module + outcome-system + the merchant trade-mission consolidations are their own later passes.*

## Building  (`curate_building.py`)  — Tier E #32, THE MONSTER (5202 records) + SpecialBuilding #31 (36 records)

The deepest modifier surface (288 fields), curated from `classifications/building-classification.json` (the adversarial
classify-building workflow) + owner rulings (handover-2026-06-16-6). **0 `DllExport` -> data UNCONSTRAINED.** A fully
BESPOKE curator (modeled on `curate_promotion`) with a COVERAGE CHECK + era foldering (era from the PrereqTech). The
SOURCE->building enabler edges (tech/bonus/civic/religion/corp/cultureLevel `enables`, `ReplacementBuildings`->replaces,
`ObsoleteTech`->tech) are ALREADY store-wired -> DROP building-side. The ~70 scalar/percent families + the yield/commerce
split are field->family.member.unit with **scopes corrected from the classification** (the mapping's were often wrong:
`iCoastalTradeRoutes`->empire, Area*->area, Global*->empire). Mechanical de-Hungarian not re-logged. Structural / manual:

| old XML | new JSON path | note |
|---|---|---|
| (prereqs) `PrereqTech`/`TechTypes`/`Bonus`/`PrereqBonuses`/`VicinityBonus`/`RawVicinityBonus`/civic/religion/corp/cultureLevel/`bWater`/`bRiver`/`bFreshWater`/`bPower`/counts/`ConstructCondition`/… | `requires.{build,operate}` (`all`/`any`/`noneOf`) | The TARGET-side MEANS gate. **build** = greying (resources `{type,scope:city,connection:"trade|vicinity"}`, plot predicates `IS_WATER`/`HAS_RIVER`/`IS_FRESHWATER`/**`HAS_POWER`**, counts→tally `{type:POPULATION\|CITY\|TEAM\|UNIT_LEVEL\|AREA_SIZE,min}` of OTHER types, in-city buildings, `{latitude:{min,max}}`); **operate** = dormancy (civics, religion/corp/cultureLevel, `HAS_STATE_RELIGION`/`STATE_RELIGION_IN_CITY`). **RawVicinity FOLDS into `connection:"vicinity"`** (owner: lose the adjacency strictness). ⚑ NEW tokens (owner-approved): `HAS_POWER`, `HAS_STATE_RELIGION`, `STATE_RELIGION_IN_CITY`, the count kinds, `{latitude}`. **PALACE-TYPE (`bCapital` Palace + 8 `bGovernmentCenter` pseudo-palaces — Forbidden Palace/El Escorial/Versailles/Edinburgh Castle/…) → `requires.build.disabled:"IS_CAPITAL"`** (owner 2026-06-16): the `disabled` twin (enabler-spec §3), can't player-build where a government center already exists (CvCity.cpp:2654); PLAYER gate only — the engine's forced capital-move bypasses (#437 placement-gate invariant). `IS_CAPITAL` = "city has a palace/gov-center building". Surfaced by the readjson harness render. |
| `iMaxGlobalInstances` / `iMaxTeamInstances` / `iMaxPlayerInstances` | `allowed.{world,team,empire}` | **The instance CAP → declarative `allowed`, NOT a `requires` SELF-atom** (owner 2026-06-17). `allowed:{world:1}` = "at most 1 may exist in the world" (the real number; the old `requires.build SELF max:N` forced an off-by-one — `max:0` for a cap of 1 — and conflated *needed* with *allowed*). The cap scope ALSO derives the wonder category (`isWorldWonder == getMaxGlobalInstances()!=-1`, CvGameCoreUtils.cpp:340-369). 1307 buildings (1044 world + 263 empire). Engine enforces (build while `tally.count(SELF,scope) < N`) + owns ignoring it under `NO_WONDER_LIMIT`/`NO_NATIONAL_UNIT_LIMIT`/`CHALLENGE_ONE_CITY`, era-scaling, `+extra`. `SELF` left `requires` entirely. enabler-spec §5a/§13.7. |
| `iCoastalTradeRoutes`/Area*/Global*/National* etc. (~70 scalars) | `<family>.<scope>[.<member>].<unit>` | Corrected scopes. NEW families: **`cityCapture`** (National/Local Capture Probability/Resistance — capturing CITIES, distinct from the §5 unit `capture` gradient), `espionage`{insidiousness,investigation}, `espionageDefense`, `healing`, `foodKept`, `hurryCost`/`hurryAnger`, `cityCapture`, `pillageGold` (`iPillageGoldModifier` REVIVED, §8-ii), `populationGrowthRate`, `occupationTime`. `defense` is grouped (`amount`/`min` floor/`bombardDefense`/`nukeDefense`/`airDefense`/`noEntryLevel`/`dynamicDefense`/`riverDefensePenalty`/recovery/`damageAttackerChance`/`damageToAttacker`/`adjacentDamage`/`repel`). |
| the 22 "inversions" (`Tech*Changes`/`Bonus*Changes`/`Improvement`/`Terrain`/`Plot`/`ReligionChanges`/Building-on-building/`Specialist*`/`UnitCombat*`/`Unit*`/`Domain*`) | KEEP-ON-BUILDING (§6.1) | The mapping's `inversionsOut` is the STALE pre-v3 rule. **CONDITION-gated** (`enabled`): Tech (`{type:TECH,scope:team}`, PROVISIONAL pending Phase-F), Bonus (`{type:BONUS,scope:city,min:1}`), Building-on-building (`{type:BUILDING}`), Power (`HAS_POWER`). **TARGET-keyed** (effect lands on the entity): Improvement/Terrain/Plot yields (`food.city.improvements.{IMP}.flat`), `ReligionChanges` (`religion.city.{RELIGION}.flat`), Specialist yields/commerce (`.specialist.specialists.{SPEC}`), UnitCombat/Domain experience+production+strength (`experience.city.unitCombats.{UC}` — the §5 building→unit crossover). |
| `CommerceChangeDoubleTimes` | a 2nd commerce deposit `enabled:{existedFor:{min:N}}` | The age-gated doubling -> a second conditional deposit pairing the base `CommerceChanges` (banked predicate, modifier-spec §10/§4.1). |
| `StateReligionCommerces` / `iStateReligionHappiness` | `<commerce>`/`happiness`.city.flat `enabled:{STATE_RELIGION:<bldg religion>}` / `HAS_STATE_RELIGION` | State-religion-gated. |
| `GlobalReligionCommerce` / `GlobalCorporationCommerce` | `identity.{shrine,corporationHQ}` (the FK) | Both are single enum FKs (NOT per-commerce). The building declares it's the SHRINE/HQ for that religion/corp; the per-commerce VALUES live on the religion (#15) / corporation (#16); the modifier `value x countReligion/CorporationLevels` at world scope ASSEMBLES at #430 (owner: park the FK). |
| `Properties` / `PropertiesAllCities` / `PropertyManipulators` | per-`PROPERTY_*` family (`.city`/`.empire`/v3) | Properties first-class: `PROPERTY_CRIME.city.flat: -10` (Courthouse) etc. `PropertyManipulators` via `engine.property_source_v3`. |
| `PropertySpawnUnit`+`PropertySpawnProperty` / `iNumUnitFullHeal` / `HealUnitCombatTypes` | `grants.repeatable[]` | **The NEW `repeatable` grant + `interval` primitive (modifier-spec §4.1).** PropertySpawn: `{unit, interval:perTurn, chance:{per:{type:PROPERTY_X,scope:city}}}` (Pawn-Shop spawns UNIT_ROBBER, chance ∝ crime; engine owns SorenRand + barb-owner). iNumUnitFullHeal/HealUnitCombat = per-turn heal grants (count, owner-approved generalization). |
| `FreeBuilding`/`FreeAreaBuilding`/`ExtraFreeBonuses`/`FreeTraitTypes`/`FreeSpecialTech`/`iFreeTechs`/`HolyCity`/`iPopulationChange`/`iGlobalPopulationChange`/`bGoldenAge`/`FreeSpecialistCounts`/`FreePromoTypes` | `grants.{buildings,bonuses,traits,techs,holyCity,population,goldenAge,specialists,promotions}` | One-shot provisions/pulses. (FreeBuilding/FreeAreaBuilding are store-wired.) |
| `FoundsCorporation`/`Hurrys`/`bForceTeamVoteEligible` | `enables.{corporations,hurries,votes}` | enables-family authored on the building. `enables.votes:["FORCE_TEAM_ELIGIBLE"]` (NEW token). |
| `ObsoletesToBuilding` | `obsoletes.buildings` | The building's OWN obsolescence edge (read in the curator; not store-wired). |
| `NewCityFree` | **RELOCATED → founder `grants.foundBuildings`** (NOT emitted building-side) | The settler "carries buildings into settling": DONE in the BoolExpr/settler follow-up (see that section below). The 13 NewCityFree buildings + their condition move onto the `bFound` units as `grants.foundBuildings` (each `enabled` by the converted NewCityFree BoolExpr). Nothing emitted on the building. |
| `CommerceFlexibles` | `identity.commerceFlexible: [commerce…]` | Capability: which commerce SLIDERS the building unlocks (`isCommerceFlexible`). |
| `MayDamageAttackingUnitCombatTypes` | `identity.damageAttackingUnitCombats` | Selective counter-damage list (pairs with the `defense` family). |
| `iCost`/`iCostSizeModifier`/…/`iCostComplexityModifier` | `cost.{production,sizeModifier,countModifier,materialsModifier,complexityModifier}` | The C2C 5-part real-building-cost mechanic (intrinsic `cost` section). `GlobalBuildingCostModifiers` -> a distinct **`costs`** family (avoids the `cost` clash). |
| capability bools (`bNukeImmune`/`bQuarantine`/`bProtectedCulture`/`bNoUnhappiness`/`bForceAllTradeRoutes`/…) | `identity.{…}` | Whole-city capability flags (owner: identity, revisit Phase F). `iAsset`->`worth`, `iPower`->`militaryWorth`, `iConquestProb`->`conquestProbability`, `iWorkableRadius`/`iLineOfSight`/`iAirlift`/`iAirUnitCapacity`->identity (overrides/capacities, not modifiers). |
| `AidRateChanges` / `BonusAidModifiers` | — (DROPPED) | An UNWIRED property "aid" mechanic with NO gameplay effect (city arrays allocated+saved but never written-from-building or read-for-effect; only AI building-valuation `/3` + pedia read the raw values). Owner case-by-case DROP. |
| `iNukeExplosionRand` | — (NOT emitted) | Live meltdown CODE (`doMeltdown` every turn) but populated ONLY by the EXCLUDED `Bad_Karma/Building_Meltdown` module -> no included building sets it. (modifier-spec §8(i) corrected.) |
| `iMaxPopulationAllowed` / `iMaxPopulationChange` / `iDCMNukesOkay`/`bDCMNukesOkay` / `bForceOverwrite` | — (DROPPED) | Dead (§8-i, zero consumers) / `bForceOverwrite` = a module-merge loader directive (not gameplay). |

**SpecialBuilding #31** (`curate_special`, rides the Building pass): a per-player-capped building GROUP. `iMaxPlayerInstances`
-> `allowed.empire` (the group cap → the unified `allowed` idiom, owner 2026-06-17; was `identity.maxPlayerInstances`;
`isBuildingGroupMaxedOut`/`getBuildingGroupCount`); `bValid=0` ->
`identity.valid:false`; `TechPrereq`/`TechPrereqAnyone` -> store (`tech.enables.specialBuildings`); `Button` -> `ui.art.icon`.
Buildings join via their `SpecialBuildingType` FK (`identity.specialBuildingType`). Written to `Assets/Data/specialbuildings/`.

## Unit  (`curate_unit.py`)  — Tier E #34, THE LAST MONSTER (2073 records) + SpecialUnit #33 (7 records)

The TARGET of everything, but §5-DOMINATED: the combat/capability/vision/heal surface is the locked shared vocabulary
(Promotion #28 / UnitCombat #29), so the curator REUSES the family NAMES/members (the unit deposits at `unit` scope, the
§5 self-accumulator). CvUnitInfo has NO getDataMembers (legacy read()) -> the inventory is the live XML (219 tags). The
SOURCE->unit enabler edges (tech/building-prereq/bonus/religion/civic + ObsoleteTech) are store-wired -> dropped unit-side.

| old XML | new JSON path | note |
|---|---|---|
| `iCombat`/`iMoves`/`iWorkRate`/`iAirCombat`/`iCargo`/`iCombatLimit`/`iAirUnitCap` + `Combat` | `identity.base.{combat,moves,workRate,airCombat,cargo,combatLimit,airUnitCap,combatClass}` | The create-unit FOUNDATION (§0.6) the subroutine sets. ⚑ The base/deposit boundary is the one genuine judgement (flagged for inspection). |
| the combat TRAITS (`iCityAttack`/`iCityDefense`/`iHillsAttack`/`iWithdrawalProb`/`iFirstStrikes`/`iCollateralDamage*`/`iBombardRate`/`iAirRange`/`iCapture*`/`iInsidiousness`/`iVSBarbs`/`iLunge`/`iEnclose`/…) | `strength`/`withdrawal`/`firstStrike`/`bombard`/`collateral`/`air`/`capture`/`espionage`/`heal`.`unit`.… | §5 unit-scope self-accumulator deposits (summed with promotions). SAME vocab as Promotion (the `*Change` suffix dropped on the unit). |
| vs-keyed (`TerrainAttacks`/`FeatureAttacks`/`UnitCombatMods`/`DomainMods`/`FlankingStrikesbyUnitCombat`/`UnitAttackMods`/`UnitDefenseMods`) | `strength.unit.{terrain\|feature\|unitCombat\|domain\|flanking\|vsUnit}.{TYPE}[.attack\|defense].percent` | The unit's keyed combat mods. `UnitCombatTargets`/`Defenders`/`CollateralImmunes` -> `capabilities.{targets,…}`. |
| `Invisible`/`SeeInvisible`/`VisibilityIntensityTypes`/`InvisibilityIntensityTypes`/`Invisible{Terrain,Feature}Changes` | `vision.{invisible,seeInvisible,visibilityIntensity,…}` | The hide-&-seek LOS resolver (non-cascade, §7). |
| (prereqs) `PrereqTech`/`BonusType`/`PrereqBonuses`/`VicinityBonusType`/`PrereqAnd\|OrBuildings`/`PrereqReligion`/`PrereqCorporation`/`PrereqOrCivics`/`Prereq*Heritage`/`StateReligion`/`iMinAreaSize` | `requires.build` (`all`/`any`/`noneOf`) | Unit `requires.build` ONLY (units are leaf actions — no operate/dormancy yet; future fuel-disable = operate). |
| `iMaxGlobalInstances` / `iMaxPlayerInstances` | `allowed.{world,empire}` | The unit instance CAP → declarative `allowed` (owner 2026-06-17; same idiom as Building/Tech/CultureLevel; was a `requires` `{SELF,max}` atom). `allowed:{empire:1}` = a unique unit (one per player). `bUnlimitedException`→`identity.unlimitedException` is the per-unit "stays capped under `NO_NATIONAL_UNIT_LIMIT`" exception (engine honors it). Engine owns the dynamic parts (option opt-out, base-5 era-scaling, `+extra`); `SELF` left `requires`. enabler-spec §5a/§13.7. |
| `UnitUpgrades`/`SupersedingUnits` | `succession.{upgradesTo,supersededBy}` | MANUAL upgrade chain (NOT `replaces`). |
| `FreePromotions`/`GreatPeoples`/`Builds`/`GroupSpawnUnitCombatTypes`/`ReligionSpreads`/`CorporationSpreads`/`Buildings` | `grants.{promotions,greatPeople,builds,groupSpawn,religionSpreads,corporationSpreads,buildings}` | One-shot provisions. `bGoldenAge` -> `grants.goldenAge`. |
| GP-action magnitudes (`iBaseDiscover`/`iDiscoverMultiplier`/`iBaseHurry`/…/`iGreatWorkCulture`/`iBaseFoodChange`) | `grants.greatPersonAction.{discover,hurry,trade,greatWork,food}.{base,multiplier}` | One-time great-person action magnitudes. |
| `KillOutcomes` / `Actions` | `outcomes.{kill,actions}` | The CvOutcome mission system, carried FAITHFULLY (`engine.generic`) — DEFS = the `CvOutcome` Tier-G straggler; the outcome-system pass (#430) refines (same as UnitCombat #29). |
| `iInstanceCostModifier` | `costs.empire.perInstance` `{percent, per:{type:SELF}}` | Cost rises per existing instance (the priority count-scaled cost case). `iCost`->`cost.production`, `iBaseUpkeep`->`cost.upkeep`. |
| `BonusProductionModifiers` | `buildRate.self.percent[].{value, enabled:{type:BONUS_X,scope:city,min:1}}` | **Build THIS unit faster while a bonus is present** (Worker +5% w/ donkey/camel/cow/horse; `CvCity::getProductionModifier(eUnit)` shrinks the build cost). A **SELF** build-rate, NOT `production` (= total city OUTPUT). Owner 2026-06-16; unified with building/project into the `buildRate` family. **Supersedes** the earlier `production.unit.bonuses.{BONUS}.percent`. See "Production vs buildRate" at the end of this file. |
| `PropertyManipulators` | per-`PROPERTY_*` family (v3) | Crime/disease/etc. unit->city emission. |
| `Capture` | `identity.captures` | The subdue/capture-into unit FK (subdued animals). |
| `Domain`/`DefaultUnitAI`/`UnitAIs`/`NotUnitAIs`/`SubCombatTypes`/`FormationType`/`bMilitary*`/`iXPValue*`/`iAsset`->worth/`iPower`->militaryWorth/… | `identity.{…}` / `capabilities.{…}` | Domain, AI roles, combat classes, military flags, XP/score. ~50 capability bools -> `capabilities`. `DCMAirBomb1-5` -> `capabilities.dcmAirBomb` (tier = count set). Tech-passability/heritage/cargo-kinds/advancedStart -> identity (parked). |

**SpecialUnit #33** (`curate_special_unit`, rides Unit): cargo-load config — `bValid=0`->`identity.valid:false`, `bCityLoad`/
`bSMLoadSame` -> `identity.{cityLoad,smLoadSame}`. Newly registered in `store.ENTITIES`. Written to `Assets/Data/specialunits/`.

✅ **DONE — BoolExpr/settler follow-up (owner 2026-06-16; GitHub #7).** The shared BoolExpr→enabler-condition converter
(`Tools/Migration/boolexpr.py`) + all four consumers landed — see the dedicated **"BoolExpr converter + settler-grants
follow-up"** section at the end of this file.

*Toolkit: `curate_unit.py` (bespoke; REUSES the §5 family vocab; `BASE`/`UNIT_FAMILIES`/`VS_KEYED`/`CAP_BOOL`/`VISION_STRUCTS`/
`GP_ACTIONS` tables + `requires_unit` + `pass2` + `curate_special_unit`) with a COVERAGE CHECK. `SpecialUnitInfo` registered in
`store.ENTITIES`. ⚑ PARKED to pass-2/identity: tech-passability, heritage, cargo-kinds, advanced-start, the BoolExprs above.*

*Toolkit: `curate_building.py` (bespoke, modeled on `curate_promotion`) — `SCALAR_FAMILIES`/`YIELD_FAMILIES`/`COND_KEYED`/
`TARGET_KEYED` tables + `requires_building` + a `pass2` for the keyed/property/grant/repeatable shapes + `curate_special`
for SpecialBuilding; COVERAGE CHECK + era foldering. ⚑ KNOWN: `loadPrune` is 0 (buildings gate at the module level, no
`PrereqGameOption` tag). Issues expected at the #430 reading pass (owner: "a solid start").*

## BoolExpr converter + settler-grants follow-up  (`boolexpr.py`)  — #428 (owner 2026-06-16; GitHub #7)

A focused SHARED pass: a `BoolExpr → enabler-condition` converter (`Tools/Migration/boolexpr.py`) + the four parked
consumers it unblocks. The XML `BoolExpr` machinery (`Sources/BoolExpr.{h,cpp}`: `And`/`Or`/`Not`/`Has[GOMType,ID]`/
`Is[TAG]` + integer-compare) → the LOCKED `requires` vocabulary (enabler-spec §3, `all`/`any`/`noneOf` over atoms +
predicates). **The real, module-included vocabulary is tiny** (verified, `_survey_boolexpr.py`): `NewCityFree` = `GOM_TECH`
(±`Is TAG_COASTAL`); `ConstructCondition` = `And`/`Or` of `Has` over `{BONUS,FEATURE,BUILDING,TECH,TERRAIN}`;
`TrainCondition` = `And`/`Or` of `Has` over `{BONUS,BUILDING}` + ONE `GreaterEqual(ATTRIBUTE_POPULATION,10)` (UNIT_IMMIGRANT).
So a small deterministic converter covers 100% — no hand-recreation. It RAISES on anything outside the map (a future module
addition is caught, never silently mis-converted — owner: "if parsing is too cumbersome we hand-recreate by hand").

| BoolExpr | new JSON | note |
|---|---|---|
| `Has GOM_TECH X` | `{type:TECH_X, scope:team}` | Per-candidate confirm (Tech/Building precedent). |
| `Has GOM_BONUS X` | `{type:BONUS_X, scope:city, connection:"trade\|vicinity"}` | City has the resource (matches `requires_building`/`requires_unit` bonus atoms). |
| `Has GOM_BUILDING X` | `{type:BUILDING_X, scope:city}` | In-city building presence (matches existing in-city building atoms). |
| `Has GOM_FEATURE X` | `{HAS_FEATURE: FEATURE_X}` | **CANONICAL parameterized predicate** (uniform w/ `HAS_BONUS`/`HAS_CORPORATION`). ✅ Improvement #22's `{feature:[…]}` is the membership SUGAR that desugars to `any`-of-`HAS_FEATURE` (owner 2026-06-16, hole #1 RESOLVED — see "Production vs buildRate" siblings + data-model-spec §2.5). |
| `Has GOM_TERRAIN X` | `{HAS_TERRAIN: TERRAIN_X}` | **NEW parameterized predicate.** ⚑ diverges from Improvement #22's `{terrain:[…]}` → Phase-F. |
| `Is TAG_COASTAL` | `IS_COASTAL` | **NEW bare predicate** (`CvCity::isCoastal`). ⚑ diverges from Improvement #22's `COASTAL_LAND` plot token → Phase-F. |
| `And` / `Or` / `Not` | `all` / `any` (one OR-group) / `noneOf` | `Not` unused today; built for completeness. |
| `GreaterEqual(ATTRIBUTE_POPULATION, N)` | `{type:POPULATION, scope:city, min:N}` | Established count kind (Building #32); the lone UNIT_IMMIGRANT case. |

**The four consumers (retrofit; Building + Unit JSON regenerated):**
| field (entity) | old | new | note |
|---|---|---|---|
| `ConstructCondition` (Building) | parked/coverage-only | folded into `requires.build` (`merge_into`) | Checked ONLY at `canConstruct` (CvCity.cpp:2976-2999), never `isActiveBuilding` → **build (greying), NOT operate** (losing a ConstructCondition bonus post-build does nothing). 97 buildings. |
| `TrainCondition` (Unit) | parked/coverage-only | folded into `requires.build` | Checked at `canTrain` (CvCity.cpp:1961-1963). 238 units. |
| `NewCityFree` (Building) | `identity.newCityFree` (parked) | **relocated** → founder `grants.foundBuildings` | See below; removed building-side. |
| `bCapital` (Building = Palace) | `identity.capital` (kept) | **+** founder `grants.foundBuildings` entry, `enabled:{type:CITY,scope:empire,max:0}` | The capital-defining flag STAYS on the building (`isCapital`); the first-city auto-Palace becomes a found-grant gated on "no cities yet" (owner). |

**`grants.foundBuildings` (NEW found-time grant key, on the 8 `bFound` units).** Every founder
(SETTLER/COLONIST/PIONEER/BAND/TRIBE/MARSSETTLER/VENUS_SETTLER/AIRSETTLER) seeds its new city with the **identical** list
(no settler-type difference yet — owner-verified): the 13 NewCityFree buildings (each `{building, enabled:<converted
NewCityFree BoolExpr>}`) + `{building:BUILDING_PALACE, enabled:{type:CITY,scope:empire,max:0}}`. Computed once from the
merged BuildingInfo table (`curate_unit.found_buildings`). Realizes the owner invariant: **a tech-gated building not
available at settle time is not granted → not pre-built.** Distinct from the unit's `grants.buildings` (always-grants) —
`foundBuildings` is the found-time seed (matches GitHub #7's "FoundBuildings list on CvUnitInfo"). The space-settler
variants that are NOT `bFound` (lunar/cislunar/oneill/planetary) correctly receive no `foundBuildings` (they found via a
different path).

*Toolkit: `boolexpr.py` (shared, imported by `curate_building` + `curate_unit`); `curate_building.requires_building`
merges `ConstructCondition`; `curate_unit.requires_unit` merges `TrainCondition`; `curate_unit.found_buildings(store)`
builds the list. `_survey_boolexpr.py` was a throwaway vocabulary survey (deleted). ✅ Membership reconcile RESOLVED
2026-06-16 (owner, hole #1): `HAS_TERRAIN`/`HAS_FEATURE`/`HAS_BONUS` canonical; `{terrain\|feature\|bonus:[…]}` is sugar
(any-of-`HAS_X`); `COASTAL_LAND` unused (0)→moot; `IS_COASTAL` (city) stays distinct. Cleared 229 flags; data unchanged.*

## Production vs buildRate — the production-modifier cluster (owner ruling 2026-06-16; cross-entity)

**Two distinct concepts the first-pass migration flattened into `production.city` — the "Versailles bug" (DESPAIR_INDEX
#12 sibling):** a production-modifier may scale the **whole city's hammer output** OR speed the **construction of a
specific target**. Verified in the C++ (the two are applied in different places):
- **`production` = TOTAL CITY OUTPUT.** `CvCity::getYieldRate100(PRODUCTION) = (base + specialistYield) ×
  getBaseYieldRateModifier(PRODUCTION) + 100·extraYield`. Authored as `production.city.flat` (a hammer ADD —
  `YieldChanges[PRODUCTION]`) + `production.city.percent` (the city-wide multiplier on *everything* —
  `YieldModifiers[PRODUCTION]` = Factory, Power/Area/Capital/Bonus yield-rate). Scales every build, every turn.
- **`buildRate` = FASTER TO BUILD A TARGET/CATEGORY.** `CvCity::getProductionModifier(eItem)` shrinks the COST of the
  *specific* item under construction (CvCity.cpp:3611-3621/3857-3946); never touches the per-turn yield number. The
  `buildRate` family **already existed** (modifier-spec §1.1; unit/domain/unitCombat production mods were already here).

**Owner ruling: `production.city` = total output ONLY; everything "faster to build a target/category" → `buildRate`,
keyed/scoped by what is produced.** Full corrected cluster (all entities):

| old XML field (on Building/Unit/Civic/Project/Trait) | new home | consumer / note |
|---|---|---|
| `BonusProductionModifiers` (build THIS faster w/ a bonus — Versailles+marble, Worker+donkey) | `buildRate.self.percent[].{value, enabled:{type:BONUS_X,scope:city,min:1}}` | `getBonusProductionModifier`; the bonus is a CONDITION (city has it), the target is SELF. **Was the bug:** emitted `production.city.percent enabled:{bonus}`. Building/Unit/Project all identical. |
| `BuildingProductionModifiers` / `GlobalBuildingProductionModifiers` (speed a TARGET building) | `buildRate.{city,empire}.buildings.{BUILDING}.percent` | `getBuildingProductionModifier(eBuilding)` — the keyed building is the TARGET being sped up. **Was the bug:** the target was misread as an `enabled` condition at `production.city`. |
| `UnitProductionModifiers` / `UnitCombatProductionModifiers` / `DomainProductionModifiers` | `buildRate.{scope}.{units\|unitCombats\|domains}.{TARGET}.percent` | already `buildRate` (not corrupt) — kept. |
| `iMilitaryProductionModifier` | `buildRate.{scope}.military.percent` | was a one-off `militaryProduction` family → folded into `buildRate` (category member). |
| `iSpaceProductionModifier` / `iGlobalSpaceProductionModifier` | `buildRate.{city,empire}.space.percent` | was a one-off `spaceProduction` family → folded into `buildRate`. |
| Trait `iMax{Global,Team,Player}BuildingProductionModifier` (wonder-class build-rate) | `buildRate.empire.{worldWonder,teamWonder,nationalWonder}.percent` | wonder-category build-rate; was `production`. |
| `YieldModifiers[PRODUCTION]` (Factory), `YieldChanges[PRODUCTION]` (a building's flat hammers) | `production.city.{percent,flat}` | the ONLY legitimate `production.city` — total city output. Unchanged. |

**LEFT AS-IS (verdict, NOT corrupt):** the `stateReligion`-grouped `iStateReligion{Unit,Building}ProductionModifier`
(Civic + Trait) stay members of the `stateReligion` family (`stateReligion.empire.{unitProduction,buildingProduction}`)
— they are state-religion-gated build-rate correctly bundled with the family's other members (happiness/GP/experience),
the gate carried by the family name; moving them to `buildRate` would break that grouping and need an explicit
`HAS_STATE_RELIGION` gate. Flagged for Phase-F only if member-name tidying is wanted.

**Curators updated (2026-06-16; regen owed):** `curate_building.py` (SCALAR military/space; COND_KEYED Bonus→`buildRate.self`;
Building/GlobalBuilding moved COND_KEYED→TARGET_KEYED), `curate_unit.py` (Bonus→`buildRate.self`), `curate_civic.py`
(military + Building/Unit/UnitCombat), `curate_project.py` (Bonus **restored** from a stale DROP → `buildRate.self`;
curate_bonus's `BONUS_BOOSTS` fold it referenced was already removed, so it was being silently lost),
`curate_trait.py` (military/wonder + the keyed building/unit/domain/specialBuilding/specialUnit/unitCombat mods).
Harness `Tools/ReadJson/readjson.cpp` gained the `self` scope. ⚑ **RERUN owed** (shell down at capture time): re-run
the five curators + `--write`, owner-inspect the regenerated JSON, then commit — so live data is only rewritten correctly.
Tests: `building_torture_wonder.json` (BUILDING_TEMPLE_OF_GNARL) + `unit_sir_gnarlalot.json` demonstrate the split.

## Property sources → v3 (property IS a yield-like family; owner ruling 2026-06-16, hole #5)

**Property behaves "like any other yield" — same `flat`/`percent`/`per`/`enabled` deposit shape; it just has more varied
GENERATION methods (owner 2026-06-16).** All map onto the v3 vocabulary via `engine.property_source_v3`:
- `PROPERTYSOURCE_CONSTANT` (`iAmountPerTurn`) → **`flat`**.
- `PROPERTYSOURCE_DECAY` → currently **`percent`** (`iPercent`; all decay sources today), though decay's magnitude can
  equally be flat/other — the unit is just read like any modifier. **The JSON does NOT know or care about the equilibrium
  (owner 2026-06-16): it emits plain modifiers onto the property; the ENGINE owns the toward-`targetLevel` handling**
  (§0.6) — exactly as it should. `targetLevel` is property-specific in SIGN (positive for `PROPERTY_EDUCATION` — want it
  high; negative for `PROPERTY_CRIME`/`PROPERTY_DISEASE` — want them low) and is the isolated equilibrium field (renames
  §Property), an engine concern, never the deposit's.
- attribute-scaled (`AttributeType` / `Mult(attr,C)` / `Div(Mult(attr,C),D)` / `Div(attr,D)`) → **`per`:{type, each, scope}**,
  **REDUCED to a sensible fraction** (owner 2026-06-16): positive `each`, sign on `value`, lowest terms (same rational →
  identical integer result). The XML's integer-only `Mult(attr,1)`/`Div(…,-N)` contortions — an invented math-language
  that was literally a **first draft of this very JSON modifier system**, faking fractions an integer XML couldn't express
  — collapse to "value per `each` of type": e.g. handicap crime `(POP×5)/2` → `{value:5, per:{type:POPULATION, each:2}}`;
  `−POP/3` → `{value:-1, per:{…, each:3}}`. No negative `each` survives. **We deliberately do NOT resurrect the
  formula-language (owner 2026-06-16):** the only legitimate multiplication is "scale a value by a COUNT" (× popcount),
  which is exactly `per:{type, each}` (`value × count / each`) — one bounded linear operation. Arbitrary `Mult`/`Div`
  nesting could compound to runaway values fast; the bounded `per` is the sane sufficient form (same principle as the
  `&`/`|` deferral — keep the data vocabulary bounded).
- an `<Active>` BoolExpr gate → **`enabled`** (via `boolexpr.convert_field` — e.g. heritage `active:[GOM_TECH]` →
  `flat:[{value:1, enabled:{type:TECH_EDUCATION, scope:team}}, …]`, multiple sources = a LIST of entries).
- scope from `GameObjectType`, **default `city`** when absent (property effects are city-level).

**This RETIRED a rogue-agent shape (owner 2026-06-16).** `curate_handicap` + `curate_heritage` had used the old raw
`engine.clean_property_source` (`perTurn`/`mult`/`div`/`attribute`/`active` formula trees + a hardcoded `empire` scope) — a
non-sanctioned invention. Both now route through `property_source_v3` (the documented standard, like civic). Cleared 68
conformance flags. `clean_property_source` survives ONLY for **feature/improvement** (their `RELATION_NEAR` spatial
sources that v3 rejects → #429-parked raw) + `engine.generic` (the `outcomes` trees, deferred to the #430 outcome
pass). Every other property-bearing entity (handicap/heritage/civic/trait/building/unit/specialist) is now v3 —
property is uniformly first-class.

**Heritage scope CORRECTED `empire` → `city` (owner-confirmed):** education is a city property; v3 reads
`GameObjectType`-or-default-`city` (the old `empire` was the rogue hardcode). **The empire-wide feel is emulated by the
autobuild placing a per-city copy** (FreeBuilding / effect-building), NOT an empire deposit. ⚑ FUTURE: a number of these
per-city-autobuilt modifiers will move UP to **empire/team scope** in the **#421 team-buildings** cleanup (retiring the
autobuild fan-out) — so city-scope-now is interim-correct, not the end state.

**NET WIN — properties are now FIRST-CLASS (the #428 properties-first-class goal, achieved):** a property is just another
yield, like commerce/culture/food. Invent a brand-new `PROPERTY_X` and it works with **ZERO per-property data plumbing** —
deposit modifiers onto `PROPERTY_X.<scope>.<unit>` exactly like any yield, give it a `targetLevel`, and the engine's
equilibrium machinery (§0.6) does the rest. The varied generation methods (constant/decay/attribute-scaled/active-gated)
all collapse onto the one shared v3 vocabulary; nothing about a property is special at the data layer.

**ESSENCE (owner 2026-06-16): a property is simply a YIELD THE EXE DOES NOT HANDLE.** Where food/production/commerce +
the four commerces are **EXE-bound** (a fixed, immutable enum the reader maps name↔index for the closed `.exe`'s ABI
arrays — cascade-engine-430.md §3), properties carry no such binding: they are **DLL-owned, unlimited, and in whatever
form we want**. Same cascade vocabulary; the *only* difference is the EXE binds the former and not the latter — which is
precisely the freedom that lets us make properties first-class and invent more at will, with no plumbing.
