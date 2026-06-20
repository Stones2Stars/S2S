# UnitCombat (`CvUnitCombatInfo` / `UnitCombatTypes`) — the combat-class axis & its proliferation

> **Status:** reference   ·   **Verified against:** `Sources/Infos/CvUnitCombatInfo.{h,cpp}`, `Sources/Engine/CvUnit.cpp`, `Sources/Engine/CvCombatModel.cpp`, `Sources/UI/CvGameTextMgr.cpp`, `Assets/XML/Units/CIV4UnitCombatInfos.xml` — re-walked 2026-06-20. The 2026-06-14 combat-class audit (data counts below) was NOT re-counted this pass; treat the magnitudes as that audit's snapshot.
> **Grounding:** the resolution math + the `unitCombatModifier`/`<UnitCombatMods>` accumulation were read from the live `CvUnit`/`CvCombatModel`; the "proper info class" header comment from `CvUnitCombatInfo.h:9`. The `Cv*` engine files moved into `Sources/Engine/`, the info class into `Sources/Infos/`, and `CvGameTextMgr` into `Sources/UI/` since the original authoring; paths below reflect those homes. Line numbers **drift** — confirm the named function, not the integer.
>
> **BLUF.** A *unit combat type* is a unit's **combat class** — a core Civ4 axis (`UNITCOMBAT_MELEE`, `UNITCOMBAT_ARCHER`, …, ~8–10 in BtS). In S2S/C2C it became three compounding things: **(1)** a fat "proper info class" (`CvUnitCombatInfo`, ~150 fields) that carries nearly the whole `CvPromotionInfo` effect vocabulary — *a combat class is now a free promotion every member unit is born with*; **(2)** a **many-to-many** membership (a unit has a primary class **plus a set**, each contributing its bundle); and **(3)** a **type proliferation** to **~981 classes** (vanilla ~10) of which **~77% sit on no unit**. The friction is part (3) — a mostly-dead silt of classes — and a four-channel "vs" contradiction nobody can audit, **not** the framework itself. Keep the framework, shed the silt.

> **Line numbers drift.** Every `file:line` here means "the function named here, around this line" — confirm the **function**, not the integer. Several citations below have already moved tens of lines since the audit.

---

## 1. What it is in vanilla Civ4 (BtS) — a thin, core axis

A **unit combat type** is the *combat class* of a unit: `UNITCOMBAT_MELEE`, `UNITCOMBAT_ARCHER`, `UNITCOMBAT_MOUNTED`, `UNITCOMBAT_GUN`, `UNITCOMBAT_ARMOR`, `UNITCOMBAT_NAVAL`, `UNITCOMBAT_AIR`, `UNITCOMBAT_SIEGE`, … (~8–10 in BtS). It is one of the most *referenced* enums in the combat system, but `CvUnitCombatInfo` itself was almost empty — essentially `Type` + `Description`. The class was a **category label**; the behaviour lived on the things that *point at* the class:

- **Promotion gating** — a promotion lists the combat classes it can be taken by (`PromotionInfo.UnitCombats`): "Cover is available to Archery and Gunpowder units."
- **Vs-class combat bonuses** — `unitCombatModifier(class)`: "+25% vs Melee" (granted by promotions / buildings / units). Consumed in the combat math (`CvUnit::maxCombatStr`).
- **Free experience / free promotions per class** — traits/civics/buildings grant "all Mounted units get +2 XP" or "free Combat I to Gunpowder."
- **AI** — counter-unit selection and threat assessment pivot on combat class.

In vanilla: **core mechanic, thin data.** A unit had exactly **one** combat type (`UnitInfo.UnitCombat`).

## 2. What S2S/C2C did to it — three compounding expansions

The class's own header announces the design: `// AIAndy: A proper info class for unit combat types` (`Sources/Infos/CvUnitCombatInfo.h:9`). The phrase is the tell — it was *not* a proper info class before; it was a label. Three expansions turned the label into a sprawling system:

