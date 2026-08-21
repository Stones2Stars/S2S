export const meta = {
  name: 'classify-building',
  description: 'Adversarially classify every CvBuildingInfo field for the #428 JSON migration (Building #32 + SpecialBuilding #31). NO GUESSING: a home is only assigned when verified from a live consumer; anything ambiguous -> needsRuling=NEEDS_OWNER_RULING (the owner rules it, never a guessed home).',
  phases: [
    { title: 'Understand', detail: 'one agent per field-slice traces every field to its live C++/Python consumer + assigns a VERIFIED home, or needsRuling if not determinable' },
    { title: 'Verify', detail: 'an adversary tries to REFUTE every disposition (dead-claims, scope, ownership, missed consumers) AND demotes any GUESSED home to needsRuling' },
  ],
}

// ── Shared model context embedded in every agent prompt ───────────────────────
const MODEL = `
You are classifying fields of CvBuildingInfo for the S2S #428 XML->JSON cascade migration.
Building is the deepest, LEAST-TRUSTWORTHY entity in the codebase — owner: "I trust nothing in
the building data, it is by far the biggest target of creative use of code mechanics." Treat EVERY
field as guilty-until-audited. The first-pass mapping (Tools/Migration/mapping/BuildingInfo.json) is
a FILTER, NOT gospel — its scopes/channels are frequently WRONG; do not trust them.

READ THE LOCKED SPECS for the rules (do not reconstruct from memory):
- Sources/docs/plans/modifier-cascade-spec.md (v3) — families/scopes/units; §0.8 dedicated blocks;
  §3 enabled/disabled; §4 per-count; §5 unit-plane; §6.1 DELIVERYGUY ownership; §8 four-way drop.
- Sources/docs/plans/enabler-cascade-spec.md (v0.3) — enables family (PERMANENT: enables/obsoletes/
  replaces/disables) vs requires (REVERSIBLE means: build vs operate); §3 predicates; §7 tally counts.
- Sources/docs/plans/migration-renames.md — the canonical old->new registry + decisions already made
  for sibling entities (Tech/Civic/Religion/Corporation/Bonus/Terrain/Improvement/Promotion/UnitCombat).
- Sources/docs/plans/migration-entity-ranking.md §32 — the Building-specific rulings.

KEY RULES (apply, but verify against the specs):
- THREE GOVERNING RULINGS: (1) author the datum for WHAT IT IS, not how today's C++ combines it
  (a percent is 'percent' even if applied multiplicatively); (2) the C++ is reworked to fit the JSON,
  NEVER the reverse — read the consumer only to learn what the datum MEANS + find every consumer;
  (3) the JSON must read COLD to a modder with zero codebase knowledge.
- Top-level keys are reserved sections (enables/obsoletes/replaces/disables/requires/grants/text/cost/
  ui/world/sound/identity/ai) OR a MODIFIER FAMILY (food/production/commerce/gold/research/culture/
  espionage/happiness/health/maintenance/upkeep/defense/...). yield SPLITS to food/production/commerce;
  commerce SPLITS to gold/research/culture/espionage; property SPLITS one family per PROPERTY_*.
- Modifier entry: <family>.<scope>.<member>.<unit> ; scope in world|team|empire|area|city|plot|building|
  specialist|unit ; unit in flat|percent|multiplier ; optional enabled/disabled (a predicate object,
  bare string for parameter-free e.g. HAS_RIVER/IS_CAPITAL) + per:{type,each,scope} count-scaler.
- §6.1 DELIVERYGUY (CRITICAL for Building's 22 'inversions'): a building-keyed-by-X modifier — does the
  EFFECT land on X (then keep-on-building, keyed by X target) or is X merely a CONDITIONER you possess
  (then condition via enabled/per)? A building boosting a terrain's/improvement's tiles, or another
  building, is the DELIVERYGUY -> the modifier STAYS ON THE BUILDING keyed by the target. The old
  mapping 'inversionsOut' onto Tech/Bonus/Improvement/Terrain is the STALE pre-v3 rule — re-decide each:
  tech-conditioned effects are a downward deposit FROM the tech (kept-on-tech, §0.4, PROVISIONAL pending
  Phase-F); a resource is NEVER a target so bonus-conditioned effects keep-on-building gated by the bonus.
- enables vs requires: PERMANENT unlocks the building makes (FreeBuilding autobuild, FoundsCorporation,
  enables.units/hurries, ObsoletesToBuilding, ReplacementBuildings, ExtendsBuilding) -> enables-family,
  forward on the building. REVERSIBLE MEANS it needs to build/operate (PrereqBonus/Vicinity, PrereqReligion/
  Corporation/CultureLevel/Civic, bWater/bRiver/bPower, StateReligion, population/area/latitude gates,
  count-thresholds, ConstructCondition) -> requires (build vs operate; greying vs dormancy). NOTE: most
  source->building enabler edges are ALREADY store-wired (store.py PREREQ_FIELDS) onto the SOURCE — so on
  the BUILDING side they are DROPPED (the building only AUTHORS its own requires-means + its forward enables).
- grants: one-shot pulses (population/goldenAge/founding bursts) + entity provisions, fired on an event.
- §8 four-way drop: (i) truly dead -> drop; (ii) unwired-but-intended modifier -> revive; (iii) unwired
  world-state feature -> separate issue; (iv) deliberate balance cut. Re-check 'dead' against Assets/Python
  AND intent, not just C++. Do NOT pre-assume a 'revive home' for an unwired field -> if its intended home is
  not certain, needsRuling. (Drop a field ONLY when grep PROVES zero consumers in C++ AND Python.)
- ⛔ NO GUESSING — ASK (owner HARD RULE): NEVER invent, best-fit, or "SHOEHORN?" a home. A home is assigned ONLY
  when determined with HIGH confidence from a VERIFIED live consumer (the real getter grep'd to its read site). If the
  correct home is NOT determinable that way — ambiguous, cross-domain, a creative/non-obvious mechanic, or a field that
  resists the v3/v0.3 structure — set needsRuling=true, home="NEEDS_OWNER_RULING", and state in notes what is ambiguous
  + the verified facts (real getter + consumer). Do NOT contort the structure to swallow a field; do NOT ship a guess.
  The owner rules every needsRuling field. A guessed home is the EXACT failure (30 hours of guessed XML mappings, then
  weeks of agents inheriting the guesses) this workflow exists to PREVENT.
- CvBuildingInfo has ZERO DllExport (verified) -> data shape is UNCONSTRAINED (no EXE ABI on building getters).
- SpecialBuilding (#31) RIDES this pass: a per-player-capped building GROUP (iMaxPlayerInstances ->
  isBuildingGroupMaxedOut/getBuildingGroupCount); shares building vocab.

FIELD INVENTORY lives in: Sources/CvBuildingInfo.cpp getDataMembers() (lines 1666-1923) + the
hand-written read() remainder (the comment at 1633-1665 lists every hand-read field: SetVariableListTagPair
arrays, 2D int** tables, delayed-resolution vectors, struct-vectors, CvProperties sub-objects, BoolExprs).
Header: Sources/CvBuildingInfo.h. Consumers: grep the getter (getXxx) across Sources/*.cpp AND Assets/Python.

OUTPUT DISCIPLINE: every 'notes' <= 2 tight sentences; cite file:line, NEVER dump code; if a
StructuredOutput call is rejected for size, SHORTEN and resubmit — NEVER probe the byte ceiling.`

