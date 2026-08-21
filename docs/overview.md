# The Cascade Rebuild — how the S2S engine works, and what it replaced

> **What this is.** A guided tour of the engine's data side: how game data is stored, how "can I build
> it?" and "how much does it produce?" are answered, how state changes are announced, and how things get
> handed over. It exists so a newcomer — human or agent — can get a real overview in one read.
>
> ⛔ **It is a PRIMER, not an authority.** Every ruling below is stated in full in the doc that owns it, and
> this page LINKS there rather than restating it. Where this page and a spec disagree, **the spec wins**; where
> a spec and the tree disagree, **the tree wins** ([AGENTS.md](../AGENTS.md)). Use this to find the right file,
> then read that file end to end.
>
> **The owning docs:** [json.md](specs/json.md) (the data model) · [enabler.md](specs/enabler.md) ("can I?") ·
> [cascade.md](cascade.md) ("how much?") · [spine.md](spine.md) (the event spine) ·
> [triggers.md](specs/triggers.md) (provisions) · [north-star.md](architecture/north-star.md) (the compass).

Stones2Stars is a mod of *Civilization IV: Beyond the Sword*, descended from Caveman2Cosmos. Over one
development cycle its derived-state layer was rebuilt from the ground up. This is what changed and why each
piece is shaped the way it is.

| | |
|---|---|
| entities migrated XML → JSON | 13,088 across 37 types |
| legacy XML retired | ~570,000 lines |
| distinct state facts on the spine | 161 |

---

## 1. The machine we inherited

### A compiler from 2003

The game executable is closed and was compiled in 2003, and everything we ship must stay ABI-compatible with
it. So the DLL is built with the Microsoft Visual C++ Toolkit 2003: genuine C++03, 32-bit, against Python 2.4
and Boost 1.32. No `std::thread`, no lambdas, no `auto`, no `nullptr`, and a ~3.2 GB address-space ceiling we
actually hit.

This is a hard compiler limit, not a style preference. It constrains **syntax**. It turns out it does not
constrain architecture nearly as much as everyone assumed. (Details: [reference/engine.md](reference/engine.md).)

### Data as 570,000 lines of XML

Civ4's data model is XML: a set of `CIV4<Thing>Infos.xml` files, each validated against a hand-written schema,
each loaded into a C++ class with one member per tag. Across this mod that is 63 info files and roughly 570,000
lines of XML.

The C++ side is what decided how much a change cost. `CvBuildingInfo` alone was **4,008 lines with 218 data
members and 292 getters**. Adding one field to a building meant touching the XML, the schema, a member, the
reader, a getter, and usually a Python binding — six places in lockstep for one number.

And the format leaked the implementation. From the real, shipped Forge:

```xml
<YieldModifiers>
    <iYield>0</iYield>
    <iYield>15</iYield>
</YieldModifiers>
```

That is *+15% production*. You can only know it if you know yields are an enum whose second entry is
production — the array is positional, the labels are absent, and a trailing zero is simply omitted. Nothing in
the file says "production".

### The relationships were stored backwards

The Forge's XML entry is 144 lines and contains **no statement of what the Forge unlocks**. The nineteen
buildings it leads to each name the Forge in their own prerequisite list, pointing the other way.

So the question every tech tree, build list, tooltip and AI evaluation actually asks — *what does this
unlock?* — could not be answered by reading the thing. It was answered by scanning every building in the
database and testing each one. That pattern is pervasive; there are still **628** registry-bounded loops in the
AI and engine code today (`python Tools/verify-registry-scans.py`).

### Three ways to say "no longer available"

The same Forge entry carries `ObsoleteTech`, `ObsoletesToBuilding` and `ReplacementBuildings` — three tags for
three different concepts, none stating its own semantics, two actively misleading:

- `ReplacementBuildings` looks like removal and is not. The engine only *disabled* the building while the
  successor existed and re-enabled it when the successor went away. Reversible dormancy, wearing the name of a
  deletion.
- `ObsoletesToBuilding` reads like "what obsoletes me" and means the opposite: *what I turn into* when my own
  tech obsoletes me. A destination, named like a cause.

⚑ Writing this section is what exposed a live contradiction in the shipped data — the successor was listed
*both* as obsoleting the predecessor and as parking it, in **1,521 of 1,522** buildings. Both cannot be true:
obsolescence is checked before the still-running verdict, so it wins and destroys what the chain meant to park.
The rule now is that no building ever obsoletes a building ([enabler.md §2](specs/enabler.md)).

