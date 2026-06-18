# Combat Simplification Scope (consolidated)

The combat rework has grown from "unify combat odds" into stripping the TB Combat Mod
suite down to a clean, maintainable core, then unifying that core into one engine with
pluggable game-option rules. This doc is the single source of what stays vs goes.
Priorities: OOS-safe > maintainable/sane > save-compat (save-compat downprioritized).

## KEEP — the lean combat core

- Strength/firepower: base/curr/max combat strength, firepower (incl. `SIZE_MATTERS`
  scaling and `STRENGTH_IN_NUMBERS` support).
- Combat rounds: `COMBAT_DAMAGE`, HP, needed-rounds, win odds (the unified engine).
- Combat limit (capture / can't-fully-kill).
- Collateral damage.
- Barb free-wins (standardized via one rule).
- `VANILLA_ENGINE` toggle (combat-mode rule).
- Endurance (explicitly kept).

## REMOVE — confirmed

- R1 Cold damage — **DONE** (code cold-free across 12 files incl. terrain-cold trigger,
  CvUnit fields/serialization, Info flags+XML schema, UI, AI; Assert build OK). Leftover
  unused TXT_KEY_*COLD* GameText entries are harmless; trim later if desired.
- R2 Stuns — **DONE** (CvUnit fields/serialization incl. keyed-info member + temp-array
  save/load; resolveCombat gates rewritten to keep first-strike/hit logic; checkForStun
  - accessors; Info vectors+XML+schema; UI combat-odds preview + help; AI valuation;
  Assert build OK; zero functional stun refs, only 2 harmless comments left for R4).
- R3 Power-shots — **DONE** (5 scalar modifier sub-types; build OK).
- R4 Afflictions + critical + `OUTBREAKS_AND_AFFLICTIONS` option — **DONE**:
  - Phase 1: `OUTBREAKS_AND_AFFLICTIONS` `#define` + `GAMEOPTION_COMBAT_*` enum/Python/
    GameOptionInfos/GameText removed (build green).
  - Phases 2-7: the entire inert affliction/critical/fortitude scaffolding removed
    wholesale (~600 refs, 14 .cpp/.h + 3 XML). Confirmed behavior-neutral (no runtime
    processing, no XML data drove it — only `iFortitudeChange` had live data, also pulled).
    - CvUnit: keyed-info affliction members, m_iExtraFortitude/CriticalModifier, all
      `g_paiTempAfflict*/Fortitude*/CriticalVS*` temp arrays + alloc, interleaved
      read/write serialization (kept trapSetWithPromotion/promotionFromTrait/validBuildUp/
      flanking), criticalModifierTotal/criticalVS{UnitCombat,Opponent}Total/canInflictCritical/
      getExtra{Fortitude,CriticalModifier,CriticalVSUnitCombatType}, promotion/unitcombat
      apply-sites, two "developing critical chance" combat hints.
    - Info classes (CvUnitInfo/CvPromotionInfo/CvPromotionLineInfo/CvUnitCombatInfo/
      CvBuildingInfo): Fortitude/CriticalModifier/CriticalVSUnitCombat/CureAffliction/
      isCritical/CriticalOriginCombatClass/Overcome (UnitCombat+Tech+OvercomeBase)/
      Outbreak (TechOutbreakLevel+AfflictionOutbreakLevel)/TradeCommunicability/
      DistanceAttackCommunicability/AfflictionFortitudeChangeModifier.
    - CvStructs: AfflictOnAttack(Change), AfflictionLineChanges, PromotionLineAfflictionModifier.
    - CvCity: 5 affliction arrays (NewAffliction*/Overcome/Communicability) + serialization.
    - CvPlayer: m_paiPlayerWideAfflictionCount.
    - Consumers: CvGameTextMgr UI help blocks, CvPlayerAI critical valuation.
    - XML: orphaned schema ElementTypes + refs in both schemas + dead `iFortitudeChange` data.
  - **Follow-up orphan sweep — DONE:** removed the remaining inert remnants too —
    `PromotionRequirements::Afflict` flag + `bAfflict` param (canAcquirePromotion);
    `bAffliction` param (CvCity/CvPlayer canConstruct, the always-false `!bAffliction`
    gates unwrapped — `bExposed` is a separate live concept, kept); the `NoSpread*`
    affliction disease-spread flags (CvPromotionLineInfo + promotion-help UI + schema);
    the `CATEGORY_UNITCOMBAT_EQUIPMENT_GROUPS` category (def + 5 weapon-category parent
    links + GameText); dead Pedia.py Equipment/Affliction promotion categories; and
    orphaned affliction schema element refs. KEPT: the property/
    disease system (CvProperties/PropertyManipulators/PROPERTY_*, AidRate/BonusAid),
    PromotionLine class itself + its `m_ePropertyType`, keyed-info structs, endurance,
    and vanilla withdrawal are all untouched. Serialization sentinel key strings
    ("hasAfflicationInfo"/"hasAfflictOnAttackInfo") kept (now key the surviving blocks).
    Build green, all 461 XML valid.
- R5 modifier family (dodge/precision/armor/puncture/withdraw/repel/...).  R6 terrain damage.

## REMOVE — TB Combat Mods come out wholesale (user-confirmed)

TB Combat Mods are overcomplicated, unmaintainable, poorly implemented. Remove the suite
now; note the good ideas for proper reimplementation later (below). Boundary = "keep
vanilla BTS combat, remove TB additions."

- **R5 modifier family** (~250 refs) decomposed into THREE removals, **all DONE**:
  - **R5a accuracy:** dodge + precision — **DONE** (commit b6e35a42). Hit-chance modifiers
    gone; per-round odds are now plain strength-ratio (the resolveCombat hit-modifier is a
    no-op). The leftover ordering bug it exposed was fixed in Phase 3a (round-1 hit chance).
  - **R5b mitigation:** armor + puncture — **DONE** (commit b6e35a42). Damage-reduction
    terms gone; per-round damage is firepower-ratio + damageModifier only.
  - **R5c escape/control = `GAMEOPTION_COMBAT_FIGHT_OR_FLIGHT`** — **DONE**. Removed the
    withdraw/pursuit/repel/overrun/unyielding/knockback TB layer + the option/enum/Python/
    XML. KEPT **vanilla** `withdrawalProbability()` and dropped the old halving conditional
    so vanilla withdrawal works unconditionally. Concept kept in the good-ideas list.
- **R6 terrain/feature turn damage** — **DONE** (removed wholesale across ~15 files:
  CvUnit::doTurn application + XP; CvPlot getTotalTurnDamage/getTerrainTurnDamage/
  getFeatureTurnDamage; CvUnit isTerrainProtected/getTerrainProtectedCount/
  changeTerrainProtected + TerrainKeyedInfo.m_iTerrainProtected serialization;
  CvFeatureInfo iTurnDamage; CvPromotionInfo IgnoreTerrainDamage; CvUnitCombatInfo
  TerrainIgnoreDamageChangeTypes + orphan m_iIgnoreTerrainDamage; AI avoidance
  (CvUnitAI guard/explore/retreat/heal/defensive/tile-work, CvPlayerAI promotion
  valuation), pathfinding cost (CvGameCoreUtils x2), CvSelectionGroup heal gates;
  UI help (feature/terrain/plot/promotion/unitcombat); `MODDERGAMEOPTION_TERRAIN_DAMAGE`
  enum + dual Python registration; XML schema + data (FeatureInfos/PromotionInfos +
  2 modules); GameText keys; BUG toggle (RoMSettings.xml + ANewDawnSettings.py +
  RoMOptionsTab.py); Pedia. Build-verified, XML well-formed. Reimplement later as a
  terrain-attrition rule the AI understands — see good-ideas list.).

### `GAMEOPTION_COMBAT_*` triage (user-decided)

Footprint measured as functional C++ refs (excludes the 3 boilerplate refs every option
has: enum decl + 2 Python registrations).

- **KEEP:** `SIZE_MATTERS` (~129, core feature), `STRENGTH_IN_NUMBERS` *(see below — now
  slated remove)*, `VANILLA_ENGINE` (1, the lean-core switch), `NEW_RANDOM_SEED` (~1,
  RNG/reload infra, not combat spaghetti), `WITHOUT_WARNING` *(user-confirmed keep — see below)*.
- **REMOVE — dead enum** (0 functional C++ refs, no XML content): `HEART_OF_WAR` (was
  repel/knockback/unyielding — gutted by R5c; only a commented building line + def + text).
  Trivial: enum + 2 Python + XML option def + GameText.
- **`EQUIPMENT` / `UPRANGE` — DONE** (user: "remove it all"). 0 C++ logic, but they gated the
  whole off-by-default equipment/weapon-loadout system: **88 unitcombat classes** (22
  `WEAPON_STYLE_*` holding styles + 26 `WEAPON_SIDEARM_*` + 40 `WEAPON_BACKUP_*` named
  weapons) via `<OnGameOptions>`. Removed the 88 `<UnitCombatInfo>` defs + **4,906
  `<SubCombatType>` refs across 19 unit/module files** + 88 orphaned GameText keys + both
  options (enum/Python/GameOptionInfos/GameText, incl. EQUIPMENT→UPRANGE enforce). KEPT
  non-gated weapons (primary `WEAPON_H2H_*`/`WEAPON_METHOD_*` and always-on futuristic
  `WEAPON_BACKUP_*` like antimatter/coilgun/plasma). Build green, all XML valid.
- **REMOVE:** `BATTLEWORN` — **DONE**. Was a commented-out `#define BATTLEWORN` (data-wiring
  dead-compiled → already inert). Purged 31 dead `#ifdef BATTLEWORN` blocks (283 lines) +
  the live-but-inert strAdjper Rnd/Att/Def layer (CvUnit accessors/members/serialization,
  currCombatStr application, CvGameTextMgr unit+promotion help) + schema/option/enum/Python/
  GameText (incl. 26 orphaned RAGE/FATIGUE/RAMPAGE/TIRES/DETERMINATION/DEMORALIZATION keys).
  KEPT the round/attack/defense counters (getDefenseCount still used by visibility logic) and
  the separate `iStrAdjperTurn` field. Build green, XML valid.
- **KEEP (reversed from earlier "remove"):** `SURROUND_DESTROY`. Per user it is a *functional*
  game option (AI underutilizes it, but it works) — distinct from the broken/non-functioning
  TB options. It's a LIVE system (real `surroundedDefenseModifier` combat application + city
  dynamic defense, ~250 refs / 27 files). We strip broken cruft and leave working features.
  A turnkey removal map exists at surround-destroy-removal-map.md IF ever revisited; no
  obligation. Concept also in good-ideas list.
