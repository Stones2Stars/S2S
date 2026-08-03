# UnitCombat purge candidates — owner worklist (#430 §E)

> **Status:** CANDIDATE LIST for owner review. Read-only mapping — no data was edited. Tick / strike / annotate
> below, then a follow-up change executes the ticked items. Companion to
> [unitcombat-distillation.md](../../reference/engine.md) §E.
>
> **Method (owner 2026-07-19):** map the OBVIOUS, FLAG the unsure — do NOT blunt-purge (the 2026-06-14 blunt purge
> over-reached and was reverted). Only genuinely-dead / genuinely-identical items are proposed; everything ambiguous
> is FLAG/KEEP.

## Runtime-selection rule (confirmed from live code)

`CvUnit::doSetUnitCombats()` — **`Sources/Engine/CvUnit.cpp:26121-26152`**. A unit is attached its unit-combats from:
1. primary `combatClass` (`:26126`), 2. sub `combatClasses` (`:26130`), 3. **exactly one era-match**: the first
`UnitCombatInfo` whose `getEra() == unit's era` (`:26140-26147`, `break`), 4. promotion grants
(`setHasUnitCombat` at `:19042`), 5. heal-as types (`:6489`).

**The ONLY attribute that drives runtime attachment is `getEra()`** (JSON `identity.era`). The engine.md line claiming
`getReligion()`/`getCulture()` also select is **STALE**:
- `identity.religion` is **read FROM already-attached combats** to set the unit's religion (`CvUnit.cpp:30868`,
  `:30883-30897` — `isHasUnitCombat` guard), it is **not** an attach selector.
- `identity.culture` has **no attach path at all** (no `getCulture()` consumer on `CvUnitCombatInfo`).

Religion and culture classes are therefore not runtime-attached — but they are live **module content** (Cultures /
Alt_Timelines / Ideas project), so they stay FLAG/KEEP under the "inactive-module ≠ dead" caveat, not delete.

## Counts (live data, 2026-07-19)

| bucket | count |
|---|---|
| unit-combat JSON files (excl. `_order.json`) | **814** |
| referenced somewhere in `Assets/Data/**` | **413** |
| unreferenced | **401** |
| &nbsp;&nbsp;├ carry `identity.era` (runtime-selected) → KEEP | 14 |
| &nbsp;&nbsp;├ carry `identity.religion` (Ideas module) → KEEP | 27 |
| &nbsp;&nbsp;├ carry `identity.culture` → **DROPPED** (owner 2026-07-19; see below) | 344 |
| &nbsp;&nbsp;└ no runtime attr → **delete pool** | 16 |
| **Genuine MERGE/collapse sets** | **0** |
| **DELETE candidates (obviously dead)** | **15** |
| **FLAG/KEEP (unreferenced but kept)** | **42** (14 era + 27 religion + 1 ExoticAnimals module) |

> **⛔ CULTURE unit-combats DROPPED (owner ruling 2026-07-19; DONE).** The 344 `UNITCOMBAT_CULTURE_*` are
> **redundant double-data**, not module content to keep. Each is a pure `{description, culture: BONUS_X}` shell;
> the culture↔unit identity is already owned by the culture **BONUS** (`BONUS_X.enables.units` +
> `identity.bonusClassType: BONUSCLASS_CULTURE`) and gated on the units (`requires.build: BONUS_X`), and
> `CvUnitCombatInfo::getCulture()` has **zero engine consumers** (`getReligion()`, by contrast, IS live — hence
> religion stays). They attach to no unit. **Executed:** `curate_unitcombat.py` filters any culture shell out of
> the emit (guarded so a culture record carrying real combat content is never silently dropped); the 344
> `Assets/Data/unitcombats/unitcombat_culture_*.json` files are deleted; unit-combats load from JSON
> (`LoadGlobalClassInfoJson`), so the drop manifests in-game. The `unitcombats/_order.json` manifest is
> **regenerated on the actual output (814 → 470)**. `curate_order.py` now generates EVERY manifest on-disk (legacy
> XML order ∩ emitted files) — a curator-dropped legacy type is simply not listed. This is provably a no-op for
> engine ids: the loader reads `_order.json` into `order[type]=index` and sorts only the PRESENT entity files by
> that index, so a fileless phantom never gets an id and dropping it shifts only absolute index values, never the
> relative order of present types (`CvXMLLoadUtilitySet.cpp:1876-1896`).

