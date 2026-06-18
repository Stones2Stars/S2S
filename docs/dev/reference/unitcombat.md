# UnitCombat (`CvUnitCombatInfo` / `UnitCombatTypes`)

**Short answer to "is this core Civ4 or another mod addition?": both — and that's the whole
story.** The *concept* (a unit's combat class) is core Civ4. The *fat info class* it has become
in S2S/C2C is a mod expansion — a deliberate one, announced in the class's own header comment:
`// AIAndy: A proper info class for unit combat types`. That phrase is the tell: it was *not* a
proper info class before; it was a label.

## What it is in vanilla Civ4 (BtS) — a thin, core axis

A **unit combat type** is the *combat class* of a unit: `UNITCOMBAT_MELEE`, `UNITCOMBAT_ARCHER`,
`UNITCOMBAT_MOUNTED`, `UNITCOMBAT_GUN`, `UNITCOMBAT_ARMOR`, `UNITCOMBAT_NAVAL`, `UNITCOMBAT_AIR`,
`UNITCOMBAT_SIEGE`, … (~8–10 in BtS). In vanilla it is one of the most *referenced* enums in the
combat system, but `CvUnitCombatInfo` itself was almost empty — essentially `Type` +
`Description`. The class was a **category label**; the behaviour lived on the things that *point
at* the class:

- **Promotion gating** — a promotion lists the combat classes it can be taken by
  (`PromotionInfo.UnitCombats`): "Cover is available to Archery and Gunpowder units."
- **Vs-class combat bonuses** — `unitCombatModifier(class)`: "+25% vs Melee" (granted by
  promotions / buildings / units). Consumed in the combat math (`CvUnit.cpp:11901/11917`).
- **Free experience / free promotions per class** — traits/civics/buildings grant "all Mounted
  units get +2 XP" or "free Combat I to Gunpowder."
- **AI** — counter-unit selection and threat assessment pivot on combat class.

So in vanilla: **core mechanic, thin data.** A unit had exactly **one** combat type
(`UnitInfo.UnitCombat`).

## What S2S/C2C did to it — the "greatness"

Three compounding expansions turned the label into a sprawling system:

1. **A "proper info class" (~150 fields).** `CvUnitCombatInfo` (`Sources/CvUnitCombatInfo.h`)
   now carries nearly the *entire `CvPromotionInfo` effect vocabulary* directly on the class:
   `combatPercent`, `cityAttack/DefensePercent`, `withdrawalChange`, `firstStrikes`, the heal
   family, `movesChange`/`moveDiscount`, vision/invisibility intensity arrays, terrain/feature
   attack/defense/work modifiers, `unitCombatChangeModifiers` (vs-class), size-matters fields,
   `KillOutcomeList` + action `OutcomeMission`s (see [CvOutcome](CvOutcome.md)),
   `PropertyManipulators`, on/notOnGameOptions, and more. In effect, a combat class is now **a
   free promotion every member unit is born with.** The class is a near-mirror of
   [Promotion](../plans/building-cascade-conversion.md) (same families).

2. **Many-to-many membership.** A unit no longer has just one combat class. It has a *primary*
   (`CvUnit::getUnitCombatType()` → `UnitInfo.UnitCombat`) **plus a set** of additional classes,
   queried with `isHasUnitCombat(eClass)` and iterated across all classes
   (`CvUnit.cpp:17633`, `19302`, `20371`). Each class a unit *has* contributes its bundle. So a
   combat class is a stackable **tag with effects**.

3. **Type proliferation — the part to actually distrust.** Item 1 (AIAndy's *framework*) is a
   defensible design; the bloat is what came **after** it. Base
   `Assets/XML/Units/CIV4UnitCombatInfos.xml` already defines **636** classes; with modules merged
   it is **~981** (vanilla: ~10). And only **~230 are on any unit** — roughly **77% (≈751) are
   attached to no unit at all**: a large dead/vestigial tier, much of it from since-purged
   TB-mega-mod content. The combat-class enum became a general-purpose unit-tag dimension and then
   mostly silted up.

Net: a **core axis (the combat class)** plus **a defensible mod-built effect framework**, buried
under **a mostly-dead proliferation of ~981 classes**. The friction is the third part, not the
first two — keep the framework, shed the silt.

## How it is consumed (today)

- **Bundle application:** a unit iterates the combat classes it `isHasUnitCombat`, and each
  class's `CvUnitCombatInfo` contributes its effects (heal rules `CvUnit.cpp:19302`, movement
  characteristics `:20371`, era gating `:17633`, merged kill/action outcomes `:3630/:9290`, …).
- **Vs-class modifier:** `CvUnit::unitCombatModifier()` adds the attacker/defender's
  per-class bonuses during combat (`CvUnit.cpp:11901`).
- **Promotion gating / free promotions / free XP:** other entities key *by* combat class.
- **Outcomes & properties:** kill/action `CvOutcomeList`s and `PropertyManipulators` ride on the
  class (e.g. subdue/capture/butcher outcomes for animal classes).

## Relevance to the #428 JSON migration

UnitCombat is on the remaining heavy-entity list. When it migrates it is a **source/enabler**
(§0 ontology — like a trait or a promotion): it deposits a unit-stat modifier bundle onto every
unit that *has* the class. Two consequences:

- **Shared vocabulary with Promotion.** Because the field surface is a near-clone of
  `CvPromotionInfo`, the two should share the cascade's modifier-family definitions (combat,
  heal, movement, vision, terrain/feature modifiers, …) rather than inventing parallel ones.
  Do Promotion and UnitCombat together (or back-to-back) so the vocabulary is reconciled once.
