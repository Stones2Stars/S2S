# readJson conformance corpus — the "gnarl" torture set (#428/#430)

A deliberately ADVERSARIAL, hand-authored test corpus for the readJson harness (`../readjson.cpp`). Where the
migrated `Assets/Data` is real-but-quirky, this set is **evil on purpose** — it pushes every legal shape to the
limit and, separately, breaks every rule — so the parser is calibrated against *intent*, not migration accidents.
Every flag against the VALID tree is a parser gap or a spec ambiguity; every flag the INVALID tree fails to raise
is a hole in the harness's diagnostics.

## Two trees
- **valid** (`buildings/ units/ bonuses/ techs/ civics/ religions/ heritages/`) — spec-legal, maximally gnarly.
  **Goal: ZERO conformance flags.** Run: `readjson.exe Tools/ReadJson/testdata` (skip `invalid/` — see below).
- **invalid** (`invalid/`) — deliberately malformed. **Goal: each known error is FLAGGED.** Negative-test oracle.
  Run it alone: `readjson.exe Tools/ReadJson/testdata/invalid`.

## The linked "gnarl" web (to surface BONUS weirdness)
The valid entities cross-reference each other, centered on **`BONUS_GNARLITE`**, because the model's invariant is
that **a bonus is only ever a conditioner / enabler / own-deposit — NEVER a modifier target** (the "coal test").
If anything inverts a modifier ONTO the bonus, `--render BONUS_GNARLITE` will show it receiving instead of only
emitting/enabling. The edges:
- `BONUS_GNARLITE` — `enables` the building/unit/route; owns plot yields + empire health/happiness. (hub)
- `TECH_GNARLOLOGY` — spine root: `enables` the building/unit/bonus/civic/religion/build; `obsoletes`; multi-parent
  `requires.build` (all + any); a downward deposit keyed onto the building (`gold.city.buildings.{TEMPLE}`).
- `CIVIC_GNARCHY` — empire conditioner; happiness/gold *gated by* `BONUS_GNARLITE` (keep-on-source — proves nothing
  inverts onto the bonus); `buildRate.empire.military`.
- `RELIGION_GNARLODOXY` — commerce gated by `STATE_RELIGION`/`HOLY_CITY`/`HAS_RELIGION` (object-evaluated predicates).
- `BUILDING_TEMPLE_OF_GNARL` — `production.city` (total output) vs `buildRate.self` (build-this-faster-with-gnarlite)
  side by side (the Versailles distinction); `requires.operate` dormancy on gnarlite + gnarchy; `enables` the unit.
- `UNIT_SIR_GNARLALOT` — full §5 unit plane; `requires.build` the temple + gnarlite; `buildRate.self`.
- `BUILDING_GNARL_SINGULARITY` — the **everything-at-once** monster (the "22"): 5-deep `requires` nesting, every bare
  AND parameterized predicate, all five units (`flat`/`percent`/`multiplier`/`postMultiplier`/`rawPercent`), all 13
  scopes incl. `self`, `per:anyOf`/`per:SELF`, `enabled`+`disabled` twins on one entry, `ai` siblings, every `enables`
  bucket, every grant kind. Pure parser-recursion stress; meant to be the hardest legal file in the set.
- `HERITAGE_TAXON_GNARLIDAE` + `BUILDING_EFFECT_GNARL_INFESTATION` — the **autobuilding / "insane taxonomy"** case:
  the heritage GRANTS the effect-building (auto-built, not player-built; §6.1 enable→grant→auto-build→`requires`-active),
  which is **dormant outside its property band** (`requires.operate {PROPERTY_GNARL_INFESTATION, min:50, max:100}`),
  feeds back INTO the property, and spawns beasts via `grants.repeatable`. Deeply nested `requires`, `per:anyOf`,
  `ai` siblings, active-gated property sources (`enabled:{TECH,team}` — the corrected `active.GOM_TECH` shape).

## Negative-test oracle — `invalid/building_gnarl_abomination.json` SHOULD flag (≈15):
`enables: unknown bucket 'gizmos'` · `enables bucket not array` (units) · `enables-family not object` (obsoletes) ·
`requires: unknown sub 'maintain'` · `'all' not array` · `atom: bad scope 'galaxy'` · `atom: unknown key 'wat'` ·
`unrecognized condition object` (noneOf atom w/ no type) · `condition: unexpected value type` (enabled:42) ·
`unknown bare predicate 'IS_GNARLY_NONEXISTENT'` · `unknown predicate object 'IS_GNARLY_OBJECT_PREDICATE'` ·
`grants not object` · `family 'happiness' first key not a scope: 'galaxy'` · `deposit: unexpected value type`
(food) · `unit value: unexpected type` (production flat) · `entry: unknown key 'GREMLIN'` · `per: unknown key 'wat'`.
(If the harness raises FEWER than these, its diagnostics have a gap — fix the harness, not the file.)

Other invalid files (each a separate error path the abomination can't reach):
- `invalid/not_an_object.json` (root is a JSON array) → `entity not an object`.
- `invalid/requires_not_object.json` (`requires` is a string) → `requires not object`.
- `invalid/broken_containers.json` → `'any' not array` · `'noneOf' not array` · `per not object`.

## The MODDER-TOOL vision (owner 2026-06-16)
This harness is a candidate to **ship to modders** as a standalone validator: feed it your JSON, get (1) conformance
flags (grammar/vocab errors) and (2) a clear-text RENDER (`--render TYPE`) of what the entity MEANS. The render is
the **intent check** — a modder reads "Temple of Gnarl builds faster with gnarlite; +2 happiness; dorms without
gnarchy" and confirms *"yes, that's what I meant"* (or catches the Versailles-style "no, I meant the wonder, not the
city"). So: grammar errors → flags; semantic errors → the render, by eye. Implications: keep diagnostics
modder-friendly, and ship it from `Tools/` (the relocation queued) — possibly vendored like `FpkBuilder`.