Requirements were split across two unrelated mechanisms in one entity too: `PrereqBonuses`, a flat list with
implicit OR checked continuously, and `ConstructCondition`, a bespoke nested expression language with its own
vocabulary (`<GOMType>GOM_BONUS</GOMType>`) checked only at build time.

### Derived state nobody could trust

Every derived value — a city's yields, its happiness, its maintenance — was a hand-maintained cache with
ad-hoc, gappy invalidation. A building is constructed and a `processBuilding` routine adds its contributions
into per-city accumulators; it is removed and a matching routine subtracts them. There is no single "the source
changed, refresh me" primitive anywhere.

One disease, many symptoms:

- A building goes dormant and its improvement yield is never decremented — the city keeps producing from
  something that stopped working.
- Two screens report different worked-plot yields for the same city at the same moment, because they walked
  different accumulators.
- Those accumulators are **serialized into the save**, so they carry years of history no live source can
  reproduce. Recomputing and diffing doesn't reveal a bug; it reveals the stored number has been drifting for
  as long as the save has existed, with no way to tell which side is right.
- The workaround for a cache nobody could trust was a getter that recomputes on every read — correct, and
  paying full price on the hottest paths. One was measured at **913 million plot reads in a single turn**
  inside the governor's valuation loop.

### Nothing announced anything

There were interface "dirty" bits — flags telling the UI to repaint, carrying no information about *what*
changed — and a reporting hub that called into Python for a fixed list of happenings, so a fact reaching Python
was invisible to every C++ consumer. Between them, no component could ask "what just changed?" and get an
answer. Which is why every cache had to guess.

### And you could not watch it run

The engine's log files are held open by the process, so they cannot be read while the game runs. There was no
state endpoint and no event stream. To learn what the engine believed, you looked at the screen and inferred —
which makes deleting anything dangerous, because you cannot demonstrate that a replacement does the same job.

### The shape of the problem

> A cache is only necessary when inputs can arrive **unannounced**. Every staleness flag is a formal claim that
> we do not know what changed. If every mutation announced itself, that claim would be false by construction —
> and the flag would be a lossy summary of an answer already in hand.

That sentence is the whole rebuild. Everything below is what it costs to make it true.

### The same five questions

| The question | Before | After |
|---|---|---|
| What does this unlock? | Not stored. Scan every entity and test each one. | A forward edge on the entity — `enables`. |
| Can I build it? | Scattered ad-hoc checks re-scanning the database, answering yes or no. | One two-pass machine over a small frontier, answering *and saying why not*. |
| How much does it produce? | Hand-maintained accumulators, serialized, drifting. | One summed slot per channel per scope, applied by the fact that moved it. |
| What just changed? | Unanswerable. Repaint flags and a Python callback hub. | 161 named facts on one spine. |
| What does the engine believe now? | Look at the screen and infer. | Read it off an HTTP endpoint, a live stream, or a log written while it runs. |

---

## 2. Four machines, one job each

The first ruling was organisational. The data side is **four separate systems**, each with exactly one job,
chained in one direction. → [architecture/north-star.md](architecture/north-star.md)

| System | Its one job | Ends at |
|---|---|---|
| `readJson` | Puts the authored data *into* the infos | The info is populated — nothing else is its business |
| `infos` | *Serve* that data, in the shape consumers need | Handing data out; an info never computes with it |
| `cascade` | Sums modifiers — *"how much?"* | A magnitude |
| `enabler` | What we have and can get — *"can I?"* | An availability verdict |

Beside them sits the **tally** — *"how many?"* — not a fifth system with state of its own but a read-only
accessor over counts the game objects already own ([specs/tally.md](specs/tally.md)).

```
                                          ┌──────────────┐
                                     ┌───▶│   cascade    │──┐
                                     │    │ "how much?"  │  │
  ┌──────────┐    ┌──────────┐       │    └──────────────┘  │  asks   ┌ ─ ─ ─ ─ ─ ─ ─┐
  │ readJson │───▶│  infos   │───────┤                      ├────────▶   tally
  │ puts in  │    │ serve it │       │    ┌──────────────┐  │         │ "how many?"  │
  └──────────┘    └──────────┘       └───▶│   enabler    │──┘          ─ ─ ─ ─ ─ ─ ─ ┘
                                          │  "can I?"    │
                                          └──────────────┘
```

**The test for any new code is one question: whose job is this?** If the answer names two systems, the design
is wrong — not the implementation. Nearly every boundary defect hit was one system doing another's job, which
is why they presented as unrelated bugs and got fixed one at a time.

