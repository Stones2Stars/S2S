# 8. Classification — unit `skills`/`tags`/`state`, building `attributes` & empire `capabilities`

> Part of the **[json](../json.md)** spec.

A unit's classification splits into **three blocks, distinguished by lifecycle**. The **operative test: *can a
promotion grant it?***

- **`skills`** — **mutable** unit abilities, gained/lost via promotions (`blitz`, amphibious, walk-on-mountains,
  fly-over-water, …). *Promotion-grantable ⇒ skill.* **A skill is a PURE BOOLEAN ENABLER — the unit mirror of empire
  [`capabilities`](../capabilities.md)** ("can walk over river", "can fly over water", "can pass peaks"): it carries
  **no value**, so a **UNIT authors `skills` as an ARRAY OF STRINGS** (`["pillage","blitz"]`), never `{name:true}` —
  a skill cannot be `false` (absent ⇒ not held). Anything that carries a value is **not a skill**: keyed
  targeting/immunity (`targets`/`unitTargets`/`defenders`) → the combat (`strength`) family; a per-type variant that
  collapses to one enabler stays a skill (`collateralImmune` = immune to the siege-variant collateral). Only where
  **revoke** is real — a **PROMOTION** granting *or removing* a skill — is the object form (`{name:true|false}`) used
  (the grant/revoke plane, skills.md §4). Glossary: [skills.md](../skills.md).
- **`tags`** — **immutable, type-derived** membership: set at creation, re-set on **upgrade**, **purely for
  accounting** — overlapping (`military`/`civilian`/`worker`/`spy`/`landUnit`/`gunpowder`/`mechanized`/…), counted by
  the engine/tally, **no behaviour or modifiers**. Like skills, a tag is pure membership with **no value**, so a unit
  authors `tags` as an **ARRAY OF STRINGS that is ALWAYS PRESENT** (empty `[]` until the unitcombat→tags distillation
  fills it — never optional/absent; there is no real unit with zero tags, but the schema keeps the array mandatory).
  A tag is **queried via its `IS_<TAG>` predicate** — a unit `IS_MILITARY` ⟺ it has the `military` tag (§3.5), a unit
  `IS_LAND` ⟺ `landUnit` (the domain `DOMAIN_*` is a tag: `landUnit`/`seaUnit`/`airUnit`; the enum stays engine-side
  for movement/stacking) — while the *generic* `IS_*` predicates (`IS_WATER`) read game state, not a tag. *Not*
  promotion-grantable (a swordsman must upgrade to a rifleman to gain `gunpowder`).
- **`state`** — **transient** conditions (fired → counted down → over: `paralyze`/immobilise). **Greenfield** —
  never first-class; historically faked via pseudo-promotions + Python events.

The **empire** counterpart to unit `skills` is **`capabilities`** — **team-wide, unlocked** civilization
abilities (found-on-peaks, pass-peaks, move-on-water, tech-trading, irrigation, bridge-building, river-trade,
and the commerce sliders `setScienceRate`/`setCultureRate`/`setEspionageRate`). **Capabilities are empire-HELD but
grantor-PROVIDED** — a **tech**, a **civic**, or a **building** *provides* one, and the empire then *holds* it. A
grantor **provides**, never **holds**; a capability appears in a grantor's `capabilities` block to mean "I hand this
to the empire." (This is exactly parallel to a tech granting an ability — the same block, three grantor kinds.) The
**section name carries the scope**, so the engine never guesses. **Behaviourally nothing is *granted*:** the
empire's active set is **derived on query** — an enabler-style union over the currently-live sources (the enabler's
HAVE axis); "provides" is the data direction, not an apply event, so a capability lapses with its last live source —
headroom only: in practice no capability is ever disabled today. See [capabilities.md](../capabilities.md).

A **building** additionally has its own **`attributes`** block — what the building **IS or DOES ITSELF**, held and
immutable: `teamShare`, `destroyedOnCapture`, `orbital`, `orbitalInfrastructure`. Plain booleans, like
`skills`/`capabilities`, and again the section name carries the scope (building). The **hold-vs-provide
distinction is load-bearing**, and it runs in three directions: `attributes` are the building's own property,
`capabilities` are what it *hands to the empire*, and `amenities` (below) are what it *hands to its own city*. So
`destroyedOnCapture` is an `attribute` (a fact about the building), `setCultureRate` is a `capability` (handed to
the empire), and `nukeImmune` is an `amenity` (it makes the CITY immune — the building is not the thing protected).

