# What a context STORES vs FORWARDS — ⛔ a context is an EVENT-BUILT STORE, not a forwarding facade

> Part of the **[cascade](../cascade.md)** spec.

**"Context should be built on events — that is the design of it."** And the purpose of storing is that the state
becomes DISTINGUISHABLE, so an info can say it will actually deliver this, based on this state.
A context that merely forwards to its bound object delivers none of that — it is the same pointer hop with an
extra name, so the design collapses into "pass the god-object like always."

> **⚖ THERE SHOULD VIRTUALLY NEVER BE AN ORDERING PROBLEM — EVERYTHING IS POPULATED BY THE REPLAY OF SPINE
> EVENTS.** That is what makes consumer registration order almost irrelevant: each consumer builds its
> own state from the SAME fact stream, so no consumer waits on another's build.
> ⛔ **The anti-pattern that manufactures ordering is a store that RE-DERIVES by READING another system's built
> state.** It cannot run until that system is built, which instantly turns registration order into a dependency —
> and the dependencies go both ways (the enabler gates THROUGH these stores, so a store reading the enabler is
> circular). ⇒ **A store LISTENS and applies a delta; it does not read a set and recount.** The city's
> `amenities` fold is the worked case: as a delta off the per-building fact it builds itself identically at load
> (the save read's own emits) and at play, with no phase ordering; written as a re-derivation over the enabler's
> operating set it could not build at load at all.
> ⚠ **The exception is a HARD COUNTER, and it is SERIALIZED STATE: a city's POPULATION, its CULTURE, its
> STORED PRODUCTION — "these kinds of things have to obviously just be serialized out."** They are not derived
> from anything, so the VALUE comes back off the save directly and is FORWARDED (below) rather than stored.
> **⛔ BUT THEY DO EMIT, AND THE SAVE READ IS WHERE.** Reading the counter off the stream fires
> `CITY_POPULATION_ADDED <the stored amount>` — the ordinary `_ADDED` fact with its magnitude
> ([spine.md](../spine.md)), not a bespoke load verb. ⚑ **The counter needing no event and its
> CONSUMERS needing one are different questions, and conflating them is what left a hole:** every deposit
> scaled `per: {POPULATION}` is maintained from ZERO by applying, so without that fact a loaded city's
> population-scaled deposits would all be missing — the value present on the object and absent from every sum
> derived off it. ⚑ It also needs no load special case: the same `_ADDED` fact the growth path emits, with the
> save's amount instead of 1 ([the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed) — read, emit, populate). That raises no ordering question at all.
> ⇒ The three-way test, and the exception confirms the split rather than bending it: **DERIVED ⇒ built by the
> event replay, never serialized** ([derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-)); **genuine
> non-derivable state ⇒ serialized, and forwarded live** ([save.md §5](../specs/save.md) — a serialized store
> survives ONLY for state no derivation can produce); a context never stores the second kind.

The split is by **DERIVED vs RAW**, not by convenience:

- **STORE — every DERIVED fact the evaluation reads.** Predicate verdicts, aggregates, unions: computed ONCE by
  the ONE derivation for that fact and maintained by the spine events, never recomputed at read. This is the
  context's substance. It is derived state, so it is **never serialized** and is rebuilt at load by the reseed
  ([derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-), [the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)).
- **FORWARD — only the object's OWN RAW data** that it already maintains O(1) (the substrate ids a parameterized
  predicate keys on, population, …). Forwarding raw data is not duplication; storing a second copy of it would be.