- **REMOVE + document:** `STRENGTH_IN_NUMBERS` — **DONE**. Confirmed commented-out
  `#define STRENGTH_IN_NUMBERS` (dead-compiled) — the entire stack-support mechanic
  (getDefenderSupportValue/getAttackerSupportValue/ClearSupports + front/short/medium/long/
  flank support percents + support-formation unit refs) was inert. Purged ~2,574 lines of
  dead `#ifdef` across 15 files (incl. CvUnit.cpp -1,908) + the serialization-skip cruft +
  `bSINView` Alt-key preview mode + option enum/Python/GameOptionInfos/GameText + schema
  (Unit + Buildings) + `TB_PRESSALT_STR`. No XML data used these fields (confirms "never
  implemented"). Build green, XML valid. Concept in good-ideas list.
- **KEEP (user-confirmed):** `REALISTIC_SIEGE` (~4, breakdown/bombard siege), `AMNESTY`
  (~3, Hidden-Nationality RoP attack block), `HIDE_SEEK` (~31, multi-layer invisibility),
  `WITHOUT_WARNING` (~23, ambush/assassination/stealth — a real gameplay feature with
  assassination missions + UI). These are functional gameplay options (like
  `SURROUND_DESTROY`), not the broken/non-functioning TB cruft — they stay.
- **DEFER:** `OUTBREAKS_AND_AFFLICTIONS` — handled by the R4 afflictions+critical task.
- NOTE: GAMEOPTION enum order must stay aligned with `CIV4GameOptionInfos.xml`; removing a
  value shifts later options' save bits (save-compat downprioritized) and requires deleting
  the matching XML `<GameOptionInfo>` + GameText.

## KEEP — vanilla BTS combat (the boundary)

First strikes, vanilla withdrawal probability, collateral, combat limit, strength/
firepower, XP/promotion-on-combat. (If any of these are actually TB reimplementations
rather than vanilla, revisit.)

## Weapon types — RESOLVED

This is the `UNITCOMBAT_WEAPON_STYLE_*` taxonomy gated by `GAMEOPTION_COMBAT_EQUIPMENT`
(and `_UPRANGE`): ~75 weapon-style unitcombat classes in CIV4UnitCombatInfos.xml
(single-hand / two-handed / primary+secondary / shield / H2H-vs-distance, …) toggled via
`<OnGameOptions>`. No C++ logic reads the EQUIPMENT/UPRANGE options, but the content exists.
Decision pending: remove the whole weapon-style system (and its options) or keep it. Until
decided, EQUIPMENT/UPRANGE stay.

## Good ideas to reimplement properly later (do NOT lose the concepts)

Captured so the spirit survives the removal:

- **Terrain/feature turn damage** — environmental attrition the AI actually understands.
- **Accuracy (precision/dodge)** — hit-chance modifiers as a clean, optional rule.
- **Mitigation (armor/puncture)** — damage-reduction vs penetration.
- **Escape/control (withdraw/pursuit/repel/knockback)** — disengage & push mechanics.
- **Afflictions/disease, cold/environmental damage, stun, power-shot, critical** — status
  & burst effects, if wanted, as discrete well-scoped systems.
- **Surround & destroy** (`SURROUND_DESTROY`) — positional bonuses for surrounding enemy
  units/cities (land-ambush simulation). Good concept; the original needed AI that
  actually exploits it. Reimplement as a rule with AI that values encirclement.
- **Strength in numbers** (`STRENGTH_IN_NUMBERS`) — stacking-based combat scaling. Concept
  worth keeping; the TB version was never properly implemented (no observable in-game
  effect). Reimplement cleanly as an optional rule.
These become future "rules" plugged into the unified engine, not inline spaghetti.

## Status (current)

The removal phase is **complete**: R1 cold, R2 stun, R3 power-shot, R4 afflictions+critical
(+ orphan sweep), R5a dodge/precision, R5b armor/puncture, R5c FIGHT_OR_FLIGHT, R6 terrain
damage — all removed and building green. Dead game options removed: HEART_OF_WAR, BATTLEWORN,
STRENGTH_IN_NUMBERS, EQUIPMENT, UPRANGE, FIGHT_OR_FLIGHT, OUTBREAKS_AND_AFFLICTIONS,
TERRAIN_DAMAGE. Kept (functional) options: SIZE_MATTERS, SURROUND_DESTROY, VANILLA_ENGINE,
NEW_RANDOM_SEED, REALISTIC_SIEGE, AMNESTY, HIDE_SEEK, WITHOUT_WARNING. A follow-up assert
pass surfaced no combat-path asserts; first real combat ran clean.

Schema note: a prior bulk schema edit had wrongly deleted ~177 *live* `<ElementType>` defs
from `C2C_CIV4UnitSchema.xml` (alongside the dead ones) — restored from baseline so every
referenced element is declared again (validator clean).

## Unify phase

Removals done → now unifying the lean core onto `CvCombatModel`:

- **Phase 3a — DONE.** Layer-1 `buildRoundModel()` is the single per-round formula;
  resolution, the odds functions, the preview, and the AI's damage inputs all consume it
  (prediction == resolution; barb free-wins + the round-1 hit-chance bug reconciled).
  See `combat-model-sketch.md`.
- **Phase 3b — IMPLEMENTED** (PR #318; FinalRelease-playtested as a foundation, long-term
  balance/calibration tracked as separate followups). Non-air AI attacks
  now decide on `getCombatOdds()/10` (the UI/resolver number) instead of the strength-ratio
  heuristic; `AI_finalOddsThreshold *23/20` recentres the bar (air-exempt); heuristic kept
  for predicted-HP; personality bias preserved; `[COM/calib]` log added. NB: first done on
  branch `combat-ai-binomial-odds` (PR #23) which never merged — re-applied to `main`. Full
  detail + GlobalDefine follow-up in `combat-phase3b-plan.md`.