---

## 3. The data reads cold

Every entity is **one JSON object in its own file** under `Assets/Data/<type>/`.
→ [specs/json.md](specs/json.md)

The promise the format is held to:

> A well-authored file is understandable with zero engine knowledge. Keys say what they mean; values say what
> they are. If a shape only makes sense once you know the C++, it is wrong — the engine is built to fit the
> data.

The same Forge, abridged to the parts that answer the same questions:

**Before — 144 lines of XML**

```xml
<PrereqTech>TECH_METAL_CASTING</PrereqTech>
<ObsoleteTech>TECH_NANOMINING</ObsoleteTech>
<ObsoletesToBuilding>BUILDING_FOUNDRY</ObsoletesToBuilding>
<ReplacementBuildings>
  <BuildingType>BUILDING_FOUNDRY</BuildingType>
</ReplacementBuildings>
<PrereqBonuses>
  <Bonus>BONUS_CHARCOAL</Bonus>
  <Bonus>BONUS_COAL</Bonus>
</PrereqBonuses>
<ConstructCondition>
  <Or>
    <Has><GOMType>GOM_BONUS</GOMType><ID>BONUS_COPPER_INGOTS</ID></Has>
    ... 11 more ...
  </Or>
</ConstructCondition>
<YieldModifiers>
  <iYield>0</iYield><iYield>15</iYield>
</YieldModifiers>
<ExtraFreeBonuses>
  <ExtraFreeBonus>
    <FreeBonus>BONUS_TOOLS</FreeBonus><iNumFreeBonuses>1</iNumFreeBonuses>
  </ExtraFreeBonus>
</ExtraFreeBonuses>

<!-- what it unlocks: not stored at all -->
```

**After — one file, one entity**

```jsonc
"enables": {
  "buildings": ["BUILDING_ARMOURER", ...19 of them, stated forward...]
},
"obsoletedBy": { "techs": ["TECH_NANOMINING"] },
"requires": {
  "build": { "all": [
    "TECH_METAL_CASTING",
    { "any": [
      { "type": "BONUS_COPPER_INGOTS", "scope": "city", "connection": "trade" },
      ...11 more...
    ] }
  ] },
  "operate": {
    "all": [ { "any": [ { "type": "BONUS_CHARCOAL", ... },
                        { "type": "BONUS_COAL", ... } ] } ],
    "dormant": ["BUILDING_FOUNDRY"]
  }
},
"production": { "city": { "percent": 15 } },
"provides":   { "bonuses": ["BONUS_TOOLS"] }
```

The same facts are on both sides; each now says what it is. The positional `<iYield>` pair became a named
channel. Two unrelated requirement mechanisms became one `requires` with two timings and the same condition
vocabulary in both. `ReplacementBuildings` stopped pretending to be a removal and is filed as `dormant`.
`ExtraFreeBonuses` became `provides`.

One tag moves somewhere non-obvious: `ObsoletesToBuilding` was a swap *destination*, never a cause. So the
obsolescence is carried by the tech alone, and the Foundry appears twice for two different reasons — as the
upgrade the Forge becomes (`whenObsolete.becomes`), and as the thing whose mere presence parks it (`dormant`).
Those are the two halves of an **upgrade chain**: building the successor parks the predecessor reversibly; the
tech landing turns it into the successor, one-way ([enabler.md §2](specs/enabler.md),
[json.md §4.2](specs/json.md)).

And `enables` — the forward edge that did not exist in the source data at all — is now stated on the thing
doing the enabling. That inversion is what lets the availability machine stop scanning the database.

### The shared vocabulary

| | |
|---|---|
| **Scope** (singular) | `world › team › empire › city › plot` — where an effect applies, or where a count is taken |
| **Target** (plural) | `plots · units · cities · empires` — all objects of that kind in the scope. Grammatical number is the whole differentiator |
| **Combinators** | `all` (AND) · `any` (OR) · `noneOf` — a recursive tree, nestable to any depth |
| **Units** | `flat` (+N) · `percent` (+%) · `multiplier` (×) |
| **Predicates** | `IS_CAPITAL`, `HAS_POWER`, `HAS_RIVER`, `{existedFor: {min: 1000}}` — an extensible registry |

The same atoms compose `requires`, a deposit's `enabled` condition, a count-scaler and a grant. There is
deliberately **no expression syntax** — a composite formula is a *list* of entries that sum, because a list is
inspectable and an expression language is a second engine nobody asked for.

