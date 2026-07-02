# Unit skills — glossary

The catalogue of a unit's **innate boolean abilities** — the `blitz`/`amphibious` vein. This is the **glossary**
(the specific namings); the **system** is the [json spec](json.md) §8.

> **The classification model (owner).** A **unit** carries three blocks. The **operative test: *can a promotion
> grant it?*** — yes → a **`skill`** (you can train into `blitz`); no, it comes only from the unit's *type* (you
> can't promote your way to firing guns — a swordsman must *upgrade* to a rifleman) → a **`tag`**; and if it's
> **transient** (fired, then expires) it is neither — it is a **`state`**. The three, by lifecycle/mutability:
> — **`skills`** — **mutable**: gained/lost over the unit's life (promotions); the classic abilities (`blitz`,
>   attack-over-river, …) catalogued in *this* file;
> — **`state`** — **transient**: fired, counted down, then over (`paralyze`/immobilise/…). *Never a first-class
>   concept* — historically faked via **pseudo-promotions** + **Python event handlers** (e.g. `paralyze` is a
>   promotion firing `setImmobileTimer` from an event); the block **formalizes those hacks** — greenfield, with no
>   clean legacy field to migrate from;
> — **`tags`** — **derived from the unit's *type***, re-set only at **creation — and upgrade counts as creation**
>   (a `mounted` horseman re-derives its tags on upgrade to a helicopter, which isn't mounted). No other change
>   (*a worker never spontaneously becomes military*). Overlapping, accounting-only membership
>   (`military`/`civilian`/`worker`/`spy`/criminal-types) read by `IS_<TAG>` predicates, where those memberships
>   are *defined*.
>
> The **empire** counterpart to unit `skills` is **`capabilities`**. This file is the `skills` glossary only;
> `state`, `tags`, and `capabilities` get their own sibling glossaries (json.md §8 = the model).

> **`capabilities` = empire, `skills` = unit — the rule, and the curator fold is DONE (verified 2026-06-29).** The
> unit-level ability block is **`skills`**; the empire-level block is **`capabilities`**
> ([capabilities.md](capabilities.md)). The curators already emit accordingly — `curate_unit.py` / `curate_promotion.py`
> / `curate_unitcombat.py` each write `out["skills"] = caps` (the internal `CAP_*` table names are legacy XML-bool
> spellings, not the output key) — and the data confirms it: **every unit / promotion / unit-combat ability block is
> `skills`, with ZERO `capabilities` blocks anywhere in `Assets/Data`** (`capabilities` is reserved for the empire
> block, which is not yet curated). Do **not** re-muddy the rule with migration history — there is no pending rename.

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
| `canPassPeaks` | can move through peak tiles (renamed from legacy `bCanMovePeaks` / prior `canMovePeaks` — owner ruling 2026-07-02: **dual-plane, same name as the empire capability**; a promotion grants the unit skill, `TECH_MOUNTAINEERING` grants it empire-wide as `capabilities.canPassPeaks`; effective check = skill ∪ capability, see [capabilities.md](capabilities.md)) |
| `cannotMergeSplit` | cannot merge with / split from other units |
| `enemyRoute` | can use enemy (rival) roads |
| `excile` | an investigation / criminal **ability** (legacy spelling, from `iExcileChange`) — distinct from the `exile` *unit* in the criminal-type tags ([tags.md](tags.md)) |
| `firstStrikeImmune` / `immuneToFirstStrikes` | immune to first strikes |
| `flatMovementCost` | every tile costs 1 movement |
| `fliesToMove` | flies to move (grant/revoke, §4) |
| `found` | can found a city (settler) |
| `freeDrop` | a free paradrop (paratrooper drop) action |
| `goldenAge` | can trigger a golden age |
| `greatGeneral` | is a great general |
| `hiddenNationality` | hides its owning civilization — a **skill** (mutable, promotion-grantable: `PROMOTION_PROUD_PIRATE` grants it via `iHiddenNationalityChange`), **not** the gate for the criminal-type `outlaw` [tag](tags.md) (owner 2026-06-23) |
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
| `tradable` | can be traded with another empire — consolidates the legacy `militaryTrade` + `workerTrade` (curator fold) |
| `unlimitedException` | exempt from instance-cap limits |
| `upgradeAnywhere` | can upgrade regardless of location |
| `zoneOfControl` | exerts a zone of control |

### Per-type keyed (`skill: { TYPE: true }`)

| skill | what it does |
|---|---|
| `terrainDoubleMove` | double movement on the listed `TERRAIN_*` |
| `featureDoubleMove` | double movement on the listed `FEATURE_*` |
| `trapImmunity` | ❌ **DEAD** (traps removed) — drop |
| `trapTarget` | ❌ **DEAD** (traps removed) — drop |
| `trapSetWith` | ❌ **DEAD** (traps removed) — drop |
| `targets` | preferentially targets the listed `UNITCOMBAT_*` |
| `collateralImmune` | immune to collateral damage from the listed `UNITCOMBAT_*` |
| `unitTargets` | specifically targets the listed `UNIT_*` |

---

## 2. Validated from engine (grounded this pass)

Every previously-`⚠` skill was traced to its engine consumption — **all LIVE, none dead, none unclear**. Meanings
are grounded in the consuming code (high confidence unless noted), not general knowledge.

| skill | what it does |
|---|---|
| `counterSpy` | espionage counter-agent — cuts enemy spy-mission success on/near its plot, intercepts spies (+XP) |
| `dcmAirBomb` | DCM air-strike capability (5 legacy tiers → a tier count); each tier gates an air-strike mission |
| `dcmFighterEngage` | can fly the DCM fighter-intercept (FEngage) mission (option-gated) |
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
| `paralyze` | ⚠ **not a skill** — a transient unit **state** (immobilise, fired by an event); **migrate to [state.md](state.md)** and remove from skills |
| `pillageMarauder` | gains gold from pillaging / combat pillage |
| `rBombardDirect` | (unit-combat) exempt from first-defender deprioritisation in ranged-bombard targeting |
| `rBombardForceAbility` | lets a defensive-only unit still ranged-bombard (overrides `isOnlyDefensive`) |
| `stampede` | can chain attacks after a kill while more defenders share the plot (grant/revoke, §4) |
| `stateReligion` | buildable only in a city that has the player's state religion |
| `stealthDefense` | stealth ambusher — first-strike vs attackers, suppresses their move cost (option-gated, `COMBAT_WITHOUT_WARNING`) |
| `triggerBeforeAttack` | ❌ **DEAD** — traps are a removed mechanic (owner); drop |

> **Curator-gap claim — verified FALSE.** The minion flagged `bOnslaught`/`bGatherHerd`/`bTriggerBeforeAttack` as
> silently dropped by `curate_unit.py`, but grepping `Assets/XML` shows they appear only in the *schema* and
> `CIV4PromotionInfos.xml` — **never in a unit record** (and the curator's COVERAGE check is clean). No unit
> authors them; the promotion delta variants (in `curate_promotion.py`) are the only authoring, and those are
> handled. So there is **no gap** — "CvUnitInfo has the member" ≠ "units author it." (Traps are also dead — the
> trap family drops regardless.)

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

> **⏳ Needs its own spec — the unit-category system:** the full category list, how membership is authored
> (overlapping, opt-in), and the `IS_<TAG>` predicate surface. Captured here only as the home the `military*`
> flags fold into; the tags themselves are not skills.

---

## 3b. ✅ VERIFIED — the effective-skill parity run (2026-07-02) + the composition rules

Owner method: **direct HTTP parity** — `/computed/unitSkills` (the engine's per-unit COMPOSITE getters —
`isBlitz()` etc., unit-info + promotion + unitcombat counts folded) diffed against the offline derivation
(unit JSON `skills` ∪ combat-class JSON `skills` ∪ held promotions' JSON `skills`). **365,474 effective-skill
facts → 5 residual, all attributed** (3 = the kamikaze composition below on units holding FIRE_SHIP-class promos;
2 = a SERIALIZED stale ability count whose source promo/class is gone — the accepted dropped-event-state class).
Static sweeps: building `attributes` 2,266 XML facts EXACT; unit/promotion/unitcombat skill blocks **0 LOST**.

**The derivation rules a consumer must know (engine compositions that survive as CODE, not data):**
- a unit's combat classes = **`identity.base.combatClass` (the PRIMARY — XML `Combat`) + `identity.combatClasses`
  (the subs) + promotion-granted (`skills.unitCombats`) − promotion-removed**; every class's `skills` contribute
  (the engine applies a ~30-field unitcombat ability battery, `CvUnit.cpp:18400-18474`). Missing the PRIMARY was
  the sniper-immunity find (UNITCOMBAT_STRIKE_TEAM).
- **`fliesToMove` ⇒ `amphib` + `river` + `canMoveImpassable`** (`CvUnit.cpp:12830/14949/14965` fold
  `canFliesToMove()`).
- **`kamikaze ≠ 0 ⇒ suicide`** (`isSuicide` folds `getKamikazePercent()` — a modifier-family magnitude driving a
  skill-plane composite).
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

> **⏳ Future (owner) — make skills & capabilities GRANT-ONLY.** These grant/revoke pairs are slated to be
> transformed so an ability can only be **granted**, never revoked via a `false` — removing this special case.
> Parked for later.

---

## See also
- [json.md](json.md) §8 — the **system**: what a skill is, and the unit-`skills` vs empire-`capabilities` split.
- [naming.md](naming.md) — the sibling glossary (infotype id prefixes); same spec-defines-the-system,
  glossary-lists-the-namings split.
- [capabilities.md](capabilities.md) — the **empire `capabilities`** glossary (the sibling; started).
