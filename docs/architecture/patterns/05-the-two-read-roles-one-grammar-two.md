# ⚖ THE TWO READ ROLES — ONE GRAMMAR, TWO ANSWERS

> Part of the **[patterns](../patterns.md)** spec.

> The keystone of the ACCESS surface. The section above is the INFO half; this states what the info half and the
> GAME-OBJECT half share, and what must stay different. Binding:
> [build a new getter surface, never widen a legacy one](#-the-two-read-roles--one-grammar-two-answers).

**⛔ The new surface is NOT a replacement mapping of the existing getters.** No legacy getter name,
signature, or shape survives into it. The measured 622 channel-shaped declarations on `CvCity`/`CvPlayer` are a
**DELETION LIST and a COVERAGE CHECKLIST** — the set of values that must be answerable somewhere on the new
surface — never a per-getter migration worklist. Mapping legacy→new one signature at a time is the
half-migration reflex in its purest form: it lets the legacy contract dictate the replacement's shape, which is
precisely how that surface accumulated.

> **⚖ AND THE DELIVERABLE IS THE SURVIVING SURFACE, NOT "A GETTER CUT": *"we should ensure that we
> just have the getters we need, and have an understandable structure."*** The `CvCity`/`CvPlayer` work is
> judged by its END STATE — a surface carrying exactly the reads consumers genuinely NEED, organized so a
> reader can tell where a value lives — never by how much legacy was deleted. The deletion of the legacy
> channel-shaped names is the CONSEQUENCE of consumers moving onto that surface; it is not the unit of work
> and not the acceptance test.
> ⚑ It is the SAME ruling the Python boundary below already carries — *"minimal amount of endpoints is not
> the target here, properly organized is"* — applied to the C++ game-object half: NEED decides membership
> (demand-driven, freely given where a consumer genuinely wants a read, and never derived from the legacy
> list), and COMPREHENSIBILITY decides structure (the group reads and named concepts of the grammar below).
> ⛔ Two failure modes it bans equally, because both measure the wrong thing: sweeping getters for deletion's
> own sake (a falling def count read as progress), and sparing a legacy name because deleting it is work. A
> read nothing needs GOES; a read something needs is served on the coherent surface; the census is DEMAND,
> never the surviving getter count.

**The two roles are DISTINCT, and the distinction is load-bearing:**

| role | asks | answers from |
|---|---|---|
| **INFO** | *"what do I CARRY?"* | authored data, compiled once at load — static, per-owner-agnostic, shared by every player |
| **GAME OBJECT** | *"what do I HAVE, right now?"* | live realized state — the roll-up over the ~5 scope packages, with per-city gates applied at the combine |

⛔ **They are NOT interchangeable and must never LOOK interchangeable.** Giving both the identical signature
invites a consumer to treat authored data and live state as the same answer — the shared-vocabulary trap that
[the pollution guardrail](../../specs/validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in) exists to police from the other direction. Two roles,
two surfaces.

**What IS standardized is the GRAMMAR — both surfaces obey all of it:**

1. **⛔ ONE GETTER PER GROUP — the getter IS the group.** `getYields()`, `getProperties()`,
   `getCommerces()`, … — the read hands back **the whole group**, and there is **NO scalar getter per channel**.
   A consumer wanting one value takes the group and indexes it. This is the standardization: the surface grows by
   GROUPS (a handful), never by channels (hundreds), and the per-channel scalar getter is the very shape the
   rebuild is deleting.
   > **⚖ THE ONE QUALIFICATION — A GATED VALUE EARNS AN EXPLICIT GETTER, BECAUSE A GATE NEEDS A TAP POINT
   >.** Where a STATUS gates what a source delivers, the gated value gets a named read for something else
   > to attach to; a channel-indexed group read offers no such point. The full ruling, its ungated/gated shape and
   > why the announced crossing follows the GATED value live at its home,
   > [state.md](../../specs/state.md) § A STATUS IS MIDDLEWARE.
   > ⛔ This is NOT licence to grow the per-channel surface back (what I don't want is to have the getter
   > spaghetti we used to have). **The test is whether the getter carries a CONCEPT something else attaches
   > to** — a gate tapping it, a predicate resolving through it — never whether a caller would like one value
   > without indexing. A getter that only names a channel is the spaghetti; a getter that names the thing a
   > status suppresses is a seam.
   >
   > **⛔ AND A GROUP IS A VECTOR OF ONE KIND OF QUANTITY — NEVER A BAG OF UNRELATED STATUS BITS.**
   > `getYields()` is a group because every slot answers the same question in the same unit. A flags list does
   > not: `CITY_FLAG_POWER` and `CITY_FLAG_OCCUPATION` are DIFFERENT QUESTIONS sharing a bus. So a flags read
   > serves a caller that genuinely wants MANY BITS FROM ONE FETCH — a status bar drawing four icons — and a
   > single status question is asked by NAME. ⛔ Answering `is this city powered?` with
   > `getFlags()[CityFlagKind.CITY_FLAG_POWER]` is the banned shape even though the value is right.
   >
   > **⚑ THE REASON IS THE MODDER, and it is why this is a hard line rather than a preference: *"the
   > moment we start using a generic getFlags for things like isPowered, is the day we gonna end up being
   > screwed by a modder taking that too far."*** An indexed bag teaches every consumer that city state is a
   > bit array addressed by ordinal — and once mod code indexes it, the LAYOUT is frozen: a flag cannot be
   > reordered, retired, or split without breaking code we do not control. A named getter keeps the layout an
   > implementation detail and leaves exactly one thing published: the question.
   >
   > **⚖ THE FRAME — C++ IS THE API FOR THE FRONTEND, and it answers like a normal web API** (*"yes I
   > know there are differences"*). A web API publishes NAMED resources whose internal representation stays
   > private; it does not hand back an array the client indexes by magic number and call that an endpoint.
   > ⇒ Read every published binding as an endpoint someone else will build against, and the two halves above
   > follow from it directly: name the question, keep the layout yours.
2. **The EXISTING ENGINE ENUM indexes the RESULT, not the call** (`YieldTypes`, `CommerceTypes`, …); a family
   with no engine enum uses its own kind enum (`CvInfoKinds.h`). So the enum stays the consumer's vocabulary
   while the call itself carries no channel argument. The data-minted channel id remains the CACHE's internal
   key and is never something a consumer learns.
3. **×100 native, always** — no `100` in any name, no `getX`/`getX100` pair, no scale variant
   ([the ×100 fixed-point model](../../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)). A reader ÷100s at the point of use.
4. **Scope is a spelled-out ARGUMENT, never a name fragment** ([scope is a separate axis, never folded into the kind](04-the-info-data-out-contract-what-an.md#the-coherent-surface--grouped-storage-parameterized-getters-clarity-and-predictability-is-king)).
5. **⛔ THE VALUATION PROTOCOL — THE LIVE CONTEXTS GO IN, THE PROPOSED INCREASE COMES OUT.** The caller
   passes the live [contexts](../../cascade.md) and gets back **the DELTA** — what this candidate would ADD — never
   the raw percentage and never the new total.
   - **⛔ THE CONTEXT *IS* THE CURRENT VALUE — that is the whole point of it.** A percent deposit has no
     value on its own: *"+25% production"* is worth a little in a small city and a lot in a large one, so it
     only becomes a number against the base it multiplies. The context supplies that base, because it is the
     bound live-state surface for its scope. ⛔ **Do NOT pass current amounts as a separate parameter** — that
     hands the read data the context already carries, and re-introduces the ad-hoc state-reach the contexts
     exist to end. A context that cannot answer a base the resolution needs is a **CONTEXT GAP to close by
     adding the forward** ([contexts.md](../../cascade.md)), never a reason to widen the signature.
   - **Why the DELTA comes out:** the question is *"what do I gain from this?"*. A delta is directly weighable;
     a new total forces every caller to subtract against a base it must fetch separately.
   - The contexts serve BOTH halves in one pass: they carry the base the percent resolves against (the
     `CityContext` forwards the city's CURRENT REALIZED YIELDS for exactly this, [contexts.md](../../cascade.md)),
     and they are what the compiled CONDITIONED tail is evaluated over (*"+25% more while coal is connected"*).
   - **⚖ A CITY-LESS VIEW EVALUATES AGAINST THE CAPITAL.** The valuation needs a `CityContext`, and a
     player-level "all buildings" view (the build list) has no city bound. The rule is the AI's own precedent
     made explicit: **the bound city if there is one, else the player's CAPITAL** — and it lives in ONE place
     every criterion reads ([the DRY single-implementation law](03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)), never
     re-derived per filter or per sort. A player with no capital has no valuation to give, and the criterion
     ranks neutral rather than inventing one.
   - **⚑ TWO CONSUMERS, ONE CALL: the AI's evaluation AND the build-list HOVER TOOLTIP.** The same
     valuation answers *"what do I gain from this?"* for the AI weighting it and for the player reading the
     tooltip. That is not a convenience — it is what makes the displayed number and the acted-on number the
     SAME number, structurally. The classic failure it removes is a UI advertising one value while the AI plans
     against another, which no amount of care prevents once they are two implementations
     ([the DRY single-implementation law](03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). It is also why the resolved DELTA is
     the right return: it is simultaneously what an AI weight multiplies and what a tooltip line prints.
   - **⛔ A "HOW VALUABLE IS THIS YIELD" WEIGHT IS ASKED AT MOST ONCE PER YIELD, AT THE START OF A doTurn
.** *"Those 'how valuable is this yield' questions is a question that at most should be asked once
     per yield at start of a doTurn, at absolute most."* This is the CADENCE half of the protocol, and it is a
     ceiling rather than a target: once per (yield × turn) is the most that is ever legitimate, and less is
     better. ⛔ Per CANDIDATE is the banned shape — a weight describes the EMPIRE's standing, so it cannot
     differ between two buildings scored in the same pass, and asking it per candidate multiplies whatever it
     costs by the frontier.
     ⚠ It must therefore not be keyed on anything that moves WITHIN a turn (a treasury balance is the tempting
     one), or the ceiling is silently lost the moment that input twitches.
   - **⛔ AND THE WEIGHT FOLLOWS WHAT THE EMPIRE *NEEDS*, NEVER A TOTAL OF WHAT EVERY OTHER CITY HAS:**
     *"It should not start caring about what all others have, but what that empire needs."* A need is a
     property of the asking empire — its obligations against its means — so it is answerable from that
     empire's own standing. ⛔ Deriving it by re-totalling every member's realized output is the wrong
     question wearing the right answer's clothes: it makes a per-empire constant cost `O(cities)`, and at the
     receiver Σ that is `O(cities)` per ask ([cascade.md](../../cascade.md) § A CROSS-SCOPE
     receiver total).
     ⚑ **The measured case this rules on:** the gold-value weight reached the empire's realized gold commerce,
     which re-sums all 185 cities' §2a combines — asked once per BUILDING CANDIDATE, it was the whole of a
     45-second `AI_chooseProduction` on the standing save.
6. **⛔ A GROUP HANDS OUT ITS CHANNELS; A FINAL-STATE CALCULATION IS DOWNSTREAM OF IT.** The wellbeing
   group returns `happiness` and `anger` as **two separate numbers** (and `health`/`unhealth` likewise) — *"then
   you will know the results from that"*. The realized end-state values (`angryPopulation`, `healthRate`) are
   **NOT group entries and NOT getters**: they are a final-state calculation over numbers the group already
   handed out ([modifier.md §2b](../../cascade.md) specs the arithmetic). ⛔ Folding a final-state value into
   the channel array is a category error — it puts a computed OUTCOME in a slot that means "a channel a source
   deposited into", and it hides the opposing-pair structure the four channels exist to express. The calculation
   still exists **exactly once** as a pure static function on the calc surface
   ([the DRY single-implementation law](03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)); it is simply not part of the read.
7. **The group read FILLS A CALLER-OWNED ARRAY** — one call in, the whole group out, indexed by the group's
   enum. Passing state once and getting the whole resolved group back is also what keeps a future
   whole-candidate snapshot possible without building it now; a design answering one scalar per call would
   foreclose it *and* would re-resolve the same state per channel.
6. **Extensible by DATA, not by new members/getters** — a new channel is a new id, not a new function.
7. **Parameters spelled in full**, index parameters named for the enum they key
   ([contexts.md](../../cascade.md) naming rule).

**The GAME-OBJECT half's realized shape.** Each scope owner (`CvPlot` / `CvCity` / `CvPlayer` / `CvTeam`)
carries **one group read per modifier FAMILY whose channels the data authors AT
that scope** — the set comes from the census scope masks + the minted channel sets, never a hand-written list.
Every group folds through the **ONE cross-scope roll-up on the calc surface** (`InfoValuation::realizedAt*`,
beside the `cityRate` combine it specializes): modifier.md §1's downward roll realized AT READ over the chain the
object sits under (city = team + empire + city · empire = team + empire · team/plot = itself;
WORLD is CONFIG and carries no package, and PLOT never enters an upper chain — a per-plot value resolves in
isolation first). A channel the scope **CONSUMES** answers its maintained receiver sum instead, and which side of
a channel is the answer comes from the vocabulary's canonical-unit verdict (`infoKindUnit`): a percent-unit
channel IS the additive stack, a flat-unit channel is the flat sum that stack scales. **Naming:** an
engine-enum-indexed group takes the engine plural (`getYields`, `getCommerces`); a kind-enum-indexed group says so
(`get<Family>Kinds`), which also keeps this surface from colliding with — or overloading — the legacy scalar
getters that hold the bare family name.