1. **A "proper info class" (~150 fields).** `CvUnitCombatInfo` now carries nearly the *entire `CvPromotionInfo` effect vocabulary* directly on the class: `combatPercent`, `cityAttack/DefensePercent`, `withdrawalChange`, `firstStrikes`, the heal family, `movesChange`/`moveDiscount`, vision/invisibility intensity arrays, terrain/feature attack/defense/work modifiers, `unitCombatChangeModifiers` (vs-class), size-matters fields, `KillOutcomeList` + action `OutcomeMission`s, `PropertyManipulators`, on/notOnGameOptions, and more. In effect a combat class is **a free promotion every member unit is born with** — the field surface is a near-mirror of `CvPromotionInfo` (same families).

2. **Many-to-many membership.** A unit no longer has just one combat class. It has a *primary* (`CvUnit::getUnitCombatType()` → `UnitInfo.UnitCombat`) **plus a set** of additional classes, queried with `isHasUnitCombat(eClass)` and iterated across all classes. Each class a unit *has* contributes its bundle. So a combat class is a stackable **tag with effects**.

3. **Type proliferation — the part to actually distrust.** Item 1 (AIAndy's *framework*) is a defensible design; the bloat is what came **after** it. Base `Assets/XML/Units/CIV4UnitCombatInfos.xml` already defines **636** classes; with modules merged it is **~981** (vanilla ~10). Only **~230 are on any unit** — roughly **77% (≈751) are attached to no unit at all**: a large dead/vestigial tier, much of it from since-purged TB-mega-mod content. The combat-class enum became a general-purpose unit-tag dimension and then mostly silted up.

Net: a **core axis** + **a defensible mod-built effect framework**, buried under **a mostly-dead proliferation of ~981 classes**. The friction is the third part, not the first two.

## 3. How it is consumed today

- **Bundle application:** a unit iterates the combat classes it `isHasUnitCombat`, and each class's `CvUnitCombatInfo` contributes its effects (heal rules, movement characteristics, era gating, merged kill/action outcomes, …) — read these off the `isHasUnitCombat` loops in `Sources/Engine/CvUnit.cpp`.
- **Vs-class modifier:** `CvUnit::unitCombatModifier()` (`Sources/Engine/CvUnit.cpp:13293`) adds the attacker/defender's per-class bonuses during combat; it is summed in `maxCombatStr` (`CvUnit.cpp:11902` adds the defender's edge, `:11918` subtracts the attacker's).
- **Promotion gating / free promotions / free XP:** other entities key *by* combat class.
- **Outcomes & properties:** kill/action `CvOutcomeList`s and `PropertyManipulators` ride on the class (e.g. subdue/capture/butcher outcomes for animal classes). See [`properties.md`](properties.md) for the property-manipulator side.

## 4. How combat actually resolves — the 2026-06-14 audit

A 4-agent audit (wf_e1bffcc4) traced the resolution math, the modifier sources, the live data, and the contradiction. Findings below; the **data magnitudes are that audit's snapshot** and were not re-counted this pass.

### 4.1 Resolution is additive-accumulate, multiply-once

A fight is a ratio of two strength numbers: `iDefenderOdds = COMBAT_DIE_SIDES * defStr / (atkStr + defStr)` (`Sources/Engine/CvCombatModel.cpp:39`). Each strength is built in `CvUnit::maxCombatStr` by summing **~40 signed-percentage layers into ONE int** (`iModifier`), then applying it **once**, multiplicatively, at the end. So `+50% vs melee` and `+50% in hills` do **not** compound — they add to `+100` and multiply once. Asymmetry: the attacker's strength is computed opponent/plot-blind; every situational "vs X" modifier is folded into the **defender's** number as a signed delta (attacker offence enters as a *negative* delta on the defender — the `-=` at `CvUnit.cpp:11918`).

### 4.2 The class-vs-class matrix (the concept worth keeping) is vestigial

The "join a class → innately strong/weak vs another class" matrix is authored in one shared tag, `<UnitCombatMods>` (key `<UnitCombatType>` + `<iUnitCombatMod>`), summed in `maxCombatStr` over every class the opponent *has*. The concept is real and well-localized — **but barely used** (audit counts):

- **418 live classes; only 14 (3.3%) define any vs-class edge — 18 edges total.** 0 class-level flanking. The 18 are accreted one-offs (`GUNSHIP +50% vs TRACKED`, `CRIMINAL +100% vs TRADE`, the CANINE/STRIKE_TEAM animal triad, equipment `SHIELD_* vs MELEE`), not a designed matrix.
- **~353 of 367 live classes (96%) are inert TAGS** — orthogonal taxonomies crammed into the same enum: size (`UNITCOMBAT_SIZE_MEDIUM`), species (`SPECIES_HUMAN`), motility (`MOTILITY_FOOT`), heal-type (`HEALS_AS_PEOPLE`), weapon-method — they grant no strength, only categorize / gate promotions.

### 4.3 The contradiction — four overlapping "vs" channels, no precedence

The real strength logic migrated **down to individual units**, into **four overlapping, additive, unreconciled "vs" channels** with no precedence, all summing into the same `iModifier` in `maxCombatStr`:

1. **vs-CLASS** — `<UnitCombatMods>` on the unit + on its promotions + on each class it belongs to. **528–668 units author 966 per-unit edges — 54× the 18 class edges.**
2. **vs-SPECIFIC-UNIT attack** — `<UnitAttackMods>` (keyed by `UnitType`); ~110 units, 436 edges.
3. **vs-SPECIFIC-UNIT defense** — `<UnitDefenseMods>`.
4. **flanking** — by-class and by-unit-type, added in one loop.

So a Mounted unit can hand-author `+50% vs UNIT_SPEARMAN` even though Spearmen are the canonical anti-Mounted class — the per-unit number just **adds on top, silently inverting/double-counting the class matrix** (canonical case: `UNIT_ZEBRA_CAVALRY`, Megafauna). Once summed, the origin (class vs promotion vs unit) is **mathematically indistinguishable** — `unitCombatModifier()` = `unit-XML term + getExtra…() term`, the same accumulator `processUnitCombat` and `processPromotion` both deposit into (a combat class *is* "a free promotion every member is born with").

- **⚠ Real bug, still live (stale flag):** the help-text labels for the two axes are **swapped** — vs-class renders as "vs. Type" and vs-unit-type as "vs. Class". Confirm at the `TXT_KEY_UNITHELP_MOD_VS_TYPE` append in `CvUnit::getUnitCombatModifier` help (`Sources/UI/CvGameTextMgr.cpp:12896`) and the `TXT_KEY_PROMOTIONHELP_VERSUS` append (`:10050`); the original audit cited `:1009/1043/12860/12896` and the integers have drifted, so anchor on the `TXT_KEY_*` strings, not the line.
- AI valuation sums the overlapping channels → **over-values** units that get the same edge twice (once vs-class, once vs the specific type in that class).

### 4.4 Removal caveats — why "unreferenced ≠ dead" (the purge blind spots)

A by-name XML scan is **not** sufficient to call a combat class dead. Two confirmed blind spots:

1. **Module content** — an inactive module's class looks unreferenced because the module's own units aren't active in the scan (the `*punk`/Cultures `UNITCOMBAT_CULTURE_*` tags). Intended ≠ dead.
2. **Engine attribute-matches** — the engine selects classes **by attribute in code**, never naming them in XML: `doSetUnitCombats` adds the class whose `getEra() == era`; parallel `getReligion()` and `getCulture()` matches exist (all in `Sources/Engine/CvUnit.cpp`). So any class carrying `<Era>`/`<Religion>`/`<Culture>` (or referenced only via a global `getUNITCOMBAT_X()`) is live despite zero XML references.

The genuinely-safe-to-delete tier is the **equipment/material taxonomy** (`UNITCOMBAT_ATTACK_FORM_*`, `ARMOR_*`, `WEAPON_*`, `SHIELD_*` — ~179 records) which carries no attribute, is not a global, and is referenced by nothing. Even that should be removed on a **consolidated surface** (modules merged into core) so no module reference is missed. **A 2026-06-14 blunt purge over-reached on both blind spots and was fully reverted; do it carefully next time.**

### 4.5 Toward a real structure (owner's aspiration)

A clean redesign: (a) ONE source of truth for "strength vs X," class-level by default with **explicit, precedence-defined** per-unit overrides (not silent addition); (b) **split the orthogonal taxonomies** (size/species/motility/heal-type) out of the combat-role enum into their own dimensions; (c) collapse the redundant vs-animal paths (`<iAnimalCombat>` + matrix + tags) into one; (d) fix the swapped help labels (§4.3). This is the combat-class counterpart to the cascade rework: a unit's combat profile becomes a readable, single-origin set of modifiers instead of four additive channels nobody can audit.

## 5. Relevance to the JSON migration

UnitCombat is on the remaining heavy-entity list. When it migrates it is a **source/enabler** — like a trait or a promotion (see [`../cascade/data-model.md`](../cascade/data-model.md) §1, the entity ontology): it deposits a unit-stat modifier bundle onto every unit that *has* the class. Consequences:

- **Shared vocabulary with Promotion.** Because the field surface is a near-clone of `CvPromotionInfo`, the two should share the cascade's modifier-family definitions (combat, heal, movement, vision, terrain/feature modifiers, …) rather than inventing parallel ones — the unit-plane families enumerated in [`../cascade/data-model.md`](../cascade/data-model.md) §4. Do Promotion and UnitCombat together (or back-to-back) so the vocabulary is reconciled once.
- **Membership is the enabler axis; the bundle is the modifier side.** "Which units have this class" (many-to-many) is the availability/tag relation; the ~150 fields are the modifier deposit. `unitCombatModifier` (vs-class) is a *keyed-by-combat-class* effect owned by the modifying source (promotion/building/unit/trait) — exactly the [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy) "lives on whoever delivers it, keyed by the target" rule.
- **Expect dead/vestigial fields** among the 150 (the Trait pass found this pattern); verify each against the live `CvUnit` consumer before carrying it.
- **Most of the *classes* are dead, not just fields** — ~77% sit on no unit. Selective curation (carry only what the cascade/consumers need) + a post-migration purge should shed that tier rather than port ~981 near-clones — but verify per class first (§4.4: a class may be live only via an attribute-match or a module). The whole shadow-then-cut discipline is [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete).

The cascade machinery this maps onto is **not** re-explained here — see [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) for the three machines that consume the migrated shape.

## See also

- [`../cascade/data-model.md`](../cascade/data-model.md) — the JSON authoring surface UnitCombat migrates into; it is a source/enabler entity sharing Promotion's modifier-family vocabulary (§1 ontology, §4 unit-plane families). This doc is the legacy mechanism; that doc is the target shape.
- [`../../explanation/cascade-architecture.md`](../../explanation/cascade-architecture.md) — the enabler/modifier/tally machines that will consume the migrated combat-class bundle; membership = the enabler axis, the ~150 fields = the modifier deposit.
- [`properties.md`](properties.md) — the `PropertyManipulators` that ride on a `CvUnitCombatInfo` (animal subdue/capture outcomes etc.) attach through the property system documented here.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — [DEC-deliveryguy](../../architecture/decisions.md#dec-deliveryguy) (the vs-class modifier lives on the delivering source, keyed by class), [DEC-map-before-delete](../../architecture/decisions.md#dec-map-before-delete) (shadow-then-cut governs the dead-class purge).
- [`../../README.md`](../../README.md) — the docs2 comprehension map.