const FIELD_SCHEMA = {
  type: 'object',
  additionalProperties: false,
  required: ['slice', 'fields'],
  properties: {
    slice: { type: 'string' },
    fields: {
      type: 'array',
      items: {
        type: 'object',
        additionalProperties: false,
        required: ['name', 'home', 'disposition', 'consumers', 'needsRuling', 'confidence', 'notes'],
        properties: {
          name: { type: 'string', description: 'the XML tag / member name' },
          home: { type: 'string', description: 'the VERIFIED JSON destination path (e.g. "happiness.city.flat" / "requires.build.all[bonus]" / "enables.units" / "grants" / "identity.X" / "DROP"), OR the literal "NEEDS_OWNER_RULING" when needsRuling=true. NEVER a guess/best-fit/"SHOEHORN?" string.' },
          disposition: { type: 'string', enum: ['modifier', 'requires', 'enables-family', 'grants', 'identity', 'cost', 'property', 'art', 'drop', 'needsRuling'] },
          scope: { type: 'string', description: 'world|team|empire|area|city|plot|building|specialist|unit, or "" if N/A' },
          unit: { type: 'string', description: 'flat|percent|multiplier|enabler|boolean|"" ' },
          consumers: { type: 'string', description: 'live C++ file:line getter usages + any Assets/Python consumer; or "NONE FOUND (grep evidence)"' },
          needsRuling: { type: 'boolean', description: 'TRUE when the correct home is NOT determinable with HIGH confidence from a verified live consumer (ambiguous / cross-domain / creative-mechanic / resists the v3 structure). When true, home MUST be "NEEDS_OWNER_RULING" and notes state what is ambiguous + the verified facts. NEVER invent/guess/force-fit a home -- the owner rules these. This replaces the old creativeMechanic/shoehornRisk guess flags (a guessed home is the exact failure this workflow exists to PREVENT).' },
          dropCategory: { type: 'string', description: 'if drop: i(dead)/ii(unwired-revive)/iii(world-state-issue)/iv(balance-cut); else ""' },
          confidence: { type: 'string', enum: ['high', 'med', 'low'] },
          notes: { type: 'string', description: '<=2 sentences: WHAT it means + why this home; flag spec-conflicts' },
        },
      },
    },
  },
}