⚑ **THE PAYOFF — this is why the design earns its cost: once contexts are PURELY event-updated, an
enormous class of per-read CALCULATION becomes obsolete.** Not "gets faster" — ceases to exist. Every read-time
scan/union/walk collapses into a stored value some event already maintained, and reads become bare fetches
(§ THE MAINTAINED SUM, above; [turn time is king](16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).
The in-tree exhibits are not hypothetical: `isCoastalLand()` is an 8-neighbour scan **per predicate
evaluation**, and the §5a vicinity check is a radius union **per check**. The win is STRUCTURAL: once the fact
is stored there is no read-time work left to do, so cost tracks EVENT volume (what changed), never read volume
(how often it is asked) — and it is observed where every performance claim is observed, on the per-turn wall
clock ([turn time is king](16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)).

⛔ **BUT THE TEST IS A SCAN, NOT A HOP — and `getNumBonuses` is the case that marks the line.** What earns a
store is read-time work that GROWS with something (neighbours, radius tiles, a registry). A read that resolves
through a POINTER to the object which already owns the number O(1) is not that, and storing it anyway makes a
third copy of one fact ([enabler.md §8](../specs/enabler.md) RESIDENCY: the plot group owns the network count,
the city relays it, the context forwards the relay). ⚠ This one had a store on exactly that mistaken reading,
and it cost a sweep of every bonus on every fact that could move one — strictly more work than the hop it was
avoiding. **Ask what the read WALKS; if the answer is "one pointer", forward it.**

> **⛔ SO A CONSUMER NEVER WALKS AN INFO'S KEYED LIST TO ASK A PER-ITEM LIVE-STATE QUESTION — THE EVENT-BUILT
> READ-ONLY STATE ANSWERS IT.** *"There should be no iterating like that; the eventspine-built read-only
> should be able to handle that."* The shape to recognise is `foreach_(key in someInfo.getKeyedList()) { …
> liveStateRead(key) … }` — the info supplies the keys and the loop asks the live state once per key. That is the
> per-read scan this whole section deletes, merely sourced from an info instead of from the map.
> ⚑ **The worked case is the corporation's consumed bonuses:** `foreach_(bonus in corp's bonuses) getNumBonuses(bonus)`
> re-executes [enabler.md §8](../specs/enabler.md)'s hottest cluster once PER BONUS, and where the question is a
> MAGNITUDE the answer is already authored — the rate carries a `per:{anyOf: consumed bonuses}` scaler, so the
> valuation resolves rate × count in one call and the loop simply disappears.
> ⛔ **And renaming the receiver is NOT the fix.** A walk that compiles against the new getter reads as migrated
> while doing exactly what it did before — the half-migration
> ([build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)), and it hides the hole the maintained read has
> not yet filled ([legacy must fail loud, never mask a cascade gap](../specs/validation.md#legacy-must-fail-loud-never-mask-a-cascade-gap)). Leave such a site DANGLING as the
> census entry it is until the maintained fetch exists.

⛔ **A forwarded read that COMPUTES is the defect this rule exists to kill.** `PlotContext::hasCoast()` forwarding
to `CvPlot::isCoastalLand()` — an 8-neighbour scan with an `area()->getNumTiles()` call per neighbour, on every
predicate evaluation — is the worked example, and it directly contradicts
[patterns.md](../architecture/patterns.md): *"every evaluator predicate is an O(1) CONTEXT fetch … a predicate that walks
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
| **CityContext** | `CvCity` | `plotAttrs` — per-predicate plot COUNTS (the fold of member plots' bits) · **`amenities`** — the `AMENITY_*` id→COUNT fold over the city's OPERATING buildings + the empire-scope grantors (json §8; the count is load-bearing — see the callout below) · **the VICINITY BONUSES available in the city** — the §5a radius union, MAP half (see the split below) · the **AREA facts** (area id, its tile count, the coastal water-body size) · the **holy-city and HEADQUARTERS counts** — how many religions / corporations name this city, each a delta store fed ±1 by its own fact · the **CORPORATION-ACTIVE verdicts** — the remembered per-corp `{HAS_CORPORATION}` verdict, held ONLY so its crossing can announce (`SEVT_CITY_CORPORATION_ACTIVE_ADDED / _REMOVED`, the fact plane C routes on): each leg's fact triggers a re-read of the ONE engine implementation (`CvCity::isActiveCorporation`, the sanctioned engine-owned input — the read side stays the live forward), and a re-read that moved nothing announces nothing | population, power, religion presence, holy-city-of, corporation, capital, government-centre, fresh-water access, property value (raw, `CvCity`-owned, O(1)); state religion (→ owner `CvPlayer`); **the TRADED count** — the gated network number, forwarded through `CvCity::getNumBonuses`, which relays to the PLOT GROUP that owns it ([enabler.md §8](../specs/enabler.md) RESIDENCY: nothing mirrors the group); **the CURRENT REALIZED YIELDS** — the city's own O(1) group read, forwarded so a valuation can resolve a percent against a real base (below); **the CURRENT REALIZED COMMERCE** — `CvCity::getCommerces`, the per-commerce SPLIT of that commerce yield by the empire's sliders plus each channel's own deposits (§2a), forwarded for the same reason |
| **EmpireContext** | `CvPlayer` | `policies` — the empire's enacted-policy set (the derived UNION over live civics'/traits' policy blocks, stored nowhere else) · **the HELD-TRAIT set** — the `TRAIT_` id→COUNT fold, a delta store fed ±1 by the trait facts (§ the callout below: enumerating what a player holds is a SCAN even though testing one trait is a hop) | state religion (single enum → `CvPlayer::getStateReligion`), civics/**trait presence**/heritages, the team-held facts; **the CURRENT REALIZED COMMERCE** — `CvPlayer::getCommerces`, the four empire RECEIVER totals: the city-yields forward's empire twin, so an empire-scope percent resolves against a real base; **the COMMERCE SLIDER PERCENTAGES** — the player's gold / research / culture / espionage rates, the `GOLD_RATE`/`RESEARCH_RATE`/`CULTURE_RATE`/`ESPIONAGE_RATE` tokens ([json.md §3.1](../specs/json.md)); a group keyed by `CommerceTypes`, forwarded because `CvPlayer` owns them O(1) |
| **PlotContext** | `CvPlot` | the `CASC_PRED_*` verdict **BITSET** — the OWN-PLOT block (water/land/relief/hills/peak/river/irrigation/feature-present/landmark/owned/**worked**) plus the ADJACENCY block (coast, fresh-water) · **`workableBy`** — the cities whose potential work area this plot is in, set by `CvCity::changeWorkableArea` and announced per plot (§ the VICINITY store) | the RAW substrate a parameterized predicate keys on — terrain/feature/improvement/route/bonus ids, owner, latitude, nature yield — plus city-presence, the one verdict with no mutation event a bit could be maintained from (→ `CvPlot`); **the plot's CURRENT REALIZED YIELDS** — `CvPlot::getYields`, the whole isolated per-plot base package as a bare cache fetch. ⛔ The PRE-IMPROVEMENT leg (`natureYield`) is a SECOND SLOT of that same package, never a per-call computation: it is asked per (plot × improvement × yield) by the placement gate and both improvement valuations, which is the cost class this whole section deletes. A read that recomputes it is the forwarded-read-that-COMPUTES defect above, and the number is already in the package |

⛔ **THE VICINITY SPLIT — the context holds the MAP half, the enabler holds the BUILDING half.** The §5a in-vicinity
supply is a union of two independently-owned halves, and storing either one twice is the duplication the model bans:

- **MAP providers** (a bonus on a radius tile providing itself) are per-scope live state with no other home, so
  `CityContext` holds them — tiered by the §3.4 ownership discriminator (`owned` / owned+neutral / `crossBorder` /
  `worked`), since the `vicinity` band selects which tiles count — the plot-set axis, distinct from `connection`.
- **ACTIVE BUILDING providers** (`provides.bonuses`) are the operate/provides **least fixpoint**, which only the
  enabler can resolve — an operate condition may consume a bonus another active building provides. They stay
  `OperatingBuildings::provided`, reached through `CvCascadeEvalCtx::vicinityProvidedBonuses`.

The reader unions the two. A mirror of the building half on the context would also *drift*, because the enabler
mutates its set in place as the fixpoint ripples.

> **⚖ THE MAP HALF IS TWO DICTIONARIES, NOT ONE — bonuses, and natural features.** There is nothing wrong with two dictionaries, one for bonuses and one for natural features; what I don't want is the constant
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
> **⚖ NEUTRAL IS THE DEFAULT STATE — IF THERE IS NO OWNER IT IS NEUTRAL.** So neutral is **not stored and
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
> ⛔ **IT NEVER CONSULTS THE NETWORK, and that is the ruling rather than an omission: onSite and traded are
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
> resource gets served.
>
> **⛔ THE TWO ARE NOT PEERS, AND THAT IS THE WHOLE CONFUSION — VICINITY IS THE PLOTS, `onSite` IS A CONNECTION
> THROUGH THEM:** *"vicinity is the plots actually in vicinity; if a bonus is on site, it means it's
> connected to a city via this vicinity band."* So a vicinity BAND selects WHICH PLOTS COUNT (the ownership
> tiers, and `worked`), while `onSite` is a VERDICT ABOUT THE BONUS reached through that band — the resource is
> available to this city because a tile in the band serves it, or an active building supplies it. One names a
> plot set; the other names how a resource arrives.
> ⚠ **The orthogonality is against the NETWORK, never against vicinity** — `onSite` and `connection:"trade"`
> are the two independent routes a bonus takes to a city (above). Reading `onSite` as "orthogonal to vicinity",
> or as merely a stricter band of it, are the two halves of the same mistake.
>
> **⛔⛔ AND HERE IS WHERE EVERY AGENT SCREWS UP, WHICH IS THE REASON THE BONUS LIST IS CALLED `onSite` AT ALL
>: A BONUS SUPPLIED BY A BUILDING IN THE CITY IS *ALSO* VICINITY.** *"Agents could not get that bonuses
> given from buildings in a city is also vicinity, which is why it was changed to onSite for the bonuslist."*
> The word "vicinity" reads as TILES, so agent after agent took the building-supplied half to be something else
> — a different mechanism, a special case, or simply absent — and wrote reads that answer only the map half.
> ⇒ **The name was changed to stop that**: `onSite` says *the resource is available here*, without implying by
> what route, so the two halves sit under one word that cannot be misread as "on a tile".
> ⛔ So a reader that answers only the map half is WRONG whatever it is called, and "it says vicinity, so it
> means tiles" is the specific inference to refuse. The union is not an implementation detail of the read — it
> IS the read ([json.md §5a](../specs/json.md): a herd BUILDING and an improved herd TILE are the same act).
> ⛔ **THE AXIS IS SPLIT, so the wrong call cannot be spelled at all** — `onSite` is a value of `connection` (the
> ORIGIN axis: `trade` = the network has it, `onSite` = it comes from the city itself, mutually exclusive), and
> `vicinity` is the PLOT-SET axis and nothing else. A gate wanting either origin states two atoms under an `any`,
> deliberately; there is no combined selector and none may be reintroduced
> ([json.md §3.4](../specs/json.md)).
> ⚑ **Why prose was not enough, recorded because it is the general lesson** — a tier-less `hasVicinityBonus`
> silently answered the on-site verdict, and the first fix was a comment telling the next caller to name its
> tier. It recurred ([the hard-typing-or-rollerskate rule](../../AGENTS.md#design): prose is the
> weakest rung, binding only an agent who reads it, believes it, and still remembers it). The split is what makes
> it unsayable instead.
> ⇒ **The NAME binds too: a function whose answer can be the on-site verdict does not carry `vicinity` in its
> name** — `EnablerKernel::cityHasBonusOnSite` for a caller holding a `CvCity*`, `ev_vicinityHas` for the
> evaluator, which must stay ctx-shaped because the eval ctx is forbidden a game object (§ THE EVAL CTX, below). Two
> entry points because they reach different planes; ⛔ neither may answer with one half.

⚖ **`CityContext.amenities` — THE CITY'S OWN FEATURE LIST, AND THE CITY IS WHAT GETS CHECKED.** A
grantor's `amenities` block ([json.md §8](../specs/json.md)) is static info data; what a consumer actually asks
is *"does THIS CITY have this?"*. So the city holds the FOLD — over its **operating** buildings, plus the
empire-scope grantors (civic / trait / tech) that reach every city — and every gate reads it O(1). ⛔ A consumer
must never loop the city's buildings asking each one, and a grantor's per-key named getter is not the consumer
surface: the fold is the ONE reader of the grantor side
([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)), alongside display/pedia.

> **⛔ IT IS A ContextDict (id→COUNT), NOT A BITSET — absent or 0 is false, anything else true.** Several
> grantors can confer the SAME amenity, so a removal DECREMENTS rather than clears: *losing one power plant must
> not darken a city that has two.* A bitset cannot express that — an "amenity removed" fact would clear a bit
> another live grantor still justifies. ⚑ So it is the ordinary `ContextDict` this doc already specifies
> (`has(id)` ≡ `count > 0`), the same refcount shape the enabler's membership formula and the operating set's
> provided-bonus counts use, and the semantic legacy already had in its per-flag counters.

> **⛔ THE DICTIONARY IS THE FINAL STOPPING PLACE — IT IS WHERE THE DATA ACTUALLY LIVES.** Every grantor
> fact lands here and comes to REST: the building leg off the enabler's active↔dormant crossing (a dormant
> building confers nothing, [enabler.md §3.2](../specs/enabler.md)), the civic / trait / tech legs off their own
> facts. ⛔ It is NOT a projection of some other system's truth, and it is NOT relayed from the enabler — the
> enabler is a SOURCE OF FACTS, never the home of this answer. One dictionary, every leg, one mechanism, and
> every reader — the enabler's own gate included — reads it HERE.
> **⛔ AND IT IS ITSELF A SPINE CONSUMER THAT KNOWS EXACTLY WHICH EVENTS TO LOOK FOR.** A dictionary
> REGISTERS on the spine and DECLARES the precise set of facts that maintain it; it is not fed by a central
> switch that fans out to whichever store a case happens to name. ⚑ **The interest set IS the maintenance
> contract, which is what makes it auditable at all:** with a fan-out, "does this fact reach the store that
> needs it" is answerable only by reading the router, so a missing route hides in a `switch` that looks
> complete; with a self-declaring dict the gap is visible AT the dict. It is also what makes the RECEIVED line
> name something useful — the consumer that acted is the dictionary, by name
> ([spine.md](../spine.md) § THE RECEIVED LINE).
> ⚑ It is the same move as the spine's own per-domain isolation: adding a domain touches only that domain, and
> adding a dictionary now touches only that dictionary — no shared edit, no central case to remember.
> ⚠ **REGISTRATION ORDER REMAINS A CONTRACT and self-registration must not quietly break it.** The enabler's
> load-end gate pass evaluates THROUGH these stores, so every dictionary registers inside the CONTEXTS band of
> `contexts → enabler → modifier → triggers` ([enabler.md §8](../specs/enabler.md)) — ordering is a property of
> the band, never of which translation unit happened to initialize first.
> ⛔ This does not license one consumer per SYSTEM being violated ([the enabler and the modifier cascade are two separate systems](../specs/enabler.md)):
> that ban is on one consumer routing TWO MACHINES, not a cap of one consumer per machine. Several dictionaries
> inside the contexts band are still exactly one system's worth of maintenance.
>
> **⛔ A CONTEXT DICTIONARY ONLY EVER CONSUMES; IT NEVER EMITS — which is why it can close no loop.**
> Facts go in, state comes out, nothing goes back. A later read of that state by the machine whose fact fed it is
> an ordinary read of CURRENT state, not feedback. ⚠ **The ordering ban at the top of this section does NOT
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

> **⚖ POWER IS AN AMENITY, AND IS TREATED AS ONE.** `CvCity::getPowerCount` reads the `providesPower`
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
change — it is pure data, the open-registry promise ([the classification-infos registry](../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities))
reaching the consumer side. ⛔ There is no surviving hand-named counter for any of them — no
`changeGovernmentCenterCount` / `changeNoUnhappinessCount` / `changeNoUnhealthyPopulationCount` member on
`CvCity` — the fold replaced them, exactly what
[every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta) asks for.

⚠ **Two attributes that LOOK like the same family are deliberately NOT on this fold, and neither is a hole to
close by routing it there.** `governmentCenterDistance()` is a separately STORED value (§ the sanctioned-recalc
exemplar, below) because it answers a MIN over the player's centres — a different question from
`isGovernmentCenter()`, which the amenity fold already answers. And `HAS_FRESHWATER` is a **`PlotContext`** bit,
not a `CityContext` amenity, that deliberately keeps calling the live `CvPlot::isFreshWater` engine predicate
rather than folding onto a dictionary (§ the adjacency callout, below —
[the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)).

**Pass by reference/pointer, never by value.** Passing a bound context is far cheaper than snapshotting
values; a context is never a value copy — that is *why* it forwards rather than mirrors.

**⚖ THE TWO PASS-IN SCENARIOS — a context crosses a call boundary in exactly TWO places, the two
condition-evaluation sites:** (1) **the VALUATION** — the `expected*` per-group reads and the package rebuild's
conditioned-deposit evaluation (the same machinery at event cadence); (2) **the `requires` edge** — the
enabler's build/operate gate incl. the operating-set fixpoint, re-run at HAVE-change over the affected
candidates. Both go through the ONE evaluator over the eval ctx the contexts fill. Every other read on every
surface is a straight compiled fetch and NEVER takes a context parameter — a context in any other signature is
the mechanical smell that condition evaluation (or an ad-hoc state reach) is happening where it doesn't belong.

