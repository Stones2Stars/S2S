# S2S — cascade rebuild (draft changelog, 2026-08)

> **DRAFT — pending owner curation.** Content is assembled from the repo docs and the
> cascade-rebuild git history; nothing here is final until reviewed.
>
> **Maintenance:** a commit whose change a player or modder would notice appends one bullet to
> `## Unreleased` in the SAME commit (AGENTS.md Git/delivery). The `/changelog-update` skill
> digests anything that slipped, from the marker below. The old commit-message-derived
> changelog script is dead and stays dead.

<!-- last-digested: bc719eb4e -->

## Unreleased

- Modders: the Python endpoint that brings a unit into existence is now `createUnit`, on both the
  player and the action surface — it was `initUnit`, which named the engine internal that callers
  must never reach directly. It now matches the engine's own single creation step, so a reader
  looking for how a unit comes into being finds one answer instead of two spellings. Every shipped
  call site moved with it, including the Python embedded in module XML.
- The tribal guardian arrives when the city is founded, and it no longer inherits the settler's
  accumulated experience. Walking a settler for three turns used to hand its guardian 33 experience
  — four turns, 44 — because the transfer read a hundredths-scaled value as whole points.
- Fixed unit experience being read a hundred times too large in several places: the great-general
  points Pergamon awards (a unit with 33 experience counted as 3,300, paying roughly ten times the
  points it should), and three random-event requirements that asked for a veteran of 7, 25 or 50
  experience and in practice accepted almost any unit.
- Units that arrive by any route other than training — granted, awarded, spawned, captured, bribed,
  founder escorts, free defenders, great people — now receive the free experience their city gives
  units, exactly as a trained unit does. Previously it depended on which code path created them.
- Units whose combat role has no promotions at all can no longer earn them: great people, subdued
  animals, workers, executives, corporate agents, nukes and captives among them (389 units). They
  kept picking up promotions through the species/size/quality classes attached to them for other
  reasons. Promotions a unit is simply *given* by its own type are unaffected. Modders: this is the
  new `unpromotable` unit tag, derived from whether any promotion accepts the unit's primary combat
  class — if something ought to promote and cannot, giving that class a promotion is the fix.
- Restored the combat-odds tooltip when hovering an attack target.
- Fixed a crash when loading a save from inside a running game.
- The city centre plot yields its guaranteed minimums again (3 food / 1 hammer / 1 commerce
  floor) instead of acting like a regular tile.
- Fixed a civilization-restriction misread that hid 21 empire-wide buildings (the rock/stick/
  lumber gatherer class and kin) from every player's build list.
- The civilization-whitelist mechanic is removed outright: what a civ can build is decided by
  its techs and requirements — NPCs included — and deliberate bars use the disable mechanism.
- Empire-wide gatherer-class buildings require their resource again (obsidian gatherer needs
  obsidian in the city vicinity, and kin) — the empire conversion had dropped those build
  requirements.
- A trait's extra-yield-threshold bonus reaches its cities again: the per-tile step was being
  applied to each plot but never added to the city's own food/hammer/commerce totals, so a
  trait that grants it changed the tile readouts and nothing the city actually produced.
- Golden ages give their per-tile bonus again (+1 hammer and +1 commerce on qualifying worked
  tiles). It had stopped applying entirely, while the AI still valued golden ages as though it
  were there. As before, a tile qualifies on what it makes *before* its improvement and route
  are counted — so a tile whose whole output comes from a mine or a road does not qualify.
- Negative traits impose their tile penalty again: the lazy, gluttonous, excessive and nomad
  lines lower the output of tiles above their threshold, which had stopped applying entirely
  while the matching bonus on positive traits kept working.
- The city yield breakdown now reports the route's share of a tile's output separately.
- Complex-trait games no longer hand a leader the base rung of a trait line *and* its rank-1 rung at the
  same time. A line now runs 1 → 2 → 3, as complex games have always been played, and a leader holds the
  rung rather than the rung plus a hidden extra copy underneath it. Leaders therefore start with the values
  their rank actually states — noticeably less in some cases, because the duplicate was stacking on top.

Stones2Stars (S2S) is a Civ4 / Caveman2Cosmos-derived mod. This release cycle is a ground-up
rebuild of how the mod computes and displays everything: all game data moved from XML to JSON,
all bonuses and requirements flow through one unified system, and the whole game state is
observable live from outside the game.

---

## For players — what feels different

### Build lists and the tech tree actually tell you why

- Buildings, units, and techs now show in three states: **available** (build now), **greyed**
  (missing something specific), or **hidden** (not unlocked yet).
- Greyed entries name exactly what you're missing — "go get copper", "research Astronomy" —
  per requirement (tech / resource / civic). Unmeetable entries are hidden instead of taunting you.
- Tech tree correctly distinguishes "not yet obtainable" from "never obtainable".
- Tech tree arrow layout fixed; tech splash screen restored.
- The build list correctly drops buildings you've already queued.

### Obsolescence and bans that behave sensibly

