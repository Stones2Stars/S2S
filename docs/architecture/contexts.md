# Per-scope live-state contexts — the cascade read surface

> The live-state object a cascade getter and the one condition evaluator read to compute an entity's ACTUAL value in
> a given place. One per game-object scope that needs it: **PlotContext** (`CvPlot`), **CityContext** (`CvCity`),
> **EmpireContext** (`CvPlayer`). Owner rulings; this is the concrete shape the "make the infos sane"
> `(cityContext, plotGroup)` getters ([patterns.md § INFO DATA-OUT](patterns.md)) read.

## The one idea — isolate the CHANGEABLE state a reader needs, per scope, in ONE understandable place

A building's output getter computes the ACTUAL benefit in a city, which depends on that city's live state (its
connected/vicinity bonuses, river/coast plots, power, religions, …). Rather than every getter reaching into the
`CvCity`/`CvPlayer` god-objects ad hoc, each game object that a reader needs owns **one context** — the single,
predictable home for that object's changeable state. The **symmetry IS the value**: a reader always knows where to
go (city state → `CityContext`, empire state → `EmpireContext`, plot state → `PlotContext`).

**Isolation is for RESPONSIBILITY, not decoupling (owner).** The context is bound to its game object by pointer and
freely reaches into it — coupling is fine when the structure is ironclad. The goal is a clean responsibility line
(this object is THE state surface for its scope), never running detached from the live object.

## What a context STORES vs FORWARDS — ⛔ a context is an EVENT-BUILT STORE, not a forwarding facade (owner)

**"Context should be built on events — that is the design of it."** And the purpose of storing is that the state
becomes DISTINGUISHABLE: *"so that an info can say 'yes, I will actually deliver this, based on this state.'"*
A context that merely forwards to its bound object delivers none of that — it is the same pointer hop with an
extra name, so the design collapses into "pass the god-object like always."

