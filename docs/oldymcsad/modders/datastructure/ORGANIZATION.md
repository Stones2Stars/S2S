# S2S Data Organization — where every entity file lives

Companion to [`README.md`](README.md) (which defines what a file *contains*). This is the authoritative reference for
**how the `Assets/Data/` tree is organized**: which entity types live where, and how the larger sets are foldered.

## The rule

Every game entity is one JSON file at:

```
Assets/Data/<entity-type>/[<group>/]<entity>.json
```

- **`<entity-type>`** — a plural folder, one per Info kind (`buildings`, `units`, `techs`, `bonuses`, …).
- **`<group>`** — an **optional, single** grouping sub-folder for the larger sets (era / category / source / …). The
  tree is **never deeper than one grouping level** — always `type/entity.json` or `type/group/entity.json`, never
  `type/group/subgroup/…`.
- **`<entity>.json`** — the file, named from the entity's `type` id lower-cased: `BUILDING_ABU_SIMBEL` →
  `buildings/ancient/building_abu_simbel.json`, `BONUS_IRON` → `bonuses/…/bonus_iron.json`.

### Grouping is for navigation, not meaning
The loader reads **every** file under `Assets/Data/<type>/` regardless of sub-folder. A `<group>` folder is **derived
from the entity's own data** (its era from its prereq tech, its category from its civic-option, …) and carries no
information the file doesn't already hold. It exists only to keep folders human-navigable and bounded — it never drives
loading, and moving a file between sibling group folders changes nothing the engine sees. (A "when everything is
nailed" per-info-type schema, README §intro, is the future guard against *misplaced* data — distinct from this folder
organization.)

## The grouping axes

| axis | applies to | sub-folders |
|---|---|---|
| **era** | `buildings`, `techs` | `prehistoric` · `ancient` · `classical` · `medieval` · `renaissance` · `industrial` · `atomic` · `information` · `nanotech` · `transhuman` · `galactic` · `cosmic` · `transcendent` — **+ `none`** (buildings with no era) / **+ `future`** (techs). Era is derived from the prereq tech. |
| **category** | `civics` | the 15 civic-option short-names: `agriculture` · `currency` · `economy` · `education` · `garbage` · `government` · `immigration` · `language` · `military` · `power` · `religion` · `rule` · `society` · `welfare` · `workforce` |
| **outcome** | `builds` | by primary produced outcome: `bonus` · `clearing` · `features` · `forts` · `improvements` · `routes` · `terraform` |
| **source** | `bonuses` | `cultures` (culture bonuses — ~45% of all bonuses) · `manufactured` · `map` |
| **system** | `traits` · `promotionlines` | traits → `simple` / `complex` (the two trait systems); promotionlines → `buildups` |
| **flat** | every other type | no sub-folder; files sit directly in the type folder |

## The complete map (file counts are a 2026-06-17 snapshot — they drift; the structure is the durable part)

**Foldered sets**

| type | files | organized by |
|---|---|---|
| `buildings` | 5202 | era (14 folders incl. `none`) |
| `units` | 2073 | **flat** — see note below |
| `techs` | 943 | era (14 folders incl. `future`) |
| `traits` | 390 | system (`simple` / `complex`) |
| `promotionlines` | 333 | `buildups` |
| `builds` | 304 | outcome (7) |
| `bonuses` | 902 | source (`cultures` / `manufactured` / `map`) |
| `civics` | 175 | category (15 civic-options) |

**Flat sets** (one folder, files directly inside)

| type | files | | type | files |
|---|---|---|---|---|
| `promotions` | 1229 | | `corporations` | 23 |
| `unitcombats` | 814 | | `routes` | 21 |
| `improvements` | 266 | | `culturelevels` | 19 |
| `leaderheads` | 119 | | `civicoptions` | 15 |
| `heritages` | 113 | | `eras` | 14 |
| `terrains` | 102 | | `processes` | 13 |
| `features` | 101 | | `handicaps` | 12 |
| `civilizations` | 54 | | `bonusclasses` | 11 |
| `specialists` | 39 | | `victories` | 10 |
| `specialbuildings` | 36 | | `gamespeeds` | 9 |
| `religions` | 29 | | `properties` | 7 |
| `votes` | 30 | | `specialunits` | 7 |
| `projects` | 25 | | `hurries` | 2 |

> **Note — `units` is flat; era is the intended axis, but a culture-unit pass blocks finishing it (owner 2026-06-17).**
> Era (derived from prereq tech, exactly as for `buildings`/`techs`) is the sensible axis for the **bulk** of units, and
> the leaning. What keeps `units` flat for now is the large body of **culture-unlocked "unique" units** — civ/culture-
> specific units that are *not* genuinely unique (largely reskins / near-duplicates). They don't slot cleanly onto the
> era axis and want a dedicated **rationalization / dedup pass once the migration is done**. So flat is intentional
> *pending that pass*; era-foldering the rest is then a pure, engine-neutral file move (grouping is navigation-only,
> above). **Deferred follow-up: the culture-"unique"-unit dedup + era-fold of `units`.**

## Authoring / loading notes

- **Adding an entity:** create `Assets/Data/<type>/[<group>/]<prefix>_<name>.json`. Put it in the group its data
  implies (a building's era folder, a civic's civic-option folder); if a type is flat, just drop it in the type folder.
- **Validate with the tool:** `Tools/ReadJson/` — `readjson.exe Assets/Data` parses the whole tree and flags anything
  unrecognised; `readjson.exe Assets/Data --render BUILDING_FORGE` renders one entity to plain English.
- **The loader walks the tree;** it does not depend on the folder names. Excluded work-in-progress modules are handled
  separately (`store.EXCLUDED_MODULE_SUBPATHS`), not by this layout.

## Cross-references
- **What a file contains** (sections, vocabulary, examples): [`README.md`](README.md).
- **Field-by-field migration mapping + engine rationale** (dev): `docs/dev/plans/migration-renames.md`,
  `docs/dev/plans/building-cascade-conversion.md`, and the cascade specs (`data-model-spec.md`, …).
