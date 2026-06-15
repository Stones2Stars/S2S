export const meta = {
  name: 'map-data-model-428',
  description: 'Classify every gameplay Info entity field into the #428 JSON channel/inversion data model',
  phases: [
    { title: 'Classify', detail: 'one agent per gameplay entity — full tag classification, writes per-entity mapping file' },
    { title: 'Synthesize', detail: 'cross-check inversion consistency, unify channel vocab, consolidate edge cases' },
  ],
}

// 34 gameplay relationship entities (blueprint scope). Tag files already extracted to Tools/Migration/tags/.
const ENTITIES = [
  ['BuildingInfo', 'Buildings/*CIV4BuildingInfos.xml'], ['SpecialBuildingInfo', '**/CIV4SpecialBuildingInfos.xml'],
  ['UnitInfo', 'Units/*CIV4UnitInfos.xml'], ['SpecialUnitInfo', '**/CIV4SpecialUnitInfos.xml'],
  ['UnitCombatInfo', '**/*CIV4UnitCombatInfos.xml'], ['PromotionInfo', '**/CIV4PromotionInfos.xml'],
  ['PromotionLineInfo', '**/CIV4PromotionLineInfos.xml'], ['TechInfo', '**/CIV4TechInfos.xml'],
  ['BonusInfo', '**/*CIV4BonusInfos.xml'], ['BonusClassInfo', '**/CIV4BonusClassInfos.xml'],
  ['CivicInfo', '**/CIV4CivicInfos.xml'], ['CivicOptionInfo', '**/CIV4CivicOptionInfos.xml'],
  ['ImprovementInfo', '**/CIV4ImprovementInfos.xml'], ['TerrainInfo', '**/CIV4TerrainInfos.xml'],
  ['FeatureInfo', '**/CIV4FeatureInfos.xml'], ['ReligionInfo', '**/CIV4ReligionInfo.xml'],
  ['CorporationInfo', '**/CIV4CorporationInfo.xml'], ['ProjectInfo', '**/CIV4ProjectInfo.xml'],
  ['ProcessInfo', '**/CIV4ProcessInfo.xml'], ['SpecialistInfo', '**/CIV4SpecialistInfos.xml'],
  ['CivilizationInfo', '**/CIV4CivilizationInfos.xml'], ['LeaderHeadInfo', '**/CIV4LeaderHeadInfos.xml'],
  ['TraitInfo', '**/CIV4TraitInfos.xml'], ['RouteInfo', '**/CIV4RouteInfos.xml'],
  ['BuildInfo', '**/CIV4BuildInfos.xml'], ['VoteInfo', '**/CIV4VoteInfo.xml'],
  ['VictoryInfo', '**/CIV4VictoryInfo.xml'], ['CultureLevelInfo', '**/CIV4CultureLevelInfo.xml'],
  ['GameSpeedInfo', '**/CIV4GameSpeedInfo.xml'], ['EraInfo', '**/CIV4EraInfos.xml'],
  ['HurryInfo', '**/CIV4HurryInfo.xml'], ['HandicapInfo', '**/CIV4HandicapInfo.xml'],
  ['PropertyInfo', '**/CIV4PropertyInfos.xml'], ['HeritageInfo', '**/HeritageInfos.xml'],
]

const RULES = [
  'THE #428 DATA MODEL — classify EVERY tag into exactly ONE category:',
  '',
  'CHANNEL — a standing modifier this entity contributes. Give {scope, channel, kind}:',
  '  scope is one of game|team|player|area|city (where the effect lands; Global/World->player or team, Area->area, else city).',
  '  kind is one of flat (additive int) | percent (a Modifier/Percent field) | enabler (a bool capability -> value true).',
  '  channel = a short stable name (happiness, health, greatPeopleRate, maintenance, hurryCost, defense, commerce,',
  '            yield, tradeRoutes, experience, foodKept, anarchy, ...). Reuse obvious names across entities.',
  '  Yield/commerce arrays carry SHORT keys: yield->[food,production,commerce]; commerce->[gold,research,culture,espionage].',
  '  Give valueKeys for those.',
  '',
  'INVERSIONOUT — a CONDITIONAL cross-entity effect that must HOME onto its conditioner entity (blueprint section 2).',
  '  The XML map is keyed by the DEST Type; you give {destEntity (Cv*Info), shape}. The SOURCE entity stops owning it.',
  '  e.g. Building.TechCommerceChanges -> destEntity=CvTechInfo, shape=buildingBoosts.{BUILDING}.commerce.',
  '  ONLY assign inversionOut for edges the blueprint section 2 actually lists. Unsure it inverts -> edgeCase.',
  '',
  'ENABLEREDGE — a TOP-DOWN enabler list (the source lists what it enables): tech->enabledBuildings,',
  '  building->enabledUnits, etc. Targets never name their source. Give {enables:"<listName>"}.',
  '',
  'PREREQ — a dependency/gate: Prereq*, requires*, ConstructCondition/TrainCondition BoolExpr, *MakesValid,',
  '  PrereqGameOption, era/civ/civic/religion/corp/bonus/building/tech prereqs. STAYS on the dependent (issue 195).',
  'COST — base production/build cost fields (iCost*). NOTE iHurryCostModifier etc. are CHANNELS (percent), not cost.',
  'ART — art/sound/movie/button/display/advisor/icon/formation tags.',
  'IDENTITY — genuine identity & metadata (Type, Description, Civilopedia, Help, flags, placement, latitude, era) AND',
  '  blueprint section 5 STAYS: lifecycle FKs (Obsolete*, Replacement*, *UpgradeTo, Capture, HolyCity, ReligionType,',
  '  SpecialBuildingType, GreatPeopleUnitType, PropertySpawn*), intrinsic grants (Free*, ExtraFreeBonuses, FirstFree*,',
  '  InitialCivics), OWN-STAT keyed combat/movement/invisibility maps (reshape only, NEVER invert), leader personality',
  '  bias (*WeightModifiers, *AttitudeThreshold, Favorite*, Traits), fixed-enum-keyed tables (PlotYield, RiverPlotYield,',
  '  Yield/Commerce/Domain-indexed), MapCategoryTypes/categories. PropertyManipulators -> identity (self-reading, NEVER a channel).',
  '',
  'EDGECASE — anything you cannot confidently place, or that needs an owner decision. Give issue + options + recommendation.',
  '',
  'COMPLETENESS IS MANDATORY: every tag in your tag file gets exactly one classification entry. Count them.',
].join('\n')

