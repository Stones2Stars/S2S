> **Provenance note:** Imported and adapted from the C2C community Modder's Documentation thread on CivFanatics, originally maintained by Dancing Hoskuld and contributors. Review all content for S2S accuracy before relying on it — S2S has diverged from C2C in significant areas (combat model, AI architecture, assert/logging infrastructure). Source thread: https://forums.civfanatics.com/threads/modders-documentation.441325/

---

# C2C Modder's Documentation — Adapted for S2S

## Overview

This document synthesizes the modding reference material collected in the C2C "Modder's Documentation" thread and the key sub-threads it indexes. It is organized by subject area. Where S2S has already diverged from C2C (e.g. the combat model, AI logging), those sections are flagged.

---

## 1. XML Modding Fundamentals

### How XML Loading Works in C2C / S2S

The engine reads XML via a schema-and-info-file pair for each data type. The schema (`CIV4XxxSchema.xml`) declares which tags exist; the info file (`CIV4XxxInfos.xml`) contains the actual data records.

**Schema anatomy (from Thunderbrd's Modder's Guide to XML):**

- Lines beginning with `<ElementType name=` declare a tag and its allowed content type.
- Lines beginning with `<element type=` inside a declaration bind a child tag to its parent: if a child tag appears in the schema as applied to some info types but not others, it will be silently ignored (no error) when used in an unsupported info file.
- **Tag ordering** no longer matters in C2C/S2S due to updated XML loader behavior.
- **Omit optional tags** when not needed — do not include them with zero/empty values unless required.

**Tag naming conventions (prefix indicates type):**

| Prefix | Type | Example |
|--------|------|---------|
| `i` | Integer | `<iXPValueAttack>6</iXPValueAttack>` |
| `b` | Boolean (0 or 1) | `<bAnimal>1</bAnimal>` |
| (none / `Type`) | Reference to another info type | `<UnitAIType>UNITAI_ANIMAL</UnitAIType>` |
| Multi-tag | Nested collection | `<UnitAIs><UnitAI>…</UnitAI></UnitAIs>` |

Boolean tags can have dangerous defaults if omitted — consult the schema for each tag.

**Multi-tag pattern:**

```xml
<UnitAIs>
  <UnitAI>
    <UnitAIType>UNITAI_ANIMAL</UnitAIType>
    <bUnitAI>1</bUnitAI>
  </UnitAI>
</UnitAIs>
```

### Modular Loading System

C2C introduced a module loading system inherited by S2S. Modules live under `Assets/Modules/` (or `Alt_Timelines/<ModName>/`). Each module directory registers itself in `MLF_CIV4ModularLoadingControls.xml` using `<Directory>` and `<bLoad>` tags (`1` = active, `0` = disabled). A module that is a standalone content addition needs its own schema file, info file, art defines file, and text file.

**Minimum file set for a new building module:**

- `CIV4BuildingsSchema.xml`
- `CIV4BuildingInfos.xml`
- `CIV4BuildingClassInfos.xml`
- `CIV4ArtDefines_Building.xml` + `CIV4ArtDefinesSchema.xml`
- Text key file (`CIV4GameText_…xml`)

**Text key naming convention:**

- Display name: `TXT_KEY_BUILDING_<NAME>`
- Strategy blurb: `TXT_KEY_BUILDING_<NAME>_STRATEGY`
- Pedia entry: `TXT_KEY_BUILDING_<NAME>_PEDIA`

Text files support multiple language blocks (English, French, German, Italian, Spanish) in the same file.

---

## 2. AI Flavor Tags

Flavor tags are AI decision weights. They do **not** affect player gameplay at all — they tell the AI how much it should want to build a given building, research a given tech, etc., relative to its personality.

**How flavors work:**
Each AI leader has personality preferences for certain flavor types (e.g. `FLAVOR_MILITARY`, `FLAVOR_CULTURE`, `FLAVOR_GROWTH`). A building tagged with `FLAVOR_CULTURE 50` signals to any culture-oriented AI that this building is a high priority. Flavor values are not globally consistent — calibrate by comparing similar existing entries in the same category.