Values are human-readable: `7`, `25`, `1.5`. Internally everything is integer fixed-point at ×100 — two
decimals without floats, which matters when multiplayer is deterministic lockstep and a float divergence is a
desync. The conversion happens once, in the loader. **A ×100 value in a JSON file is a bug.**

---

## 4. The enabler: generate, then gate

→ [specs/enabler.md](specs/enabler.md)

In the old engine "can I build this?" was not a system. It was a family of functions — `canConstruct`,
`canTrain`, `canResearch` and variants — each walking conditions in whatever order had accumulated, called from
the build list, the AI's production decision, the tech screen, the pedia and a dozen other places, several
re-scanning the whole database per call. They returned a boolean, so the moment you wanted to *explain* a
refusal, a second body of code re-derived the reason and was free to disagree with the answer.

Replacing it starts with noticing that it is two questions which cannot fold into one:

```
  ┌──────────┐   ┌────────────────────────┐   ┌──────────────────┐    ┌──────────────────────┐
  │   HAVE   │──▶│   PASS 1 · GENERATE    │──▶│  PASS 2 · GATE   │──┬▶│ LISTED  build it now │
  │  built · │   │  union(enables)        │   │  requires.build  │  │ └──────────────────────┘
  │ researched│  │    − disables          │   │  requires.operate│  │ ┌──────────────────────┐
  └──────────┘   │    − obsoletes         │   │  allowed         │  ├▶│ GREYED  "go get      │
                 │    − replaces          │   │                  │  │ │          copper"     │
                 │  pure set algebra,     │   │  cannot change   │  │ └──────────────────────┘
                 │  ZERO conditions       │   │  membership      │  │ ┌──────────────────────┐
                 └────────────────────────┘   └──────────────────┘  └▶│ HIDDEN  nothing to do│
                          ↑ CAN GET                                    └──────────────────────┘
```

Pass 1 alone decides what is in the tree, and evaluates no conditions at all. Pass 2 decides whether a tree
member is reachable *now* — it never adds or removes a candidate. Generation is a cheap top-down sweep, so the
only calculation is the gate, and it runs over just the frontier, never the whole database.

### The gate carries *why*, not a boolean

A greyed entry that doesn't say what is missing hands the player a question instead of an answer. The stored
verdict is the identity of the failing clause, and grey-vs-hide is read off it. The discriminator: **can the
asker act on it?** A missing resource greys. An unresearched tech hides (greying it would double-list every
future building). The ground hides — a city cannot acquire the tile it stands on.

> Otherwise a user would just have to guess what is wrong when they see greyed stuff, be it human or AI, and we
> try to avoid that.

The frontier is **one shared choice set**: the UI greys from it and the AI iterates it to decide what to
produce. They cannot disagree, because there is only one. And the reason is *stored* rather than re-derived — a
consumer that re-evaluated the clauses to explain a verdict would be a second gate implementation.

Scale: 4,381 of 5,180 buildings name a tech in their build requirement, 1,216 of those capped.

### Why it runs in both directions

Generation flows down from sources, but the requirement gate resolves by a callback **up** the scope chain — a
city-scope candidate asking its empire about civics, counts, state religion. A down-only design can model OR
but cannot reliably model AND, and forces the author to maintain every requirement at the top of the chain.
Roughly **75% of building requirements are AND**. The up-walk stays.

---

## 5. The spine: a fact names the happening

→ [spine.md](spine.md)

Replacing the repaint flags and the Python callback hub meant building what the old engine never had: a single
place where a happening is announced that anything can listen to.

A caller emits an event; every consumer registered for that event's KIND receives it, synchronously, inline at
the mutation site. There are 161 distinct facts, and the intended number is *all of them*.

> As long as it's not duplicate events, go nuts — add all the events, ever.

The costs are wildly asymmetric: a **missing** emit is a silently wrong value no compiler catches; a
**surplus** emit costs one consumer branch that declines to act. The single bar is duplicates.

### KIND is a firewall

Multiplayer is deterministic lockstep, so an authoritative count differing per machine is a desync. KIND keeps
the synced and unsynced streams apart, declared at the call site rather than inferred.

| Kind | Means | Consumed by |
|---|---|---|
| `DOMAIN` | Game *state* changed | Everything — caches, grants, replay, logging |
| `SAVELOAD` | A fact was read off the save stream | Logging only. Never counted, never gates |
| `DIAGNOSTIC` | *Code* ran | Logging only |
| `TRACE` | Every step | Logging only |