> **⚖ A UNIT CARRIES `status`, AND A STATUS IS A PER-TURN COUNTER.** It is **a specific counter that gets
> DECREMENTED EVERY TURN** — applied to the unit, ticking down, and over when it reaches zero. That is the whole
> of what separates it from the unit's other blocks.
> ⚑ **So it is an id→COUNT like a city's `amenities`, but the COUNT MEANS SOMETHING ELSE, and the difference is
> the model:** an amenity's count is a REFCOUNT of live grantors (it moves when a grantor is added or repealed,
> which is what events do to it), while a status's count is **TURNS REMAINING** and moves on its own, every
> turn, with no grantor involved after the moment it was applied. ⛔ Do not fold the two onto one mechanism on
> the strength of both being id→COUNT: one expires, the other is held.
> ⚑ **The READ is therefore the ordinary `ContextDict` one: the status HOLDS while its value is above
> zero** — `hasStatus(id)` ≡ `count > 0` ([contexts.md](../../cascade.md)), so nothing needs a
> separate present/absent plane beside the counter. Expiry is the counter reaching 0, not a second fact.
> ⛔ A status is emphatically **NOT a [skill](../skills.md)**: a skill is an ability the unit HAS, a status is a
> condition something PUT ON it, for a number of turns.
> ⚑ The worked case is **`paralyze`** (immobilises the unit — `setImmobileTimer`), applied by an EVENT through a
> status pseudo-promotion: the promotion is the DELIVERY mechanism, never the holder, so the flag must never
> land in a `skills` block. ⚠ It has been mis-filed as a skill more than once, so the curator no longer maps it
> — an unmapped tag reports LOUDLY rather than emitting into the wrong block. Glossary: [state.md](../state.md).

A **CITY** has its own **`amenities`** block — the **city-HELD, grantor-PROVIDED** counterpart, standing to the
city exactly as `capabilities` stands to the empire. The hold-vs-provide axis otherwise stops one scope short:
`attributes` is what the building *is/does itself*, `capabilities` is what it *hands to the empire*, and
`amenities` is what it *hands to its own CITY* — which is what most authored keys actually do. The two split
by asking whose property it is:

- **`attributes` — about the BUILDING** — `teamShare` · `destroyedOnCapture` · `orbital` · `orbitalInfrastructure`.
- **`amenities` — conferred on the CITY** — `nukeImmune` · `governmentCenter` · `providesFreshWater` ·
  `providesPower` · `abolishedAnger` · `abolishedUnhealthFromPopulation` · `abolishedUnhealthFromBuildings` ·
  `adds3rdRing` · `borderObstacle` · `forceAllTradeRoutes` · `capital` · `protectedCulture` · `zoneOfControl`.

