# Unit skills — glossary

The catalogue of a unit's **innate boolean abilities** — the `blitz`/`amphibious` vein. This is the **glossary**
(the specific namings); the **system** is the [json spec](json.md) §8.

> **Every skill key is a runtime-generated `SKILL_*` info** ([the classification-infos registry](json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities),
> [json.md §8](json.md)): minted at load from the union of authored keys, resolved onto per-entity bitsets, read by
> the getters as O(1) id bit tests — never per-call string lookups.
>
> **`capabilities` = empire, `skills` = unit — the rule.** The
> unit-level ability block is **`skills`**; the empire-level block is **`capabilities`**
> ([capabilities.md](capabilities.md)). The curators emit accordingly — `curate_unit.py` / `curate_promotion.py`
> / `curate_unitcombat.py` each write `out["skills"] = caps` (the internal `CAP_*` table names are legacy XML-bool
> spellings, not the output key): **every unit / promotion / unit-combat ability block is
> `skills`** (`capabilities` is reserved for the empire block). There is no pending rename.

> **⚖ REPRESENTATION — pure boolean enablers, so the shape differs by carrier.** A skill is a
> **pure boolean ENABLER** ("can walk over river", "can pass peaks") — the direct mirror of empire `capabilities`,
> carrying no value; it cannot be `false` (absent ⇒ not held). So a **UNIT authors `skills` as an ARRAY OF STRINGS**
> (`["pillage","blitz"]`), never `{name:true}`. The **object form** (`{name:true|false}`) is used **only where
> revoke is real — a PROMOTION** granting *or removing* a skill (the grant/revoke plane, §4). Anything that carries a
> value is not a skill (§1's per-type keyed abilities → modifier families).

**Grounding:** entries come from the curator capability tables (`CAP_BOOL`/`CAP_PAIR`/`CAP_COUNT`/`CAP_LIST` in
`curate_promotion.py`/`curate_unit.py`/`curate_unitcombat.py`) and owner rulings. Meanings are **not** asserted
from general game knowledge — anything unconfirmed sits in §2, not §1.

---

## 1. Validated skills

Owner-ruled or curator-grounded with a clear meaning.

| skill | what it does |
|---|---|
| `alwaysHeal` | heals every turn |
| `alwaysHostile` | always treated as hostile |
| `alwaysInvisible` | always invisible to enemies |
| `amphib` | attacks over a river, or from a cargo ship / water, without penalty |
| `animalIgnoresBorders` | animal unit ignores border restrictions |
| `assassin` | can attack from the same plot (distinct from `arrest`, a separate mechanic) |
| `attackOnlyCities` | can only attack cities (grant/revoke, §4) |
| `barbCoExist` | can coexist with barbarians |
| `blendIntoCity` | becomes invisible while inside a city |
| `blitz` | multiple attacks per turn |
| `canLeadThroughPeaks` | can lead a stack through peak tiles |
| `canMoveAllTerrain` | can move through any terrain |
| `canMoveImpassable` | can move through impassable terrain |
| `canPassPeaks` | can move through peak tiles (**dual-plane, same name as the empire capability**: a promotion grants the unit skill, `TECH_MOUNTAINEERING` grants it empire-wide as `capabilities.canPassPeaks`; effective check = skill ∪ capability, see [capabilities.md](capabilities.md)) |
| `cannotMergeSplit` | cannot merge with / split from other units |
| `celebrity` | grants the city it's in a happiness bonus (`iCelebrityHappy != 0`, folded to a boolean skill — the numeric amount is dropped, `CvCity` scans for celebrity-skilled units and awards the happiness itself); authored by `PROMOTION_INSPIRE3`/`6`/`9` |
| `enemyRoute` | can use enemy (rival) roads |
| `excile` | an investigation / criminal **ability** (legacy spelling, from `iExcileChange`) — distinct from the `exile` *unit* in the criminal-type tags ([tags.md](tags.md)) |
| `firstStrikeImmune` / `immuneToFirstStrikes` | immune to first strikes |
| `flatMovementCost` | every tile costs 1 movement |
| `fliesToMove` | flies to move (grant/revoke, §4) |
| `found` | can found a city (settler) |
| `freeDrop` | a free paradrop (paratrooper drop) action |
| `goldenAge` | can trigger a golden age |
| `greatGeneral` | is a great general |
| `hiddenNationality` | hides its owning civilization — a **skill** (mutable, promotion-grantable: `PROMOTION_PROUD_PIRATE` grants it via `iHiddenNationalityChange`), **not** the gate for the criminal-type `outlaw` [tag](tags.md) |
| `hillsDoubleMove` | double movement on hills |
| `ignoreBuildingDefense` | ignores building-based city defense |
| `ignoreNoEntryLevel` | ignores no-entry-level restrictions (grant/revoke, §4) |
| `ignoreTerrainCost` | ignores terrain movement cost |
| `ignoreZoneOfControl` | ignores enemy zones of control (grant/revoke, §4) |
| `inquisitor` | can remove a religion from a city (inquisition) |
| `investigate` | can perform investigation actions |
| `mechanized` | → **tag**, not a skill — a tech/equipment-class membership (like `gunpowder`); see §3 |
| `noBadGoodies` | never gets bad goody-hut results |
| `noCapture` | cannot be captured |
| `noDefensiveBonus` | receives no terrain/city defensive bonus |
| `noNonOwnedCityEntry` | cannot enter cities it does not own |
| `noSelfHeal` | cannot heal itself |
| `nukeImmune` | immune to nuclear weapons |
| `onlyDefensive` | can only fight defensively |
| `passage` | non-combat units enter foreign land without granting military passage |
| `pillage` | can pillage improvements |
| `pillageEspionage` | pillages for espionage points |
| `pillageOnMove` | pillages when it moves |
| `pillageOnVictory` | pillages on winning combat |
| `pillageResearch` | pillages for research |
| `renderBelowWater` | rendered below the waterline (graphics) |
| `rivalTerritory` | can enter rival territory |
| `river` | attacks over a river without penalty (the river-only subset of `amphib`) |
| `sabotage` | can perform sabotage |
| `stealPlans` | can steal plans (espionage mission) |
| `suicide` | destroyed after attacking |
| `tradable` | can be traded with another empire. **ONE key** — a unit is tradable or it is not. ⛔ The legacy `workerTrade` / `militaryTrade` pair named the DEAL SLOT, not the unit (279 of its 678 `militaryTrade` units are not military at all — merchants, scouts, pack animals), so both fold onto this. **WHICH slot a trade goes through is filtered at the deal case** on the unit's own `civilian` / `military` [tag](tags.md), never by a second skill |
| `unlimitedException` | exempt from instance-cap limits |
| `upgradeAnywhere` | can upgrade regardless of location |
| `zoneOfControl` | exerts a zone of control |

### The HIDE-AND-SEEK METHODS are skills

The way a unit hides — `camouflage` · `cloaked` · `disguised` · `navalDisguise` · `political` · `size` ·
`submarine` · `captive` · `stealth` · `submerged` · `invisible` — is a **skill**, by the §0 test: **a promotion
can grant one**, and *optical camouflage* is exactly that. Pure boolean enablers like every other skill; the
STRENGTH is the `hideAndSeek.concealment` magnitude beside them ([json.md §9](json.md), [vision.md §4](vision.md)),
and a seeker's `hideAndSeek.detection` entry names the method it answers as `{unit: HAS_<SKILL>}`.

⛔ **They are NOT [tags](tags.md).** A tag says what a unit IS; a method says how it HIDES — and the carrier
question is settled by the data, not by taste: **73 promotions author a method**, which a tag cannot hold
because tags are not promotion-grantable. ⚑ **`submarine` is the case that shows both planes at once** — it is a
genuine identity TAG *and* carries the method SKILL, because a surfaced submarine is not hidden.

### Per-type keyed abilities are NOT skills

**A skill is a pure boolean ENABLER** (§0 / json.md §8) — it carries no value. An ability **keyed by a type** carries a
value (*which* type), so it is not a skill; it lives in a modifier family:

| ability | new home | what it does |
|---|---|---|
| `targets` | combat — `strength.unit.targets.{UNITCOMBAT_*}` | preferentially targets those combat classes (this is what defines flanking — narrow per-target, "cannot be fucked with" granularity) |
| `unitTargets` | combat — `strength.unit.unitTargets.{UNIT_*}` | targets those specific units |
| `defenders` | combat — `strength.unit.defenders.{UNITCOMBAT_*}` | is a valid target for attackers of those combat classes |
| `terrainDoubleMove` / `featureDoubleMove` | movement — `movement.unit.{terrain\|feature}.{TYPE}.percent` | **HALF MOVEMENT COST on that terrain/feature while the promotion is held** — so it is an ordinary keyed movement modifier (`-50`), never a boolean. ⛔ It is NOT a skill in any form: a skill is a pure boolean enabler carrying no value, and this carries both a TARGET and a MAGNITUDE |
| `trapImmunity` / `trapTarget` / `trapSetWith` | ❌ **DEAD** (traps removed) — drop |

**The one that STAYS a skill: `collateralImmune`** — its legacy per-source keying (`UNITCOMBAT_SIEGE`/`ASSAULT_MECH`/
`ROBOT`, all the siege variant, never mounted-flanking) **collapses to one boolean** (immune to the siege-variant
collateral; the narrow granularity is deliberately not preserved). So it is a pure enabler → an ordinary `skills`
string. `flankImmune` is not needed (siege units are the flankable ones). (Collateral has two flavours: flanking —
mounted vs siege — and siege/ranged.)

---

## 2. Validated from engine

Each skill below is traced to its engine consumption — **all LIVE unless marked**. Meanings are grounded in the
consuming code (high confidence unless noted), not general knowledge.

| skill | what it does |
|---|---|
| `counterSpy` | espionage counter-agent — cuts enemy spy-mission success on/near its plot, intercepts spies (+XP) |
| `dcmFighterEngage` | can fly the fighter-intercept (FEngage) mission (option-gated). ⚠ The `dcm` prefix is MOD PROVENANCE on a live mechanic, not part of what it does -- fighter engage is the airplane ranged attack and stays; the key wants renaming |
| `defenders` | per-`UnitCombat` list — unit is a valid target for attackers of those combat types (+ AI value) |
| `defenseOnly` | stackable count feeding `isOnlyDefensive()` (with the static `onlyDefensive` bool) — blocks initiating attacks |
| `defensiveVictoryMove` | free move after winning a defensive battle |
| `destroy` | can run MISSION_DESTROY (halves an enemy city's production progress) |
| `food` | food-production unit — the city converts food surplus into this unit's production |
| `gatherHerd` | gates animal-unit merging |
| `healsAs` | (unit-combat) acts as a healer for its combat type; drives AI healer demand |
| `noInvisibility` | cancels a unit's invisibility (option-gated, `COMBAT_HIDE_SEEK`) |
| `noNonTypeProdMods` | suppresses domain/combat/era/research production modifiers when building this unit |
| `offensiveVictoryMove` | expends a full move point after a successful attack |
| `oneUp` | ❌ **DEAD?** — believed unused; possible entertainer city-revolt-reduction use (verify); else drop |
| `onslaught` | can chain attacks in a turn after a no-damage kill while defenders remain |
| `pillageMarauder` | gains gold from pillaging / combat pillage |
| `stampede` | can chain attacks after a kill while more defenders share the plot (grant/revoke, §4) |
| `stateReligion` | buildable only in a city that has the player's state religion |
| `stealthDefense` | stealth ambusher — first-strike vs attackers, suppresses their move cost (option-gated, `COMBAT_WITHOUT_WARNING`) |
| `triggerBeforeAttack` | ❌ **DEAD** — traps are a removed mechanic; drop |

> **No curator gap for `bOnslaught`/`bGatherHerd`/`bTriggerBeforeAttack`:** they appear only in the *schema* and
> `CIV4PromotionInfos.xml` — never in a unit record — so the promotion delta variants (`curate_promotion.py`) are
> the only authoring, and those are handled. "CvUnitInfo has the member" ≠ "units author it."

---

## 3. Not skills — the `military*` flags fold into `tags`

The legacy `military*` flags aren't abilities; they're **classification/counting** flags, and they belong to the
**`tags`** block — *not* this skills glossary. `tags` is **overarching, overlapping**
classifications a unit can hold several of at once — **role/type** (`military`, `civilian`, `worker`, `spy`, the
three hidden-nationality "criminal-type" TB unit types) and **tech/equipment class** (`gunpowder`, `mechanized`,
…). A unit commonly holds several, and they grow on upgrade: a swordsman is `military`; upgrade it to a rifleman
and it's `military` **and** `gunpowder`. The block is **purely for accounting** — it holds *only* membership
("what type of unit this is"), nothing else: **no behaviour, no modifiers**. The `IS_<TAG>` predicates that read
it do the counting and gating. Its opt-in rule (a unit
explicitly carries a group, so a non-combatant like a criminal is never auto-counted as military — the historic
AI bug) lives there, not here.

The three legacy `military*` flags all resolve via the **`military` category** / `IS_MILITARY` predicate —
verified against the engine (`militarySupport()` routes upkeep into the military pool *and* drives the military
count/cap; `militaryHappiness` is the count source for `getMilitaryHappiness`; `militaryProduction` gates the
city military-production bonus):

| legacy flag (data count) | resolution |
|---|---|
| `militaryHappiness` (1007) | **DROP** — happiness modifier counts `IS_MILITARY` units (`unit: IS_MILITARY`) |
| `militaryProduction` (1325) | **DROP** — production engine applies `buildRate.<scope>.military` to `IS_MILITARY` units |
| `militarySupport` (1276) | **DROP** — its real job *is* `IS_MILITARY`; "military upkeep" is just the pool those units feed |

(The legacy sets differed — 1007/1325/1276 — so unifying them onto one `IS_MILITARY` is a deliberate behaviour
change, expected to show in the shadow, not a bug.)

> **The unit-category system's home is [tags.md](tags.md)** ([json.md §8](json.md) is the model): the category
> list, the overlapping opt-in membership, and the `IS_<TAG>` predicate surface. The `military*` flags fold into
> the `military` tag there; they are not skills.

---

## 3b. The effective-skill composition rules

A unit's EFFECTIVE skills are the engine's per-unit COMPOSITE getters (`isBlitz()` etc., unit-info + promotion +
unitcombat counts folded). The offline derivation is
unit JSON `skills` ∪ combat-class JSON `skills` ∪ held promotions' JSON `skills`; the route that served the
engine-side oracle is gone, so confirming a composite means emitting it on the spine.

**The derivation rules a consumer must know (engine compositions that survive as CODE, not data):**
- a unit's combat classes = **`identity.base.combatClass` (the PRIMARY — XML `Combat`) + `identity.combatClasses`
  (the subs) + promotion-granted (`skills.unitCombats`) − promotion-removed**; every class's `skills` contribute
  (the engine applies a ~30-field unitcombat ability battery, `CvUnit.cpp:18400-18474`). Missing the PRIMARY was
  the sniper-immunity find (UNITCOMBAT_STRIKE_TEAM).
- **`fliesToMove` ⇒ `amphib` + `river` + `canMoveImpassable`** (`CvUnit.cpp:12830/14949/14965` fold
  `canFliesToMove()`).
- **`kamikaze ≠ 0 ⇒ suicide`** (`isSuicide` folds `getKamikazePercent()` — a modifier-family magnitude driving a
  skill-plane composite).
- **the `missile` [tag](tags.md) ⇒ `suicide`** — a missile is expended by being used, so kill-on-use is
  what the tag MEANS rather than a second fact to author beside it. ⚑ Like the domain tags, it reaches a unit
  through its combat classes (`UNITCOMBAT_MISSILE`/`BALLISTIC`), so a unit whose own block lists only `military`
  still holds it — read the FOLDED set, never the authored line.
- **`defenseOnly` (the stackable count) feeds `onlyDefensive`** (the composite) — two names, one verdict.
- **`noCapture` folds a RUNTIME rule** (`!canAttack()` ⇒ uncapturable, `CvUnit.cpp:11031`) — the data half is the
  flag+count only.
- **Negative count-abilities are REVOKES** (`iAssassinChange=-1` on PROMOTION_WANTED takes assassin away) — the
  curators now emit `false` (the CAP_PAIR revoke shape); collapsing every nonzero to `true` silently inverted
  revokes (the THUG-as-assassin find). ⚠ Bool-collapse still cannot express count ARITHMETIC (a −1 cancelling one
  of two +1s); no live case exists — revisit if one appears.

## 4. Grant / revoke

A few skills are authored as **add/remove pairs** — `true` grants, `false` revokes (a promotion can take an
ability *away*): `stampede`, `attackOnlyCities`, `ignoreNoEntryLevel`, `ignoreZoneOfControl`, `fliesToMove`.

> **Revoke is real — this is the ONE place the object form (`{name:true|false}`) is used** ([json.md
> §8](json.md)): a skill is otherwise a pure boolean enabler authored as an array of strings (§0), never
> `{name:true}`, because it cannot be `false` — absent already means not held. The grant/revoke pairs above are
> the sole exception, and collapsing them to grant-only is the historical bug, not a simplification: it silently
> inverted a real revoke into a grant (the THUG-as-assassin find, §3b) — `iAssassinChange=-1` on
> `PROMOTION_WANTED` takes `assassin` away, and reading that as `true` gave the unit an ability it had just lost.
> The curator (`curate_promotion.py`'s `CAP_PAIR` table) emits `false` for exactly this reason.

---

## See also
- [json.md](json.md) §8 — the **system**: what a skill is, and the unit-`skills` vs empire-`capabilities` split.
- [naming.md](naming.md) — the sibling glossary (infotype id prefixes); same spec-defines-the-system,
  glossary-lists-the-namings split.
- [capabilities.md](capabilities.md) — the **empire `capabilities`** glossary (the sibling).