Making `SAVELOAD` its own kind makes "nothing derives held state from the load log" structural rather than
remembered — a state-building consumer registers for `DOMAIN`, so the interest mask enforces it.

### "Changed" is not a valid event name

> `BUILDING_CHANGED` is not a valid event — it says that *something happened*, not what actually happened. Any
> event that is not specific relies on actual calculation to happen.

A fact naming only the movement hands the consumer a question, and the only way to answer a question is to
calculate — so the calculation the spine exists to delete reappears inside every consumer at once. **A
non-specific event is a staleness flag that learned to travel.**

| Was | Is |
|---|---|
| `PLOT_TERRAIN_CHANGED` | `PLOT_TERRAIN_ADDED` · `PLOT_TERRAIN_REMOVED` |
| `PLOT_BONUS_CHANGED` (`iB` = ±1) | `PLOT_BONUS_ADDED` · `PLOT_BONUS_REMOVED` |
| `BONUS_ADDED` (unqualified) | `CITY_BONUS_ADDED` · `PLOT_BONUS_ADDED` |

The payload may carry *how many* — `CITY_POPULATION_REMOVED 2` withdraws twice over. It must never carry
*which way*. A signed delta, a presence boolean, an old value beside a new one are the same defect: a
discriminator the consumer branches on, which is the calculation relocated into a `switch`.

Because a `REMOVED` fact is emitted *while the old state still holds*, a withdrawal resolves against exactly
what was deposited — a derivation that is impossible once the state has moved.

### The order is commit, then announce

The first version had it backwards; events were going to *set* the state.

> That is the core principle I violated. If you try to set state with events, you start getting real
> concurrency issues, and you have to start responding to state setting with more events, and the clownfiesta
> gets real.

| | Set by | The event is |
|---|---|---|
| **Base state** — a building placed, population, research | Its own setter, directly | Testimony, after the fact |
| **Derived state** — the cascade sums, the enabler's sets | *The events themselves* | The maintenance path |

Under commit-then-announce there is nothing to schedule: the state is already correct when the fact fires, so a
consumer derives from settled state and announces nothing back.

```
   ┌────────────────┐     ┌──────────────────┐         ┌──────────────────────────────┐
   │ internal setter│────▶│  emit(DOMAIN)    │────┬───▶│ contexts    live-state stores│
   │ commits member │     │ WHAT · WHO · WHERE│    │    ├──────────────────────────────┤
   └────────────────┘     └──────────────────┘    ├───▶│ enabler     re-gates dependents│
      1 · COMMIT              2 · ANNOUNCE        │    ├──────────────────────────────┤
                                                  ├───▶│ modifier    applies deposits │
                                                  │    ├──────────────────────────────┤
                                                  ├───▶│ triggers    grants, payloads │
                                                  │    ├──────────────────────────────┤
                                                  ├───▶│ file log    gated, off-thread│
                                                  │    ├──────────────────────────────┤
                                                  └───▶│ /events     live, out of proc│
                                                       └──────────────────────────────┘
```

Logging is not the spine — it is one consumer of it, beside the machinery that builds state.

### Loading a save runs the same path

A loaded save used to deserialize straight into members, so the setters never fired and the derived layer had
nothing to build from. Now each slot deserializes into a local and is handed to that slot's internal setter —
the one body that commits, maintains and announces. The save stream is authoritative for base state; the fact
stream builds everything derived. One code path to be correct instead of two.

---

## 6. The cascade: nothing is ever recalculated

→ [cascade.md](cascade.md)

Sources **deposit** values; a target reads the **combined total**. Magnitudes flow down the scope spine — an
empire-scope deposit on a civic rolls down to each of the player's cities; a city-scope deposit lands locally.

Deposits accumulate in a package *at their own scope*, and the downward roll is realised at read time as the
trivial sum of the packages an object sits under. A lower scope never stores an upper scope's sums.

```
   world   │ Σpercent          ┐
   team    │ Σpercent          │
   empire  │ Σpercent          ├──▶  read = sum the ~5 packages
   city    │ Σflat + Σpercent  │      (no re-walk of sources,
   plot    │ Σflat             ┘       no fan-out downward)
```

City is the only scope carrying both halves: yields originate at plot and city; modifiers come from everything
but plot.

### The correction that removed a whole protocol

> What I got wrong is that I thought the yield packages had to be marked and recalculated all the time, when it
> is in essence just a compiled sum that is always updated, based on incoming spine events.