**Rough scale:**

- `0`: No weight (default)
- `~50`: Moderate priority
- `100+`: High priority (factories, spaceship parts, unique wonders)
- `-999`: Strong avoidance signal

**Multi-tag structure (C2C schema style):**

```xml
<Flavors>
  <Flavor>
    <FlavorType>FLAVOR_CULTURE</FlavorType>
    <iFlavor>50</iFlavor>
  </Flavor>
</Flavors>
```

**AI Strategy integration:** The AI city-strategy system loops through each city each turn, checks strategy criteria (population, threats, yields), and applies additional flavor weights for that strategy. This is separate from leader personality weighting.

**Weighting new properties (crime, pollution, etc.):** When you add a new Property to the game, you must also supply flavor tag guidance so the AI knows to build counter-buildings. The C2C documentation thread specifically cross-references the sub-thread "Information for AI to weight new properties like flammability and crime" for this workflow.

> **S2S note:** Check whether S2S has moved AI strategy weighting into CvDecisionAI [DAI] or retained the city-strategy flavor loop. The tagged-logging memory entries suggest significant AI restructuring.

---

## 3. The Generic Property System

Properties are a generalized "resource on a game object" framework. They can exist on plots, units, cities, players, teams, or the game object itself.

**Core concepts:**

- **Property Sources/Drains:** XML-defined manipulators that generate or consume property value each turn (constant, capped, or percentage-decay).
- **Property Interactions:** One property affects another on the same object (e.g. Health reduces Crime buildup rate).
- **Property Propagators:** Properties spread between adjacent game objects (city-to-plot diffusion, trade-route spread, etc.).

**Where manipulators attach:**

| Attachment point | Object type affected |
|---|---|
| Property definition itself | All objects |
| Civic | Player |
| Unit promotion / unit type | Unit |
| Building | City |
| Technology | Team |
| Terrain / feature / improvement | Plot |

**Practical examples of C2C properties:** Crime, Pollution, Flammability, Disease vectors (as the O&A system property base).

**`bOAType` flag:** If a property is marked with this boolean, the standard `PropertyBuildings` emergence rules are replaced by the Outbreaks & Afflictions system (see §6). This is a full replacement, not an addition.

---

## 4. The Expression System

The Expression System allows conditional XML logic — a tag's value or a check's outcome can depend on a computed expression rather than a hardcoded constant. This enables promotion prerequisites, mission costs, and outcome probabilities to vary with game state.

**Outcome system example (mission cost with property payment):**

```xml
<Action>
  <MissionType>MISSION_NOMAD_DEFENDER</MissionType>
  <bKill>0</bKill>
  <iCost>100</iCost>
  <ActionOutcomes>
    <Outcome>
      <OutcomeType>OUTCOME_NOMAD_DEFENDER</OutcomeType>
      <iChance>100</iChance>
      <UnitType>UNIT_STONE_THROWER</UnitType>
    </Outcome>
  </ActionOutcomes>
</Action>
```

`<iCost>` inside `<Action>` is a gold cost; `<PropertyCost>` is an alternative that draws from a property value. If the unit/city/player cannot pay, the mission is unavailable.

The expression system tutorial is linked from the Modder's Documentation index (no standalone URL recovered — search the thread for "expression system tutorial").

---

## 5. The Combat System (C2C Combat Mod)

> **S2S note:** S2S has substantially reworked the combat model in `CvCombatModel`. The C2C SubCombat / Step I–IV documentation below describes the XML schema that S2S inherits; the underlying engine calculations have been refactored. Verify specific combat attribute behavior against `Sources/CvCombatModel.cpp`.

### 5.1 SubCombat Classes (Step I)

Every unit has one primary `<Combat>` type and can additionally carry multiple `<SubCombatTypes>`. Both primary and sub-combat types behave identically in all game effects:

- Promotion access and availability
- Building training bonuses (XP, free promotions, production modifiers)
- Combat modifiers from opposing units
- Leader trait interactions

**Unit XML:**

```xml
<Combat>UNITCOMBAT_ANIMAL</Combat>
<SubCombatTypes>
  <SubCombatType>UNITCOMBAT_MELEE</SubCombatType>
</SubCombatTypes>
```

**Promotion granting a SubCombat:**

```xml
<SubCombatChangeTypes>
  <SubCombatChangeType>UNITCOMBAT_MELEE</SubCombatChangeType>
</SubCombatChangeTypes>
```

**Free promotions via `<FreetoUnitCombats>`:** A promotion can specify that any unit possessing a given combat class automatically receives a particular free promotion. This enables cascading chains: e.g., a unit earns Combat V → gains Military Police promotion → acquires Law Enforcement subcombat → auto-receives Law Enforcement I free promotion.

**SubCombat removal:** When equipment upgrades obsolete a promotion, dependent subcombats are stripped. Skill promotions that required the stripped subcombat are also removed, but the unit can retrain them without re-earning the XP that originally granted them.

**SubCombat category taxonomy (as designed in C2C; verify what S2S actually uses):**

- **Size:** Fine / Small / Medium / Large / Huge / Colossal
- **Biological genus:** Mammal, Avian, Reptile, Amphibian, Fish, Arthropod, Insect
- **Species:** Hominid, Feline, Ursine, Canine, Pachyderm, Equine, Bovine, Serpent, Swarm, etc.
- **Combat approach:** Footman, Mounted, Vehicle, Operated Construct
- **Standard methods:** Melee, Archery, Gunpowder, Healer, Automaton, Warship, etc.
- **Role categories:** Civilian, Scoundrel, Law Enforcement, Mercenary, Pirate
- **Weapon types (melee):** Club, Spiked Club, Mace, Flail, Small Blades, One/Two-Handed Blade, Dual Blades, Polearms (Short/Long Spear, Staff), etc.
- **Weapon types (ranged):** Blowgun, Crossbow variants, Sling, Bow variants, Javelins, Throwing Axes, Boomerang
- **Armor:** Light/Medium/Heavy Natural, Light/Medium/Heavy, Ultra Heavy, Armorless
- **Shield:** Buckler, Small, Large, Tower

### 5.2 New Promotion Types (Step II — Equipment & Afflictions)

Step II introduced promotion categories beyond vanilla skill lines: **Equipment** promotions (weapon, armor, shield type selections) and **Affliction** promotions (poisons, diseases, critical injuries). The full sub-thread URL for Step II was not recovered; it should be linked from the Modder's Documentation index.

### 5.3 New Combat Mechanics (Step III)

C2C replaced the vanilla single-round-winner system with independent per-round hit rolls and dynamic recalculation. **Key new unit/promotion attributes:**

| Attribute | Unit tag | Promotion tag | Purpose |
|---|---|---|---|
| Pursuit | `iPursuit` | `iPursuitChange` | Reduces opponent withdrawal chance |
| Early Withdrawal | `iEarlyWithdrawal` | `iEarlyWithdrawalChange` | Begin withdrawal attempts at set HP% |
| VS Barbs | `iVSBarbs` | `iVSBarbsChange` | Combat modifier vs. non-animal barbarians |
| Armor | `iArmor` | `iArmorChange` | Damage reduction %; capped at 95% |
| Puncture | `iPuncture` | `iPunctureChange` | Negates opponent Armor by equal amount |
| Dig In | `iDigIn` | `iDigInChange` | Increases fortification bonus per round (max 5) |
| Fortified Collateral Defense | `iFortCollatDef` | `iFortCollatDefChange` | Collateral defense bonus per fortification round |
| Overrun | `iOverrun` | `iOverrunChange` | Reduces defender fortification bonuses |
| Repel | `iRepel` | `iRepelChange` | Defender-initiated non-lethal exit on first hit |
| Fortified Repel | `iFortRepel` | `iFortRepelChange` | Repel increases with fortification rounds |
| Early Repel | `iEarlyRepel` | `iEarlyRepelChange` | Repel triggers at lower HP |
| Unyielding | `iUnyielding` | `iUnyieldingChange` | Counter to Repel and Knockback |
| Knockback | `iKnockback` | `iKnockbackChange` | Attacker-initiated displacement on first hit |
| Early Knockback | `iEarlyKnockback` | `iEarlyKnockbackChange` | Knockback at lower HP threshold |
| Dodge Modifier | `iDodgeModifier` | `iDodgeModifierChange` | Affects opponent hit chance |
| Precision Modifier | `iPrecisionModifier` | `iPrecisionModifierChange` | Affects own to-hit rolls |
| Strength Change | — | `iStrengthChange` | ±1 base strength per promotion |