// ── The 7 field-slices ────────────────────────────────────────────────────────
const SLICES = [
  {
    key: 'yield-commerce',
    label: 'Yield/Commerce modifier families',
    fields: `YieldChanges, YieldModifiers, YieldPerPopChanges, RiverPlotYieldChanges, PowerYieldModifiers,
AreaYieldModifiers, GlobalYieldModifiers, GlobalSeaPlotYieldChanges, PlotYieldChanges;
CommerceChanges, CommerceModifiers, CommercePerPopChanges, CommerceChangeDoubleTimes, GlobalCommerceModifiers,
StateReligionCommerces, CommerceHappinesses, CommerceFlexibles, GlobalReligionCommerce (shrine);
SpecialistYieldChanges, SpecialistCommerceChanges, LocalSpecialistCommerceChanges, SpecialistExtraCommerces.`,
    focus: `SPLIT yield->food/production/commerce and commerce->gold/research/culture/espionage. Decide scope
(city vs area vs player->empire vs plot). CommerceChangeDoubleTimes is an AGE-GATE -> a 2nd enabled deposit
(existedFor), NOT a magnitude. StateReligionCommerces -> enabled:{STATE_RELIGION} (see Religion renames).
GlobalReligionCommerce -> the shrine world-scaling case (Religion #15 parked it to "the Building pass" — resolve
it). PowerYieldModifiers -> enabled by the power predicate. RiverPlotYieldChanges -> enabled:HAS_RIVER, deliveryguy
= building. Specialist* deposits: are they on the SPECIALIST (already owned, double-author?) or building-keyed?`,
  },
  {
    key: 'scalar-modifiers',
    label: 'City/area/player scalar + percent modifiers',
    fields: `iHealth, iAreaHealth, iGlobalHealth, iHappiness, iAreaHappiness, iGlobalHappiness, iStateReligionHappiness,
iHealRateChange, iFoodKept, iGreatPeopleRateChange/Modifier, iGreatGeneralRateModifier, iDomesticGreatGeneralRateModifier,
iGlobalGreatPeopleRateModifier, iGlobalGreatGeneral..., iMaintenanceModifier, iGlobalMaintenanceModifier,
iAreaMaintenanceModifier, iOtherAreaMaintenanceModifier, iDistanceMaintenanceModifier, iNumCitiesMaintenanceModifier,
iCoastalDistanceMaintenanceModifier, iConnectedCityMaintenanceModifier, iInflationModifier, iWarWearinessModifier,
iGlobalWarWearinessModifier, iEnemyWarWearinessModifier, iHurryCostModifier, iGlobalHurryModifier, iHurryAngerModifier,
iMilitaryProductionModifier, iSpaceProductionModifier, iGlobalSpaceProductionModifier, iWorkerSpeedModifier,
iTradeRoutes, iCoastalTradeRoutes, iGlobalTradeRoutes, iWorldTradeRoutes, iTradeRouteModifier, iForeignTradeRouteModifier,
iFreeExperience, iGlobalFreeExperience, iFreeSpecialist, iAreaFreeSpecialist, iGlobalFreeSpecialist, iAnarchyModifier,
iGoldenAgeModifier, iOccupationTimeModifier, iPopulationgrowthratepercentage, iGlobalPopulationgrowthratepercentage,
iHealthPercentPerPopulation, iHappinessPercentPerPopulation, iRevIdxLocal/National/DistanceModifier, iPillageGoldModifier,
iInsidiousness, iInvestigation, iNational/LocalCaptureProbability/ResistanceModifier, iUnitUpgradePriceModifier.`,
    focus: `The bulk of the ~101 channels. Get SCOPE right (the mapping's player/city is often wrong; Area* = area
scope; Global*/National* = empire). iStateReligionHappiness -> enabled:{STATE_RELIGION}. iPillageGoldModifier is
DEAD (getPillageGoldModifier has no C++/Python consumer) -> drop; the live pillage-gold modifier is the PROMOTION
PillageChange, not this building field. capture modifiers -> the capture family. maintenance members -> grouped
maintenance family (cost-style). Any field whose correct home is not certain -> needsRuling (never guess).`,
  },
  {
    key: 'defense-combat-military',
    label: 'Defense / combat / military / unit-facing modifiers',
    fields: `iDefenseModifier, iBombardDefense, iAllCityDefense, iEspionageDefense, iNukeModifier, iAirModifier,
iAirlift, iAirUnitCapacity, iMinDefense, iNoEntryDefenseLevel, iLocalDynamicDefense, iRiverDefensePenalty,
iBuildingDefenseRecoverySpeedModifier, iCityDefenseRecoverySpeedModifier, iDamageAttackerChance, iDamageToAttacker,
bDamageAllAttackers, iAdjacentDamagePercent, iLocalRepel, iLineOfSight, iNumUnitFullHeal, iWorkableRadius,
UnitCombatFreeExperiences, DomainFreeExperiences, UnitCombatExtraStrengths, UnitProductionModifiers,
UnitCombatProdModifiers, DomainProductionModifiers, UnitCombatDefenseAgainstModifiers, MayDamageAttackingUnitCombatTypes,
HealUnitCombatTypes, BonusDefenseChanges, BonusAidModifiers, AidRateChanges, FreePromoTypes, bApplyFreePromotionOnMove.`,
    focus: `iMinDefense -> the defense family 'min' floor member (clamp on channel total, §7), NOT a separate channel.
iDefenseModifier -> defense.city.amount.percent. UnitCombatExtraStrengths is a building granting UNIT combat strength
-> the §5 unit-plane strength family (byOccupant/garrison cross-edge?) — flag the crossover. Unit*ProductionModifiers
= unit-build %, keyed by unit/unitcombat/domain (keep-on-building keyed by target). FreePromoTypes/HealUnitCombat =
grants of unit capability. iWorkableRadius is a REPLACE/override (setWorkableRadiusOverride) -> NOT additive (identity).
bDamageAllAttackers derives m_bDamageAttackerCapable (recompute-on-load).`,
  },
  {
    key: 'requires-availability',
    label: 'Availability: requires (means) + count-thresholds + load-prune',
    fields: `PrereqTech, ObsoleteTech, TechTypes, Bonus(PrereqAndBonus), PrereqBonuses(Or), VicinityBonus, RawVicinityBonus,
PrereqVicinityBonuses, PrereqRawVicinityBonuses, PrereqReligion, PrereqCorporation, PrereqCultureLevel, PrereqCivic,
PrereqAndCivics, PrereqOrCivics, bRequiresActiveCivics, StateReligion, bNeedStateReligionInCity, bNoHolyCity, HolyCity,
ReligionType, bWater, bRiver, bFreshWater, bProvidesFreshWater, bPower, bPrereqPower, PowerBonus, iMinAreaSize,
iMinLatitude, iMaxLatitude, iPrereqPopulation, bPrereqWar, ConstructCondition(BoolExpr), NewCityFree(BoolExpr),
PrereqInCityBuildings, PrereqNotInCityBuildings, PrereqOrBuildings, PrereqAmountBuildings, PrereqAnyoneBuilding,
PrereqOrTerrain, PrereqAndTerrain, PrereqOrFeature, PrereqOrImprovement, PrereqOrHeritage, VictoryPrereq,
iCitiesPrereq, iTeamsPrereq, iLevelPrereq, iMaxGlobalInstances, iMaxTeamInstances, iMaxPlayerInstances,
iExtraPlayerInstances, bNoLimit, bForceNoPrereqScaling, MapCategoryTypes, EnabledCivilizationTypes,
PrereqGameOption, NotGameOption, FreeStartEra, MaxStartEra, CvProperties Prereq{Min,Max}Properties + PrereqPlayer{Min,Max}.`,
    focus: `Most SOURCE->building edges are ALREADY store-wired (PrereqTech/Bonus/Religion/Corp/CultureLevel/Civic/
ObsoleteTech/ReplacementBuildings) onto the SOURCE -> on the BUILDING side they DROP. What STAYS = the building's OWN
requires-means: build vs operate split (PrereqBonus=build greying; PrereqVicinity=city-scope; civic/religion=operate
dormancy). Count-thresholds (iCitiesPrereq/iTeamsPrereq/iLevelPrereq/PrereqAmountBuildings) -> requires min(...) reading
TALLY. instance CAPS (iMaxPlayer/Team/GlobalInstances) -> requires max(SELF,N) / world-uniqueness. ConstructCondition/
NewCityFree = OR/NOT BoolExpr -> requires composition. bWater/bRiver/bFreshWater -> bare plot predicates. GameOption ->
loadPrune. Confirm which prereqs are store-wired (read store.py) vs building-authored.`,
  },
  {
    key: 'enables-grants-pulses',
    label: 'Forward enables-family + grants + one-shot pulses',
    fields: `FreeBuilding, FreeAreaBuilding, bAutoBuild, ObsoletesToBuilding, ReplacementBuildings, ExtendsBuilding,
ProductionContinueBuilding, FoundsCorporation, ExtraFreeBonuses, GreatPeopleUnitType, FreeTraitTypes, iFreeTechs,
FreeSpecialTech, NewCityFree(grant side), Hurrys(isHurry), bGoldenAge, bGovernmentCenter, bCapital, bAllowsNukes,
bDCMNukesOkay, PropertySpawnUnit, PropertySpawnProperty, iGlobalPopulationChange, iPopulationChange, iMaxPopAllowed
(iObsoletePopulation), iMaxPopulationAllowed, iMaxPopulationChange, iNumPopulationEmployed, bForceTeamVoteEligible,
DiploVoteType, VictoryThresholds, FreeSpecialistCounts, SpecialistCounts.`,
    focus: `enables-family (PERMANENT, forward): FreeBuilding/FreeAreaBuilding (autobuild -> enables + grant-on-enable),
ObsoletesToBuilding -> obsoletes.buildings, ReplacementBuildings -> replaces (already store-wired — confirm), Extends/
ProductionContinue/PrereqAnyoneBuilding = building-succession/continuation chains (creative mechanics — classify),
FoundsCorporation -> enables corp HQ, Hurrys/isHurry -> enables.hurries (slavery), ExtraFreeBonuses -> grants.bonuses
(culture chain). grants/pulses: population & goldenAge bursts -> grants; iMaxPopAllowed -> requires.operate dormancy
(§8). FreeSpecialistCounts vs SpecialistCounts (slots vs free) — distinguish (one is specialist SLOTS = a modifier,
one is FREE specialists). PropertySpawnUnit/Property = creative spawn mechanic — flag. VictoryThresholds/DiploVoteType
-> the UN/victory wiring (Victory #5 deferred it here).`,
  },
  {
    key: 'cost-properties-buildingkeyed',
    label: 'Cost + CvProperties + building-on-building modifiers + the 22 inversions',
    fields: `iCost, iCostSizeModifier, iCostCountModifier, iCostMaterialsModifier, iCostComplexityModifier;
CvProperties: Properties, PropertiesAllCities, PropertyManipulators/PropertySource; BuildingHappinessChanges,
BuildingProductionModifiers, GlobalBuildingProductionModifiers, GlobalBuildingCostModifiers, GlobalBuildingExtraCommerces;
the INVERSIONS: TechYieldChanges, TechYieldModifiers, TechCommerceChanges, TechCommerceModifiers, TechHappinessChanges,
TechHealthChanges, TechSpecialistChanges, BonusHealthChanges, BonusHappinessChanges, BonusYieldChanges, BonusYieldModifiers,
BonusCommercePercentChanges, VicinityBonusYieldChanges, BonusProductionModifiers, ImprovementYieldChanges,
GlobalImprovementYieldChanges, TerrainYieldChanges, ReligionChanges, RawVicinityBonus aid; PlotYieldChanges.`,
    focus: `THE §6.1 DELIVERYGUY CRUX. The mapping 'inversionsOut' onto Tech/Bonus/Improvement/Terrain/Religion is the
STALE pre-v3 rule — re-decide EACH: Improvement/Terrain YieldChanges = building is the DELIVERYGUY -> keep-on-building
keyed by the improvement/terrain (NOT inverted, per renames Terrain note); Bonus*Changes = resource is NEVER a target ->
keep-on-building gated by bonus (enabled/per); Tech*Changes = downward deposit kept-on-TECH (§0.4, PROVISIONAL Phase-F).
ReligionChanges -> religionSpread (keep-on-building? or religion?). Building-keyed (BuildingHappiness/Production/Cost/
ExtraCommerce, GlobalBuilding*) = building-on-building deliveryguy -> keep-on-building keyed by the other building.
iCost size/count/materials/complexity = the C2C real-building-cost mechanic -> cost section (cost-vs-costs convention).
CvProperties: Properties/AllCities -> per-PROPERTY_* family deposits (city/empire scope, §5 property_source_v3);
Prereq{Min,Max}Properties -> requires negative/positive bands (pseudobuilding dormancy). Flag every creative mechanic.`,
  },
  {
    key: 'identity-art-special-ai',
    label: 'Identity / capability flags / art / AI / SpecialBuilding #31',
    fields: `Type, Description, Civilopedia, Help, ArtDefineTag, Button, MovieDefineTag, ConstructSound, Advisor,
fVisibilityPriority, iAsset, iPower, iAIWeight, iConquestProb, Flavors, bNukeImmune, bNeverCapture, bZoneOfControl,
bProtectedCulture, bBorderObstacle(bAreaBorderObstacle), bNoUnhappiness, bNoUnhealthyPopulation, bBuildingOnlyHealthy,
bForceAllTradeRoutes, bNoEnemyPillagingIncome, bQuarantine, bMapCentering, bCenterInCity, bTeamShare, bOrbital,
bOrbitalInfrastructure, iDCMAirbombMission, iDCMNukesOkay, iNukeExplosionRand, SpecialBuildingType, PromotionLineType,
iLinePriority, UnitCombatRetrainTypes, HealUnitCombatTypes(if identity), Categories, m_iMissionType, bForceNoPrereqScaling;
SpecialBuilding entity: Type, Description, bValid, iMaxPlayerInstances, ObsoleteTech, TechPrereq, Button.`,
    focus: `Capability ENABLER bools (bNoUnhappiness/bNoUnhealthy/bBuildingOnlyHealthy/bForceAllTradeRoutes/bNukeImmune/
bZoneOfControl/bProtectedCulture/bBorderObstacle/bQuarantine/bNoEnemyPillagingIncome) -> these are whole-city capability
flags: city-scope enabler deposits OR identity — decide per the §3 'pure capability' rule. iAsset->worth, iPower->
militaryWorth (identity score). iConquestProb->conquestProbability (a committed modifier per old flags §). Flavors+
iAIWeight->ai group. Art->ui/world/sound blocks (ART_BLOCK; ArtDefineTag->world.art.icon, Button->ui.art.icon,
MovieDefineTag->ui.art.movie, ConstructSound->sound.construct). iNukeExplosionRand->drop (RNG, §8-i). SpecialBuilding:
iMaxPlayerInstances = per-player build-GROUP cap (isBuildingGroupMaxedOut) -> requires max / a group-cap concept;
bValid -> loadPrune?; TechPrereq/ObsoleteTech already store-wired. Flag every creative/non-obvious flag.`,
  },
]