Every slot is one identity, and reading it settles the maintenance question:

```
slot = Σ over the scope's live sources S,
         over S's compiled deposits d, of
         value(d) × multiplier(S) × perScale(d) × [condition(d) holds]
```

All four operands are already maintained by an event. `value(d)` is compiled at load; `multiplier(S)` and
`perScale(d)` are counts the game objects and context stores hold; the condition verdict reads stored predicate
state. **Nothing on the right-hand side arrives unannounced** — so there is nothing left for a recompute to
discover.

```
  WAS · mark, then recompute              IS · the maintained sum
  ─────────────────────────────           ─────────────────────────────────────
  a fact ──▶ set stale flag               a fact ──▶ apply THIS source's deposits
                    ┆                                 (a handful of adds)
                    ┆ later…                          O(what CHANGED)
                    ▼                                        │
  a read ──▶ WALK EVERY SOURCE                               ▼  slot now correct
             at this scope                 a read ──▶ return slot
             O(what EXISTS)

  Cost scales with what the city HAS.     Same cost in a 900-building city as a 3-building one.
  A missed mark is plausible forever.     A missed emit is loud, and louder over time.
```

### Why this is also the easier correctness problem

The mark model needs *two* censuses complete; the maintained sum needs *one* — and it drops the harder of the pair.

| | The emit census | The mark census |
|---|---|---|
| The question | Does this choke point announce? | Does this fact reach every slot it could move, at every scope, for every owner? |
| Answerable | **Locally** — read the setter | **Nowhere local** — the answer lives in the authored data |
| Moves with data? | No — an emit is engine mechanism | **Yes** — a new deposit can silently need a new route |
| Safe to over-include? | **Yes** — a surplus emit costs one declining branch | **No** — a surplus mark is a real rebuild on the turn path |

> It is far easier to ensure we have all the events than to ensure that we have all packages correctly marked.

And the emit census is owed anyway — the enabler, the contexts, the trigger plane, the logs and the live stream
all depend on it. The mark derivation was a second census serving one consumer, whose correctness nothing else
was checking.

### The failure mode is deliberate

Under recompute-on-mark a missed invalidation leaves a stale but internally consistent value — plausible
forever, nobody looks. Under a maintained sum it leaves a phantom contribution nothing clears, compounding on
repetition: loud, and louder over time.

> We have to take that cost — the system will by its very definition collapse if we do not saturate with
> events.

The damage is bounded to one session, because nothing derived is ever serialized: loading rebuilds every slot
from the reseed's own facts.

---

## 7. Triggers: the plane that acts

→ [specs/triggers.md](specs/triggers.md)

The spine announces what happened; the cascade works out how much. Neither *does* anything — and a game is
mostly things being handed over.

### A grant is a trigger with a null condition

One plane, not two. Three named parts in reading order: **when/why → odds → what it does**.

```jsonc
"triggers": [
  { "trigger": { "type": "PROPERTY_FLAMMABILITY", "min": 200 },
    "chance": 5,
    "action": { "destroy": { "building": "random" } } },

  { "trigger": "onTurn",
    "chance": { "value": 5, "per": { "type": "PROPERTY_CRIME", "scope": "city" } },
    "action": { "grant": { "units": ["UNIT_PROPERTY_CRIMINAL"] } } }
]
```

A `grants` block is the degenerate case: the happening is implicit — the source's own considered action — with
no condition and no roll. It stays a first-class authoring shape because "acquiring me gives this" is
overwhelmingly the common case, but underneath there is one compiled entry list and one engine.

**Odds are data; the roll is not.** The draw comes off the synchronized stream, which is shared save state, so
no JSON authors a seed, a stream or a draw.

### A granted entity is an ordinary entity

> The only difference between a building granted and a building constructed is that we didn't use production if
> granted.

No parallel apply path, no "granted" flag, no separate lifecycle. It fires the ordinary state facts, so the
enabler, cascade and tally see it exactly as a constructed one, and it runs its own first-build effects.
**Nothing downstream may branch on "was this granted?"**

The same rule binds units, and there it fixed something measurable:

```
  WAS                                          IS
  production order ─▶ gives the city's XP      production order ┐
  religion founder ─▶ gives none               any trigger      ├─▶ createUnit()
  property spawn   ─▶ gives the city's XP      capture · bribe  │   settles what
  first discoverer ─▶ a third path entirely    the map editor   ┘   creation owes
```