> **⚖ THE WELLBEING OFF-SWITCHES ARE ONE NAMED FAMILY — `abolished<Channel>` optionally `From<Source>` (> "a group of names that all tell the same story for different targets).** They are HARD off-switches, not
> modifiers ([modifier.md §2b](../../cascade.md)): the side ceases to exist rather than being reduced. The unqualified
> form abolishes the channel from EVERY source; a `From<Source>` suffix narrows it to one:
>
> | key | abolishes |
> |---|---|
> | `abolishedAnger` | anger, all sources |
> | `abolishedUnhealth` | unhealth, all sources |
> | `abolishedUnhealthFromPopulation` | the population term only |
> | `abolishedUnhealthFromBuildings` | the building term only |
>
> ⚑ It extends to the remaining wellbeing sources (features, bonuses, corporations, specialists — §2b's deposit
> list) without coining a new spelling each time, which is the whole point of the family: a reader meeting an
> unfamiliar one already knows what it does.
> ⛔ **The legacy spellings it retires each hid something in the NAME.** `noUnhappiness`/`noUnhealthyPopulation`
> used the `no…` negation; `buildingOnlyHealthy` named a CONSEQUENCE ("buildings are only ever healthy") rather
> than the mechanic; and `noCapitalUnhappiness` baked the WHERE in — which is the condition-as-member shape
> [conditions are predicates, never bespoke members](03-the-shared-vocabulary.md#35-predicates--a-systems-runtime-state-query) retires, and it is
> now `abolishedAnger` gated `IS_CAPITAL`. ⛔ A future narrowing is a PREDICATE or a target, never a new key
> spelling.

⚑ **The grantor is not only a building, and the SCOPE says how far it reaches (a city or cities).** A
building's `amenities` land on its OWN city; a civic / trait / tech authoring the same block reaches EVERY city
of the empire — the ordinary scope spine (§3.2), on the same derived-union-over-live-sources mechanic
`capabilities` uses, so no new machinery and no per-grantor special case.

> **⛔ STORED AS AN ID→COUNT DICTIONARY, NEVER A BITSET — absent or 0 is false, anything else true.**
> *Several* sources can confer the same amenity, so the city holds a COUNT per amenity id and a removal
> decrements it: losing one power plant must not darken a city that has two. A bitset cannot express that — an
> "amenity removed" fact would clear a bit another live source still justifies. ⚑ This is the existing
> `ContextDict` (`id → count`, `has(id)` ≡ `count > 0`, [contexts.md](../../cascade.md)), the same
> refcount shape the enabler's membership formula and the operating set's provided-bonus counts already use —
> and the semantic legacy had right all along in its per-flag counters. The city read is therefore O(1) and the
> ⛔ **REMOVAL-WINS trap is structurally absent**, exactly as it is for enabler membership.
> ⚑ **AND THE DICTIONARY IMMEDIATELY SOLVES VOLUMETRIC — a second payoff, free.** Because the slot is
> already an int rather than a bit, an amenity that later becomes a QUANTITY (power CAPACITY a city draws
> against, rather than a yes/no) needs **no reshape** — only a change in what the number means. That is the
> reasoning [contexts.md](../../cascade.md) already applies to power specifically ("power carries 0/1
> today but stays `int` so a future volumetric model needs no reshape"), generalized to the whole block. ⛔ So
> the count is never to be "optimized" into a bitset on the grounds that every value happens to be 0 or 1 today.
>
> ⚠ `nukeImmune` is the standing exhibit for why the split is load-bearing: the same key means two DIFFERENT
> mechanics on two carriers — a BUILDING's makes its **city** immune (so it is an `amenity`), while a plot
> substrate's means the **feature itself** survives (a `characteristic`). One word, two mechanics, kept
> separable only by the blocks being distinct.

> **⚖ A GRANT MAY BE CONDITIONED — the ENTRY carries it, never the KEY (make the block carry the
> condition).** A classification entry's value may be the §3.9 entry object instead of a bare `true`, so a
> grantor states *when* its grant applies:
>
> ```jsonc
> "amenities": { "abolishedAnger": { "enabled": "IS_CAPITAL" } }   // a civic: the CAPITAL only
> ```
>
> ⛔ **The condition is evaluated PER RECEIVER, at FOLD time, on the grant AND on the repeal** — not once at
> load. A city that gains or loses capital status must therefore re-fold, exactly as one that gains or loses the
> grantor does; a gate read only on arrival strands the amenity wherever it last landed.
> ⚑ **This is what retires a WHERE baked into a key NAME.** The legacy `noCapitalUnhappiness` encoded its
> condition in its spelling — the condition-as-member shape
> [conditions are predicates, never bespoke members](03-the-shared-vocabulary.md#35-predicates--a-systems-runtime-state-query) retires — so it is
> ONE key, `abolishedAnger`, gated by `IS_CAPITAL`, never a second key meaning "the same thing but over there".
> ⛔ And the reverse is equally binding: dropping the condition to keep the block a plain bitset would abolish
> anger in EVERY city. A shape whose only faithful reading is the wrong behaviour must not ship.

The **PLOT SUBSTRATE** has its own **`characteristics`** block — the **HELD**, immutable, **plot-scope** intrinsics
of whatever IS the plot: **terrain · feature · improvement · route**, all four carriers, one block and one registry
(the §3.2 spine's `plot{improvement|feature|terrain|route}`). Plain booleans like its siblings, the section name
carrying the scope. `countsAsPeak`, `actsAsCity`, `bombardable`, `zoneOfControl`, `ignoreTerrainCulture`,
`nukeImmune`, …

> **⚖ ITS OWN BLOCK, NOT `attributes` EXTENDED — names are never conflated.** The substrate is a distinct
> carrier at a distinct scope, so it gets a distinct word rather than borrowing the building's. The cost of one
> more vocabulary entry is trivial; the cost of a name that means two things is paid forever by every reader who
> has to work out which one is meant.
> ⚠ **`nukeImmune` is the standing exhibit for why: the SAME key names two DIFFERENT mechanics on two carriers.**
> A BUILDING's `nukeImmune` makes its **city** immune; a plot substrate's means the **feature itself survives** the
> blast. Separate blocks keep them separable — a single shared block would have quietly merged them.

⛔ **The bound — does it describe THE THING, or WHERE THE THING MAY GO?**

- **A CHARACTERISTIC describes the substrate itself**, intrinsically and always, including what may be done *to*
  or *on* it: `unfoundable` (no city may be founded here), `unimprovable` (no improvement may be built here),
  `prohibitsBonus`, `bombardable`, `nukeImmune`, `actsAsCity`, `countsAsPeak`. These are read BY other systems —
  the founding gate, the worker-build gate, bonus placement — but they are properties of the feature, not
  conditions the feature is tested against.
- **A PLACEMENT CONDITION says where this substrate may EXIST**, and is evaluated against the plot's live state:
  `requiresFlatlands`, `validTerrains`, `noCoast` / `coastalOnly`, `noRiver` / `requiresRiver`, `noAdjacent`.
  Those author in `requires` (§4) like any other condition.

⚑ **The grammatical tell tracks the real one:** a characteristic is an adjective about the thing (`bombardable`,
`unfoundable`) — a placement condition names the ground it needs. ⚠ Do not read "it restricts something else" as
disqualifying: `unfoundable` restricts city founding and is still a characteristic, exactly as `bombardable`
restricts nothing and is one. The question is whose property it is, never who is affected by it.

**Two further unit blocks:**

- **`builds`** — the unit's per-type **`BUILD_*` repertoire** (which worker-builds it can perform), owned **per unit-type**
  (tech gates *which builds are unlocked* — via `enables.builds` / the BUILD's own prereq; `builds` is *which THIS unit
  can do*; NOT "all workers, tech-gated"), promotion-augmentable. Wired as an **intrinsic key** (the readJson base skips
  it; `CvUnitInfo` parses it). Same shared-vocabulary word as `enables.builds` — a `BUILD_*` list either way; the
  enclosing section gives the relationship (`enables.builds` = "unlocks these," unit `builds` = "can perform these").
- **`missions`** *(PERMANENT carve-out — missions/CvOutcome ground-up rework)* — the actions a unit **performs**, each
  producing an **outcome**; **a mission carries its `grants`** — the outcome (what lands) IS the mission's grant payload.
  Unifies the hardcoded mission-abilities (MISSION_CONSTRUCT/DISCOVER/GOLDEN_AGE — carried today by `grants.buildings` /
  `greatPersonAction` / `goldenAge`) with the **`CvOutcome`** system (`CvUnitInfo` `KillOutcomes` + `m_aOutcomeMissions`
  — data-driven MISSION→outcome-list with cost/conditions/kill). The CvOutcome DATA is ALREADY JSON-migrated into the
  `outcomes` block below; what this future `missions` block adds is the CONCEPT unification with the
  hardcoded abilities.
  The distinction from `skills`: a **skill** is a standing/permanent property; a **mission** is an action (often
  consuming the unit). `grants` is therefore BOTH an entity-level handout AND a mission's outcome payload.
  > **⚖ OUTCOME PAYLOAD VOCABULARY — the `outcomes` block uses clean VERB-PER-PAYLOAD keys.**
  > `outcomes.kill[]` (combat-kill) / `outcomes.actions[]` (missions), each entry
  > `{ requires:{outcome:OUTCOME_*, plot?, unit?}, chance, <reward verbs> }`. Each effect is a verb, collision-checked
  > against the reserved words (avoiding `builds`/`provides`/`grants`/`construct`):
  > **`spawns`** `{unit,toCity?}` · **`places`** a bonus · **`promotes`** · **`triggers`** an event ·
  > **`consumes`** the unit · reused families for one-shot yields (`food`/`production`/`commerce`/`gold`/…),
  > `greatPeople`/`population`/`revolution`, `happiness:{duration}`, `PROPERTY_*`; `{python}` for Python-authoritative
  > outcomes. **The engine CONSUMES this** — the `CvOutcome` classes are fed from it via `mapFrom` (the CvOutcome
  > engine/dispatch is unchanged, just JSON-loaded; conditions eval through `cascadeEvalCondition`, no BoolExpr
  > round-trip). `Adapt*` gamespeed scaling is pure-engine, applied at grant time — never in the data. See
  > [mission-outcome-system.md](../../reference/mission-outcome-system.md).

A **building's** `grants.traits` (a whole trait conferred on the **OWNER** empire *while the building is active*, reverting
on loss — `owner.setHasTrait`, civ-traits only, `CvCity.cpp:4614`) is the same grantor-**provides** / empire-**holds**
pattern as `capabilities` — but the held thing is a full **trait** (effect-bundle), not a boolean ability. So it re-homes to
**`enables.traits`** (the building contributes trait T to the empire's active-trait HAVE, which the modifier already reads),
NOT `grants` (it is held-while-active, not a one-shot handout).

```jsonc
"skills": [ "amphibious", "blitz" ],                        // UNIT skills: pure boolean ENABLERS -> array of strings
"tags":   [ "military", "gunpowder", "landUnit" ],          // UNIT tags: immutable membership -> always-present string array
"combatClass":  "UNITCOMBAT_GUN",                           // UNIT: primary combat class -> ROOT (not identity)
"combatClasses": [ "UNITCOMBAT_SPECIES_HUMAN", "…" ],       // UNIT: sub combat classes -> ROOT
"strength": { "unit": { "flat": 26 } },                     // UNIT base STRENGTH (a modifier family, not identity.base; absent if it can't attack/defend)
"movement": { "unit": { "flat": 1 } },                      // UNIT base MOVES (the movement subsystem, not identity.base)
"attributes":   { "teamShare": true, "destroyedOnCapture": true }, // BUILDING: what it IS/DOES itself (held)
"amenities":    { "nukeImmune": true, "providesPower": true },     // city-HELD, grantor-PROVIDED (id->COUNT on the city)
"capabilities": { "moveOnWater": true, "setCultureRate": true }    // empire-HELD, grantor-PROVIDED (tech/civic/building)
```

> **⚖ The classification categories are RUNTIME-GENERATED INFOS ([the classification-infos registry](#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)).** Every distinct
> authored block key mints one generated info at load — the camelCase key becomes an `INFOTYPE_NAME` id
> (`"setScienceRate"` → `CAPABILITY_SET_SCIENCE_RATE`; prefixes `SKILL_` / `TAG_` / `ATTRIBUTE_` / `CHARACTERISTIC_` / `CAPABILITY_` /
> `POLICY_`, [naming.md](../naming.md)) registered in the global infotype map and created as an info in its category's
> repo — *"clear data to refer to, even if they are only in essence a boolean switch."* Nothing is authored per
> category (no data folder): the registry derives from the union of keys across all entities
> (`ClassificationRegistry`, minted append-only per load), and every entity's blocks resolve their names to by-id
> bitsets, so the whole classification getter surface is an O(1) bit test — never a per-call string lookup
> ([materialize at mapFrom](../../architecture/patterns/07-materialize-at-mapfrom-no-runtime.md#materialize-at-mapfrom--no-runtime-string-reads-in-info-getters-the-single-source-laws-load-time-sibling)). A block authors BOTH
> planes: `true` = grant, `false` = revoke (the skills.md §4 grant/revoke pairs ride the same two-plane block).
>
> **⛔ These registries are OPEN BY DESIGN — the member set grows with authored data, permanently.** Because
> the categories mint from the union of authored keys, identifying new **tags / skills / capabilities / attributes / characteristics /
> policies** is an ONGOING activity expected to continue for the life of the mod — a new one is authored data that
> mints its info, never an engine change. So a glossary ([tags](../tags.md) / [skills](../skills.md) /
> [capabilities](../capabilities.md) / [state](../state.md)) is **never "incomplete" against a finish line**: it catalogues
> the members identified so far, and more arriving is the normal state, not a gap to close. The building counterpart
> of unit skills is **`attributes`** (held city-scope) — same open-registry rule.

Full glossaries: [skills.md](../skills.md) · [tags.md](../tags.md) · [state.md](../state.md) · [capabilities.md](../capabilities.md).

---