**Hit chance:** Precision minus opponent Dodge. **Damage:** Base damage × (1 - max(0, Armor - Puncture)/100).

### 5.4 Stamina, Surround, and Stampede (Step IV)

**Stamina / Rage / Fatigue tags:**

| Attribute | Unit tag | Promotion tag | Effect |
|---|---|---|---|
| Strength adj. per round | `iStrAdjperRnd` | `iStrAdjperRndChange` | Positive = Rage; negative = Fatigue |
| Strength adj. per attack | `iStrAdjperAtt` | `iStrAdjperAttChange` | Positive = Rampage; negative = Tires |
| Strength adj. per defense | `iStrAdjperDef` | `iStrAdjperDefChange` | Positive = Determination; negative = Demoralization |
| Withdraw adj. per defense | `iWithdrawAdjperDef` | `iWithdrawAdjperDefChange` | Positive = Reflexes; negative = Frays |
| Fortitude | `iFortitude` | `iFortitudeChange` | Generic affliction resistance |
| Aid | `iAid` | `iAidChange` | Affliction resistance to stack members (highest wins) |
| Endurance | `iEndurance` | `iEnduranceChange` | Counteracts Fatigue/Tires without flipping to Rage |
| Deals Cold Damage | `bDealsColdDamage` | `bMakesDamageCold` / `bMakesDamageNotCold` | Reduces target Dodge/Precision |

**Surround system (requires "Surround and Destroy" game option):**

| Attribute | Unit tag | Promotion tag | Purpose |
|---|---|---|---|
| Unnerve | `iUnnerve` | `iUnnerveChange` | % strength contribution when supporting S&D attack; cap 60% |
| Enclose | `iEnclose` | `iEncloseChange` | Raises the 60% S&D cap cumulatively |
| Lunge | `iLunge` | `iLungeChange` | Primary attacker multiplier on total S&D bonus |
| Dynamic Defense | `iDynamicDefense` | `iDynamicDefenseChange` | Reduces attacker's total S&D bonus |

**Stampede system:**

| Attribute | Boolean flag | Purpose |
|---|---|---|
| Animal Ignores Borders | `bAnimalIgnoresBorders` / `bAnimalIgnoresBordersChange` | Lets animals enter civilized territory |
| Stampede | `bStampede` / `bStampedeChange` / `bRemovesStampede` | Sequential auto-attacks until death or plot cleared |
| Onslaught | `bOnslaught` / `bOnslaughtChange` | Mechanized repeat-attack on same target until damaged |

---

## 6. Outbreaks and Afflictions System

The Outbreaks and Afflictions (O&A) system is activated by the `Outbreaks and Afflictions` game option. It replaces the standard Property emergence behavior for any property marked `<bOAType>1</bOAType>`, using PromotionLines as the scaffolding for disease/poison/injury severity.

**Source thread:** https://forums.civfanatics.com/threads/a-new-modders-guide-to-v38s-c2c-combat-mod-outbreaks-and-afflictions.613839/

### 6.1 Severity Structure