A unit's starting experience depended on which payload created it. **The tell is a divergent side-effect set,
not a divergent call** — every route already reached the same low-level function; each had simply remembered a
different subset of what a new unit is owed.

Transformations — upgrade, gift, merge — must **not** ride the creation step, and the reason is concrete: the
step settles what a city owes a unit *born* there, so routing an upgrade through it would hand out free
experience on every upgrade and turn a barracks city into an XP faucet.

> If there is one place that can create a unit in other ways, that is a rollerskating surface — particularly for
> modders.

Which is why the map editor goes through the same step. An alternate path is exactly where the wrong lesson
gets taught.

### Why it registers last

Consumers dispatch in registration order, and this one goes after the contexts, the enabler and the cascade. It
reads all three, and gates on the enabler's operating-building set because **a dormant building must grant
nothing**. Unlike those machines it *applies*: a stale read hands out a wrong grant, not merely a wrong number.

The spine is its only front door. Per-turn work arrives as a turn fact, never a direct call from the turn loop —
two front doors is the scattered-endpoint disease it exists to cure.

### Telling a trigger from a modifier

A number whose value is a **threshold deciding whether something happens** is a trigger condition. A number that
is **added, scaled or stacked** is a modifier. If two copies would never sum, no channel was ever involved.

### What the plane refuses to build

The standing temptation is a verb for a mechanic somebody might author later. A trap building damaging attackers
is a trigger by shape, but modelling it needs an `onAttacked` happening and a `damage` verb, neither of which
exists. **If we want it, it is a trigger; until then it is nothing.** Neither shortcut is taken: verbs are not
minted speculatively, and the old data member is not kept alive meanwhile — a member parked on the wrong family
is a half-migration that reads as finished.

A trigger that fails to parse or land *says so*. Fail-closed is right; fail-closed **and silent** is not.

### Start packages: engine logic becoming data

The starting units are half data and half hardcoded: the counts are curated, but **the unit identity is not
authored anywhere** — chosen at runtime by scanning the whole unit database, filtering on trainability and
scoring with an AI valuation. The settler is not in data at all.

```jsonc
{
  "type": "STARTPACKAGE_ANCIENT_DEFAULT",
  "enabled": { "type": "ERA", "max": 1 },
  "grants": {
    "units": [ { "unit": "UNIT_SETTLER", "count": 1 },
               { "unit": "UNIT_BRUTE",   "count": 2 } ],
    "startingGold": 40
  }
}
```

No new vocabulary — it reuses `grants` wholesale, and packages **stack**: the condition lives on the package and
is evaluated once, so every civilization it applies to gets it without authoring anything. A civilization writes
something only when it deviates.

### And this is what the events become

The event system is the half that has not moved. A large amount of gameplay still lives in Python scripts —
per-wonder effects, combat reactions, respawn handouts — hardcoded, invisible to every C++ consumer, and
impossible to author without writing code.

> Having scripts like this in Python is the root of all evil. Most of it can even be expressed as triggers.

Those scripts are *already* trigger-shaped: a happening, a chance, an effect. The bridge is built and
deliberately left unfinished in one specific way:

- **The reporting hub now emits a spine fact beside every Python call**, so a happening that used to reach only
  Python is on the wire for every consumer.
- **Those facts are raw on purpose** — the reporter's own arguments, inventing nothing. The authoring vocabulary
  should be settled when the events are genuinely formalized; anything designed now gets undone by that.
- **It already pays for itself in provenance.** A captured unit used to announce only "a unit was created",
  indistinguishable from one trained, granted or editor-placed — and no flag could express a capture's *second
  party*.

From here: happenings get names, the verbs they need get defined in data, and a Python script becomes an
authored entry a modder can read and change without touching code. The discipline meanwhile is not to start
opportunistically — writing one as a trigger requires its happening and verb to exist.

---

## 8. Watching the game from outside it

→ [specs/http-endpoints.md](specs/http-endpoints.md)

> Reconstruct full game state from the endpoints, the event stream and the logs alone — never by looking at the
> screen.

This was a precondition, not a nicety. You cannot safely delete a maintainer you cannot observe, because you
cannot prove a replacement does the same job. Every deletion was paid for by an instrument built first.

- **Log files** — spine-written domains render on the game thread and hand off to a writer thread, so they are
  *readable while the game runs*. This is the one that captures the whole load reseed.
- **A live event stream** — `/events`, server-sent, on `127.0.0.1:7227`. State facts stream unconditionally;
  diagnostics ride their own knob. Bounded slots, and a dropped frame is reported as a gap.