const FILE_SHAPE = [
  'Then WRITE the per-entity mapping to Tools/Migration/mapping/<Entity>.json with EXACTLY this shape:',
  '{',
  '  "entity":"<Entity>", "rootElement":"<Entity>", "glob":"<the glob you were given>",',
  '  "channels": { "<tag>": {"scope":"city","channel":"happiness","kind":"flat","valueKeys":["food"]}, ... },',
  '  "inversionsOut": { "<tag>": {"destEntity":"CvTechInfo","shape":"buildingBoosts.{BUILDING}.commerce"}, ... },',
  '  "enablerEdges": { "<tag>": {"enables":"enabledBuildings"}, ... },',
  '  "prereqs": ["<tag>"], "cost": ["<tag>"], "art": ["<tag>"], "identity": ["<tag>"],',
  '  "boostsIn": ["buildingBoosts"]',
  '}',
  'Every tag from the tag file MUST appear as a key in channels/inversionsOut/enablerEdges OR in one of the',
  'prereqs/cost/art/identity arrays. boostsIn = boost channels THIS entity RECEIVES from other entities (e.g.',
  'CvTechInfo/CvBonusInfo receive "buildingBoosts"; most entities receive none -> []).',
].join('\n')

const SPEC_SCHEMA = {
  type: 'object',
  required: ['entity', 'distinctTagsSeen', 'classifiedCount', 'wroteFile', 'inversionsOut', 'boostsIn', 'channelsUsed', 'edgeCases'],
  properties: {
    entity: { type: 'string' },
    distinctTagsSeen: { type: 'integer' },
    classifiedCount: { type: 'integer' },
    wroteFile: { type: 'boolean' },
    inversionsOut: {
      type: 'array', items: {
        type: 'object', required: ['tag', 'destEntity', 'shape'],
        properties: { tag: { type: 'string' }, destEntity: { type: 'string' }, shape: { type: 'string' } },
      },
    },
    boostsIn: { type: 'array', items: { type: 'string' } },
    channelsUsed: { type: 'array', items: { type: 'string' } },
    edgeCases: {
      type: 'array', items: {
        type: 'object', required: ['tag', 'issue', 'recommendation'],
        properties: { tag: { type: 'string' }, issue: { type: 'string' }, options: { type: 'array', items: { type: 'string' } }, recommendation: { type: 'string' } },
      },
    },
  },
}

function classifyPrompt(entity, glob) {
  return 'You classify ONE gameplay entity\'s XML fields into the #428 JSON data model for entity ' + entity + '.\n\n' +
    'STEPS:\n' +
    '1. Read your authoritative tag list: Tools/Migration/tags/' + entity + '.json (every distinct XML tag with kind+sample+count).\n' +
    '2. To confirm cross-entity inversions, GREP (do not full-read) the inversion map\n' +
    '   Sources/docs/plans/cross-entity-inversion-blueprint.md for "' + entity + '" and your field names\n' +
    '   (section 2 = inversions, section 5 = what STAYS). The RULES below are self-contained — do NOT read other docs.\n' +
    '3. Classify EVERY tag per the rules below.\n' +
    '4. ' + FILE_SHAPE + '\n' +
    '5. Return the lean summary (structured). distinctTagsSeen and classifiedCount MUST be equal.\n\n' +
    RULES + '\n\nThe glob for your XML files is: ' + glob
}

