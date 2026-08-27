# 5. `grants` — pure payload on the considered action · `triggers` — when/why → odds → effect

> Part of the **[json](../json.md)** spec.

> **⚖ TRIGGER IS THE TOP-LEVEL CONCEPT — A GRANT IS A TRIGGER WITH A NULL CONDITION.** The two are one
> plane in the MACHINE: `triggers` is the general form (a happening, odds, an action) and `grants` is its
> **degenerate case** — the happening is implicit (the source's own considered action), there is no condition and
> no roll, so the action simply applies. That is why one engine and one spine domain (`SD_TRIGGERS`, the
> `[TRIGGERS/*]` tags) serve both, and why nothing about a grant needs its own machinery.
> ⚑ **`grants` remains a first-class AUTHORING shape** — *"we allow grants directly, but it works out of
> triggers"*. A modder writes the plain `grants` block below for the overwhelmingly common "acquiring me gives
> this" case and never spells out a trigger; the split that follows is therefore about AUTHORING, not about two
> runtime mechanisms.
>
> **The split.** **`grants` holds ONLY what is given on the CONSIDERED ACTION** — the source's own
> realization moment, whatever that is: a building's construction, a tech's research, a civic's adoption, a
> settler's FOUNDING, a mission's execution. No trigger field, no odds, no recurrence — "acquiring/doing me
> gives this," full stop. **Everything that fires on anything ELSE is a `triggers` entry**: the WHEN/WHY comes
> FIRST (`per` — the cadence/happening: a turn, a population growth, …— plus `chance`, the odds), THEN what it
> does if successful — and the effect plane is wider than granting (burn down a building, spawn a unit, move a
> property, grant something). Odds live on the trigger, never inside a payload.
>
> **The entry anatomy is three named parts, in reading order: `trigger` → `chance` → `action`.**
>
> ```jsonc
> "triggers": [
>   { "trigger": { "type": "PROPERTY_FLAMMABILITY", "min": 200 },      // WHEN/WHY -- the shared §3 vocabulary
>     "chance": 5,                                                      // the odds (percent; may carry a per-scaler)
>     "action": { "destroy": { "building": "random" } } },              // WHAT IT DOES if successful
>   { "trigger": "onTurn",
>     "chance": { "value": 5, "per": { "type": "PROPERTY_CRIME", "scope": "city" } },
>     "action": { "grant": { "units": ["UNIT_PROPERTY_CRIMINAL"] } } }
> ]
> ```
>
> - **`trigger`** — the when/why, two composable forms and no bespoke dialect: an **`on<Happening>` token** —
>   the spine's DOMAIN events in authoring form (`onCreation`, `onFound`, `onTurn`, `onPopulationGrowth`, … an
>   OPEN registry, one name per spine fact) — and/or a **state condition** in the shared §3 vocabulary
>   (atoms/predicates/combinators — the fire band above); a state-only trigger is evaluated each turn. An
>   `on*` happening may be ANOTHER entity's moment in scope (a building acting `onCreation` of a unit in its
>   city; a trait acting `onFound` of each new city) — that is exactly what distinguishes a trigger from a
>   grant (the source's OWN considered action, implicit, never written).
> - **`chance`** — the odds, always here on the trigger, never inside a payload; scalable by the §3.7 `per`. A
>   `chance` carrying ONLY a `per` (no `value`) means the scaled count IS the odds — the roll is
>   per-count-derived (the property-spawn shape the data authors).
> - **`action`** — an OPEN verb registry: **`destroy`** · **`grant`** (the §5 payload vocabulary nested whole) ·
>   `spawn` / `place` / `promote` / property deltas / anything else the data needs — one verb vocabulary shared
>   with the §8 outcome plane (the outcome verb `triggers` renames to **`fires`** to clear this section's name).
>   ⚖ **The action's SUBJECT defaults to the entity the trigger is authored on — `"destroy": "self"`.**
>   A verb needs no way to name its own carrier, exactly as a `grants` happening never names the source whose
>   considered action it is. ⛔ So do NOT extend the `SELF` count-token (§3.1) into a target vocabulary; the
>   off-spine `self` scope (§3.2) is the word, and the carrier is implicit everywhere else.
>
> ⚖ **A trigger may read an event from UP the containment spine — the plot hears its city.** The spine
> (§3.2) puts `plot` directly under `city`, so a city's happening reaching the plot beneath it is an ordinary
> downward flow, not a special case: *"the city knows what plot it's on, so the plot can handle the events from
> the city — if a feature is on the plot, it reads the pop-increase event from the city, and voila"*. The worked
> case is a feature destroyed as its city grows: `trigger: {type: POPULATION, scope: city, min: N}` +
> `action: {destroy: self}`, off the `SEVT_CITY_POPULATION_ADDED / _REMOVED` fact the spine already carries.
> ⚑ This is what lets ONE condition replace a legacy special case: the engine destroyed such a feature at
> FOUNDING when its threshold was 0 or 1 and at `newPop >= N` thereafter — two code paths that are uniformly
> "city population ≥ N", since a founded city always has population ≥ 1.
> - The OUTCOME plane already conforms — a mission's roll (`chance` + the per-promotion `odds` table) is the
>   trigger, its verbs the action; nothing re-homes there.
>
> ⚖ **DIRECTION: a future `removes` verb, mirroring `grants`, belongs on the PAYLOAD plane, never the
> enabler's.** The take-away side today exists only as scattered partial verbs (the `destroy` action verb here,
> `consumes` on the §8 outcome plane) while the give side has a whole named vocabulary. The enabler's
> `disables`/`obsoletes`/`replaces` are **availability RULES** — standing edges, evaluated continuously
> ([enabler.md §2](../enabler.md)), where repealing the law brings the building back — while `removes` would be a
> **one-shot PAYLOAD**: this action, now, takes this away, with nothing to re-evaluate, exactly as `grants` is a
> payload rather than an availability edge. Building it as an enabler edge would make a momentary effect into a
> permanent rule. If it lands: curator + regen ride in the same work item
> ([recurate on every decision](../../../AGENTS.md#git--delivery)), with the scattered
> `destroy`/`consumes` verbs as migration input.

> **`grants` is ONLY genuine provisions handed out on the considered action.** A unit's MISSION_CONSTRUCT
> repertoire — the buildings it can hand over by consuming itself — IS `grants.buildings` (the construct mission
> reads exactly that surface; the founder's list and the hero's are one grammar, distinguished only by the
> considered action that delivers them: founding vs the mission). What does NOT belong here (and where it lives
> instead): `greatPersonAction` / `goldenAge` → **`missions`** (§8 — the rest of
> the mission-CONCEPT unification is a PERMANENT carve-out: missions/CvOutcome ground-up rework);
> `builds` → the **`builds`** block (§8); promotion `unitCombats`/`removesUnitCombats` → **`skills`**; project
> `grantsSpecialBuilding` → **`enables.specialBuildings`** (flips SpecialBuildingValid — unlocks, hands out nothing);
> corp `bonusProduced` → **`provides.bonuses`** (continuous supply, §5a); building `holyCity` → **`requires.build`**
> (a read-only "only in RELIGION_X's holy city" gate — `canConstruct`, `CvCity.cpp:2728`; the holy city is set by
> religion FOUNDING, never a building); building `traits` → **`enables.traits`** (held-trait, §8).
> **`freePromotions`** (building-list + trait-dict) is a **`triggers`** entry (`onUnitEnteredCity` → promote the
> units present; the callout below). And a
> **mission carries its `grants`** as its outcome (§8), so `grants` is both an entity-level handout and a
> mission's outcome payload.

```jsonc
"grants": {
  "techs": ["TECH_POTTERY"], "units": ["UNIT_WARRIOR"],   // entity lists
  "population": 1, "revolution": -100,                     // numeric pulses: grants.<channel>: value
  "buildings": [ { "building": "BUILDING_PALACE" } ]       // on a SETTLER: the considered action IS founding
}
```

- **lists** — `buildings · units · techs · civics · promotions · traits · bonuses · specialists`.
  ⛔ **`specialists` is a NARROW carve-out, and the test is the LIFETIME — never the payload.** Free specialists
  are ordinarily the `freeSpecialists` MODIFIER family ([modifier.md §6](../../cascade.md)): alive-with-source, dying
  with the building or civic that pays for them, on the two-part amount/placement seam. Authoring THOSE as a
  grant is the retired shape ([superseded-ideas #10](../../architecture/superseded-ideas.md)) and stays retired.
  A specialist is a GRANT in exactly one case: a **persisted PULSE that outlives its source** — handed over
  once, never reclaimed. ⚑ The live instance is the trait's ERA-ADVANCE specialist: it fires on the era
  advancing (`onEraChanged`, §5's trigger plane — not the trait's own considered action) and lands in the
  city's **UNATTRIBUTED** typed-free ledger, so losing the trait does not take it back. ⛔ So the question to
  ask is never "is this a specialist?" but "does removing the source remove it?" — if it does, it is the
  modifier family and belongs nowhere near `grants`.
- **numeric pulses** — `grants.<channel>: value` (`grants.revolution: -100`, `grants.goldenAge`).
- **Founder buildings are PLAIN `grants.buildings` on the settler** — the settler's considered action IS
  founding, so no bespoke section exists; an entry may carry `enabled` (`{ "building": BUILDING_X, "enabled"?:
  <condition> }`). **A settler granting buildings at settle time is a NEW mechanic coined for this rework
 — there is no legacy engine apply to port.** Do not go looking for one: the legacy `bNewCityFree` path
  (`CvPlayer::found`, gated on `isNewCityFree()`) is a DIFFERENT, now-dead mechanic that merely sits at the same
  call site. This lands with the grants machine's apply-loop
  ([grants-machine.md](../triggers.md) increment 5); the data is authored and
  waiting. The settler ALONE carries the founder buildings — no civilization authors a duplicate in its own
  `grants.buildings`.
  > **⛔ EVERY founder provision rides this shape — the NUMERIC PULSES too, not just buildings.** A
  > trait's start culture and its bonus starting population are **conditional grants that live on the FOUNDER**:
  > `grants.culture` / `grants.population` on the settler, with the trait as the entry's `enabled` condition.
  > Both channels are already §5 numeric-pulse vocabulary, so this needs no new key and no new machinery —
  > it is the founder shape above, applied to the payloads it was always meant to carry.
  > ⛔ **There is NO founding SECTION, and inventing one is the recurring rollerskate this callout exists to
  > stop.** A `cityFounding` block was minted by an agent, written into this spec's bespoke list, emitted by
  > the curator and read by `CvTraitInfo` — so every layer ratified it and the next reader found it sanctioned.
  > It is fantasy: the settler's considered action IS founding, which is exactly why the payload needs no home
  > of its own. ⚠ A standing per-city EFFECT is a modifier and belongs to its own family; a one-shot pulse
  > handed over AT founding is a grant. Neither is ever a bespoke section.
- **Recurring / chance-rolled / state-conditioned handouts are NOT grants — they are `triggers` entries**
  (trigger → chance → `action.grant`): the old `repeatable` wrapper and its `interval` field dissolve into the
  trigger; `freePromotions` is a `triggers` entry whose action promotes the units present.
  > **⚖ ITS HAPPENING IS `onUnitEnteredCity` — there is NO per-turn sweep on this plane.** The applier is
  > TARGETED PROPAGATION off the unit entering, with the source going ACTIVE completing the same relation for
  > units already present — the two-leg fold shape. The rescan that shape replaced measured 42,336 assign calls
  > in ONE turn, nearly all re-checking promotions the units already held, so an end-turn pass is not to be added.
  > ⚑ `action.promote.units: "present"` states the RELATION *(active source × unit present)*; the happening states
  > when it is re-checked. Both legs together are what keep it current — neither is a cadence.
  > ⛔ **The genuine per-turn trigger `onTurn` is a DIFFERENT thing and STAYS** — a recurring roll is a real
  > happening, and the property-scaled criminal spawn above is exactly it. Do not read this as trimming the turn
  > trigger; it retires ONE fossilised token, not the cadence vocabulary.
  > ⚑ **AND THE GATE IS PER PROMOTION, NOT PER ENTRY.** `action.promote.promotions` is a list of ordinary §3.9
  > entries — a bare `"PROMOTION_X"`, or `{"promotion": "PROMOTION_X", "enabled": <condition>}` — because ONE
  > source arms different unit classes differently from a SINGLE entry (a Riding School's `mounted` promotion
  > beside an unconditional one). The gate is the legacy `<FreePromotionCondition>`, mapped onto the `IS_<TAG>`
  > predicate (§3.5), and it reads the UNIT being promoted.
  > ⛔ **A reader that takes only the bare string does not degrade to "ungated" — it drops the conditioned entry
  > WHOLE, so the promotion reaches NOBODY.** That is the silent shape to watch for on every §3.9 list: the
  > unconditional authorings keep working, so the feature looks alive while every targeted one is missing.
- **Property pulses are `triggers` entries carrying spatial intent in the action** — a per-turn `PROPERTY_*`
  change an entity emits (the engine's `PropertyManipulator`):
  `{ "trigger": "onTurn", "action": { "PROPERTY_AIR_POLLUTION": -5, "on": "plot", "relation": "near",
  "distance": 1 } }`. **Properties are first-class** (early design decision) — a property source is **never a
  parked raw block**; the action carries the `on`/`relation`/`distance` so the (#429) spatial distribution reads
  its target from here. A scaling (non-`CONSTANT`) source carries a `per` count-scaler in its `chance`/value; a
  flat (`CONSTANT`) source is the bare amount.

---