Each affliction is a PromotionLine. Severity levels are Buildings (for cities) or Promotions (for units). One level worsens or improves per turn at most. Building naming convention: `BUILDING_AFFLICTION_<PROPERTY>_<NAME><#>` (e.g. `BUILDING_AFFLICTION_DISEASE_COMMON_COLD1` through `COMMON_COLD10`). Each step represents roughly 10% population infection.

`<iLinePriority>` on the PromotionLine determines severity ranking — higher = worse.

### 6.2 City Outbreak Mechanics

**Exposure sources (each has its own communicability tag):**

| Source | XML tag |
|---|---|
| Trade route cities | `<iTradeCommunicability>` (on buildings) |
| Terrain / feature / bonus in worked tile | `<AfflictionCommunicabilityTypes>` with `<bWorkedTile>1` |
| Bonus in city vicinity | `<bVicinity>1` |
| Bonus accessible via trade network | `<bAccessVolume>1` |

**Outbreak threshold calculation:** Base = `<iOutbreakBase>` from the affliction building, minus Communicability, minus building modifiers (`<AfflictionOutbreakLevelChanges>`), minus tech effects (`<TechOutbreakLevelChanges>`), plus accumulated Tolerance. Chance = (Property value − adjusted threshold) × `<iOutbreakModifier>` / 100.

**Overcome threshold calculation:** Inverse logic — higher threshold = better recovery. Components: `<iOvercomeBase>` + Aid from `<AidRateChanges>` + Tolerance − Communicability + turn-by-turn `<iOvercomeAdjperTurn>`. Chance = (overcome threshold − property value) × `<iOvercomeModifier>` / 100. Success removes the highest-priority affliction building.

### 6.3 Unit Affliction Mechanics

Units check exposure each turn from: terrain/feature/bonus vicinity, afflicted units on same tile, occupied city, weaponized attacks.

**Probability modifiers:**

| Tag | Purpose |
|---|---|
| `<UnitCombatContractChanceChanges>` | Combat-class vulnerability |
| `<TechContractChanceChanges>` | Tech-based resistance |
| `<AfflictionFortitudeModifiers>` | Unit natural resistance |
| `<iWorseningProbabilityIncrementModifier>` | Higher severity = higher infection chance |
| `<iToleranceBuildup>` / `<iToleranceDecay>` | Prior-exposure immunity mechanics |

**Unit overcome:** `<iOvercomeProbability>` + Fortitude + local Aid + `<iOvercomeAdjperTurn>` + `<UnitCombatOvercomeChanges>` + `<TechOvercomeChanges>` + `<iWorsenedOvercomeIncrementModifier>`.

### 6.4 Weaponized Transmission

**Poison/Venom:** `<AfflictOnAttackTypes>` specifies probability, delivery mode (melee/ranged/immediate). Promotions enhance via `<AfflictOnAttackChangeTypes>`. Immediate delivery mid-battle affects remaining combat rounds.

**Critical hits:** Base chance = damage dealt that round × 10 (of 10000). Enhanced by `<iCriticalModifier>` / `<iCriticalModifierChange>`. Selects randomly from qualifying afflictions; follows standard recovery rules.

### 6.5 Spread Control Booleans (on PromotionLineInfos)

- `<bNoSpreadOnBattle>` — no battlefield transmission
- `<bNoSpreadUnitProximity>` — no unit-to-unit tile spread
- `<bNoSpreadUnittoCity>` — no unit→city spread
- `<bNoSpreadCitytoUnit>` — no city→unit spread

### 6.6 Healer Capabilities

`<CureAfflictionTypes>` / `<CureAfflictionChangeTypes>` let units immediately reduce affliction severity on same-tile team units. Cities cannot be directly cured but benefit as resident unit infection levels drop.

### 6.7 Creating a New Affliction: Steps