phase('Classify')
// Rate-limit fix: the first run burst 16 concurrent heavy agents and tripped the server throttle.
// Process in small chunks (caps concurrency well under the limit) and skip entities already mapped.
const DONE = new Set(['BonusClassInfo', 'CivicInfo', 'CivicOptionInfo', 'PromotionInfo', 'TerrainInfo'])
const TODO = ENTITIES.filter(function (e) { return !DONE.has(e[0]) })
const CHUNK = 5
const specs = []
for (let i = 0; i < TODO.length; i += CHUNK) {
  const batch = TODO.slice(i, i + CHUNK)
  const r = await parallel(batch.map(function (e) {
    return function () {
      return agent(classifyPrompt(e[0], e[1]), { label: e[0], phase: 'Classify', schema: SPEC_SCHEMA })
    }
  }))
  for (let j = 0; j < r.length; j++) if (r[j]) specs.push(r[j])
  log('batch ' + (Math.floor(i / CHUNK) + 1) + ': +' + r.filter(Boolean).length + ' ok (' + specs.length + '/' + TODO.length + ' new entities mapped)')
}

let invEdges = 0, edgeN = 0
for (let i = 0; i < specs.length; i++) {
  invEdges += specs[i].inversionsOut ? specs[i].inversionsOut.length : 0
  edgeN += specs[i].edgeCases ? specs[i].edgeCases.length : 0
}
log('classified ' + specs.length + '/' + ENTITIES.length + ' entities; ' + invEdges + ' inversion edges; ' + edgeN + ' edge cases')

const incomplete = []
for (let i = 0; i < specs.length; i++) {
  if (specs[i].classifiedCount !== specs[i].distinctTagsSeen) {
    incomplete.push(specs[i].entity + ': ' + specs[i].classifiedCount + '/' + specs[i].distinctTagsSeen)
  }
}

phase('Synthesize')
const SYNTH_SCHEMA = {
  type: 'object',
  required: ['totalEntities', 'totalInversionEdges', 'inversionConsistency', 'channelVocabulary', 'edgeCaseDecisions', 'readyForEngine', 'notes'],
  properties: {
    totalEntities: { type: 'integer' },
    totalInversionEdges: { type: 'integer' },
    inversionConsistency: {
      type: 'array', items: {
        type: 'object', required: ['issue', 'severity'],
        properties: { issue: { type: 'string' }, severity: { type: 'string', enum: ['blocker', 'warn', 'info'] } },
      },
    },
    channelVocabulary: {
      type: 'array', items: {
        type: 'object', required: ['channel', 'variants'],
        properties: { channel: { type: 'string' }, variants: { type: 'array', items: { type: 'string' } }, usedByCount: { type: 'integer' } },
      },
    },
    edgeCaseDecisions: {
      type: 'array', items: {
        type: 'object', required: ['entity', 'tag', 'issue', 'recommendation'],
        properties: { entity: { type: 'string' }, tag: { type: 'string' }, issue: { type: 'string' }, options: { type: 'array', items: { type: 'string' } }, recommendation: { type: 'string' } },
      },
    },
    readyForEngine: { type: 'boolean' },
    notes: { type: 'string' },
  },
}

const incompleteNote = incomplete.length
  ? 'INCOMPLETE CLASSIFICATIONS to flag as blockers: ' + incomplete.join('; ')
  : 'All entities classified completely.'

const synthPrompt =
  'You are the synthesis pass for the #428 data-model mapping. The 34 per-entity mapping files are written under\n' +
  'Tools/Migration/mapping/*.json. Here are the lean per-entity summaries (JSON):\n\n' +
  JSON.stringify(specs) + '\n\n' + incompleteNote + '\n\n' +
  'YOUR JOB:\n' +
  '1. INVERSION CONSISTENCY: for every inversionsOut edge, verify destEntity is one of the 34 entities AND that\n' +
  '   destEntity declares the matching boost channel in its boostsIn (an edge with shape "buildingBoosts.{BUILDING}.x"\n' +
  '   needs the dest boostsIn to include "buildingBoosts"). Read the relevant Tools/Migration/mapping/*.json to confirm.\n' +
  '   Report every orphan (dest missing, or boostsIn slot missing) as a blocker with exact source entity+tag and dest.\n' +
  '2. CHANNEL VOCABULARY: collect channel names used across entities (channelsUsed). Group near-duplicate names that\n' +
  '   mean the SAME concept (production vs productionModifier) into one row with its variants, so we can unify.\n' +
  '3. EDGE CASES: consolidate every entity edgeCases into one decision list (entity+tag+issue+options+recommendation).\n' +
  '4. readyForEngine=true only if no blocker inversion inconsistencies and no incomplete classifications.\n' +
  '5. notes: anything the human must know before running the converter engine. Do NOT rewrite the per-entity files.'

const synth = await agent(synthPrompt, { label: 'synthesize', phase: 'Synthesize', schema: SYNTH_SCHEMA })

return { classified: specs.length, incomplete: incomplete, synthesis: synth }