> **⚖ THERE SHOULD VIRTUALLY NEVER BE AN ORDERING PROBLEM — EVERYTHING IS POPULATED BY THE REPLAY OF SPINE
> EVENTS (owner).** That is what makes consumer registration order almost irrelevant: each consumer builds its
> own state from the SAME fact stream, so no consumer waits on another's build.
> ⛔ **The anti-pattern that manufactures ordering is a store that RE-DERIVES by READING another system's built
> state.** It cannot run until that system is built, which instantly turns registration order into a dependency —
> and the dependencies go both ways (the enabler gates THROUGH these stores, so a store reading the enabler is
> circular). ⇒ **A store LISTENS and applies a delta; it does not read a set and recount.** The city's
> `amenities` fold is the worked case: as a delta off the per-building fact it builds itself identically at load
> (the save read's own emits) and at play, with no phase ordering; written as a re-derivation over the enabler's
> operating set it could not build at load at all.
> ⚠ **The exception is a HARD COUNTER, and it is SERIALIZED STATE (owner): a city's POPULATION, its CULTURE, its
> STORED PRODUCTION — "these kinds of things have to obviously just be serialized out."** They are not derived
> from anything, so the VALUE comes back off the save directly and is FORWARDED (below) rather than stored.
> **⛔ BUT THEY DO EMIT, AND THE SAVE READ IS WHERE (owner).** Reading the counter off the stream fires
> `CITY_POPULATION_ADDED <the stored amount>` — the ordinary `_ADDED` fact with its magnitude
> ([event-spine.md](../specs/event-spine.md)), not a bespoke load verb. ⚑ **The counter needing no event and its
> CONSUMERS needing one are different questions, and conflating them is what left a hole:** every deposit
> scaled `per: {POPULATION}` is maintained from ZERO by applying, so without that fact a loaded city's
> population-scaled deposits would all be missing — the value present on the object and absent from every sum
> derived off it. ⚑ It also needs no load special case: the same `_ADDED` fact the growth path emits, with the
> save's amount instead of 1 ([DEC-spine-reseed](decisions.md#dec-spine-reseed) — read, emit, populate). That raises no ordering question at all.
> ⇒ The three-way test, and the exception confirms the split rather than bending it: **DERIVED ⇒ built by the
> event replay, never serialized** ([DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted)); **genuine
> non-derivable state ⇒ serialized, and forwarded live** ([save.md §5](../specs/save.md) — a serialized store
> survives ONLY for state no derivation can produce); a context never stores the second kind.

The split is by **DERIVED vs RAW**, not by convenience:

- **STORE — every DERIVED fact the evaluation reads.** Predicate verdicts, aggregates, unions: computed ONCE by
  the ONE derivation for that fact and maintained by the spine events, never recomputed at read. This is the
  context's substance. It is derived state, so it is **never serialized** and is rebuilt at load by the reseed
  ([DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted), [DEC-spine-reseed](decisions.md#dec-spine-reseed)).
- **FORWARD — only the object's OWN RAW data** that it already maintains O(1) (the substrate ids a parameterized
  predicate keys on, population, …). Forwarding raw data is not duplication; storing a second copy of it would be.

⚑ **THE PAYOFF — this is why the design earns its cost (owner): once contexts are PURELY event-updated, an
enormous class of per-read CALCULATION becomes obsolete.** Not "gets faster" — ceases to exist. Every read-time
scan/union/walk collapses into a stored value some event already maintained, and reads become bare fetches
([state-repositories.md](state-repositories.md); [DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)).
The in-tree exhibits are not hypothetical: `isCoastalLand()` is an 8-neighbour scan **per predicate
evaluation**, and the §5a vicinity check is a radius union **per check**. The win is STRUCTURAL: once the fact
is stored there is no read-time work left to do, so cost tracks EVENT volume (what changed), never read volume
(how often it is asked) — and it is observed where every performance claim is observed, on the per-turn wall
clock ([DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)).

⛔ **BUT THE TEST IS A SCAN, NOT A HOP — and `getNumBonuses` is the case that marks the line.** What earns a
store is read-time work that GROWS with something (neighbours, radius tiles, a registry). A read that resolves
through a POINTER to the object which already owns the number O(1) is not that, and storing it anyway makes a
third copy of one fact ([enabler.md §8](../specs/enabler.md) RESIDENCY: the plot group owns the network count,
the city relays it, the context forwards the relay). ⚠ This one had a store on exactly that mistaken reading,
and it cost a sweep of every bonus on every fact that could move one — strictly more work than the hop it was
avoiding. **Ask what the read WALKS; if the answer is "one pointer", forward it.**

> **⛔ SO A CONSUMER NEVER WALKS AN INFO'S KEYED LIST TO ASK A PER-ITEM LIVE-STATE QUESTION — THE EVENT-BUILT
> READ-ONLY STATE ANSWERS IT (owner).** *"There should be no iterating like that; the eventspine-built read-only
> should be able to handle that."* The shape to recognise is `foreach_(key in someInfo.getKeyedList()) { …
> liveStateRead(key) … }` — the info supplies the keys and the loop asks the live state once per key. That is the
> per-read scan this whole section deletes, merely sourced from an info instead of from the map.
> ⚑ **The worked case is the corporation's consumed bonuses:** `foreach_(bonus in corp's bonuses) getNumBonuses(bonus)`
> re-executes [enabler.md §8](../specs/enabler.md)'s hottest cluster once PER BONUS, and where the question is a
> MAGNITUDE the answer is already authored — the rate carries a `per:{anyOf: consumed bonuses}` scaler, so the
> valuation resolves rate × count in one call and the loop simply disappears.
> ⛔ **And renaming the receiver is NOT the fix.** A walk that compiles against the new getter reads as migrated
> while doing exactly what it did before — the half-migration
> ([DEC-new-getter-surface](decisions.md#dec-new-getter-surface)), and it hides the hole the maintained read has
> not yet filled ([DEC-no-legacy-masking](decisions.md#dec-no-legacy-masking)). Leave such a site DANGLING as the
> census entry it is until the maintained fetch exists.

⛔ **A forwarded read that COMPUTES is the defect this rule exists to kill.** `PlotContext::hasCoast()` forwarding
to `CvPlot::isCoastalLand()` — an 8-neighbour scan with an `area()->getNumTiles()` call per neighbour, on every
predicate evaluation — is the worked example, and it directly contradicts
[patterns.md](patterns.md): *"every evaluator predicate is an O(1) CONTEXT fetch … a predicate that walks
plots/units per call is the efficiency defect to reject in review."*

**The storage is keyed by the CONDITION VOCABULARY** — that is what makes the state distinguishable. One key
space (`CASC_PRED_*` / the classification ids), three granularities of the same design:

| context | stores | granularity |
|---|---|---|
| **PlotContext** | the plot's own predicate verdicts | a `CASC_PRED_*` BITSET |
| **CityContext** | `plotAttrs` | per-predicate COUNTS over the same keys — the FOLD of its member plots' bits |
| **EmpireContext** | `policies` | the `POLICY_*` id set (the union over live civics + held traits) |

So the city aggregate is not a second derivation: `CvCity::onCityPlotChanged(plot, ±1)` folds the plot's bits,
and the two granularities cannot drift.

⚠ **Adjacency-derived predicates fan out.** `HAS_COAST` / fresh-water depend on NEIGHBOURS, so the event that
changes a plot re-derives that plot's own bits AND its adjacent plots' adjacency bits. Bounded (8 neighbours) and
event-driven — never a read-time scan, and never left on the old accessor as an interim.

| context | owner | STORES (unique aggregate) | FORWARDS (read through the bound object / its owner) |
|---|---|---|---|
| **CityContext** | `CvCity` | `plotAttrs` — per-predicate plot COUNTS (the fold of member plots' bits) · **`amenities`** — the `AMENITY_*` id→COUNT fold over the city's OPERATING buildings + the empire-scope grantors (json §8; the count is load-bearing — see the callout below) · **the VICINITY BONUSES available in the city** (owner) — the §5a radius union, MAP half (see the split below) · the **AREA facts** (area id, its tile count, the coastal water-body size) · the **holy-city and HEADQUARTERS counts** — how many religions / corporations name this city, each a delta store fed ±1 by its own fact · the **CORPORATION-ACTIVE verdicts** — the remembered per-corp `{HAS_CORPORATION}` verdict, held ONLY so its crossing can announce (`SEVT_CITY_CORPORATION_ACTIVE_ADDED / _REMOVED`, the fact plane C routes on): each leg's fact triggers a re-read of the ONE engine implementation (`CvCity::isActiveCorporation`, the sanctioned engine-owned input — the read side stays the live forward), and a re-read that moved nothing announces nothing | population, power, religion presence, holy-city-of, corporation, capital, government-centre, fresh-water access, property value (raw, `CvCity`-owned, O(1)); state religion (→ owner `CvPlayer`); **the TRADED count** — the gated network number, forwarded through `CvCity::getNumBonuses`, which relays to the PLOT GROUP that owns it ([enabler.md §8](../specs/enabler.md) RESIDENCY: nothing mirrors the group); **the CURRENT REALIZED YIELDS** (owner) — the city's own O(1) group read, forwarded so a valuation can resolve a percent against a real base (below); **the CURRENT REALIZED COMMERCE** — `CvCity::getCommerces`, the per-commerce SPLIT of that commerce yield by the empire's sliders plus each channel's own deposits ([modifier.md §2a](../specs/modifier.md)), forwarded for the same reason |
| **EmpireContext** | `CvPlayer` | `policies` — the empire's enacted-policy set (the derived UNION over live civics'/traits' policy blocks, stored nowhere else) · **the HELD-TRAIT set** — the `TRAIT_` id→COUNT fold, a delta store fed ±1 by the trait facts (§ the callout below: enumerating what a player holds is a SCAN even though testing one trait is a hop) | state religion (single enum → `CvPlayer::getStateReligion`), civics/**trait presence**/heritages, the team-held facts; **the CURRENT REALIZED COMMERCE** — `CvPlayer::getCommerces`, the four empire RECEIVER totals: the city-yields forward's empire twin, so an empire-scope percent resolves against a real base; **the COMMERCE SLIDER PERCENTAGES** (owner) — the player's gold / research / culture / espionage rates, the `GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE` tokens ([json.md §3.1](../specs/json.md)); a group keyed by `CommerceTypes`, forwarded because `CvPlayer` owns them O(1) |
| **PlotContext** | `CvPlot` | the `CASC_PRED_*` verdict **BITSET** — the OWN-PLOT block (water/land/relief/hills/peak/river/irrigation/feature-present/landmark/owned/**worked**) plus the ADJACENCY block (coast, fresh-water) · **`workableBy`** — the cities whose potential work area this plot is in, set by `CvCity::changeWorkableArea` and announced per plot (§ the VICINITY store) | the RAW substrate a parameterized predicate keys on — terrain/feature/improvement/route/bonus ids, owner, latitude, nature yield — plus city-presence, the one verdict with no mutation event a bit could be maintained from (→ `CvPlot`); **the plot's CURRENT REALIZED YIELDS** — `CvPlot::getYields`, the whole isolated per-plot base package as a bare cache fetch. ⛔ The PRE-IMPROVEMENT leg (`natureYield`) is a SECOND SLOT of that same package, never a per-call computation: it is asked per (plot × improvement × yield) by the placement gate and both improvement valuations, which is the cost class this whole section deletes. A read that recomputes it is the forwarded-read-that-COMPUTES defect above, and the number is already in the package |

⛔ **THE VICINITY SPLIT — the context holds the MAP half, the enabler holds the BUILDING half.** The §5a in-vicinity
supply is a union of two independently-owned halves, and storing either one twice is the duplication the model bans:

- **MAP providers** (a bonus on a radius tile providing itself) are per-scope live state with no other home, so
  `CityContext` holds them — tiered by the §3.4 ownership discriminator (`owned` / owned+neutral / `crossBorder` /
  `worked` / `onSite`), since a `connection:"vicinity"` atom selects which tiles count.
- **ACTIVE BUILDING providers** (`provides.bonuses`) are the operate/provides **least fixpoint**, which only the
  enabler can resolve — an operate condition may consume a bonus another active building provides. They stay
  `OperatingBuildings::provided`, reached through `CvCascadeEvalCtx::vicinityProvidedBonuses`.

The reader unions the two. A mirror of the building half on the context would also *drift*, because the enabler
mutates its set in place as the fixpoint ripples.

> **⚖ THE MAP HALF IS TWO DICTIONARIES, NOT ONE — bonuses, and natural features (owner).** *"There is nothing
> wrong with having 2 dictionaries, 1 for bonuses and 1 for natural features; what I don't want is the constant
> rewalk."* So the vicinity store is a **`BONUS_*`-keyed** dictionary beside a **`CASC_PRED_*`-keyed** one (the
> vicinity twin of `plotAttrs` — river / coast / hills / peak / fresh water), each an ordinary `ContextDict`.
> ⛔ **They are NOT merged into one dictionary**, and the reason is the one `ContextDict` already states as its
> first: the two key spaces are DISJOINT REGISTRIES both starting at 0, so a merged store re-opens the
> cross-registry id collision the `CLS_` prefix closed by construction. One dict per area of responsibility.
> ⚑ **The objection the ruling answers is the REWALK, never the count of dictionaries** — a second dictionary
> costs one more `add(id, ±1)` on a fact that is already being handled, while the absence of one costs a radius
> scan per read. Adding a dictionary is how the walk disappears.
> ⚠ **The ownership TIERS partition; they do not nest in storage.** [json.md §3.4](../specs/json.md) defines
> `owned ⊂ owned+neutral ⊂ crossBorder`, so storing them as overlapping tiers would double-count on a fold.
>
> **⚖ NEUTRAL IS THE DEFAULT STATE — IF THERE IS NO OWNER IT IS NEUTRAL (owner).** So neutral is **not stored and
> needs no fact**: the store holds `all` (the bonus is on a radius tile at all, moved only by the BONUS facts)
> beside the two ownership bands `owned` and `foreign`, and the neutral count is the RESIDUAL
> `all − owned − foreign`. The bands are then carved out of the total — `crossBorder` IS `all`, the default band
> is `all − foreign`, `owned` is itself.
> ⚑ **That is what makes the store maintainable at all.** `SEVT_PLOT_OWNER_ADDED / _REMOVED` are both guarded on
> `!= NO_PLAYER`, so they announce only the OWNED ends — a *stored* neutral tier would have no announced
> transition across `unowned ⇄ owned` and no delta could keep it correct. As a residual it needs none: all four
> transitions (`unowned→A`, `A→unowned`, `A→B`, and a bonus arriving) balance exactly, because each fact names
> the owner ITS half is about while the plot's own `m_eOwner` has already moved.
> ⛔ Two workarounds were considered and are wrong wirings: reading the dict to decide a withdrawal
> (`if (neutral.has(b)) add(b,-1)`) makes a GATE read the refcount and picks the wrong plot when two radius tiles
> carry one bonus; and composing the neutral end from the `IS_OWNED` predicate crossing double-applies on
> `unowned → owned`, where that crossing AND `OWNER_ADDED` both fire.
>
> ⚖ **THE SEEDING MOMENT IS `SEVT_PLOT_CITY_ADDED`, and it is ordering rather than taste.** `CvCity::init` sits
> the city on its plot and only THEN claims its radius through `updateCultureLevel`, while `emitCityFounded` comes
> later still from `CvPlayer::found`. That fact is therefore the one window where the city is already visible to
> the radius inverse and its radius has NOT yet taken ownership: the seed books what is already there, and the
> ownership claims that follow apply their bands for exactly the tiles that change. ⛔ Seeding at `CITY_FOUNDED`
> would double every band the claim had announced. ⚠ It is guarded to the non-load path, with the
> `GAME_LOAD_FINISHED` fold as its load-time twin — the map streams before the players, so at load the radius
> facts reach no city at all (the amenity fold guards its play-time fan the same way, for the same reason).
>
> `worked` and `onSite` are different predicates rather than ownership bands, so they stay their own — and each is
> its OWN stored dictionary. **`onSite` = an OWNED radius tile whose IMPROVEMENT trades the resource**, which is
> strictly stronger than `owned` (raw presence, improved or not) and therefore not a filter over it: two owned
> radius tiles can carry one resource with only one improved, so only a count answers it.
> ⛔ **IT NEVER CONSULTS THE NETWORK, and that is the ruling rather than an omission (owner): onSite and traded are
> two COMPLETELY SEPARATE LISTS, neither derivable from the other** — you can hold a resource on site and not in
> trade, *having traded your only copy to another civ*. A mounted unit needs horses ON SITE; a swordsman only needs
> iron wares in the NETWORK ([json.md §3.4](../specs/json.md): the two are ORTHOGONAL, not nested).
> ⚑ **The tile's half is a VERDICT the PLOT owns and announces**, exactly as its predicate bits are: `PlotContext`
> holds the SERVED RESOURCE — an id, because a plot carries at most one bonus — derived from the bonus and
> improvement axes and announced as `SEVT_PLOT_SERVED_BONUS_ADDED / _REMOVED`. ⛔ A city-side derivation is
> impossible for the same reason the per-bit fact exists: by the time any consumer runs the plot already holds the
> new value, so the old contribution is gone. The OWNERSHIP half stays the CITY's, applied where the asker's own
> owner is known — no per-plot verdict can answer it for every city that may work the tile.
> ⚠ It is NOT gated on the tile being WORKED: a fort cannot be worked by definition, and a fort is exactly how a
> resource gets served (owner).
>
> **⛔ THE TWO ARE NOT PEERS, AND THAT IS THE WHOLE CONFUSION — VICINITY IS THE PLOTS, `onSite` IS A CONNECTION
> THROUGH THEM (owner):** *"vicinity is the plots actually in vicinity; if a bonus is on site, it means it's
> connected to a city via this vicinity band."* So a vicinity BAND selects WHICH PLOTS COUNT (the ownership
> tiers, and `worked`), while `onSite` is a VERDICT ABOUT THE BONUS reached through that band — the resource is
> available to this city because a tile in the band serves it, or an active building supplies it. One names a
> plot set; the other names how a resource arrives.
> ⚠ **The orthogonality is against the NETWORK, never against vicinity** — `onSite` and `connection:"trade"`
> are the two independent routes a bonus takes to a city (above). Reading `onSite` as "orthogonal to vicinity",
> or as merely a stricter band of it, are the two halves of the same mistake.
>
> **⛔⛔ AND HERE IS WHERE EVERY AGENT SCREWS UP, WHICH IS THE REASON THE BONUS LIST IS CALLED `onSite` AT ALL
> (owner): A BONUS SUPPLIED BY A BUILDING IN THE CITY IS *ALSO* VICINITY.** *"Agents could not get that bonuses
> given from buildings in a city is also vicinity, which is why it was changed to onSite for the bonuslist."*
> The word "vicinity" reads as TILES, so agent after agent took the building-supplied half to be something else
> — a different mechanism, a special case, or simply absent — and wrote reads that answer only the map half.
> ⇒ **The name was changed to stop that**: `onSite` says *the resource is available here*, without implying by
> what route, so the two halves sit under one word that cannot be misread as "on a tile".
> ⛔ So a reader that answers only the map half is WRONG whatever it is called, and "it says vicinity, so it
> means tiles" is the specific inference to refuse. The union is not an implementation detail of the read — it
> IS the read ([json.md §5a](../specs/json.md): a herd BUILDING and an improved herd TILE are the same act).
> ⛔ **So the code must not spell `onSite` as a band of vicinity, and the SHAPE is what teaches it.** The storage
> is right — `onSite` and `worked` have their own dictionaries, the bands have theirs — but `CvCascVicinity`
> carries the plot-set selectors and the connection verdict in ONE enum, so a reader named for VICINITY returns
> the on-site verdict whenever it is handed that value, and the call site reads as though onSite were just a
> stricter tier.
> ⚑ **This has already bitten once and was patched with PROSE, which is exactly why it recurred** — a tier-less
> `hasVicinityBonus` silently answered `CASC_VIC_ONSITE`, and the fix was a comment telling the next caller to
> name its tier ([DEC-hard-typing-or-rollerskate](decisions.md#dec-hard-typing-or-rollerskate): prose is the
> weakest rung, and it binds only an agent who reads it, believes it, and still remembers it).
> ⇒ **So the rule binds the NAME, not just the argument: a function whose answer can be the on-site verdict does
> not carry `vicinity` in its name, and it takes NO tier** — `EnablerKernel::cityHasBonusOnSite` for a caller
> holding a `CvCity*`, `ev_vicinityHas` for the evaluator, which must stay ctx-shaped because the eval ctx is
> forbidden a game object (§ THE EVAL CTX). Two entry points because they reach different planes; ⛔ neither may
> answer with one half.
> ⚠ The durable fix is to SPLIT THE AXIS so the wrong call cannot be spelled at all; that reaches the authored
> `vicinity:` key, the condition struct and the evaluator, so it is a structure call rather than a sweep.

⚖ **`CityContext.amenities` — THE CITY'S OWN FEATURE LIST, AND THE CITY IS WHAT GETS CHECKED (owner).** A
grantor's `amenities` block ([json.md §8](../specs/json.md)) is static info data; what a consumer actually asks
is *"does THIS CITY have this?"*. So the city holds the FOLD — over its **operating** buildings, plus the
empire-scope grantors (civic / trait / tech) that reach every city — and every gate reads it O(1). ⛔ A consumer
must never loop the city's buildings asking each one, and a grantor's per-key named getter is not the consumer
surface: the fold is the ONE reader of the grantor side
([DEC-single-implementation](decisions.md#dec-single-implementation)), alongside display/pedia.

> **⛔ IT IS A ContextDict (id→COUNT), NOT A BITSET (owner) — absent or 0 is false, anything else true.** Several
> grantors can confer the SAME amenity, so a removal DECREMENTS rather than clears: *losing one power plant must
> not darken a city that has two.* A bitset cannot express that — an "amenity removed" fact would clear a bit
> another live grantor still justifies. ⚑ So it is the ordinary `ContextDict` this doc already specifies
> (`has(id)` ≡ `count > 0`), the same refcount shape the enabler's membership formula and the operating set's
> provided-bonus counts use, and the semantic legacy already had in its per-flag counters.

> **⛔ THE DICTIONARY IS THE FINAL STOPPING PLACE — IT IS WHERE THE DATA ACTUALLY LIVES (owner).** Every grantor
> fact lands here and comes to REST: the building leg off the enabler's active↔dormant crossing (a dormant
> building confers nothing, [enabler.md §3.2](../specs/enabler.md)), the civic / trait / tech legs off their own
> facts. ⛔ It is NOT a projection of some other system's truth, and it is NOT relayed from the enabler — the
> enabler is a SOURCE OF FACTS, never the home of this answer. One dictionary, every leg, one mechanism, and
> every reader — the enabler's own gate included — reads it HERE.
> **⛔ AND IT IS ITSELF A SPINE CONSUMER THAT KNOWS EXACTLY WHICH EVENTS TO LOOK FOR (owner).** A dictionary
> REGISTERS on the spine and DECLARES the precise set of facts that maintain it; it is not fed by a central
> switch that fans out to whichever store a case happens to name. ⚑ **The interest set IS the maintenance
> contract, which is what makes it auditable at all:** with a fan-out, "does this fact reach the store that
> needs it" is answerable only by reading the router, so a missing route hides in a `switch` that looks
> complete; with a self-declaring dict the gap is visible AT the dict. It is also what makes the RECEIVED line
> name something useful — the consumer that acted is the dictionary, by name
> ([event-spine.md](../specs/event-spine.md) § THE RECEIVED LINE).
> ⚑ It is the same move as the spine's own per-domain isolation: adding a domain touches only that domain, and
> adding a dictionary now touches only that dictionary — no shared edit, no central case to remember.
> ⚠ **REGISTRATION ORDER REMAINS A CONTRACT and self-registration must not quietly break it.** The enabler's
> load-end gate pass evaluates THROUGH these stores, so every dictionary registers inside the CONTEXTS band of
> `contexts → enabler → modifier → triggers` ([enabler.md §8](../specs/enabler.md)) — ordering is a property of
> the band, never of which translation unit happened to initialize first.
> ⛔ This does not license one consumer per SYSTEM being violated ([DEC-enabler-not-cascade](decisions.md#dec-enabler-not-cascade)):
> that ban is on one consumer routing TWO MACHINES, not a cap of one consumer per machine. Several dictionaries
> inside the contexts band are still exactly one system's worth of maintenance.
>
> **⛔ A CONTEXT DICTIONARY ONLY EVER CONSUMES; IT NEVER EMITS (owner) — which is why it can close no loop.**
> Facts go in, state comes out, nothing goes back. A later read of that state by the machine whose fact fed it is
> an ordinary read of CURRENT state, not feedback. ⚠ **The ordering ban at the top of this document does NOT
> reach it, and reading it as though it does is the misapplication to avoid:** that ban is on a store that
> RE-DERIVES BY READING another system's built set — which cannot run until that system is built. A
> delta-CONSUMING store has no such dependency; it builds identically whenever the facts arrive, which is
> precisely why the delta form is the one this document prescribes.
> ⚑ Distinct from `ecOp.activeBuildings = NULL`, which breaks a genuine RECURSION INSIDE THE EVALUATOR (an
> operate condition asking for the very set being computed). A dictionary updated by an earlier synchronous fold
> is not recursion; it is simply current.

> **⚖ THE FOLD HAS TWO LEGS, BECAUSE THE GRANTORS SIT AT DIFFERENT SCOPES — one implementation, two triggers.**
> A BUILDING confers on its OWN city, so its leg is a pure delta off the per-building fact and needs nothing
> else. A CIVIC confers on EVERY city of the empire ([json.md §8](../specs/json.md)), and that leg cannot ride
> the grantor fact alone: **at load the civic facts fire from `CvPlayer::read` BEFORE the cities deserialize**,
> so there is no city to fan to. It therefore folds from the other side — **when a CITY starts existing** (the
> load build, and city-founded) it folds what its owner already holds — while the grantor fact fans the delta
> (`−`old, `+`new) over the cities that already stand.
> ⚠ **Both halves are needed, and the load ordering is NOT uniform across grantors:** the civic reseed emits
> before the cities, but the TRAIT reseed emits *after* them. So the play-time fan is guarded to the non-load
> path — unguarded, a trait would be counted twice against the load build.
> ⚑ Reading the owner's adopted civics there is a **FORWARD of raw, object-owned state**, not the banned
> re-derivation. What is forbidden is a store reading ANOTHER SYSTEM's built state (the enabler's operating
> set) — that is what manufactures an ordering dependency; `policies` already makes exactly this read.

> **⚖ POWER IS AN AMENITY, AND IS TREATED AS ONE (owner).** `CvCity::getPowerCount` reads the `providesPower`
> fold rather than a hand-named counter, and the counter, its changer and its Python binding are gone. ⚑ The
> REFCOUNT is what earns it: losing one of two power plants must leave the city powered, which is precisely the
> failure a plain counter or a bitset cannot express.
> ⚑ **The fold ANNOUNCES its crossings** (0 ⇄ non-zero, never a second grantor of a key the city already holds),
> because a consumer routing on an amenity must not re-derive which key moved — the modifier's `HAS_POWER`
> dependency route and the enabler's power gate both ride that fact.
> ⛔ **Where a STATUS gates delivery, the announced crossing is the GATED verdict's, not the store's** — for power,
> `CvCity::isPowered` rather than the refcount ([state.md](../specs/state.md) § A STATUS IS MIDDLEWARE). The two
> genuinely differ, so announcing the store would put the fact and every consumer's read on different values; the
> status reaches this fold for that reason alone and never becomes a store entry or a cascade input.
> ⛔ The crossing is emitted by the FOLD, not by a mutation site: the fold IS the maintenance path, so an emit
> anywhere else would be a second one.

**Every boolean city attribute of this shape is generalized onto the ONE fold, not just power:**
`governmentCenter`, `abolishedAnger`, `abolishedUnhealthFromPopulation` and `abolishedUnhealthFromBuildings` are
all ordinary `CITY_HAS_AMENITY` keys (`CvCity::isGovernmentCenter` / `isNoUnhappiness` /
`isNoUnhealthyPopulation` / `isBuildingOnlyHealthy`), so a NEW attribute of this shape costs no engine
change — it is pure data, the open-registry promise ([DEC-classification-infos](decisions.md#dec-classification-infos))
reaching the consumer side. ⛔ There is no surviving hand-named counter for any of them — no
`changeGovernmentCenterCount` / `changeNoUnhappinessCount` / `changeNoUnhealthyPopulationCount` member on
`CvCity` — the fold replaced them, exactly what
[DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) asks for.

⚠ **Two attributes that LOOK like the same family are deliberately NOT on this fold, and neither is a hole to
close by routing it there.** `governmentCenterDistance()` is a separately STORED value (§ the sanctioned-recalc
exemplar, above) because it answers a MIN over the player's centres — a different question from
`isGovernmentCenter()`, which the amenity fold already answers. And `HAS_FRESHWATER` is a **`PlotContext`** bit,
not a `CityContext` amenity, that deliberately keeps calling the live `CvPlot::isFreshWater` engine predicate
rather than folding onto a dictionary (§ the adjacency callout, below —
[DEC-single-implementation](decisions.md#dec-single-implementation)).

**Pass by reference/pointer, never by value (owner).** Passing a bound context is far cheaper than snapshotting
values; a context is never a value copy — that is *why* it forwards rather than mirrors.

**⚖ THE TWO PASS-IN SCENARIOS (owner) — a context crosses a call boundary in exactly TWO places, the two
condition-evaluation sites:** (1) **the VALUATION** — the `expected*` per-group reads and the package rebuild's
conditioned-deposit evaluation (the same machinery at event cadence); (2) **the `requires` edge** — the
enabler's build/operate gate incl. the operating-set fixpoint, re-run at HAVE-change over the affected
candidates. Both go through the ONE evaluator over the eval ctx the contexts fill. Every other read on every
surface is a straight compiled fetch and NEVER takes a context parameter — a context in any other signature is
the mechanical smell that condition evaluation (or an ad-hoc state reach) is happening where it doesn't belong.

## ⛔ THE EVAL CTX CARRIES CONTEXTS, NOT GAME OBJECTS (owner) — the contract must be STRUCTURAL

**"Otherwise we can just pass the full player, city, and whatever other objects again, without any
distinguishing."** That is the whole test, and it is a CONTRACT, not a prohibition: if the evaluation context
holds a `CvCity*`/`CvPlayer*`, then "the reader goes through the context" is enforced only by reviewer memory —
the god-object is right there, and reaching past the context is one `->` away (a derived
`&ctx.city->getCityContext()` is the tell: the ctx never held a context at all). The isolation must be
**unsayable to violate**, exactly as [patterns.md](patterns.md) states the info DATA-OUT contract: there is no
member to reach through.

So `CvCascadeEvalCtx` carries **`const CityContext*` / `const EmpireContext*` / `const PlotContext*`** — never
the bound game objects. Consequences, each already implied by the model above:

- **TEAM facts route through `EmpireContext`** (team is deliberately not a context; team-held techs are read
  through the player) — the ctx carries no `CvTeam*`.
- **`CvPlotGroup` STAYS a first-class ctx member** — it is the reserved explicit traded-bonus source (§ the
  read, above), not a scope whose state a context owns.
- **`CvUnit` stays raw FOR NOW** — units are the deliberate FUTURE role-specific scope; when that context
  lands it replaces the pointer, and until then this is the one acknowledged hole, not a licence for others.
- **The enabler's precomputed sets** (operating/active/obsolete buildings, vicinity-provided bonuses) stay ctx
  members: they are the ENABLER's derived output fed to the evaluator, never per-scope live state.
- **⚖ THE SOURCE SLOTS — a predicate about the CARRIER needs the carrier named, because an entry cannot name
  itself.** Neither a compiled entry nor an info knows its own engine id, so a condition asking about the
  DEPOSITING entity rather than the target (`existedFor` — how long has THIS building stood) has no other way
  to ask. The ctx therefore carries a slot per such axis — `religion` (the §3.7 counted-kind filter) and
  `sourceBuilding` — **set per-iteration by the walk that knows the id, on a LOCAL COPY of the ctx, and -1
  everywhere else**; the shared ctx is never mutated. ⛔ -1 means "no carrier in hand" and a source-predicate
  must answer FALSE there: a scope-wide read that never set it would otherwise resolve against whichever
  entity the walk happened to reach last.
  ⚠ **A source slot is only meaningful where the carrier is SINGULAR.** The city fold resolves one building at
  a time, so it sets one; the EMPIRE fold walks a building the player may hold N copies of, each acquired at a
  different moment, so the question has no single answer and the slot stays unset by design. That is a
  structural limit, not an unwired leg — do not "complete" it by setting the slot there.
- **A context that cannot answer a needed fact is a CONTEXT GAP to close** (add the forward), never a reason to
  re-add an object pointer. That is the forcing function the structural form buys.

**⚖ THE HAVE AXIS LIVES IN THE CONTEXTS (owner).** What a scope POSSESSES — the city's buildings-present /
religions / corporations / bonuses, the empire's civics / traits / heritages, the team-held techs (read through
the player's team — team is deliberately not a context) — is read through that scope's context, never by an
ad-hoc reach into the game object. The STORES-vs-FORWARDS discipline above is unchanged: possession state the
object already owns O(1) is FORWARDED, and only a homeless aggregate is stored (`policies` is the realized
exemplar). The context is the RESPONSIBILITY home — the one place every reader (the evaluator's atoms, the
enabler's gates, the `expected*` valuations) goes for HAVE. The enabler's DERIVED sets (the domain vectors, the
operating-building set) remain enabler-owned ([enabler.md §7](../specs/enabler.md)); the contexts serve the raw
possession facts those machines gate against.

**⚖ IF IT IS CURRENT STATE, IT IS THE CONTEXT'S — there is no third home (owner).** A value that looks like it
needs a new category almost always just IS current state, and current state has one home. The worked case: the
city's **thresholds** (growth / culture / great-people — what is REQUIRED right now, moving with population,
gamespeed and era), its **turns-left** projections, and its cross-city **ranks** are none of them a new kind of
read — they are this city's state now, so they are asked of the context like every other state fact. The
STORES-vs-FORWARDS split then decides each one on its own merits (a value the object already computes O(1) is
FORWARDED; only a homeless aggregate is stored) — the classification is never "invent a shape for it".
⚠ **Ranks are the case that STRESSES the rule**, and stressing it is not breaking it: a rank is a comparison
ACROSS cities, so no single city owns the answer and a stored rank would need maintaining on every sibling's
change. Read it at the scope that can actually answer it — the player — rather than bending the city's context
around it.

## COUNTS, not objects — "how many, not which" (owner)

An aggregate holds **counts keyed by id**, never the objects themselves. A building cares HOW MANY river plots /
vicinity bonuses it has, never WHICH. So a `plots`-target (or keyed) deposit's output is `flat × count(id)`, and a
gate is `has(id)` (count > 0).

> **⚖ THAT PRODUCT IS A YIELD, AND THE DICTIONARY IS ONE OF ITS TWO OPERANDS — which is what makes a yield
> delta-derivable at all (owner).** `flat` is a load-compiled constant and `count(id)` is maintained here, so
> `Δ(flat × count) = flat × Δcount` is EXACT: an `add(id, ±1)` IS a yield delta of `Σ(deposits keyed on id) × ±1`.
> That is plane **B** of the maintained sum ([state-repositories.md](state-repositories.md) § THE MAINTAINED SUM),
> and it is the reason a count fact is emitted in the first place — *"+1 food per river tile"* becomes one
> multiply when a river bit moves, never a re-derivation.
> ⛔ **So a dictionary is not merely a gate store beside the value plane — it IS part of the value plane.** A
> count with no route leaves every deposit scaled on it permanently wrong, exactly as a missing source fact does. The uniform keyed dictionary is **`ContextDict`** (`id → count`, read `has`/`count`,
maintained `add(id, ±1)`, zeroed `clear()` at owner reset — **there is deliberately no `set`**, which would
overwrite a refcount) — ONE kind, shared by every context, so the read is uniform and each family's key set is
OPEN (a new predicate/type is a new key, never a reshape). It is also the destination the mark-and-recompute
component retires ONTO ([DEC-contextdict-replaces-derivedcache](decisions.md#dec-contextdict-replaces-derivedcache)). `plotAttrs` keys on the `CASC_PRED_*` HAS_/IS_ plot
predicate ids; `policies` on the `POLICY_*` classification ids.

Non-dictionary scalars stay plain: population/power are `int` (power carries 0/1 today but stays `int` so a future
**volumetric** model needs no reshape); state religion is a **single enum**, not a dictionary (there is exactly one).

## Maintained EVENT-DRIVEN — never a per-turn recompute

> **⛔ WE DO NOT DIRTY CONTEXTS — THAT IS THE BOTTOM LINE (owner).** A context store carries **no staleness
> mechanism of any kind**: no flag, no stamp, no epoch, no rebuild entry point, and no `refresh*`. **The FACT
> SETS the bit it names and MOVES the count it names**, and that is the ENTIRE maintenance path
> ([DEC-contexts-are-never-marked](decisions.md#dec-contexts-are-never-marked),
> [DEC-no-staleness-vocabulary](decisions.md#dec-no-staleness-vocabulary)).
> ⛔ Re-deriving a whole BLOCK because something in its vicinity happened is the legacy read path RESCHEDULED
> from read-time to event-time, not deleted — the same single error the packages express per CHANNEL and the
> contexts express per BLOCK.
>
> **⚖ A PLOT'S PREDICATES FOLLOW MEMBERSHIP, AND OWNERSHIP IS A MEMBERSHIP FACT (owner).** *"When a city gains or
> loses ownership, the `HAS_RIVER`, `HAS_COAST` and whatever other predicates associated with that plot need to
> be added to / removed from the city in question — that is how it has to work."* So the ONE applier
> (`CvCity::onCityPlotChanged(plot, ±1)`, which folds the plot's STORED bitset) fires on **every membership
> change**, not on the worked-radius relation alone:
>
> | membership fact | what moves |
> |---|---|
> | the plot gains / loses this city's OWNERSHIP | the whole of that plot's bits, `±1` each |
> | the plot enters / leaves the worked radius | the same fold, same applier |
> | a MEMBER plot's own bits move | **the PLOT announces the bit** -- `add(bit, ±1)`, nothing re-derived |
>
> ⚑ **THE FIRST TWO ROWS ARE ONE FACT, not two routes — `CvPlot::setOwner` CALLS `updateWorkingCity`.** So an
> ownership change re-assigns the working city and announces
> `SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED`, which is the membership fold; a city cannot work a plot it does not
> own, so the two triggers cannot come apart. ⛔ **Adding a second route on `SEVT_PLOT_OWNER_*` into the same
> applier would therefore DOUBLE-COUNT** — the wrong-wiring class
> ([DEC-playability-not-a-gate](decisions.md#dec-playability-not-a-gate)), not a gap to close.
> ⚠ The ORDER composes exactly, which is worth knowing rather than re-deriving: `setOwner` writes `m_eOwner`
> first, so the `IS_OWNED` bit crosses and is withdrawn by its own predicate fact BEFORE `updateWorkingCity`
> folds the remaining bits out — no bit is subtracted twice.
>
> ⚑ **One applier, several facts** — never one fact per relation with its own derivation, and never a re-scan of
> the city's plots to find out what it now has.

> **⚖ THE SANCTIONED EXCEPTION — AN EVENT-TRIGGERED RECALC, WHERE THE FACT CANNOT NAME WHAT MOVES (owner).**
> *"It is the best example of event triggered recalc we need."* The rule above assumes the fact NAMES the thing
> that moved, so the applier can set it. Where that assumption fails the recalc is CORRECT, and banning it on the
> word `refresh` mistakes the name for the mechanism.
> ⚑ **The exemplar is `DISTANCE_TO_GOVERNMENT_CENTER`** (`CityContext::refreshGovernmentCenterDistance`, driven by
> `SEVT_CITY_GOVERNMENT_CENTER_ADDED / _REMOVED`): the value is a **MIN over the player's government centres**, so
> a centre appearing in ONE city can shorten the distance for EVERY city, and one disappearing forces a re-derive
> against the remaining set. The fact names the city that gained or lost the designation; the values that move
> belong to all the others.
> ⇒ **THE TEST, and it is narrow:** a recalc is sanctioned when (1) a genuine DOMAIN fact triggers it, (2) the
> consequence is NON-LOCAL — the fact cannot name the values that move — and (3) no finer route exists to derive,
> because the quantity is an aggregate over a set (a min, a nearest, a wholesale identity reassignment) rather
> than a sum a delta could carry. `SEVT_AREAS_RECALCULATED` is the other instance, for the same reason.
> ⛔ **What stays banned is unchanged, and none of it is this:** a recalc with NO naming fact (per-turn, blanket,
> or on-read); one that papers over a MISSED invalidation
> ([DEC-no-self-heal](decisions.md#dec-no-self-heal)); and one that RE-DERIVES A WHOLE BLOCK because something in
> its vicinity happened when the fact could simply have set what it named — the legacy read path rescheduled to
> event time, which is what the callouts above retire.
> ⚠ So *"a context carries no `refresh*`"* is about the MECHANISM, never the spelling: the question to ask of one
> is **what triggers it, and could the fact have named the value instead** — not what it is called.

The stored aggregate rides events, exactly like the rest of the spine; a missed event drifts it, but that is the
event spine's **baseline invariant** (plot-groups and vicinity drift the same way if events are incomplete), not a
context-specific weakness. There is **no blanket per-turn rebuild** and no recompute-on-read.

⛔ **AND NOTHING HEALS A MISS — that is what makes incomplete wiring safe to grow (owner).** No periodic or per-turn
context refresh, no "rebuild if it looks stale", no lazy recompute-on-read when a store looks empty, no staleness-timer
sweep, no validity/epoch stamp that triggers re-derivation, and no "recompute once per turn to be safe" backstop —
not as a safety net, not transitionally, not "just for load". The reasoning is the point: a missing emit is *cheap to
find* precisely because the wrong value stays wrong and visible, whereas a self-healing recompute converts a loud,
findable bug into permanent invisible drift **and** reinstates exactly the per-read/per-turn work the stores exist to
delete. If a store ever seems to need a "make sure it's current" call, that is a **missing fact to report**, never a
recompute to add ([DEC-no-self-heal](decisions.md#dec-no-self-heal); [state-repositories.md](state-repositories.md)
CAPSTONE — LOAD is the only full build).

- **`PlotContext`'s verdict bitset** ← the plot-substrate DOMAIN facts — terrain / feature / improvement / route /
  bonus / owner / **plot type / river / irrigation / landmark / worked** — consumed by `PlotContext` ITSELF
  ([DEC-dict-is-a-consumer](decisions.md#dec-dict-is-a-consumer)), which sets the bits the announcing fact FEEDS
  and nothing else.
  > **⚖ THE ROUTING IS DERIVED FROM A PER-BIT TABLE, never hand-written per event.** Each bit declares its own
  > derivation AND the substrate AXES it reads, side by side; a fact re-derives exactly the rows whose axes it
  > moved. That is what answers the hazard the retired whole-block derivation was right about — a hand-written
  > per-event bit mask drifting from what the bits actually read — without recomputing everything to avoid it. A
  > new bit is one row; a new fact is one axis.
  > **⚖ HAS_COAST IS SYMMETRIC: LAND WITH ADJACENT WATER, *OR WATER WITH ADJACENT LAND* (owner).** Off the stored
  > bits that is ONE statement — **a neighbour whose `IS_WATER` differs from mine**, i.e. the plot sits on the
  > land/water boundary — so the verdict reads entirely off blocks the stores already hold.
  > ⚠ It also fixes a live defect: the derivation this replaced called `isCoastalLand()`, which returns false for
  > a water plot outright, so **every water tile read `HAS_COAST` false**.
  > ⚑ **And it is what deletes the deferred-drain machinery.** The only reason the old derivation touched `CvArea`
  > at all was `isCoastal`'s `>= iMinWaterSize` test, and the bare predicate passes `-1` — a comparison no existing
  > area can fail. With no area dereference there is no unsettled-map window to defer against, so the mark/drain
  > pass, its per-plot byte vector and the `isFinalInitialized` gate all go. *(The city-scope
  > `{HAS_COAST:{minArea:N}}` form is the one that genuinely needs the water-body SIZE, and it stays
  > `CityContext`'s.)*
  > ⚖ **`HAS_FRESHWATER` keeps calling `CvPlot::isFreshWater`, deliberately** — a seven-leg verdict the ENGINE
  > still consults for irrigation and farm gates, so re-expressing it over stored bits would fork a live predicate
  > into two implementations that drift ([DEC-single-implementation](decisions.md#dec-single-implementation)).
  > Deriving a bit by calling the ONE accessor is what that rule asks for; what is banned is re-deriving the WHOLE
  > BLOCK, which per-bit routing is precisely what stops. Its neighbour leg is the one walk left, and it is now
  > paid only when a fact that actually feeds it arrives.
  > **⛔ THE PRICE OF CALLING A LIVE ENGINE PREDICATE FROM A LOAD-STREAM FACT: IT MUST BE TOTAL AGAINST THE
  > NOT-YET-READ SENTINEL.** `CvMap::read` fills the map ONE PLOT AT A TIME and each plot announces as it lands,
  > so a derivation that reaches a NEIGHBOUR reaches one that may still hold `NO_TERRAIN` / `NO_FEATURE` / no
  > city. Every such leg tests its sentinel and answers false; an unguarded `getTerrainInfo(NO_TERRAIN)` is a
  > fail-loud info-plane read ([DEC-info-plane-read-only](decisions.md#dec-info-plane-read-only)) and kills the
  > load outright. ⚠ **The self-correcting load order does NOT cover this** — it guarantees the VALUE converges
  > (the neighbour's own fact fans back and both sides re-derive), which is worth nothing if the first pass
  > raised. Convergence is about the answer; totality is about surviving to give one.
  > ⚑ **The distinction that decides which bits are exposed: what the leg READS.** `HAS_COAST` reads the
  > neighbour's STORED `IS_WATER` bit, so an unread neighbour reads false and the row is safe by construction;
  > `HAS_FRESHWATER` reaches through a live `CvPlot` accessor into the info plane, so it is not. That is the cost
  > of the [DEC-single-implementation](decisions.md#dec-single-implementation) carve-out above, and it is worth
  > paying — but it is paid HERE, by making the engine leg total, never by re-introducing a deferred drain pass.
  > **⛔ THE FACT SETS THE BIT — it does not trigger a callback that goes and asks (owner).** *"Those 'refresh'
  > functions are legacy-inspired rollerskating."* Re-deriving the WHOLE block through the same `CvPlot`
  > accessors a read used to call is the legacy read path RESCHEDULED from read-time to event-time, not
  > deleted — and this document bans that exact computation two sections up (§ a forwarded read that COMPUTES,
  > whose worked example is `isCoastalLand()`'s 8-neighbour scan). Running it once per EVENT instead of once per
  > READ is the same defect on a different clock.
  > ⚑ **It is ONE error on two planes, not two errors (owner):** recalculate-instead-of-delta-derive, which the
  > packages expressed per CHANNEL and the contexts express per BLOCK. ⚠ Their ORIGINS differ and that is worth
  > keeping straight — the package protocol was designed that way and faithfully built
  > ([superseded-ideas](superseded-ideas.md) #30: a superseded design, not a rollerskate), while these imported
  > the legacy read path. Same shape, different provenance, one fix.
  > ⚑ **It is [DEC-flag-is-fossil](decisions.md#dec-flag-is-fossil) wearing a second costume: both
  > throw away the fact's identity.** A staleness flag reduces the fact to *"something moved"*; a whole-block
  > re-derivation ignores WHICH bit the fact names. The spine already carries the answer: the fact NAMES the new
  > terrain, so a terrain fact SETS `IS_WATER` and never calls back to ask what the terrain is.
  > ⚠ **What the retired justification was right about, so the fix does not re-introduce it:** *"one uniform
  > derivation, never a bespoke per-event bit mask"* guarded against a hand-written per-event mask drifting from
  > what the bits actually read — the same hazard [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)
  > names. The answer is the packages' answer: **DERIVE the routing, never hand-write it.** What each bit reads is
  > declared beside that bit's own derivation — a small, checkable, per-BIT statement (eleven of them), never a
  > per-EVENT judgement call.
  > ⚑ **The ADJACENCY half cannot be set from one plot's payload and does not need to be rescanned either:** a
  > neighbour's coast / fresh-water verdict reads the announcing plot's **STORED block**, never a fresh walk back
  > through `CvPlot`. Same move, one hop out.
  > **⚖ THE FAN-OUT RIDES THE AXIS, NEVER A BIT'S OWN CROSSING — that is what BOUNDS it to one hop.** Only
  > `TYPE` and `TERRAIN` are neighbour-visible (a neighbour's `HAS_COAST` reads my `IS_WATER`; its
  > `HAS_FRESHWATER` reads my water + fresh-terrain state), so those two axes re-derive the 8 neighbours'
  > adjacency rows and nothing else does. An adjacency verdict is read by nobody's adjacency verdict, so a
  > cascade is structurally impossible rather than merely avoided — and `IS_WORKED` is excluded by construction
  > rather than by an exclusion anyone has to remember.
  > ⚑ **It also makes the LOAD ORDER self-correcting, which is what retires the drain pass:** whichever plot of a
  > boundary pair is read second re-derives BOTH sides, so a stream that fills the map in any order converges with
  > no deferral, no marks and no final sweep.
  **The store's own consumer is the ONLY maintenance entry** — every plot mutation that moves a stored verdict
  emits its own DOMAIN fact, so no choke point calls a derivation directly
  ([DEC-single-implementation](decisions.md)).
- **`CityContext.plotAttrs`** ← the `SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED` MEMBERSHIP fact, consumed by
  `CityContext` itself: a plot entering/leaving the city's owned worked-radius set folds through
  `CvCity::onCityPlotChanged(plot, ±1)` — the ONE applier, folding the plot's stored BITSET, so `plotAttrs` is
  literally the sum of the member plots' bits and the two granularities of one vocabulary cannot drift.
  > **⛔ THE CHOKE POINT ANNOUNCES; IT DOES NOT APPLY.** `CvPlot::updateWorkingCity` used to fold DIRECTLY beside
  > its emit, with the consumer skipping the fact at play to compensate. That is a SECOND surface maintaining one
  > fact, and it double-counts the instant the consumer grows the route — the exact failure
  > [DEC-dict-is-a-consumer](decisions.md#dec-dict-is-a-consumer) exists to prevent. The mutation site owns the
  > SOURCE, never the store. ⚑ Nothing is lost by moving it: `emit()` dispatches SYNCHRONOUSLY, so the fold still
  > lands at that instant, and each side resolves ITS city from the fact's own payload rather than from
  > `m_workingCity`, which has already moved by then.
  > ⚠ **The consumer's play-time SKIP was the tell.** It ignored the membership fact "because the choke point
  > already applied it", which is what a second maintenance surface always looks like from the store's side.
  > A wrong wiring like this is removed ON SIGHT and an interim double-count is accepted rather than weighed
  > ([DEC-playability-not-a-gate](decisions.md#dec-playability-not-a-gate)).

  A MEMBER plot's bits moving reaches the counts through the **PLOT's own announcement**
  (`SEVT_PLOT_PREDICATE_ADDED / _REMOVED`, carrying the `CASC_PRED_*` id): when a member plot's verdict bit moves,
  the PLOT says so and the dictionary applies `add(bit, ±1)`.
  > **⛔ THE PLOT SENDS IT UP THE CHAIN; THE CITY NEVER REACHES DOWN FOR IT (owner).** A city-side maintainer
  > that "unfolds the old bits and refolds the new ones" cannot work and must not be built: by the time any
  > consumer runs, the plot's bitset already holds the NEW value, so the old bits are gone and recovering them
  > means re-deriving the block -- the legacy read path rescheduled from read-time to event-time, which this
  > document bans two sections up. Let the object care about itself
  > ([tally.md](../specs/tally.md)) and the dictionary consume the fact
  > ([DEC-dict-is-a-consumer](decisions.md#dec-dict-is-a-consumer)).
  > ⚠ **THE FAILURE IF IT IS MISSING IS NOT A STALE GATE -- IT IS A COMPOUNDING MAGNITUDE.** `plotAttrs` is
  > plane B's COUNT ([state-repositories.md](state-repositories.md) § THE MAINTAINED SUM), so a bit that is
  > never withdrawn leaves every deposit scaled on it (`+1 food per flatland plot`) inflated permanently, and
  > inflated further on every subsequent substrate change.
  > ⚑ The MEMBERSHIP case is different and needs no announcement of its own: a plot joining or leaving folds
  > that plot's CURRENT bits, which are readable where they are.
- **`CityContext`'s other blocks** ← each maintained by the fact that names what moved, routed through the same
  consumer. ⚠ These are on the same re-derive-whole shape the callout above retires, and they convert the same
  way — the target is the fact SETTING what it names, never a re-run of the block's whole derivation because
  something in its vicinity happened:
  - the **VICINITY store** ← the radius tiles' bonus / owner / worked facts, each applying `±1` through the ONE
    write point.
    > **⚖ THE CITY DEFINES ITS OWN POTENTIAL WORK AREA, AND THAT IS UNAVOIDABLE (owner) — because the cross
    > GROWS** (culture level, `adds3rdRing`, the workable-radius override), so no fixed geometry can answer it.
    > The city hands that definition to the plots as `CvPlot`'s **`workableBy`** membership, announced per plot as
    > `SEVT_PLOT_WORKABLE_BY_ADDED / _REMOVED`; the fold then reads the plot's own list and is EXACT.
    > ⛔ **There is no radius inverse and no membership test** — a store keyed on the radius folds a DELTA, and a
    > radius GROWING is an ordinary fact rather than something a walk must rediscover.
    > ⚑ **THE ADDRESSING IS WHAT MAKES IT CHEAP, and it is already defined (owner): the city-plot table is
    > RING-ORDERED** — index 0 the city, 1–8 ring 1, 9–20 ring 2, 21–36 ring 3 — so a radius IS a prefix of it and
    > a level change is exactly the index range `[oldCount, newCount)`. Nothing geometric is rebuilt;
    > `CvCity::changeWorkableArea` walks that range and is the whole maintenance surface, reached from three sites
    > (the city starting to exist, ceasing to, and `setCultureLevelInternal`).
    > ⚑ **The same route SEEDS the store**, so there is no separate build pass: a city establishing its work area
    > announces one membership fact per plot, at birth and again at `GAME_LOAD_FINISHED` — where the map streamed
    > before the players, so nothing could have announced to a city that did not yet exist.
    > ⚠ It is DERIVED: zeroed at `CvPlot::reset` and never serialized, since a recycled plot would otherwise name
    > a city from the previous world ([DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted)).
    > ⚠ `m_iCityRadiusCount` / `m_aiPlayerCityRadiusCount` keep their own readers and stay — what this replaces is
    > the vicinity fold's need to re-derive membership, never those counters.
  - the **AREA facts** ← the plot-TYPE fact near the city, the per-area **`SEVT_AREA_TILE_ADDED / _REMOVED`**
    (one area's tile count moved — only the cities IN that area re-read), and the wholesale
    **areas-recalculated** fact below.
    > ⛔ **The per-area route DECLINES while `CvMap::recalculateAreas` is mid-pass** (`isRecalculatingAreas`).
    > That pass clears every plot's area and reassigns every id, firing the per-area fact once per plot, so a
    > per-tile refresh inside it would be O(plots × cities) of work against a map that does not exist yet — and
    > the wholesale fact closes the bracket by refreshing every city once, which is the answer for that window.
    > ⚑ The EMIT is untouched: the fact fires and the CONSUMER declines it
    > ([event-spine.md](../specs/event-spine.md) — never suppress an emit to fix a consumer).
  - the **holy-city and HEADQUARTERS counts** ← their own facts, applied `±1`.
    > **⚖ THE DESIGNATION LIVES ON `CvGame`, AND THE CITY HOLDS ONLY HOW MANY NAME IT.** The authoritative
    > assignment is `CvGame`'s, keyed by religion / corporation — exactly one city each, so uniqueness is
    > STRUCTURAL there and a per-city bit could never guarantee it. What the city needs is the bare verdict,
    > and that is a count.
    > ⛔ **The bare verdict is NOT asked of `CvGame` per entry.** `CvCity::isHolyCity()` / `isHeadquarters()`
    > used to walk the WHOLE religion / corporation registry asking `getHolyCity(r) == this` once per entry —
    > on AI paths, and forwarded to by `CityContext::isHeadquartersAny()`, so a context whose premise is O(1)
    > bare fetches was forwarding to a registry scan. That is the forwarded-read-that-COMPUTES defect this
    > document names, and the test settles it: **ask what the read WALKS** — one pointer forwards, a registry
    > scan earns a store.
    > ⛔ **And it is a DELTA store, not a refreshed one.** The holy-city count was previously maintained by a
    > `refreshHolyCity()` the fact CALLED — the legacy read path rescheduled from read-time to event-time, not
    > deleted. The fact now applies `±1` and nothing re-derives. ⚠ Consequently there is **no build pass** for
    > either count at city-founded or load-finish: the facts already carry them (`CvCity::read` announces every
    > designation the city holds), and a rebuild beside a delta store doubles it.
- **`EmpireContext.policies`** ← the **civic / trait / player-init DOMAIN facts**, consumed by the policy store
  itself, which applies each grantor's policy block as a DELTA — never a refill, which would recount every time
  and so hide the multi-grantor case the count exists for. It is the single
  source the one policy read (`ev_playerHasPolicy`) uses — reads never re-walk the grantors. The **player-init** fact
  is load-bearing on its own: a player's INITIAL traits are written straight into the has-array rather than through
  the trait setter, so that fact is the only announcement they ever make.
  ⛔ It is deliberately **not** maintained from `CvPlayer::setCivics` / `setHasTrait`: a direct hook beside an event
  is a second maintenance surface for one fact, and the fact already exists.
  ⚑ **This store is the ONE home of the per-flag policy verdicts** — the boolean getters (`isNoForeignTrade`,
  `isStateReligion`, `isInquisitionConditions` and their kin) read `has`, and the AI civic-value what-ifs read
  `count` with an unconditional vacuum subtraction of the option slot's own civic. The serialized per-flag
  counters `processCivics`/`processTrait` once pushed beside it were the second-surface class and are cut
  ([DEC-accumulator-cut-uniform](decisions.md#dec-accumulator-cut-uniform)); their changers' side-effect riders
  live at the adoption site (`setCivics`), diff-gated on the policy actually moving. Genuine non-cascade state
  on that same class (the revolution index, `changeMaxConscript`, `changeSpecialistValidCount`, hurry counts)
  stays serialized as-is.
- **The HELD-TRAIT set** ← the **same trait / player-init facts**, consumed by its own store beside the policy one
  (one dictionary per area of responsibility — the `TRAIT_` and `POLICY_` key spaces are disjoint registries).
  > **⚖ IT IS THE CASE WHERE ONE AXIS SPLITS ACROSS THE SCAN-vs-HOP TEST, and reading the FORWARD row as settling
  > it is the mistake to avoid.** *Does this player hold `TRAIT_X`* resolves through one pointer, so trait
  > PRESENCE is correctly a forward and earns no store. *Which traits does this player hold* is a different
  > question with the same subject: off `m_pabHasTrait` it walks all 369 trait records to rediscover the handful a
  > leader carries — the O(registry) sweep [state-repositories.md](state-repositories.md) names on the unit plane
  > (*"the sum walks what the unit HOLDS, never the registry"*). ⇒ **Ask what the READ walks, never what the
  > SUBJECT is:** the same axis can forward one question and store another.
  > ⚑ **Its reader is the keyed-deposit walk** ([modifier.md §5](../specs/modifier.md)): a trait's target-keyed
  > deposits stay SOURCE-side (the §4 per-set carve-out), so the read asks each LIVE SOURCE what it deposits onto
  > that key. That read is cheap *"because it iterates the handful an entity AUTHORED"* — which holds only if
  > discovering the live sources is itself cheap.
  > ⛔ **The sign comes from the fact's IDENTITY, never from a re-read.** `setHasTraitInternal` writes the
  > has-array BEFORE it emits, so a handler asking `hasTrait` would read the NEW value on both ends and never
  > withdraw — the same reason a city can never reach down for a plot's old bits.
- **AREAS are announced WHOLESALE.** `CvMap::recalculateAreas` clears every plot's area, empties the area list and
  recalculates, so it emits **`SEVT_AREAS_RECALCULATED`** (no payload — the fact IS "all of them") and every holder
  of an area id re-reads. Areas are virtually never recalculated (terrain levelled to sea level — the WMD mechanic —
  plus map generation), so the blanket costs nothing at its real frequency, and it is **not** the banned self-heal: a
  wholesale identity reassignment is not addressable per-source, so no finer route exists to derive
  ([DEC-no-self-heal](decisions.md#dec-no-self-heal) bans papering over a MISSED invalidation, not announcing a
  genuine wholesale one).
- **Forwarded** fields need no maintenance — they read the live source.
- **Load** — `EmpireContext.policies` rebuilds from the **in-read civic/trait/player-init emits** as they stream (a
  derived aggregate recomputes from source on load, [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted),
  never trusted from a save) — through the consumer, not a second build mechanism beside the event stream.
  `CityContext`'s other blocks build once at `GAME_LOAD_FINISHED`, because each reads state that is only complete when
  the whole stream has ended (the areas deserialize after the plots). That single pass IS the load build — the only
  full build there is — after which the facts alone maintain them.
  ⚑ The VICINITY dictionaries, `onSite` included, are not among them: they seed through the ORDINARY membership
  route, because re-establishing each city's work area announces one `SEVT_PLOT_WORKABLE_BY_ADDED` per plot and the
  applier folds that tile's CURRENT bonus and served resource. One route, both jobs — no separate build pass to keep
  in step ([DEC-spine-reseed](decisions.md#dec-spine-reseed): never a second build mechanism beside the event stream).
  `CityContext.plotAttrs` builds from the in-read DOMAIN events
  ([DEC-spine-reseed](decisions.md#dec-spine-reseed)): each `CvPlot::read` announces its deserialized working-city
  fact (`SEVT_PLOT_WORKING_CITY_ADDED / _REMOVED` — the genuine read site emits), and `CityContext`'s own consumer
  BUFFERS the load bracket's membership facts and folds them through the one applier
  (`CvCity::onCityPlotChanged`) at `GAME_LOAD_FINISHED` — the cities stream AFTER the map, so the fold applies once
  after the stream ends (the [enabler §7.1](../specs/enabler.md) order rule's second option, never the mixed form).
  ⚠ **The buffer is an ORDERING fact, not a staleness mechanism** — there is no city to fold into while the map
  streams. The per-bit facts need no such treatment and are simply dropped inside the bracket: by the drain every
  plot has announced its substrate, so the fold reads FINAL bits and applying them earlier would only count them
  twice. There is never a blanket per-turn recompute.

## Scope set — plot / city / player now; units FUTURE (role-specific); no AreaContext (owner)

Contexts exist today on **plot, city, and player**. There is deliberately **no `AreaContext`**, and the reason
generalizes: **an area is not a scope at all.** A scope must be unambiguously OWNABLE — universal (world) or
owned by exactly one player up the chain — and a landmass is shared by several empires at once, so anything on it
is a per-(landmass × player) cross-product rather than a scope
([state-repositories.md](state-repositories.md)). An area is therefore a bare **id**, "a really big plot" to
reference, and an area-shaped effect authors at **empire**.

> **⛔ AND TEAM IS NOT A CONTEXT EITHER — `CvTeam` IS THE TECH BRIDGE; `CvPlayer` HOLDS THE CONTEXT (owner).**
> A team's job on this plane is to hold the shared TECH/war facts and hand them across its members. It owns no
> live-state surface, so **every team fact a reader needs is asked of the PLAYER** — team-held techs through
> `EmpireContext::teamHasTech`, and everything else forwarded the same way.
> ⇒ The team carries the unified TECH and PROJECT lists as MEMBERSHIP; anything DERIVED off them — the capability
> union, the skill planes — is the PLAYER's. A derived store landing on `CvTeam` is misplaced by construction,
> whatever maintains it.
> ⛔ **Consequence, and it is structural: `CvCascadeEvalCtx` carries NO `CvTeam*`**, and no getter, evaluator,
> predicate or valuation reaches a team to answer a state question. A player that cannot answer one is a
> **CONTEXT GAP to close by adding the forward** (§ THE EVAL CTX), never a reason to reach a team.
> ⚠ **Do NOT read the DEPOSIT spine as licence.** `world › team › empire › city › plot` is the containment spine
> for MAGNITUDES, and a team genuinely carries a package ([state-repositories.md](state-repositories.md): three
> channels) — so "team is a scope" is true of deposits and false of state. Conflating the two is what puts a
> `CvTeam*` back in a reader's hands; the same distinction is stated on `CvTeam` itself.

⚑ **What the contexts DO carry is the area FACT** — the city's area id, its tile count and the coastal
water-body size, forwarded by `CityContext` for the `AREA_SIZE` token and the adjacency reads. *"We rather use
the area id"* (owner): the id is a fact a city reads, never a place state lives.

**Units are a deliberate FUTURE scope, held off on purpose (owner).** A unit context must be **ROLE-SPECIFIC**: the
goal is that a unit no longer carries ALL the data (the ~247-field fat-unit problem) — each unit holds only the state
its role needs. Working out that role-partitioning is *why* it waits, rather than wiring a fat unit context now.

**⚖ IDENTIFIED MEMBER — the UPGRADE resolution belongs to the UNIT CONTEXT (owner).** *"Upgrade should live in the
unit context."* **The DIRECTION is the ruling (owner): the UNIT asks.** *"When a unit asks if they can do their
upgrade in a city somewhere, then the unit has to check if a city has whatever requirement it needs."* This is
built: `CvUnit::getUpgradeCity` drives the search and fans out to `GET_PLAYER(...).cities()`, asking each
candidate's own `getUnitAvailability(eUnit)` — a city is a place the query LOOKS, never the owner of the
question.

⚑ **AND IT IS PURELY AN AI-LOOP CONCERN (owner)** — the AI deciding whether, and where, to send a unit to
upgrade. That settles its cost class: any caching this resolution earns is **AI-heuristic caching**, the
sanctioned residual ([superseded-ideas #1](superseded-ideas.md)), NOT engine state and NOT a derived cache on
the cascade plane — it would carry no staleness protocol and answer to no invalidation contract.

## The read — the per-GROUP valuation: `(CityContext, EmpireContext, CvPlotGroup)` → the group's values

An info's ACTUAL contextual output is read **one endpoint per GROUP of channels** (owner), never per single channel:
`expectedFlatYields` / `expectedYieldModifiers` / `expectedPlotYields` / `expectedFlatCommerce` / `expectedWellbeing`.
Each takes the three live contexts and fills that group's ×100 array — **you pass the contexts in, you get the group's
expected values out**:

- **CityContext** — vicinity + local state AND the river/water/… plot-attr COUNTS (`plotAttrs`). A building reads the
  CITY context for "how many river tiles", **never a PlotContext directly** (owner) — the plot-count sums live in the
  city context. It also answers the city's **traded** bonuses (through the city's own plot-group-backed reads).
- **EmpireContext** — the empire-scope state (civics/traits/policies/state religion).
- **CvPlotGroup** — the trade-network object; the reserved explicit **traded**-bonus source (`connection:"trade"` vs
  `"vicinity"`, [json.md §3.4](../specs/json.md)). Traded state is **NEVER mirrored into `CityContext`**. The
  valuation seam fills it into the eval ctx (`CvCascadeEvalCtx::plotGroup`): a `connection:"trade"` atom reads the
  city's own plot-group-backed RELAY when a city is bound (`CityContext::tradedBonusCount` forwards to
  `CvCity::getNumBonuses` — the tech-gate/minted/corp layer over the group's count), and the passed group directly
  for the city-less what-if.

Each endpoint returns the UNCONDITIONED ×100 base PLUS every conditioned `m_cond` deposit whose condition holds — summed
via the **one** evaluator (`MMKernel::applies`) over a `CvCascadeEvalCtx` the contexts fill (`CityContext::fillEvalCtx`
= city/plot, `EmpireContext::fillEvalCtx` = player/team) — so the contexts ARE the eval state, not a raw-pointer ctx
built beside them. `expectedPlotYields` scales each plots-target deposit by `cityContext.plotAttrs.count(predicate)`.

> **Everything an info holds is ×100** ([DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100)) — readJson converts
> human→×100 once at load; the info never de-scales; a reader `÷100`s at the point of use. So these endpoints add
> `value100` directly, and the materialized base members are `value100`.

> **Naming — no abbreviated parameters (owner).** Parameters are spelled in full (`cityContext`, `empireContext`,
> `plotGroup`), never `cx`/`pg`: short names are only defensible inside a tightly-scoped lambda, which C++03 lacks.
> Index parameters likewise name the enum they key (`YieldTypes eYield`, `DefenseKind eKind`), reusing the
> existing engine + family enums — a new family mints one typed enum, and the group's entries + its `expected*`
> array both key off it ([patterns.md § THE GETTER SETUP](patterns.md)).

## See also
- [patterns.md](patterns.md) — the INFO DATA-OUT contract + the per-group valuation surface that reads these contexts.
- [state-repositories.md](state-repositories.md) — the maintained-sum plane, and the model BOTH planes share.
  ⛔ **A context is cascade OUTPUT, not a separate "input" kind (owner):** *"contexts, when thinking about it,
  are in essence the output of the cascade."* Same scopes, same spine, never serialized, rebuilt by the same
  reseed, read as the same bare fetch — and maintained the same way, by the fact that names the source
  ([DEC-maintained-sum](decisions.md#dec-maintained-sum)). What differs is only WHO CONSUMES the value: a
  package answers a magnitude, a context store answers a gate. ⚠ That is a statement about the consumer, never
  about the kind of thing being stored, and treating it as two planes is what let them drift onto opposite
  maintenance mechanisms.
- [../specs/modifier.md](../specs/modifier.md) — the deposits the getters sum; [../specs/enabler.md](../specs/enabler.md)
  — the availability machine that reads the same state; [decisions.md](decisions.md#dec-scope-contexts) — the ruling.