// ── Run: pipeline(understand -> adversarial verify) per slice ──────────────────
log(`Classifying ${SLICES.length} Building field-slices + SpecialBuilding, adversarially.`)

const results = await pipeline(
  SLICES,
  (s) => agent(
    `${MODEL}

YOUR SLICE: "${s.label}".
Fields to classify (this slice only):
${s.fields}

Slice-specific focus / known traps:
${s.focus}

TASK: for EVERY field above, (1) locate its member + tag in CvBuildingInfo (getDataMembers 1666-1923 or the
hand-written read() remainder 1929+); (2) grep its getter across Sources/*.cpp AND Assets/Python to find the
LIVE consumer(s) — if none, say so with the grep you ran; (3) assign the v3/v0.3 home (family.scope.member.unit,
or requires/enables/grants/identity/cost/property/DROP); (4) ⛔ NEVER guess a home — if the correct home is NOT
determinable with HIGH confidence from the VERIFIED consumer (ambiguous / cross-domain / creative-mechanic /
resists the model), set needsRuling=true + home="NEEDS_OWNER_RULING" and say in notes what is ambiguous + the
verified facts; (5) DROP only when grep PROVES zero consumers (C++ AND Python), with dropCategory + the grep evidence.
Be exhaustive — skip no field. Trust the mapping for nothing; NEVER invent/force-fit a home. Return the structured object.`,
    { label: `understand:${s.key}`, phase: 'Understand', agentType: 'Explore', schema: FIELD_SCHEMA }
  ),
  (understood, s) => agent(
    `${MODEL}

You are the ADVERSARY for the Building slice "${s.label}". A prior agent produced these per-field
dispositions (JSON):
${JSON.stringify(understood)}

Your job is to REFUTE, not rubber-stamp. For each field, actively try to break the disposition:
- "NONE FOUND"/DROP claims: re-grep the getter (C++ AND Assets/Python, incl. sibling/indirect callers like
  CvGameTextMgr pedia, REV, event callbacks). A wrongly-dropped live field is the worst error. The
  bIsUniversalTradeBonusProvider incident: a grep-only "dead" verdict missed an OR-fold indirect consumer.
- SCOPE: is city vs area vs empire vs plot right? (the mapping is often wrong; verify against the C++ accumulation site).
- §6.1 OWNERSHIP: keep-on-building-keyed vs invert-onto-conditioner vs downward-from-tech — is it correct?
- UNIT: is a percent really a percent (not multiplier), per the "datum's nature" rule?
- GUESSED HOMES (the worst sin): did they invent/best-fit/"SHOEHORN?" a home that is NOT high-confidence-verified
  from a live consumer? That is a GUESS -> set needsRuling=true + home="NEEDS_OWNER_RULING" (the owner rules it; never
  ship a guess). A non-obvious / creative / cross-domain mechanic whose home isn't certain is a needsRuling, not a best-fit.
Return the corrected structured object (same schema) — fix every disposition you can refute, keep the rest,
and in 'notes' prefix any change with "VERIFIER:". Keep notes <=2 sentences; cite file:line; never dump code.`,
    { label: `verify:${s.key}`, phase: 'Verify', agentType: 'Explore', schema: FIELD_SCHEMA }
  )
)

const slices = results.filter(Boolean)
const totalFields = slices.reduce((n, r) => n + (r.fields ? r.fields.length : 0), 0)
// The ASK list: fields the agents could NOT verify a home for -> the owner rules these (never guessed).
const needsRuling = slices.flatMap(r => (r.fields || []).filter(f => f.needsRuling || f.disposition === 'needsRuling' || f.home === 'NEEDS_OWNER_RULING').map(f => `${r.slice}:${f.name} — ${f.notes || ''}`))
const lowConf = slices.flatMap(r => (r.fields || []).filter(f => f.confidence === 'low').map(f => `${r.slice}:${f.name}`))
log(`Done: ${slices.length}/${SLICES.length} slices, ${totalFields} fields. ${needsRuling.length} fields NEED AN OWNER RULING (home NOT guessed -- the ASK list); ${lowConf.length} low-confidence.`)

// needsRuling is the headline output: the owner adjudicates these before any home is committed (ASK, never guess).
return { slices, totalFields, needsRuling, lowConfidence: lowConf }