- New tech makes old units/buildings **obsolete**: they leave the production queue, but existing
  ones persist on the map.
- Obsoleted buildings can stay active with reduced effects, or vanish entirely — per building.
- Laws and policies can **ban** buildings (destroyed while the law holds; repeal rebuilds them)
  or block techs.
- Once a successor unit is trainable, its predecessor drops out of the build list.

### One coherent bonus system

- Food, production, gold, research, culture, espionage, happiness, health, and properties like
  pollution all flow through one unified modifier system — effects from techs, civics, traits,
  buildings, religions, and corporations combine cleanly instead of through dozens of special cases.
- Conditional effects work as advertised: "+1 happiness while powered", "−production while
  polluted", per-military-unit or per-population scaling.
- Happiness and health run a four-channel model (happiness / anger / health / unhealth); excess
  anger translates to rioting population, blocked by amenities.

### UI and information

- City and unit hover details; **ALT** shows a plot-yield breakdown.
- Building tooltips show cost and hammers already sunk.
- "Requires" tooltips name exactly what is missing, item by item.
- Five advisor screens rewired to live data.
- The Civilopedia rebuilt on the new data surface — hundreds of pedia errors eliminated.
- Leader trait display: full trait names, one per line, rendered live pre-game.
- Cities in anarchy show burning/disorder visuals.
- Promotion icons laid out in rows.
- Culture-threshold alert spam fixed.

### Game setup and difficulty

- Map scripts restored — new games generate again.
- New-game start is fully data-driven: starting units, gold, buildings, and techs come from
  start packages conditioned by era, difficulty, and civilization — and all of it announces
  correctly on game start (research popup, start techs, wellbeing cushions).
- Goody huts, traits, civics, and handicap effects all initialize and announce correctly.
- One City Challenge now removes wonder limits outright.
- Difficulty applies as a proper modifier set; flexible difficulty rewires AI advantage on the fly.

### Rules and fixes

- Golden age ends anarchy.
- Valley of the Kings requires Pyramid and Sphinx **in the same city**.
- Shrine and corporation-HQ revenue lands in the owning city.
- Random events repaired: hurricane, cyclone, champion, fires.
- Trade routes: profitability scales with civics, traits, and buildings; coastal/foreign/
  shared-civic routes get their own bonuses; route counts cap by culture level.
- Wonders cap properly as world (one per game), national (one per player), or team wonders.
- Properties like pollution auto-place and remove their band buildings as the value crosses
  thresholds — no per-turn churn.

### Performance

- Building evaluation no longer rebuilds the city per candidate.
- AI loops sweep maintained lists instead of full registries.
- Pathfinder cost fixes; runaway path searches capped.
- Specialist processing reduced roughly 40× per citizen.

---

## For modders — the data platform

### Everything is JSON now

- All ~13,400 game entities across 33 info types (buildings, units, techs, civics, traits,
  religions, corporations, terrain, features, improvements, routes, eras, handicaps, leaders, …)
  migrated from XML to curated JSON, loaded by one reader.
- One JSON file per entity: `Assets/Data/<Type>/<ENTITY_ID>.json`.
- **Cold-read promise**: keys say what they mean — you can read an entity file and understand it
  without engine knowledge.
- Every entity type shares the same top-level shape: availability (`enables`, `requires`,
  `allowed`), provisions (`grants`, `triggers`), effects (modifier families), and metadata.

### Availability: one machine answers "can I build it?"

- `enables` — what an entity unlocks (the forward edge: a tech lists what it opens up).
- `requires.build` — conditions to construct (greyed in the UI if unmet).
- `requires.operate` — conditions to keep running; losing them puts a building into dormancy.
- `allowed` — caps: `world: 1`, `empire: 1`, `team: 1`, or category caps by culture level.
- `obsoletes` / `replacedBy` / `disables` — soft supersession, hard unit replacement, and
  law-driven bans, each with distinct semantics.
- `whenObsolete` — a modifier tree applied to obsoleted buildings; empty means hard removal.
- Entity-level `enabled` / `disabled` gates turn whole entities on or off by game option,
  difficulty, or runtime predicate — no data removal needed.

### Modifiers: one format for every number

- Channels: `food`, `production`, `gold`, `research`, `culture`, `espionage`, `happiness`,
  `anger`, `health`, `unhealth`, `maintenance`, `defense`, `tradeRoutes`, one per property, …
- Three unit kinds per entry: `flat` (+N), `percent` (+%), `multiplier` (×), combined as
  `(base + Σflat) × (1 + Σpercent) × Π multiplier`.
- Scoped deposits: `food.empire.percent` vs `food.city.flat`; empire effects roll down to
  cities, city effects stay local; plural targets (`cities`, `plots`, `units`) name receivers.
- Count scaling: `per: {type, each, scope}` — e.g. happiness per military unit.
- Conditional entries: `enabled` / `disabled` gates on any entry, applied and withdrawn
  automatically when the condition crosses — including age gates (`existedFor`), which is how
  legacy "commerce doubles after N years" authors as a second age-gated deposit.
