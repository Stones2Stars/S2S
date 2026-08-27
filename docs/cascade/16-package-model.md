# ⚖ THE PER-SCOPE PACKAGE MODEL — the cascade's founding design (§1), stated as cache architecture

> Part of the **[cascade](../cascade.md)** spec.

A package lives ON EVERY SCOPED ITEM, every level (world → team → player
→ city → plot); the cascade loads **yield packages in ONE UNIFORM FORMAT** (Σflat and Σpercent each their OWN
package per channel; the unit is part of the slot key) into each scope's cache; each package is maintained
from events at its OWN scope (a world-scope fact moves the world package while every other level stands). **The
only live calculation is adding the ~5 packages together at read.**

⛔ **AND THE ORIGIN RULE'S EXTENDED FORM — CITY ITSELF SPLITS INTO THREE PACKAGES, NOT ONE:** *"a city's
yields are based on 3 packages, the yields from plots, the yields from specialists, and yields from buildings,
that is 3 separate packages."* The plot origin is the per-plot package summed over the city's worked plots; the
SPECIALIST and BUILDING origins are two distinct flat packages ON THE CITY.
⚑ **This is forced, not stylistic.** §2a puts specialists in TIER 1
(inside the percent stack) and buildings in TIER 2 (added after it), and § THE MAINTAINED SUM bans a
per-source decomposition — so once both origins land in ONE Σ slot the two tiers can never be separated
again. A single city flat package therefore cannot express the rate at all.
⛔ **The failure it produces, which is the reason this is spelled out:** the specialist half gets recovered
by a hand-walk over the city's specialists at read time, and the SAME authored deposit is then counted twice —
once inside the stack and once outside it. That walk is also the O(what EXISTS) read-time shape this whole
document deletes; a maintained specialist package needs no walk at all.
⛔ **ENFORCED BY TYPE, NEVER BY CONVENTION** ([the hard-typing-or-rollerskate rule](../../AGENTS.md#design)):
the two planes are DIFFERENT TYPES (the package template carries its ORIGIN), so a specialist deposit reaching
the building plane does not compile. A comment saying which package a source belongs in is exactly what has
been re-corrected repeatedly and does not hold.

MODIFIERS still come from everything BUT plot — city, empire, team, world. So the percent side exists at
every scope except plot, and it is ONE dictionary: percents combine into a single additive stack
(§2a), so they have no origin to keep apart.

**⛔ THE GENERAL FORM — PACKAGE IDENTITY IS `(scope × COMBINE POSITION × channel)`.** Origin is the yield
plane's instance of a wider law: within a scope, packages stay isolated **per combine position** and never
merge into one per-scope number. The city's yield positions are the two-then-three origins above; every channel
family defines its own positions (wellbeing's opposing channels, the scalar stacks), and a scope's packages
follow that channel's positions. ⛔ A per-scope blob is the defect — whatever is summed together can never be
told apart again, and the combine is what needs them apart.

**⛔ THE FOUR-PROVIDER LAW — only PLOTS, SPECIALISTS, BUILDINGS and TRADE ROUTES provide yield.** They are
what physically produces yield in game. Every other source kind — trait, civic, tech, religion, corporation —
only MODIFIES or CONDITIONS a provider's output, so **every yield deposit resolves onto a PROVIDER-KIND
package**: a trait's specialist boost lands on the SPECIALIST package, a civic's building-keyed percent on the
BUILDING percent stack. ⚑ This is what decides which package a deposit joins, and therefore which leg the
percent stack multiplies — the question the origin split exists to answer.
**⚖ FOUR PROVIDERS, THREE PACKAGES — the TRADE ROUTE is a provider that is NOT a package: *"trade
route yields are always provided by the ENGINE; the trade route buffs happen BEFORE it arrives, as its
complete package."*** The engine owns the network calculation and applies the route's own buffs, so the
cascade receives a FINISHED value and folds it at the combine — it is the one live yield INPUT, never derived
(§2a). ⛔ **So no trade-route package exists and none is to be built.**
Nothing deposits into it: a package with no depositors is an empty slot inviting a future deposit to be routed
somewhere the engine already answered, which would double the route's yield.
⚑ **WHAT THE CASCADE OWNS IS THE COUNT, AND ONLY THE COUNT: *"we only tell the engine how many trade
routes we can have."*** The `tradeRoutes` channel — how many routes a city may run — is cascade-computed like
any other modifier-influenced value; the YIELD those routes then produce is entirely the engine's.
⛔ **Do not conflate them** (§2a states this at length and is the
authority). ⚠ The trap is one-directional and worth naming: listing trade routes among the PROVIDERS reads as
licence to give them a package, because the other three have one. They are a provider of yield and a consumer
of a cascade COUNT — never a home for deposits.
⚖ **The golden-age and free-city TRAIT FLATS need no provider home:** they are plain flat bonuses
riding the flat yield packages outside the provider chain, joining BASE at the combine. Golden age is a core
engine mechanic and stays simple. ⚠ "free-city" is the trait yield accumulator, NOT the WLTKD celebration.

**⚖ THE CITY-REALIZATION LAW — a deposit whose CONDITION references the CITY is a city-realized join,
whatever its authored scope.** State-religion-in-city, a city building's presence, any city predicate:
evaluating such a deposit once at PLAYER scope resolves it against one city's context and mis-serves every
other city. So all conditioned percent stacks realize PER CITY, in the city's package, against that city's
own context; the player scope holds only the genuinely city-AGNOSTIC sums. ⚠ Measured, not theoretical: the
player-scope evaluation left persistent +18..+27 percent errors on every non-capital city.

- **⚖ THE KEY IS SAMENESS: every store is the SAME OBJECT TYPE everywhere, and they ALL MAINTAIN
  the SAME WAY.** That — not the per-scope layout — is the requirement the whole model rests on. One templated
  channel-indexed slot table on every owner, and ONE application path driving all of it, derived from the deposit
  index. What varies between scopes is only WHICH SLOTS carry a value; the type and the protocol never vary.
  - **A RECEIVER IS NOT A STORED SLOT AT ALL WHERE IT SUMS MEMBERS.** A scope that consumes a channel
    from BELOW it — the empire's research / gold / culture / espionage over its cities — answers by summing its
    members' realized values at the read (§ A CROSS-SCOPE RECEIVER TOTAL, above). There is no "receiver mechanism" to
    build, and that is because there is no receiver STORE, not because one is shared.
    ⚠ Do not read the city's own realized rate as an instance of this: a city consuming production is combining
    ITS OWN packages, not summing members, so it is an ordinary package read and no member count enters it.
  - **⛔ THIS IS WHY HAND-NAMED SCALAR FIELDS ARE THE DEFECT, not just untidy.** A named field cannot be addressed
    uniformly, so it forces its own bespoke maintenance path — which is precisely how 33 of them accumulated.
    Channel-indexed slots are reached by the deposit's own compiled address, with no per-field code.
  - **The receiving scope is NOT the storing scope.** A package never moves to its consumer (that breaks the scope
    principle); the consumer SUMS its members at the read and stores nothing.
  - **⛔ A CROSS-SCOPE receiver total is the Σ of its MEMBERS' REALIZED values — and NOTHING beside that Σ.** The
    empire's gold / research / culture / espionage sums are Σ over the player's cities of each city's realized
    rate of that channel, re-summed at the read. The
    per-city quantity for a commerce channel is the whole §2a split — the
    slider share of the city's COMMERCE yield, the channel's own deposits, and the process conversion — not the
    channel's deposits alone. ⛔ **An upper scope's own package is NEVER added on top of that Σ:** its deposits
    roll DOWN (§1) and are therefore already inside every member's realized
    value, so adding them again at the receiving scope counts each empire-scope deposit once per city PLUS once
    more — a silent multiplication that compiles, runs, and simply reports wrong numbers.
  - **⛔ NOT a push accumulator, and NOT a per-CANDIDATE ask** (§ A CROSS-SCOPE RECEIVER TOTAL, above: the cost is
    the member count, the cadence is the only defect). Rejecting the legacy incremental accumulator does not
    license an AI loop asking the Σ per candidate in a scoring pass: that caller hoists it once per pass into its
    own scratch (the sanctioned AI-heuristic residual), never a stored slot on the machine.
  - **⚖ THE CROSS-SCOPE RECEIVER — SUPPRESSION IS SETTLED; ONLY THE DELTA QUESTION IS OPEN.** A receiver total is
    the Σ of its members' **REALIZED** values (§ A CROSS-SCOPE receiver total, above), and a realized value is the §2a
    combine over the member's packages, not a stored deposit sum. Two of its apparent obstacles dissolve:
    - **⛔ DISORDER AND WLTKD ARE NOT TERMS IN THE COMBINE — they are a PARTICIPATION GATE ON THE Σ:**
      *"disorder is easy, it just means that the packages that the city under disorder is simply not sent."* The
      city's stored value stays the real one and the sum declines to take it
      ([economy.md](../reference/economy.md): *"the package is sent out to the rest of the cascade only if no
      status negates it"*).
    - **⛔ THE GATE BELONGS AT THE Σ, NOT ON A MAINTAINED MEMBERSHIP DELTA — and the reason is already ruled.**
      WLTKD is a ONE-TURN status re-applied every turn by its trigger ([state.md](../specs/state.md)), so
      maintaining participation as a delta would flip a member in and out of the Σ every single turn *over a
      number that never moved* — precisely the thrash [economy.md](../reference/economy.md) refuses to mark on
      (*"it suppresses the CONSUMPTION of the value, never its contents — so neither is a cache input and neither
      marks it"*). The filter therefore runs where the participation question is actually asked: at the sum.
    **⚖ THE RECEIVER RE-SUMS ITS PARTICIPATING MEMBERS, AND NOTHING IS BUILT TO AVOID THAT.** *"The
    summing is so trivial that it would cost more to try some efficiency shenanigans."* The read side is ~5 int
    adds for the cross-scope roll-up and one combine per participating member for a receiver Σ — against the
    per-source walk the maintained sum deletes, that is not a cost to design around.
    ⛔ **So do NOT build a per-source decomposition plane.** A `(scope × channel × SOURCE)` breakdown exists only
    to make WITHDRAWAL cheap, and withdrawal is only expensive if summing is. Its source axis dwarfs the channel
    axis that KEYS ONLY WHERE NEEDED (above) already rejected as mostly-zeros, and the shape is the
    add-another-struct failure [every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta) names. The
    unconditioned plane re-applies its compiled constant; the conditioned tail re-resolves; nothing stores a
    per-source breakdown.
    ⛔ **And do NOT push the realized delta upward** — that is the shenanigan the triviality makes pointless, and
    it is a push up the chain. The third shape is barred outright: a member EMITTING *"my realized value
    changed"* ([spine.md](../spine.md): *"yield is a computed RESULT, never an event"*).
    > **⚑ THIS IS NOT A DEFERRAL, and reading it as one is the misreading to prevent: *"IF it shows that
    > the summing requires any kind of serious power, we deal with it then."*** ["deferred" is banned](../../AGENTS.md#design)
    > bans parking work KNOWN to be needed; this declines to build machinery for a cost nobody has demonstrated
    > exists — which is what the #430 roadmap already required (*"build the
    > base first; the most efficient way comes AFTER … do not build, investigate, or pre-shape it ahead of the
    > base"*) and what the triggers spec requires of hypothetical machinery. You cannot defer
    > work whose necessity is unestablished.
    > ⚑ **The REVISIT TRIGGER is named and it is a MEASUREMENT, never an argument:** a turn-time cost on the
    > standing late-game save, attributed to the summing, on the wall clock
    > ([turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)). ⛔ Until that exists, a proposal to optimize
    > the sum is speculative structure — and an AI loop asking a receiver Σ per candidate is answered by the
    > CALLER caching its own scores ([patterns.md](../architecture/patterns.md), the sanctioned heuristic residual), never by
    > reshaping the machine.
  - **Which scope receives a channel is spec'd, not chosen per site:** one consuming scope per channel
    (food/production → city; gold/research/espionage/**maintenance** → empire), with **culture the lone
    dual-consumer** (the city sums it for plot culture + border expansion, the empire for civ culture + traits —
    two independent sums over the same packages).
    ⚑ **MAINTENANCE is the one NON-commerce receiver, and it is what makes the rule general rather than a
    commerce habit.** The empire's total maintenance is the Σ over its cities of each city's realized
    maintenance — precisely the cross-scope receiver shape above — re-summed at the read like its commerce
    siblings, never a hand-named cache beside the packages
    ([every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)).
    ⚠ Its per-city quantity is the one a package cannot answer alone: a city's realized maintenance composes the
    three component KINDS (distance / numCities / colony) each against its own modifiers, takes the `amount`
    stack over the total, and declines wholesale under WLTKD/disorder ([economy.md](../reference/economy.md)).
    ⛔ **`MAINTENANCE_CORPORATION` is NOT one of them** — corporate maintenance is its own pre-inflation expense
    beside total maintenance, so the city total SKIPS that kind. Its deposit is a city-scope FLAT and therefore
    sits in the city's package like any other: a read that folded every maintenance kind would charge the same
    corporate gold twice in one expense total, plausibly and silently. So the Σ asks the CITY for its realized value —
    which is what "the Σ of its members' REALIZED values" already says.
    ⚠ **A receiver read is therefore NOT interchangeable with a rolled-legs read on the same channel.** The
    cross-scope roll-up answers a receiver channel with its maintained SUM, so a consumer that wants the
    channel's percent STACK at that scope must read the legs directly — asking the roll-up would hand back the
    realized total instead, silently and plausibly.
- **⚖ THE TWO DERIVED PLANES SHARE ONE MECHANISM — what differs is their CONTENT, never their maintenance:**
  - **The yield + percent packages** are maintained by applying the moved source's deposits to the slot they
    feed — § THE MAINTAINED SUM. ⛔ The earlier framing called this "an INPUT/OUTPUT value cache: memoize,
    mark-invalidate on a source event, recompute from inputs" and pointed it at `CvDerivedCache`. That framing
    is RETIRED, and it is what let the modifier alone keep a staleness protocol while both of its siblings ran
    without one ([superseded-ideas](../architecture/superseded-ideas.md) #30).
  - **The ENABLER's sets (the frontier + the operating-building set)** are maintained by **TARGETED
    PROPAGATION**: each HAVE-change ripples through the **affected subset only** (re-check the affected
    candidates / ripple the fixpoint), updating the authoritative dataset **in place** via the reverse-index
    ([enabler.md](../specs/enabler.md) §7). NEVER blanket-recomputed, NEVER a parallel shadow-delta.
  - ⚑ **So the honest difference is what a slot HOLDS** — refcounted set MEMBERSHIP versus a summed MAGNITUDE —
    and each is maintained in place by the fact that moved it. ⚠ That is why the enabler was able to run without
    a staleness protocol from the start, and it is the model the packages now match rather than the exception they
    were measured against.
  ⛔ Blanket-recomputing the whole operating-building fixpoint for every city on every event runs the enabler's set AS an
  input/output cache — **"burning down the library of Alexandria" (DESPAIR_INDEX #2)**. The fix is targeted
  propagation, the shape the frontier ALREADY uses (`onBuildingChanged` / `recheckHave` off the reverse-index). It
  is likewise **not a given** the yield-package shape fits any OTHER non-package channel (the unit plane,
  properties); each is decided per-channel, only AFTER the spec is fully in place.
- **⛔ THERE IS NO BATCHED TURN-END REBUILD PASS, AND NONE IS TO BE BUILT.** A "flags all turn, one unified
  rebuild at turn end, in dependency order" phase is the recompute model wearing a better cadence — it presumes
  a rebuild exists to schedule. Under the maintained sum there is nothing to batch: the slot was already correct
  when the fact arrived ([superseded-ideas](../architecture/superseded-ideas.md) #30).
- **THE APPLICATION IS DERIVED FROM THE DATA, never hand-wired.** A DOMAIN event carries its SOURCE; the source's
  compiled deposits (the load-time strings→ints index, `Data/CvDepositIndex.{h,cpp}` — per-deposit interned
  segments + FK-resolved target id + the resolved channel/scope slot, compiled at readJson push-time) name exactly
  the channels × scopes × targets it feeds — **so what to apply, and where, falls out of the deposit addresses.**
  The routing is a pure function of the index; a hand-coded hook per event site is a per-site bespoke path of
  exactly the kind [every derived cache is one shape](04-derived-stores.md#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta) forbids. Derive it from the
  index.
- **Mid-turn read freshness: the per-player-slice SNAPSHOT** — *"getting a yield event in the middle of a turn is
  not retroactive; start of next turn is what is expected"*. A newly-founded city is the one ruled exception (it
  must read correct values the turn it exists, so its packages build eagerly at creation rather than waiting for
  the next slice).
- **EAGERLY BUILD ALL CACHES AT LOAD — the general policy stands.** *"I am happy to add even MINUTES to load time
  in order to have caches eagerly built on load in general."* ALL caches are warmed at load: a game-object's own
  derived cache (the plot-yield cache) eagerly from that object's own state, and the **cascade** eagerly by the
  **event reseed** — the spine fires every present-fact, so the cache-build/invalidation consumer populates every
  cascade package and turn 1 runs warm. What changed is ONLY the cascade's population MECHANISM: the
  recompute-from-state recalc (`playerSliceRebuild` + `worldRebuild`) is REMOVED (the CAPSTONE above); the eventspine
  reseed replaces it. No design ever serializes a derived value to save load time. **The perf LAW: "the name of any
  game in this town will always be TURN TIMES — if game load takes 50% longer it matters nothing if we can shave
  5-10-15% on turn time, because there is only 1 game load, but many many many turns."** Turn time is the objective
  EVERY perf decision optimizes; load time is the currency that pays for it. Ledgered as
  [turn time is king](#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture).

This is the Clean-Architecture north-star applied to engine state: the repository **is** the contract, and it is the
lever for thinning the `Cv*` god-classes without touching the closed-EXE-bound `CvPlot`/`CvCity` layout. See
[north-star](../architecture/north-star.md).

---