1. **PromotionLine definition:** Set `<bAffliction>1`, `<PropertyType>`, `<iCommunicability>`, `<iOutbreakModifier>`, `<iOvercomeModifier>`, tolerance tags, and spread restriction booleans.
2. **Property configuration:** Mark with `<bOAType>1`; this disables standard PropertyBuildings emergence for this property.
3. **Building creation (one per severity level):** Set `<PromotionLineType>`, `<iLinePriority>`, `<iOutbreakBase>`, `<iOvercomeBase>`, prerequisite/obsolescence techs. Set `<iCost>` to a negative value and `<iConquestProb>100`. Add penalty tags scaled by population (e.g. `<iHealthPercentPerPopulation>`, `<iHappinessPercentPerPopulation>`, `<iMaintenanceModifier>`, `<iHealRateChange>`, `<iMilitaryProductionModifier>`, `<iFreeSpecialist>` negative).

---

## 7. Size Matters Combat Option

Size Matters makes three unit attributes mechanically meaningful: Combat Quality, Group Volume, and Individual Entity Size. As of version 2.0, ranking up in any category multiplies affected values by 1.5×; ranking down divides by 1.5×.

**Three categories:**

| Category | Range | Baseline | Notes |
|---|---|---|---|
| Combat Quality | 0–10 ranks | Standard (5) | Innate combat skill; lost on upgrade; higher quality slows XP gain |
| Group Volume | 1–13 ranks | Battalion (5) | Unit count; merge 3→1 (rank up), split 1→3 (rank down) |
| Individual Entity Size | 1–9 ranks | Medium (5) | Fixed; larger = more damage per hit but worse Precision/Dodge |

**Affected values:** Strength, Max HP, Asset/Power Value, Cargo, Bombard rates, Work Rate, Revolt Protection.

**GlobalDefines.xml multipliers:**

```xml
<Define>
  <DefineName>SIZE_MATTERS_MOST_MULTIPLIER</DefineName>
  <iDefineIntVal>150</iDefineIntVal>  <!-- 1.5× -->
</Define>
<Define>
  <DefineName>SIZE_MATTERS_MOST_VOLUMETRIC_MULTIPLIER</DefineName>
  <iDefineIntVal>300</iDefineIntVal>  <!-- 3.0× for merged units -->
</Define>
```

**Restrictions:** Criminal, Law Enforcement, and Healer units cannot merge/split. Injured units and units carrying cargo cannot merge/split. XP rate: smaller groups gain +20% XP per rank below Battalion; larger groups lose 20% per rank above.

---

## 8. Improvement Upgrade System

### Standard Upgrade

Improvements accumulate upgrade points while worked (or manned for forts). At threshold, the system checks the primary `<ImprovementUpgrade>` target first, then falls through to `<AlternativeImprovementUpgradeTypes>` if the primary fails qualification.

**XML:**

```xml
<AlternativeImprovementUpgradeTypes>
  <ImprovementType>IMPROVEMENT_FARM</ImprovementType>
  <ImprovementType>IMPROVEMENT_PASTURE</ImprovementType>
</AlternativeImprovementUpgradeTypes>
```

(This tag follows immediately after `<ImprovementUpgrade>`.)

**Qualification checks:** terrain/feature requirements, resource presence, freshwater/coastal access, flatland/hill/peak specification, and crucially — a valid Build definition that the player's tech qualifies for must exist somewhere in XML.

**Freeze mechanic:** If all paths fail, the plot enters "frozen" status. Players unfreeze by toggling the plot in the city worked-plot screen. AI auto-upgrades and routes production from feature destruction to the nearest city.

---

## 9. Multi-Maps and Viewports

Multi-Maps allows the game to run multiple independent maps simultaneously (e.g. a surface map plus a space map). It is selectable as a game option in Custom Game setup, or can be included in scenario files.

**Architecture (from Koshling):**