- **Membership is the enabler axis; the bundle is the modifier side.** "Which units have this
  class" (many-to-many) is the availability/tag relation; the ~150 fields are the modifier
  deposit. `unitCombatModifier` (vs-class) is a *keyed-by-combat-class* effect owned by the
  modifying source (promotion/building/unit/trait), resolved per the stay-vs-invert rule.
- Expect dead/vestigial fields among the 150 (the Trait pass found this pattern); verify each
  against the live `CvUnit` consumer before carrying it.
- **Most of the *classes* are dead, not just fields.** ~77% (≈751 of ~981) sit on no unit.
  Selective curation (carry only what the cascade/consumers need) + the post-migration purge
  should shed that tier rather than port ~981 near-clones — but verify per class first (a handful
  may be referenced only by a promotion or another unit-combat, not directly by a unit).

## How it actually works — combat-class audit (2026-06-14)

A 4-agent audit (wf_e1bffcc4) traced the resolution math, the modifier sources, the live data,
and the contradiction. Findings:

### Combat resolution is additive-accumulate, multiply-once

A fight is a ratio of two strength numbers: `defenderOdds = DIE_SIDES * defStr / (atkStr + defStr)`
(`CvCombatModel.cpp:39`). Each strength is built in `CvUnit::maxCombatStr` (`CvUnit.cpp:11464-12189`)
by summing **~40 signed-percentage layers into ONE int** (`iModifier`), then applying it **once**,
multiplicatively, at the end (`:12164`). So `+50% vs melee` and `+50% in hills` do **not** compound
— they add to `+100` and multiply once. Asymmetry: the attacker's strength is computed
opponent/plot-blind; every situational "vs X" modifier is folded into the **defender's** number as a
signed delta (attacker offence enters as a *negative* delta on the defender).

### The class-vs-class matrix (the concept worth keeping) is vestigial

The "join a class → innately strong/weak vs another class" matrix is authored in one shared tag,
`<UnitCombatMods>` (key `<UnitCombatType>` + `<iUnitCombatMod>`), summed at `CvUnit.cpp:11897-11919`
over every class the opponent *has*. The concept is real and well-localized — **but barely used:**

- **418 live classes; only 14 (3.3%) define any vs-class edge — 18 edges total.** 0 class-level
  flanking. The 18 are accreted one-offs (`GUNSHIP +50% vs TRACKED`, `CRIMINAL +100% vs TRADE`,
  the CANINE/STRIKE_TEAM animal triad, equipment `SHIELD_* vs MELEE`), not a designed matrix.
- **~353 of 367 live classes (96%) are inert TAGS** — orthogonal taxonomies crammed into the same
  enum: size (`UNITCOMBAT_SIZE_MEDIUM`), species (`SPECIES_HUMAN`), motility (`MOTILITY_FOOT`),
  heal-type (`HEALS_AS_PEOPLE`), weapon-method — they grant no strength, only categorize / gate
  promotions.