> **Reference scan is exhaustive** (ALL channels, not just `combatClass`): `identity.base.combatClass`,
> `identity.combatClasses`, unit `identity.unitCombat`, promotion `skills.unitCombats` /
> `identity.unitCombats` / `removesUnitCombats` / `replacesUnitCombat` / `notOnUnitCombats`, promotionline
> `unitCombats` / `notOnUnitCombats`, and buildup `unitCombats`. (The initial narrow scan under-counted; this is the
> corrected set.)

---

## 1. DUPLICATE / MERGE SETS (genuinely-identical, collapse to one survivor)

**NONE.** There are no genuine redundant-alias duplicates to collapse.

18 clusters of unit-combats DO share an identical field payload (see §2), but **every one is a set of
semantically-DISTINCT taxonomy classes** (mammal families, ship eras, mount species, weapon methods, siege roles,
species) whose shared payload is an artifact of the fat-class design carrying almost nothing of its own. They are the
**tag-distillation input** (distillation.md §3.A), heavily referenced as distinct classifications — collapsing e.g.
`MAMMAL_BOVINE` into `MAMMAL_EQUINE` because both are `withdrawal 10%` would destroy the species taxonomy. That is
exactly the blunt-purge mistake. → No merge/re-point action; these route to tag-distillation, not collapse.

---

## 2. NEAR-DUPLICATE (flagged for review — do NOT auto-collapse)

Shared-payload taxonomy clusters. Listed so the owner can eyeball them; the intended treatment is **distill into
`tags` / `sizeMatters`** (distillation.md §3.A), not merge. `refs` = how many units/promotions reference each.

- [ ] **`skills.cannotMergeSplit` only** (17 classes): ATTACHE, CAPTAIN, CAPTIVE, COMMANDER, COMMODORE, DOOM,
  EXECUTIVE, HEALTH_CARE, MISSIONARY, PACIFIST(451), PRODIGY, SEA_WORKER, SETTLER, SPACE_WORKER, SPY(77),
  SUBDUED(194), WORKER — role classes, distinct identities.
- [ ] **`identity.forMilitary` only** (12): ANTIMATTER_SPACESHIP, BOMBERS, EARLY_BOMBERS, EARLY_FIGHTERS,
  EARLY_SPACESHIP, JET_FIGHTERS, NUCLEAR_SPACESHIP, ORBITAL_AIRCRAFT, SOLAR_SAIL_SPACESHIP, STEALTH,
  SUPERSONIC_PLANES, WORMHOLE_SPACESHIP.
- [ ] **`skills.healsAs` only** (9): HEALS_AS_{AIRCRAFT, ANIMALS(777), MECHANICAL, NANOMORPH, NAVAL, PEOPLE(1643),
  SPACECRAFT, SWARM, TECH} — heal-domain buckets, all live.
- [ ] **`withdrawal.unit.percent 10`** (10 mammal/invertebrate families) and **`... 15`** (7 fish/mammal families) —
  species taxonomy.
- [ ] **`capture.unit.resistance.flat 10`** (5 MOUNT_* species), **`identity.forNavalMilitary` + naval-disguise
  1/2/3** ship clusters, **`capture.probability 10/resistance 10`** (GUN 365 / HITECH 75),
  **`capture.probability -20`** (SIEGE_FIELD / SIEGE_URBAN), **`visibilityIntensitySameTile INVISIBLE_DISGUISED
  1`** (SPECIES_ALIEN / SPECIES_HUMAN 1453), etc. — full cluster dump in the analysis; all distinct-concept.
- [ ] **Empty-payload cluster (185 classes)** — pure taxonomy stubs carrying literally nothing (weapon methods,
  size/quality/group ranks, armor, shields, animal-combat abilities, species, motility). The prime tag-distillation
  target. 175 are referenced as unit `combatClasses`; the 10 unreferenced ones are in the DELETE list below.

> ⚠ A few 0-ref members of these clusters duplicate a *referenced* sibling exactly (e.g. `CARRIER` vs
> `BATTLESHIP`/`CRUISER`; `SWARMSHIP` vs the disguise-1 ships) — those specific 0-ref ones are safe to drop and are
> already in the DELETE list (§3), since deleting them loses nothing the live sibling doesn't already carry.

---

## 3. DELETE candidates (obviously dead — unreferenced everywhere, no runtime attr)