- Ranked subsets: `orderedBy: CITY_SIZE, max: 5` targets the five largest cities
  [spec — verify: ranked entries currently apply unranked until the selection lands].

### One condition vocabulary, used everywhere

- Combinators: `all` / `any` / `noneOf`, nesting to any depth.
- Atoms: any typed ID (`BUILDING_FORGE`, `TECH_ASTRONOMY`, `BONUS_IRON`) with optional
  `min` / `max` thresholds and explicit `scope` overrides.
- Resource connection filters: `trade` (via network), `vicinity` (radius tile), plus
  `onSite` / `owned` variants.
- Runtime predicates: `IS_CAPITAL`, `HAS_RIVER`, `IS_GOLDEN_AGE`, `HAS_POWER`, `IS_FOREIGN`,
  `SHARES_CIVIC`, parameterized forms like `{HAS_RELIGION: RELIGION_X}`,
  `{latitude: {min: 30, max: 60}}`, `{existedFor: {min: 1000}}`, and more — an extensible
  registry, not a fixed list.
- Property bands: `{type: PROPERTY_X, min: A, max: B}` gates on property value ranges.

### Grants, triggers, start packages

- `grants` — payloads delivered on an entity's considered action (research a tech, construct a
  building, adopt a civic): units, gold, buildings, free specialists, free techs, promotions.
- `triggers` — conditional recurring or event-fired effects: a `trigger` (when), a `chance`
  (odds), and an `action` (grant, spawn, property change, script call).
- Start packages (`STARTPACKAGE_*`) [spec — verify]: named grant bundles gated by era / handicap /
  civ, stacking — specced; the entity type is not built yet (starts currently ride the civilization's
  own grants).

### Classification

- **Tags**: immutable type membership on units (`military`, `air`, `ranged`, …) backing
  predicates, condition atoms, and AI classification. Extensible.
- **Skills**: mutable per-unit abilities (promotions, combat classes) that can be gained or
  lost mid-game.
- **Amenities**: city-held markers granted by buildings/civics/religions (power, freshwater,
  government center, …), used to gate deposits.
- **Capabilities**: team-held grants from techs and civics (trade routes, special units,
  diplomacy verbs).
- Vision, movement, cargo, and hide-and-seek rebuilt as data families on this classification.
- Trait lines: leaders seed both the simple and complex trait sets; a line's base trait enables
  its higher rungs; which set is active is a game option; all trait effects author on the trait itself.

### Live observability

Every state change in the game announces on an event spine, and you can watch from outside
the process:

- **HTTP endpoints** (`127.0.0.1:7227`, GET-only, loopback):
  - `/` — liveness check (`hello world`).
  - `/events` — SSE stream of live game facts (limited concurrent streams).
  - `/computed/...` — decomposition censuses: per-city tri-state building lists with reasons,
    per-entity gate verdicts with the failing leg named. `GET /computed` serves the live index.
- **Log files** (`Documents/My Games/Beyond The Sword/Logs/`):
  - `Cascade.log` — domain facts, modifier applications, grant firings; readable **while the
    game runs**.
  - `XmlLoad.log` — data load census with per-type counts.

### Extending the platform

- **New threshold condition**: `{type: TOKEN, min: N, max: M}` — no engine change needed.
- **New per-thing scaling**: `per: {type: TOKEN, each: N, scope: SCOPE}` — routed by type prefix.
- **New predicate**: define the evaluator, emit a spine fact when its state changes, register it.
- **New modifier family**: register the channel, add it to the family list, emit the spine fact.
- **New entity type**: create the `Assets/Data/<Type>/` folder, register the prefix, add a
  one-row dispatch, create the `_order.json` manifest, author via `_additions/`.

### Validation tooling

- `Tools/XmlValidator.exe -a` — schema and data-load check (run from `Assets/`).
- `Tools/verify-python24.py` — the embedded interpreter is Python 2.4; this catches newer syntax.
- `Tools/verify-spine-fields.py` — event field types match declarations.
- Save-migration checker for the format change (see "Under the hood").
- `readjson.exe Assets/Data --render BUILDING_X` — parse and English-render a single entity.

### Known future work [spec — verify]

- **Volumetric resources**: bonus counts moving from presence-only (0/1) to quantities (0..N)
  is specced as future work; not yet implemented.
- **Events rework**: engine events gaining spine emits (Python callbacks still live) — partial;
  full move to the trigger system is future work.

---

## Under the hood

The engine's derived-state layer was rebuilt completely. One event spine announces every state
change; caches are maintained running sums updated by those events instead of being recomputed
in passes; loading a save and starting a new game build state through the same event-driven
path, so there is one code path to be correct instead of two. Reads are O(1) against the
accumulated value — no re-walk of sources — and invalidation is targeted: an event touches only
the consumers it affects. There is one canonical answer per number: the legacy ad-hoc
accumulators are gone, not shadowed. The save format changed as part of this; old saves migrate
automatically via a named-tag mechanism.