### The contradiction (the part to defenestrate over) is quantified

The real strength logic migrated **down to individual units**, in **four overlapping, additive,
unreconciled "vs" channels** with no precedence:

1. **vs-CLASS** — `<UnitCombatMods>` on the unit + on its promotions + on each class it belongs to.
   **528–668 units author 966 per-unit edges — 54× the 18 class edges.**
2. **vs-SPECIFIC-UNIT attack** — `<UnitAttackMods>` (keyed by `UnitType`); ~110 units, 436 edges.
3. **vs-SPECIFIC-UNIT defense** — `<UnitDefenseMods>`.
4. **flanking** — by-class and by-unit-type, added in one loop.
All four sum into the same `iModifier` (`CvUnit.cpp:11867-11919`). So a Mounted unit can hand-author
`+50% vs UNIT_SPEARMAN` even though Spearmen are the canonical anti-Mounted class — the per-unit
number just **adds on top, silently inverting/double-counting the class matrix**. Canonical case:
`UNIT_ZEBRA_CAVALRY` (Megafauna). Once summed, the origin (class vs promotion vs unit) is
**mathematically indistinguishable** — `unitCombatModifier()` = `unit-XML term + getExtra…() term`
(`CvUnit.cpp:13295`), the same accumulator `processUnitCombat` and `processPromotion` both deposit
into (a combat class *is* "a free promotion every member is born with").

- **Real bug found:** the help-text labels for the two axes are **swapped** — vs-class renders as
  "vs. Type" and vs-unit-type as "vs. Class" (`CvGameTextMgr.cpp:1009/1043/12860/12896`).
- AI valuation sums the overlapping channels → **over-values** units that get the same edge twice
  (once vs-class, once vs the specific type in that class).

### Removal caveats — why "unreferenced ≠ dead" (the purge blind spots)

A by-name XML scan is **not** sufficient to call a combat class dead. Two confirmed blind spots:

1. **Module content** — an inactive module's class looks unreferenced because the module's own units
   aren't active in the scan (the `*punk`/Cultures `UNITCOMBAT_CULTURE_*` tags). Intended ≠ dead.
2. **Engine attribute-matches** — the engine selects classes **by attribute in code**, never naming
   them in XML: `doSetUnitCombats` adds the class whose `getEra() == era` (`CvUnit.cpp:26224`); a
   parallel `getReligion()` match exists (`CvUnit.cpp:30950/30969`, `13001`); `getCulture()` likewise.
   So any class carrying `<Era>`/`<Religion>`/`<Culture>` (or referenced only via a global
   `getUNITCOMBAT_X()`) is live despite zero XML references.
The genuinely-safe-to-delete tier is the **equipment/material taxonomy** (`UNITCOMBAT_ATTACK_FORM_*`,
`ARMOR_*`, `WEAPON_*`, `SHIELD_*` — ~179 records) which carries no attribute, is not a global, and is
referenced by nothing. Even that should be removed on a **consolidated surface** (modules merged into
core) so no module reference is missed. (A 2026-06-14 blunt purge over-reached on both blind spots and
was fully reverted; do it carefully next time.)

### Toward a real structure (owner's aspiration)

A clean redesign: (a) ONE source of truth for "strength vs X," class-level by default with **explicit,
precedence-defined** per-unit overrides (not silent addition); (b) **split the orthogonal taxonomies**
(size/species/motility/heal-type) out of the combat-role enum into their own dimensions; (c) collapse
the redundant vs-animal paths (`<iAnimalCombat>` + matrix + tags) into one; (d) fix the swapped help
labels. This is the combat-class counterpart to the #428 cascade: a unit's combat profile becomes a
readable, single-origin set of modifiers instead of four additive channels nobody can audit.

## Files

- Class: `Sources/CvUnitCombatInfo.{h,cpp}` · Data: `Assets/XML/Units/CIV4UnitCombatInfos.xml`
- Related: [CvOutcome](CvOutcome.md), [CvProperties](CvProperties.md), the combat model
  (`Sources/CvCombatModel.*`), and the migration plan
  (`docs/dev/plans/building-cascade-conversion.md`).