- Each map is a full CvMap instance. A "viewport" is a proxy window into a map — the active view the renderer shows.
- Map identity is treated as a third coordinate in the entity ID system, not a separate dimension. This required extensive C++ changes across many files.
- Adding a new map requires: one C++ line, a new type entry in `CIV4MapInfo.xml`, a WB map or Python mapscript, and outcome missions on units to move between maps.
- The minimap embedded in Python pages cannot display more than the currently selected map.
- Turns end globally; AI processes all maps each turn.

**Python API (partial):**

```python
bool viewportsEnabled()
int getViewportWidth()
int getViewportHeight()
bool isInViewport(int X, int Y)
void switchMap(int iMap)
```

> **S2S note:** Multi-Maps is documented in the S2S notes as a planned/aspirational feature. The C2C implementation is described by its developers as not well-documented and deeply entangled in the C++ codebase. The memory entry about graphics paging masking symbol bugs is directly related to viewport/paging interactions.

---

## 10. Building and Unit Costs

**Cost reference (from ls612's table — tech column → base production cost):**

- Column 30 → ~80 production
- Column 122 → ~41,800 production
- Values scale between these points

**Wonder multipliers:**

- National Wonder: 4× the normal building cost for that tech column
- World Wonder: 8× the normal building cost
- Project: 12× the normal building cost

---

## 11. Python Integration Points

**Key documented hooks:**

- `onCivicChange` event: fires when a civ switches civics; the C2C Python class handles all side effects. Documented in the thread "C2C - Civics class and onCivicChange event."
- Map scripts: "Adding new C2C features, bonuses and the like to all map scripts" — when you add a new bonus/feature, map Python scripts that generate random maps also need updating.
- Pathing exposed to Python: multiple sub-threads cover this.

**BUG mod options:** Adding a new toggle/slider to the in-game BUG options menu requires edits to the BUG XML config, the options-tab Python, and translatable text keys. The C2C process is described in "Adding an option to BUG" (linked from Modder's Documentation). S2S has its own `add-bug-option` skill that covers this workflow.

> **S2S note:** Civ4 Python is **Python 2.4** — new Python here must conform to 2.4 (no `with`, no `a if c else b` conditional expressions, no `except X as e`, no `any()`/`all()`, etc.).

---

## 12. Art and Asset Pipeline

- **Button images:** 64×64 pixel DDS files with alpha channel. Standard workflow: crop source image → layer with provided border frame → erase white border regions → convert to DDS with DXTBmp.
- **FPK packaging:** Use PakBuild to compress art folders into FPK archives. S2S uses a separate FpkBuilder tool.
- **Movies:** Documented in the Modder's Documentation index but no content recovered.
- **Button templates:** Separate templates exist for Build-up buttons, Status promotion icons, Trait icons, Missionaries, Buildings, and Religion icons.
- **Debugging graphics:** A sub-thread covers common graphics debug workflows (search Modder's Documentation thread for "Debugging graphics problems").

---

## 13. Revolutions System

The Revolutions system tracks civilization stability and can trigger city splits, regime changes, or barbarian spawns when stability thresholds are crossed.

**Architecture:** The system spans three layers — DLL (C++, core logic), Python (specific mechanics), and XML (partial configuration via `<iRevolutionIndexModifier>` on buildings). The code is described by Thunderbrd as "so split up between dll and python that its tough to follow as a coder," making modifications difficult.

**Key XML tag:** `<iRevolutionIndexModifier>` on buildings adjusts the city's revolution index, affecting outbreak likelihood. Negative values stabilize; positive values destabilize.

The "No Revolutions" game option disables the entire system. A dedicated discussion thread exists at https://forums.civfanatics.com/threads/c2c-revolutions-rev-discussion-thread.485361/ but contains no comprehensive modding specification.

> **S2S note:** Verify whether S2S retains C2C's Revolution mod or has replaced it.

---

## 14. Source Control Notes (for S2S)

The C2C documentation referenced SVN conventions. S2S uses Git. The general advice translates: update/pull before committing, diff your changes before submitting, and test XML validation. S2S has its own build infrastructure (`Tools/_Build.ps1`, `MakeDLL*.bat`, FastBuild).