- **On-demand snapshots** — computed on the game thread through a single-slot mailbox, depending on no log file
  and no gate.

### Why the route table is nearly empty

A legacy data member whose only remaining reader is an HTTP route is not actually still used — but the compiler
census cannot tell the difference. It survives self-referentially: the member exists because the route reads it,
and the route exists to read the member. A route is the perfect hiding place for exactly the legacy this rebuild
removes, and it hides it from the one census we trust.

So the surface stays sparse, and what it serves is **decompositions**, never totals — a city's yield rate
published as the six independent quantities that collapsed into it, with the refused deposits listed beside the
atom that refused each one.

---

## 9. What we refused to build

**Self-healing recalculation.** These never appear because someone wanted a recalculation; they appear because a
fact was not announced, the value went wrong, and recomputing was the cheapest way to stop the symptom. **Every
self-heal marks the spot where an emit is missing** — so finding one is a search, not a deletion. It is worse
than the bug it hides: the missed emit would have surfaced as a visibly wrong value somebody could chase; the
recalculation converts it into permanent invisible drift *and* reinstates the work the caches exist to delete.

**The word "dirty."** Removed with the mechanism it names, except the graphics repaint bits the executable
requires. A term that survives its mechanism teaches the next contributor to reach for it. Same for calling a
package read "hot" — a read can only be hot if reading does work, so the word smuggles the recompute model back
in over code that has none.

**Divergence as an event.** An event is an invitation to a consumer, so the next person writes the consumer that
*handles* a value known to be wrong by correcting it — self-heal wearing the authority of the spine. Anything
that recomputes to check must be structurally unable to write back what it computed.

**The in-engine profiler.** The macro family behaves differently per build configuration and some call sites
bypass every scope gate, including a per-frame one. Compiling it into a release build ran those ungated, per
frame, with a critical section per call; it crashed on end-turn and was reverted the same day. What replaced it
is a gated per-turn census teed to the event stream.

**The one cache we kept on purpose.**

> We should have some pathfinding cache, because it is the most expensive, and at the same time unmaintainable
> thing we can do — it has to scan plots by its very definition.

A path is not a sum over sources, so there is no delta to apply, and it moves non-locally: one terrain change
re-routes paths that never touch the changed tile, so no fact can name what it invalidated. That is structural,
not an exemption — and being unmaintainable-by-delta means such a cache is *cleared*, wholesale, by the events
that can move it.

---

## 10. Going forward

> Anything not enforced by hard typing gets worked around.

A rule in a document binds only someone who reads it, believes it, and still remembers it while writing the
code. So a design invariant that matters is expressed as a **type that makes the wrong move fail to compile**.
The ladder, best first:

1. A type that cannot express the error.
2. **A missing verb**, so the banned operation is unsayable. The refcounted context store has no `set`, because
   a `set` overwrites a refcount — and several buildings can confer the same thing, so an assignment would strip
   a city's third ring of workable tiles the moment it lost one of two grantors.
3. A mechanical check — a script that fails the build, because a rule has to be remembered and a check does not.
4. Only last, prose.

The worked case earned the rule: "specialists do not live in the building package" was true, documented, and
re-corrected more times than anyone cares to count — until the two yield origins became separate *package
types*, after which the wrong deposit simply does not build.

### Where this goes next

| Next | What it means |
|---|---|
| **Dissolve the AI god-classes** | The per-object AI classes become interface-bounded composition. The AI is a consumer of the data side, and it is the half not yet rebuilt |
| **A pluggable AI backend** | Once the AI reads the same maintained state as everything else, the decision layer stops needing to live in the DLL |
| **Volumetric resources** | Resources move from presence (0/1) to quantity (0..N). The storage is already an integer refcount precisely so this is a change of meaning, not a reshape |
| **The events rework** | The largest remaining piece, and the most leverage for modders — gameplay moving out of Python scripts onto the trigger plane as authored data (§7) |
| **Upgrade chains** | Building tiers as a first-class chain rather than the implicit inverse of a dormancy list ([parked](plans/parked/upgrade-chains.md)) |
| **A new Python surface** | One complete data-fetching library shaped by the new model, with the legacy binding surface disconnected rather than widened |

### The rule underneath all of them

No transitional shims. If the right design needs prerequisite work, do the prerequisite and build the real
thing — a shim that exists only to defer the real design is how a codebase accumulates a load-bearing minority
of things quietly missing while the whole looks nearly done. Which is, more or less, the condition we started
from.