**Confirmed 0 references** across `Assets/Data/**` JSON, all module/legacy `*_CIV4UnitInfos.xml` and
`*_CIV4PromotionInfos.xml`, `Assets/Python`, and `Sources` (beyond each class's own definition), **and** no
`identity.era` / `religion` / `culture`. Safe to delete the JSON file (and drop from `_order.json`).

Empty-payload taxonomy stubs (9):
- [ ] `UNITCOMBAT_DISASSEMBLY` — empty; no ref anywhere.
- [ ] `UNITCOMBAT_HOLOGRAPHIC_DIVERSIONS` — empty; no ref.
- [ ] `UNITCOMBAT_IMPROVED_HOLOGRAPHIC_DIVERSIONS` — empty; no ref.
- [ ] `UNITCOMBAT_MAMMAL_BAT` — empty; no ref (unused animal-family stub).
- [ ] `UNITCOMBAT_MOUNT_MULE` — empty; no ref (unused mount stub).
- [ ] `UNITCOMBAT_REPTILE_DINOSAUR` — empty; no ref.
- [ ] `UNITCOMBAT_SEA_LARGE` — empty; no ref (Subdue-module size stub, no assignment).
- [ ] `UNITCOMBAT_SEA_SMALL` — empty; no ref.
- [ ] `UNITCOMBAT_WHALE` — empty; no ref.

Payload classes that are still fully orphaned (6):
- [ ] `UNITCOMBAT_AMPHIBIAN_SALAMANDER` — `vision.invisibilityIntensity CAMOUFLAGE:1`; no ref (orphan animal family).
- [ ] `UNITCOMBAT_ANTIGRAV_CRAFT` — `capture + forMilitary + skills.fliesToMove`; no ref.
- [ ] `UNITCOMBAT_CARRIER` — `forNavalMilitary + ocean terrainDoubleMove`; 0 ref, payload duplicated by referenced
  `BATTLESHIP`/`CRUISER`.
- [ ] `UNITCOMBAT_CORVETTE` — `forNavalMilitary + coast terrainDoubleMove`; 0 ref.
- [ ] `UNITCOMBAT_CUTTER` — `forNavalMilitary + coast terrainDoubleMove`; 0 ref.
- [ ] `UNITCOMBAT_SWARMSHIP` — `forNavalMilitary + naval-disguise 1`; 0 ref, payload duplicated by referenced
  DIESEL/STEAM/NUCLEAR/JET/SHOCKWAVE ships.

---

## 4. FLAG / KEEP (unreferenced but runtime-selected or module content — do NOT delete)

- [ ] **14 `UNITCOMBAT_ERA_*`** — carry `identity.era`; **runtime-selected** by `doSetUnitCombats` era-match
  (`CvUnit.cpp:26140`). Every unit attaches one. Members: ERA_{PREHISTORIC, ANCIENT, CLASSICAL, MEDIEVAL,
  RENAISSANCE, INDUSTRIAL, ATOMIC, INFORMATION, NANOTECH, TRANSHUMAN, GALACTIC, COSMIC, TRANSCENDENT, FUTURE}.
- [ ] **27 `UNITCOMBAT_RELIGION_*`** — carry `identity.religion`; consumed by the religion-lock path
  (`CvUnit.cpp:30868/30883`) and are Ideas-project module content. Unreferenced in current data ≠ dead.
- **`UNITCOMBAT_CULTURE_*` — DROPPED** (owner ruling, executed — see the Counts note above).
  Not module content worth keeping: redundant double-data of the culture BONUS, `getCulture()` dead, attached to
  no unit. Curator-filtered + files deleted; `unitcombats/_order.json` regenerated on actual output (→ 470).
- [ ] **`UNITCOMBAT_MAMMAL_LAGOMORPH`** — empty + 0 data-JSON ref, BUT assigned by a unit in the **ExoticAnimals**
  module (`ExoticAnimals-0.3_CIV4UnitInfos.xml`). Kept out of DELETE on the module caveat; owner may downgrade to
  delete if ExoticAnimals is dead.

---

## Bottom line

- **DONE: 344 culture unit-combats DROPPED** (owner 2026-07-19) — redundant double-data of the culture BONUS,
  curator-filtered + files deleted, `unitcombats/_order.json` regenerated on actual output. 814 → **470** classes.
- **Conservative purge = 15 more files** (§3: 9 empty stubs + 6 orphaned classes), all verified 0-reference
  everywhere — still a CANDIDATE list awaiting owner tick.
- **0 merge/collapse sets** — the 18 shared-payload clusters are distinct taxonomy (tag-distillation input), not
  redundant aliases.
- **42 unreferenced classes remain FLAG/KEEP** (14 era runtime-selected + 27 religion + 1 ExoticAnimals module) —
  the memory reduction for the remaining taxonomy rides on the **tag-distillation** (§3.A / §2 empty cluster), not a
  raw purge, because deleting live module/runtime classes is the reverted blunt-purge mistake.
