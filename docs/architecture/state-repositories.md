# State repositories — MAINTAINED SUMS, updated by the event that names the source

The pattern for **derived engine state**: how a domain object's derived data (yields, commerce, health, …) is
computed and kept coherent. `CvPlot` and `CvCity` are **domain objects** — the in-game data entities — and they
**stay**. This is not about dissolving them (that's `CvCityAI`'s eventual job); it is about the derived layer.

> **⚖ THE FOUNDING CORRECTION (owner) — A PACKAGE IS NEVER DIRTIED AND RECALCULATED. IT IS A COMPILED SUM THAT
> IS ALWAYS CURRENT, BECAUSE EVERY EVENT THAT MOVES IT UPDATES IT.** *"What I got wrong is that I thought the
> yield packages had to be marked, and recalculated all the time, when it is in essence just a compiled sum
> that is always updated, based on incoming spine events."*
>
> A package slot is Σ over the scope's sources of their deposit into that `(channel, unit)`. A DOMAIN event
> NAMES the source, and the compiled index already holds that source's deposits — so applying them is a handful
> of adds and the slot is correct **at that instant**. There is nothing to mark, because there is nothing
> deferred. The staleness-flag / recompute protocol this document previously specified is RETIRED
> ([superseded-ideas](superseded-ideas.md) #30); what stands in its place is § THE MAINTAINED SUM below.

This is the **design the cascade plane is built to**, stated independently of any one implementation of it. The ONE
uniform package (`Sources/Cascade/CvCascadePackage.h`, channel-indexed Σflat (×100) / Σpercent (unscaled) slots +
receiver sums) is a data member on team / player / city / plot; the per-scope channel sets are minted from the
compiled deposits at load (`CvCascadeChannelRegistry`, the ClassificationRegistry precedent); the package carries
**apply verbs and no other writer**, so the modifier's own spine consumer (`CvModifierConsumer`, load-active)
applies a moved source's deposits directly into the slots they feed; the gather (`CvCascadeGather`) is the
INDEPENDENT endpoint ORACLE and never writes a stored slot (§ THE RECOMPUTE IS AN ENDPOINT ORACLE); and the
combine lives on the calc surface (`InfoValuation::cityRate` / `groupSumAt`).

## The problem: no unified `dataChanged` trigger

Every derived value in the legacy engine is a **hand-maintained cache with ad-hoc, gappy invalidation**. There is no
single "the source changed, refresh me" primitive, so caches drift out of sync with the data they derive from — one
disease, many instances: a dormant building's improvement-yield never decremented; a building value change leaving
the cache on the old value; transition-only stamps (`doVicinityBonus`) missing build-after-connect orders; two
surfaces reporting different worked-plot yields for the same city at the same moment. The legacy incremental
serialized accumulators additionally carry **history pollution** — values no live data source can produce (the
improvement-yield accumulators hold phantom yields, per-plot, bit-exact; the wellbeing accumulators the same class —
[modifier.md §2b](../specs/modifier.md)). Recompute-from-source is the cure; recompute-every-read getters
(the squirrelBanana class) were the workaround for a cache nobody could trust — correct, but paying full cost on
the hot path.

## The model

> **the state changes → the fact is emitted → the fact's own source updates the slot it feeds → every consumer
> reads that one value as a bare fetch.** One announcement, one application, one source of truth.

A derived cache in this model is:

1. **EVENT-MAINTAINED, not mark-driven.** The DOMAIN fact names the SOURCE; the compiled index names that
   source's deposits; applying them IS the maintenance. A READ is a **bare fetch** and never recomputes
   (an ensure-on-read protocol is tombstoned), and there is no staleness flag on the unconditioned plane to gate
   anything on. **The work is proportional to what CHANGED (one source's handful of deposits), never to what
   EXISTS (every source at the scope).**
2. **Recompute-only, NOT serialized** — the [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) rule,
   applied per-field. Neither the value nor the flag is saved; on load the flag is marked by default, so the first read
   recomputes from current state — **never stale-from-save**. Drop serialization by the **soft-remove**
   ([DEC-save-remove-is-soft](decisions.md#dec-save-remove-is-soft), [save.md §3](../specs/save.md)): FULL-DELETE the
   read + write and NAME the tag in `Assets/savemigration.txt`, which drains an old save's orphan bytes by name so
   nothing after it shifts (a no-op on a new save that never wrote it). **No `WRAPPER_SKIP_ELEMENT`** (it leaves the
   dead member named — a rollerskate target); and just deleting the read/write *without* the `savemigration.txt` entry
   desyncs the whole downstream read.
   **This is UNIVERSAL, not per-field-optional (owner ruling): NO cache is ever serialized** — so nothing derived
   is ever read from a save, and there is correspondingly nothing for a blanket recompute to purge.
   ⛔ **No blanket recompute of derived state exists anywhere in the engine, and none is ever to be built**
   ([DEC-no-self-heal](decisions.md#dec-no-self-heal)): the event spine builds the state, LOAD is the only full
   pass (§ THE CAPSTONE RULE), and a missed invalidation must stay visible instead of being swept away. ⛔ A
   wipe-the-totals-and-reapply pass over live game objects is therefore never a maintenance path to add or extend
   — *"it is inherently obsolete under the event-driven system, since the new system recalcs on load anyway"*
   (owner). It is the exact shape this model replaces, and it is worst where it looks most useful: firing on the
   saves most likely to have drifted is what would hide the missed emits the spine exists to expose. Each
   remaining serialized cache
   converts by the same move: skip the read, rebuild at load from source state through the live entry points
   (the bonus-network cluster — the plot-group counts AND membership, the bonus-fed wellbeing/modifier
   accumulators, power, the dormancy verdicts — is the realized exemplar: the load-end rebuild in
   `CvGame::onFinalInitialized` recolors the groups from current state, folds the counts as each plot joins,
   and reconciles dormancy to the enabler's operate fixpoint, firing the ordinary crossing emits; the city
   holds no bonus mirror at all — its read is a plot-group relay, [enabler.md §8](../specs/enabler.md)). A
   serialized store survives ONLY for genuine non-derivable state (the event/WB grant stores, e.g.
   `CvCity::m_paiFreeBonusEvents`).
3. **The single source — PULL, not push, and the rule is CROSS-SCOPE.** ⛔ **What is banned is a scope pushing
   its total into ANOTHER scope's store**: an upper scope's package never lands in a lower one, and a receiver
   total is the Σ of its MEMBERS' realized values read at the receiver, never deltas the members push upward.
   That is where "push + a parallel cache double-count and drift" actually bites — two stores holding one fact,
   one of them maintained by someone else.
   ⚑ **A deposit landing in its OWN scope's slot is NOT that, and reading it as that is the misreading this
   clause exists to prevent.** By the scope principle ([modifier.md §1](../specs/modifier.md)) a city-scope
   deposit BELONGS at city scope; the package is the only thing holding it, so there is no parallel cache to
   diverge from and nothing to double-count. Applying the fact to the slot the fact feeds is the maintenance,
   not a push.

**Worked shape (the plot-yield cache):** `getYield()` = `return cached` — a bare fetch, always O(1), because the
fact that moved the plot already applied its deposits into the slot;
**⚖ THE SUM OF THE PACKAGES IS CACHED AT ITS TARGET — as a RECEIVER SLOT, never as a member beside it (owner).**
Each channel has ONE consuming scope (production → city; the commerces further up), and the Σ that lands there is
cached in that scope's OWN package (`CvCascadePackage::sum`, read `readSum`, moved `applySum`). **ONE EVENT REACHES
BOTH LEVELS**: the same derivation that names the packages a source feeds also names the receiver sums those
packages feed, so a fact applies to both at the instant it arrives and no ordering between them exists to get
wrong.

⛔ **What is banned is a HAND-NAMED field holding that same number** — a `CvCity::m_plotYieldSum`-shaped member is
the defect [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) names (it cannot be addressed by the
derived mask, so it forces a bespoke invalidation path) and a second maintenance surface for a fact the modifier
consumer already routes. ⛔ Equally banned is the other direction: re-summing per read. **Cache it — in the slot
that already exists.** The push-maintained `m_aiBaseYieldRate` is dead, and a legacy tier-1 accessor over it
(`getPlotYield`) is a DELETION, not a value to re-home: its consumers read the channel at its receiving scope
([DEC-new-getter-surface](decisions.md#dec-new-getter-surface)). ⛔ The pull must be a CACHE at EVERY level, never a per-read walk: re-summing the radius on every
`getPlotYield` call turns the game's hottest read O(radius) — measured at 913M plot reads in one turn inside the
governor's valuation, the cost class this whole doc exists to prevent. The engine's actual base yield thereby equals the build-order-independent value the cascade computes —
stale-cache divergences resolved **at the source**, behaviour-preserving
([DEC-parity](decisions.md#dec-parity)).

### ⛔ A STALENESS FLAG IS THE FOSSIL OF AN INCOMPLETE EMIT SURFACE — the same rule, one level up

> **⛔ AND THE WORD GOES WITH THE MECHANISM — WE DO NOT USE "DIRTY" AS A TERM, FULL STOP (owner).** The only
> survivor is **the one the EXE needs for GRAPHICS**: `InterfaceDirtyBits` and the repaint helpers over it
> (`setDirty(X_DIRTY_BIT)`, `setLayoutDirty`, `setFlagDirty`, `setInfoDirty`) — EXE-bound, and resolved BY NAME
> from BUG config strings, so it is a published vocabulary rather than ours
> ([python-read-map.md](../reference/python-read-map.md)). **Every DERIVED-STATE use goes**, whatever its blast
> radius: the mark/rebuild protocol, `markMaintenanceDirty`, `setCommerceDirty`, the AI re-evaluation flags.
> ⚑ **The word is not being tidied — it is being removed with the thing it names.** A term that survives its
> mechanism is exactly the evidence-of-the-abandoned-path that teaches the next agent to reach for it
> ([DEC-no-rollerskate-evidence](decisions.md#dec-no-rollerskate-evidence)), and this one names a CLAIM the
> engine can no longer make.

> **⚖ WHEN IT WENT OBSOLETE, and why nothing announced it (owner): *"I did not recognize that marking became
> obsolete the moment we landed on eventspine for everything."*** The derived-cache protocol was one of the
> FIRST things designed for this rework — correct for the world it was designed in, and faithfully implemented.
> ⛔ **So it is a SUPERSEDED DESIGN, not a rollerskate, and reading it as one sends the next agent hunting a
> culprit that does not exist** — the surrounding implementation tracked the spec exactly
> ([superseded-ideas](superseded-ideas.md) #30; contrast #14, the ensure-on-read protocol, which genuinely was one).

**A staleness flag is a CLAIM THAT WE DO NOT KNOW WHAT CHANGED.** That is its whole job: a memo recording
*something in this bucket moved*, kept because the happening itself was not available to consult. Once every mutation
announces itself, the FACT is strictly more informative than the memo — it names the SOURCE, and the compiled
index names that source's deposits — so the flag becomes a lossy summary of an answer already in hand.

⚠ **The premise dissolved SILENTLY, which is why it survived.** A design does not fail when its justification
goes away; it keeps producing correct numbers and merely does unnecessary work to get them. There is no error,
no wrong value and no symptom to chase — the only trace here was a load that took minutes behind a drain of marks
nobody could account for.

⇒ **The mechanical test, and it applies to the whole engine, not just this plane: every staleness bit, staleness
stamp, epoch counter and version number is asserting that what changed is unknowable. Under a complete spine that
assertion is FALSE BY CONSTRUCTION.** So each surviving one is exactly two things and never a third: a **missing
emit wearing a flag** (wire the fact — [DEC-close-event-gaps-now](decisions.md#dec-close-event-gaps-now)), or
**dead weight** (delete it). ⛔ It is never a mechanism to keep because it works.

### ⛔ A SELF-HEAL IS THE FOSSIL OF A MISSING EMIT — so it is a SEARCH, not just a ban

**Where self-heal came from (owner):** the old branch was full of blanket recalculations *because agents did not
properly wire the events and shortcut by adding a self-heal calc instead*. That is the causal direction, and it is
what makes [DEC-no-self-heal](decisions.md#dec-no-self-heal) findable rather than merely prohibitive:

> **A recalc does not appear because someone wanted a recalc. It appears because a fact was not announced, the
> value went wrong, and recomputing was the cheapest way to make the symptom stop.**

⇒ **Every self-heal marks the spot where an emit is missing.** So when you find one — a periodic rebuild, a
"refresh if stale", a runaway cap that "recovers next slice", a wipe-and-reapply — do NOT simply delete it and
declare the rule enforced. **Find the fact it was compensating for and wire THAT**; the recalc then has nothing
left to do and is removed as a consequence, not as the fix.

⚠ And a self-heal is worse than the bug it hides, which is why it is banned rather than tolerated: the missed emit
would have surfaced as a visibly wrong value that someone could chase, whereas the recalc converts it into
permanent invisible drift **and** reinstates exactly the per-read/per-turn work the caches exist to delete.
⛔ A comment claiming a recalc "heals" something is itself suspect twice over — the healer may not even exist any
more (a slice rebuild that was since removed), leaving a truncation that repairs nothing and announces nothing.

### ⛔ THE LEGACY-ACCUMULATOR CUT — every accumulator, ONE uniform mechanism

> Binding: [DEC-accumulator-cut-uniform](decisions.md#dec-accumulator-cut-uniform). **NOT wellbeing-specific — it is
> EVERY legacy serialized incremental accumulator, and they all work exactly the same way.** Wellbeing was the pilot
> that proved it; the same cut then repeats UNCHANGED. **Blast radius is not a concern — it is the SIGNAL: a cut
> that does NOT reach broadly means the legacy is not actually being cut.**

**What an accumulator IS — the three-part test.** A member that is ALL of: (1) **serialized** in `read()`/`write()`;
(2) **incrementally maintained** by `change*`/`update*`/`process*` deltas, never recomputed-from-source; (3) a
**per-turn game quantity the cascade now owns**. These are the STORED-ACCUMULATOR DRIFT class
([modifier.md §2b](../specs/modifier.md)): they carry decades of save history no live source can reproduce, so a
stored-vs-recompute diff is **DRIFT (history pollution), never state to preserve** — the recompute is the correct side.

**The uniform mechanism:**

1. Add the cluster's **fresh-gather accessor** returning its term from the cascade, ×100 internally.
2. **Re-point the realized getter** to it, reducing `÷100` at the reader boundary — **no `*Legacy` fallback, no
   variant getter** ([DEC-no-legacy-masking](decisions.md#dec-no-legacy-masking): anything sneaking a legacy value
   back in is an ERROR, never a safety net; on a red tree a wrong/empty cascade value is the CORRECT exposed outcome).
3. **HARD-DELETE** the member and its maintainers.
4. **FULL-DELETE the read + write** and NAME the tag in `Assets/savemigration.txt` — the reader drains the orphan
   transparently ([save.md §3](../specs/save.md)). No `WRAPPER_SKIP_ELEMENT`; an UNLISTED deleted-read orphan is the
   one hard desync.
5. **The COMPILER is the census** — every surviving consumer is a compile error to rewire; you cannot
   flip-and-pretend. Done = endpoint-observable on a loaded save, not "it compiles."

⚠ **Audit each deleted `change*`/`update*` BODY for side effects first** — legacy changers carry non-obvious riders
(trade-network recompute, UI-dirty, power) the surviving trigger site must still fire ([save.md §6](../specs/save.md)).

**Incremental-accumulate ledgers convert to recompute-from-source.** A serialized player ledger that replays its
accumulator onto the loaded value double-counts by build order. The conversion is the uniform one above: recompute
from the player's own held sources on the mark, make the changer trigger-only, and have the cities PULL it.

**Event/vote grants are NOT cached — they are a SEPARATELY PERSISTED store.** A per-building commerce change has
two sources of fundamentally different nature: the **empire** grant (`GlobalBuildingExtraCommerces`, civics) is
DERIVABLE → the recompute-from-source cache; the **event/vote** grant (fires ONCE) is **genuine one-shot state, NOT
derivable** — *"having events just be stored in the cache is lunacy"* (a recompute cache would wipe them). They live
in their own serialized field (`CvCity::m_aBuildingCommerceChangeEvents`), outside the recompute path; the reader
sums `player-recompute (empire) + city event/vote (persisted)`.

## ⛔ THE RECOMPUTE IS AN ENDPOINT ORACLE — NOT THE READ PATH, AND NEVER AN IN-DLL DIFF (owner)

**"The ensures were some of the earliest rollerskates."** Read-side `ensure()` is tombstoned by name
([superseded-ideas](superseded-ideas.md) #14: *"never re-add … an `ensure`-on-read protocol"*) and measured:
an ensure-per-read protocol on AI-hot paths ground unit automation. A read is a **BARE FETCH, unconditionally** —
there is no gate test on it, because there is nothing on the read path to gate.

**The ruling (owner): keep the recompute, but it is called ONLY BY AN ENDPOINT, and the COMPARISON HAPPENS
EXTERNALLY.** Two endpoints, one external diff:

- **the STORED endpoint** serves what the EVENTS built — the live package/set values, read exactly as a consumer
  reads them;
- **the ORACLE endpoint** runs the recompute FROM SOURCE **into SCRATCH** and serves that;
- **an external consumer diffs the two.** A disagreement is a **missed emit**, named by scope + channel + owner.

⛔ **NEVER emit a divergence as a spine event — that is a GUARANTEED LICENSE TO BUILD SELF-HEALING (owner), and
it is the reason this is a hard rule rather than a preference.** An event is an **invitation to a consumer**.
Put a divergence on the spine and the next agent writes the consumer that handles it — and "handling" a value
known to be wrong means CORRECTING it. Self-heal then arrives because the SHAPE invited it, not because anyone
decided to add it, and it arrives wearing the authority of the event spine
([DEC-no-self-heal](decisions.md#dec-no-self-heal)). **A PULL (an endpoint someone asks) cannot grow that
consumer; a PUSH (an event fanned to whoever registers) grows it by default.** So the DLL neither compares nor
reports: a divergence is not a happening, it is an OBSERVATION an external reader makes about two served
numbers, and it has no in-DLL representation at all — no diff, no log line, no event, no field.

⛔ **Frame this as REBUILD. Shadow is dead and cutover is dead (owner) — neither is a lens for any remaining
work.** Do not describe this oracle as "shadowing" the cascade, do not reach for cutover staging, and do not
revive either vocabulary to justify a comparison: the sanctioned shape is simply *two served surfaces, diffed
outside* ([http-endpoints.md](../specs/http-endpoints.md)).

⚑ **Serving into SCRATCH is what makes "never repairs" STRUCTURAL.** The oracle cannot write the stored slots
because it is not given them — no snapshot, no restore, no discipline to remember and no window in which a
half-finished recompute could leave a repaired value behind. A verifier that repairs is self-heal wearing a
different hat ([DEC-no-self-heal](decisions.md#dec-no-self-heal)); this one structurally cannot.

⚑ **What makes this comparison REAL:** it pits **event-built state against a fresh recompute-from-source** — two
genuinely different derivations of the same number, which agree only while every mutation emitted. A comparison
whose two sides share a derivation can never turn red and verifies nothing; this one can, and that is its job.

**The identity a divergence needs to be actionable:** "some city's production flats are wrong" across 185 cities
identifies nothing, so every served value carries its owner, **interpreted per scope** as the spine's DOMAIN ints
are interpreted per event: city = `(owner, cityId)` · empire = `(playerId, —)` · team = `(—, teamId)` ·
plot = `(x, y)` (a plot has no owner-independent id, and the map index needs a map that does
not exist at bind). Identity is passed IN at bind — the scope owners share no common id accessor.

⚠ **Consequence, and it is not optional: the STORED side is built by APPLIES ONLY.** The fact that names a source
applies that source's deposits — the same shape the contexts use — and there is no rebuild anywhere for a sweep to
batch (§ THE MAINTAINED SUM).

## ⚖ EVERY DERIVED STORE IS ONE SHAPE — a KEYED ACCUMULATOR maintained by a delta (owner)

**A count is a sum.** The possession plane and the magnitude plane are not two mechanisms — they are one
structure over different payloads, and the only things that vary are the key space and the value type:

| store | key → value | the delta arrives from |
|---|---|---|
| the plot group's bonuses | `id → count` | a member plot/city joining or leaving |
| `CityContext.amenities` | `id → count` | a grantor starting or stopping conferring |
| `CityContext`'s vicinity tiers (owned/neutral/foreign/worked/connected) | `id → count` | a radius plot's bonus or tier moving |
| `EmpireContext.policies` | `id → count` | a civic / trait / project / wonder |
| the enabler's membership planes | `id → (enable, remove)` | a HAVE-change |
| `OperatingBuildings::providedCount` | `id → count` | an active flip |
| **the cascade packages** | `channel → Σvalue` | a source's compiled deposits |

⇒ **`ContextDict` and `CvCascadePackage` share a MAINTENANCE RULE**, so
[DEC-maintained-sum](decisions.md#dec-maintained-sum) is the MAGNITUDE case of one general rule, never a
cascade-only one.

> **⛔ THEY BEHAVE SIMILARLY AND ARE NOT THE SAME — sharing a mechanism is not sharing an identity (owner).**
> The rule above governs HOW a derived store stays current. It says NOTHING about which store a value belongs
> in, and reading it as licence to merge them is the conflation this callout exists to stop:
>
> | | context dictionary | package channel |
> |---|---|---|
> | the KEY is | a minted **classification** id — a named FEATURE | a minted **cascade channel** — a named QUANTITY |
> | the VALUE is | grantors present, or a held strength | a summed magnitude in a unit |
> | the SCALE rule | none — a count is a count | [DEC-fixedpoint-x100](decisions.md#dec-fixedpoint-x100): flats ×100, percents unscaled |
> | READ by | gates, conditions, `per` scalers | the combine, the realized value |
> | AUTHORED in | the `amenities` / classification block ([json.md §8](../specs/json.md)) | a family address `<family>.<scope>.<unit>` |
>
> ⚠ **The SCALE row is what bites silently if they merge** — ×100 semantics landing on a refcount, or dropping
> off a magnitude, both staying entirely plausible.
> ⚑ **The worked case: AIRLIFT CAPACITY.** A building's airlift is a NUMBER it carries and the city's total is a
> SUM of numbers — so it is a modifier-family CHANNEL and retires onto the city's PACKAGE, exactly like the other
> hand-named scalars ([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)). ⛔ It is NOT an amenity,
> however volumetric it looks: putting it in the dictionary would make an `AMENITY_*` id carry a magnitude and
> break what that registry means.
> ⚠ **Consequence for the volumetric headroom, stated so it is not mis-planned:** power becoming a CAPACITY a
> city draws against would not be an amenity carrying a magnitude — it would be power CHANGING PLANES, from a
> classification key to a channel. Do not "future-proof" the dictionary for a change that would relocate the
> value. ⛔ [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)'s scope of *"every derived
cache on the cascade plane"* was drawn too narrowly: the plane boundary is not real, and the one store that
drifted onto a different mechanism is the one that boundary excluded.

### ⛔ THE SEMIBOOLEAN STATE — the read is BOOLEAN, the storage is NOT (owner)

**That mismatch IS the trap: storing the thing as what it READS like is the whole error.** The contract:

- **STORED** `id → count`, an int.
- **READ** `has(id)` ≡ `count > 0`.
- **WRITTEN** ±1 as a grantor starts or stops participating — never `set`, never `clear`, never a recount.
- **ZEROED at owner reset** — a delta store is correct only from a known zero, and `CvCity` is recycled out of an
  `FFreeListTrashArray`, so a reused slot inherits the previous occupant's counts and **no later delta can ever
  correct them** ([contexts.md](contexts.md)).

⛔ **THE READ SURFACE IS A BOOLEAN GETTER, AND CONSUMERS NEVER SEE THE INT (owner).** *"The dictionary literally
needs to have a boolean getter that says whether it's there."* `has(id)` ≡ `count > 0` IS the contract; the count
exists so MAINTENANCE can be correct, not so a reader can inspect it. ⚠ The moment a consumer reads the number
the representation leaks — `count == 1` / `count > 2` logic appears, and then **volumetric can never land**,
because changing what the number MEANS breaks readers that were never meant to see it. The one legitimate reader
of the int is the genuinely volumetric one.
⇒ **The surface: `has(id)` → bool for every consumer · `add(id, ±1)` for maintenance · `count(id)` reserved for
a volumetric reader · and NO `set`.**
⛔ **`set(id, n)` IS THE FOOTGUN AND DOES NOT BELONG ON THIS TYPE** — it overwrites a refcount, which is exactly
how the workable-radius override became a plain assignment that zeroes the ring when a city loses ONE of TWO
grantors. A type that PERMITS the banned move forces the rule to be remembered; removing the verb makes it
unsayable, which is the enforcement model this project keeps choosing
([patterns.md](patterns.md): a contract, not a prohibition).

⛔ **ALWAYS A COUNT, NEVER A BIT — and the deciding argument is not "some keys have several grantors."** It is
that **you can never safely answer NO**: these registries are OPEN by design
([DEC-classification-infos](decisions.md#dec-classification-infos)), so a key with one grantor today gains a
second the moment someone AUTHORS data, with no engine change. A bitset breaks silently on a data edit, in a
build nobody touched. The count is not a concession to the multi-grantor cases; it is the only representation
that survives an open registry.
⚑ Two properties fall out free, and both are already ruled for the amenity instance: **VOLUMETRIC needs no
reshape** (the slot is already an int, so a state that becomes a QUANTITY only changes what the number means),
and the **REMOVAL-WINS trap is structurally absent**.
⚠ **The masking to recognise:** a set-shaped store survives only while something RECOMPUTES it whole. Convert
such a store to delta maintenance without converting its STORAGE and it breaks immediately — so the two halves
land together or not at all.

## ⚖ THE MAINTAINED SUM — THREE PLANES, ONE SLOT, AND NOTHING IS EVER RECOMPUTED

Every slot is one identity, and reading it settles the whole maintenance question:

> **`slot` = Σ over the scope's LIVE sources `S`, over `S`'s compiled deposits `d`, of
> `value(d) × multiplier(S) × perScale(d) × [condition(d) holds]`**

**All four operands are ALREADY MAINTAINED BY AN EVENT.** `value(d)` is compiled at load (the deposit index);
`multiplier(S)` and `perScale(d)` are COUNTS the game objects and the [context dictionaries](contexts.md) hold;
the condition verdict reads the contexts' own stored predicate state. Nothing on the right-hand side arrives
unannounced, so there is nothing left for a recompute to discover — [DEC-flag-is-fossil](decisions.md#dec-flag-is-fossil)'s
test applied to the VALUE plane rather than to a flag.

The compiled data splits every deposit by WHICH OPERAND VARIES, and the split decides its **ROUTE, never its
storage**. ⛔ All three planes apply into the SAME slot: there is no per-plane segment and no per-source
decomposition (§ THE CROSS-SCOPE RECEIVER bans one, and this shape needs none — that it adds no storage at all
is the tell that the cut is drawn in the right place).

| plane | the deposit | the fact that moves it | the delta applied |
|---|---|---|---|
| **A — CONSTANT** | null-condition, unscaled | the SOURCE arriving or leaving | `±value` |
| **B — SCALED** | `value × count(key)` — a `per` scaler, a `plots`-target, a keyed count | the source, **and the COUNT** | source: `±value × count` · **count: `±value × Δcount`** |
| **C — CONDITIONED** | gated on a predicate | the source, **and the ATOM's verdict crossing** | `±value`, over the deposits that atom gates |

⚑ **PLANE B IS WHAT THE DICTIONARIES BUY, AND IT IS WHY A COUNT FACT EXISTS AT ALL (owner).** `Δ(v × c) = v × Δc`
is EXACT — `v` is a compiled constant and `Δc` is what the fact carries — so a `ContextDict::add(id, ±1)` IS a
yield delta of `Σ(deposits keyed on id) × ±1`. *"+1 food per river tile"* stops being a re-derivation and becomes
one multiply the moment a river bit moves. **This is the reason a population-changed fact is emitted** (owner):
a `per: {POPULATION}` scaler is plane B, and the fact carries the delta that resolves it.

### ⛔ THE INVARIANT — the slot is correct at every instant, which is what makes plane C delta-able

> **At every instant `slot == Σ resolve(d, state_now)`, because every operand's move applies its own delta at the
> moment it moves.**

It is inductive, and it holds only if EVERY operand has a route — which is exactly what a saturated emit surface
buys. Four consequences:

- **A WITHDRAWAL IS ALWAYS EXACT.** `emit()` dispatches SYNCHRONOUSLY ([event-spine.md](../specs/event-spine.md)),
  so no two operands are ever in flight together: when a fact arrives, every other operand still holds the value
  the stored contribution was computed against.
- **⚖ THE CONDITIONED TAIL IS THEREFORE DELTA'D TOO, PER ATOM (owner) — it is NOT re-resolved.** The earlier
  ruling that plane C could only re-resolve rested on *"`perScale` at deposit time is gone"*, and that is true
  only where a count can move WITHOUT announcing. Under plane B it always announces, so the state is never gone.
  ⛔ **B AND C ARE COUPLED — deliver both, or neither.** Delta-ing C while a count can still move unrouted
  reproduces precisely the drift the earlier ruling guarded against: the slot loses an amount it was never told
  about, and nothing re-derives it ([DEC-no-self-heal](decisions.md#dec-no-self-heal)).
- **ORDER-INDEPENDENCE SURVIVES, which is why LOAD is not a special case.** Source-then-count and
  count-then-source converge: the source applies `value × count_now` (0 if the count has not arrived yet), and
  the count applies `value × Δcount` for every deposit whose source is already live. A count route therefore
  tests the source's liveness at that owner — an O(1) `has()` — and applies for nobody else.
- ⚠ **THE HAZARD IS DOUBLE APPLICATION, NOT DRIFT.** One fact drives exactly ONE route class. Where a happening
  moves both a source and a count they are two distinct FACTS
  ([DEC-facts-name-happenings](decisions.md#dec-facts-name-happenings)), each applying its own — never one fact
  applying both.

- **⛔ NO PLANE HAS AN EVALUATION MOMENT TO DEFER, WHICH IS WHY NONE OF THEM CARRIES A STALENESS FLAG.** There is
  nothing to be stale ABOUT: every operand is compiled or maintained, so a slot is either current or was never
  told — and "never told" is a MISSING EMIT that must stay visible, not a state to schedule work against.
- **⚖ THE COMPLEXITY SHIFTS FROM O(WHAT EXISTS) TO O(WHAT CHANGED), AND THAT IS THE PERFORMANCE CASE (owner).**
  A rebuild re-walks the scope's sources, so its cost scales with how much a city HAS; an application touches the
  moved source's own deposits, so it costs the same in a 900-building city as in a 3-building one — **the walks
  disappear rather than getting faster**. This is [contexts.md](contexts.md)'s payoff one plane up: there, storing
  a fact made cost track EVENT volume instead of READ volume; here, applying a fact makes it track event volume
  instead of SOURCE volume.
  ⚑ **It also makes a promise the specs already print come TRUE.** [validation.md](../specs/validation.md) states
  that *"the only path to a rebuild is a mark, so per-turn cost tracks what CHANGED — mark volume, which is event
  volume"* — which holds only if a mark is cheap. While a mark triggers a walk the real cost is
  `events × sources-at-scope`, i.e. the dominant term is the one the sentence omits. Under the maintained sum the
  sentence is literally true, which is [DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king) getting the
  property it was written for.
  ⚠ **What legitimately still walks, so the claim is not overstated:** the CONDITIONED tail evaluates when its
  dependency moves (bounded by the reverse index, never by the scope); the cross-scope roll-up at read sums the
  ~5 packages the object sits under, which IS the design ([modifier.md §1](../specs/modifier.md)); and the
  ORACLE remains a deliberate full recompute that is never trimmed (§ THE RECOMPUTE IS AN ENDPOINT ORACLE).
- **⚑ PLANES B AND C ARE WHAT THE SOURCE FACT CANNOT ANSWER, and together they are the whole of the residue.** A
  Forge's `+1 happiness while powered` moves when the POWER moves though the Forge did nothing, so it rides the
  ATOM's route (plane C); a `per: {POPULATION}` deposit moves when the population moves, so it rides the COUNT's
  route (plane B). Neither rides the building's.
  ⛔ Both routes are REVERSE INDICES derived from the compiled deposit index — atom → the deposits it gates,
  count-key → the deposits it scales — never a sweep of the scope's deposits asking each whether it cares, and
  never hand-written ([DEC-one-reverse-view](decisions.md#dec-one-reverse-view)).
- **⚖ ORDER-INDEPENDENCE IS FREE, and it is what makes LOAD stop being a special case.** Addition commutes, so an
  accumulate needs no arrival order — exactly the property [event-spine.md](../specs/event-spine.md) already
  demands of facts. The banked-marks bracket existed because *a rebuild mid-read evaluates against
  half-deserialized state*; an application of a compiled constant evaluates nothing, so it has no such hazard.
  **Only the CONDITIONED tail genuinely needs the `GAME_LOAD_STARTED`..`FINISHED` bracket**, because only it
  reads state the stream may not have delivered yet.
  ⚠ **Consumer registration order remains a contract for that half** (consumers dispatch in registration order):
  **contexts → enabler → modifier → triggers**. Anything that EVALUATES a condition registers after the contexts
  whose stores that condition reads.

### ⚖ WHY DELTA-DERIVING FAILED BEFORE — two preconditions, both now met (owner)

> *"The reason delta-deriving failed in the old model was because there was no unified eventing system, and
> random event yields was baked in, and not as a separate source."*

This is the archaeology that makes the retired protocol legible, and it matters because without it a reader
concludes delta was TRIED AND FOUND WANTING. It was not: it was unavailable.

1. **No unified eventing.** With no complete fact stream, the only honest statement a system could make was
   *"something in here moved"* — which is exactly what a staleness flag encodes. The flag was the best available
   statement, not a preference. ⇒ **The spine was never only an observability project; it is the precondition
   that makes the cache unnecessary**, which is why it had to land first.
2. **⛔ ONE-SHOT EVENT GRANTS WERE BAKED INTO THE SAME ACCUMULATOR AS THE DERIVABLE DEPOSITS — and that is the
   one that actually poisoned it.** Such a slot can be maintained by NEITHER mechanism: you cannot DELTA it,
   because the event contribution has no live source to withdraw against; and you cannot RECOMPUTE it either,
   because recomputing WIPES the grant. The accumulator becomes unrecoverable — the history pollution
   [DEC-accumulator-cut-uniform](decisions.md#dec-accumulator-cut-uniform) describes. ⚑ So the old model was not
   choosing recompute OVER delta; with a baked-in grant both were broken, and recompute was the one that failed
   quietly.

**Both preconditions are now satisfied** — the spine carries the facts, and the event/vote grant has been split
into its own persisted store outside the derivation (§ Event/vote grants, below: *"having events just be stored
in the cache is lunacy"*; the reader sums `derivable + persisted`). The model that failed then is not the model
specified here.

⛔ **THE GUARD, so it cannot recur — and it is the test to run on any slot, not a historical note.**
**Can EVERY contribution to this slot be attributed to a live source that announces itself?**
- **YES** ⇒ the maintained sum holds.
- **NO** ⇒ the non-derivable part is a SEPARATE STORE and is never folded in.

⚠ The failure is silent, which is why it needs a test rather than vigilance: a baked-in one-shot grant leaves the
number entirely plausible while making the slot unmaintainable by any mechanism at all.

### ⛔ THE COST IS THE FORCING FUNCTION — a saturated emit surface is now STRUCTURAL, not a discipline (owner)

> *"We have to take that cost — the system will by its very definition collapse if we do not saturate with
> events."*

A maintained sum fails differently from a recomputed one, and the difference is the POINT:

| | a MISSED emit leaves | how it reads |
|---|---|---|
| recompute-on-mark | a stale but internally consistent value | **plausible forever** — nobody looks |
| **the maintained sum** | a phantom contribution nothing later clears, compounding on repetition | **loud, and louder over time** |

⚑ **That is [DEC-no-self-heal](decisions.md#dec-no-self-heal) carried to its conclusion rather than a weakness
accepted against it.** The rule already says a missed invalidation must surface as a live divergence instead of
being swept away; between two failure modes, the one that ANNOUNCES itself is the one the rule asks for. ⛔ So
this is never a licence to relax the emit surface "because the number self-corrects" — nothing self-corrects,
and that is deliberate.

⚑ **It also promotes the roadmap's ordering from a preference to a law.** *"The EMIT surface comes first; the
cache build is the step AFTER"* was sequencing advice under recompute; under a maintained sum an unsaturated
spine cannot produce a correct number **at all**, so completeness of the emit surface is a PRECONDITION of the
cascade being right rather than a quality target it trends toward.
⇒ Every ruling that pushes the emit surface toward exhaustive — *"add all the events, ever"*, *"too many events
is better than not enough"*, [DEC-close-event-gaps-now](decisions.md#dec-close-event-gaps-now) — is load-bearing
on this model, not enthusiasm.

⚠ **The bound on the damage, so the trade is stated honestly: a phantom lives at most ONE SESSION.** Nothing
derived is serialized, so LOAD rebuilds every slot from the reseed's own facts — the history pollution that makes
a legacy serialized accumulator unrecoverable ([DEC-accumulator-cut-uniform](decisions.md#dec-accumulator-cut-uniform))
cannot accrue here. The missed-emit tripwire (the stored-vs-oracle endpoint pair, below) is what NAMES it inside
that session.

### ⚖ AND IT IS THE EASIER CORRECTNESS PROBLEM — the deciding argument (owner)

> *"It is far easier to ensure we have all the events, than to ensure that we have all packages correctly
> marked."*

This holds independently of the cost trade above, and it is the reason to prefer the maintained sum even where
the two models would perform alike. The mark model needs **two** censuses complete; the maintained sum needs
**one** — and the one it drops is the harder of the pair:

| | the EMIT census | the MARK census |
|---|---|---|
| the question | *does this mutation choke point announce?* | *does this fact reach every slot it could move, at every scope, for every owner?* |
| where it is answerable | **LOCALLY**, at the setter — read it and you know | **NOWHERE local** — the answer lives in the authored data |
| moves with the DATA? | no — an emit is engine mechanism | **YES** — a newly authored deposit can silently need a new route |
| safe to over-include? | **YES** — a surplus emit costs one consumer branch that declines | **NO** — a surplus mark is a real rebuild on the turn path |

⛔ **That last row is decisive, and it is already the spec's own rule** ([event-spine.md](../specs/event-spine.md):
*"emit liberally, mark precisely"*). Over-inclusion is the technique that makes a completeness census tractable —
it is how the enabler's reverse index is allowed to be safe ([enabler.md §5](../specs/enabler.md): over-inclusion
is SAFE, a miss is the bug) — and the mark derivation is the one surface that cannot use it. A census that must
be EXACT, over a surface that moves with authored data, has no cheap verification at all.

⚑ **The emit census is owed ANYWAY, which is what makes dropping the other one a pure deletion.** The enabler,
the contexts, the trigger plane, the file log, the `/events` stream and the out-of-process replay all already
depend on the emit surface being complete. The mark derivation was a SECOND census serving one consumer, whose
correctness nothing else in the engine was ever checking.

⚑ **And a missing EMIT is multiply-observable** — a wrong availability verdict, an empty context store, a silent
`/events` frame, a missing log line — while a missing MARK is observable in exactly one package, through one
oracle diff, on one plane. The easier failure to find is the one to keep.
- **ONE read surface, and it is a bare fetch.** `CvCascadePackage::readFlat/readPercent/readSum` is the whole of
  it: a package has no second, rebuild-triggering read to reach for, so a cross-scope input needs no ordering
  guarantee and the load bracket has nothing to drain. **THERE IS NO GATE ON A READ** — nothing is tested on it,
  because nothing on it can recompute.
- **The two served surfaces, per plane** (`/computed/*`, [http-endpoints.md](../specs/http-endpoints.md)):
  `.../stored` serves what the events built (`CvCascadePackage::readValuesInto`,
  `EnablerKernel::operatingBuildings`, `CascadeCapabilities::storedUnion`) and `.../oracle` serves the
  from-source recompute into a buffer the endpoint owns (`CascadeGather::gather*Into`,
  `EnablerKernel::recomputeOperatingSetInto`, `CascadeCapabilities::refreshInto`). Both sides render through
  ONE renderer per plane (`Sources/Tools/CvOracleEndpoints.cpp`), so the documents are diffable field by field.
- **Where the oracle lives is decided by where the STORAGE lives** — the storage owner supplies the scratch (the
  gather's `gather*Into` for the cascade packages), and the oracle is never handed the stored slots.
- **An oracle run is a FULL RECALC, by design (owner) — it reads NOTHING off the stored surface.** Every input,
  including every cross-scope one, is recomputed from source. ⛔ The tempting alternative — recompute only the
  asked-about object and read its cross-scope inputs off the stored packages — is **WRONG, and wrong in the
  specific way that killed the old twin surface**: an oracle that consumes stored values is partly built on the
  very state it exists to check, so a wrong input is silently INHERITED and the two sides quietly share a
  derivation again. **Independence is the entire value of the oracle**; nothing may be traded for it.
  *(Attribution is not lost by going full: a drifted low scope diverges on its OWN row too, so the external
  differ walks from the lowest diverging scope up to find the root cause — and attribution is the EXTERNAL
  reader's job anyway, which is the whole point of taking the comparison out of the DLL.)*
- **⛔ An oracle run's COST IS IRRELEVANT — "correct is correct" (owner).** It is invoked deliberately, by a
  human or a tool asking a question, never on a turn path. So it is never trimmed, sampled, memoized, or made
  incremental to look cheap: a full recompute that takes as long as it takes IS the deliverable. Do not optimize
  it, and never let a performance argument reshape it — that is how an oracle stops being independent.
- **An oracle run ANNOUNCES nothing.** It emits no `[CASCADE] rebuilt` line: nothing was rebuilt, and a
  verification sweep must not move the numbers that describe real work.

## ⛔ `CvDerivedCache` IS REPLACED BY `ContextDict` — VIRTUALLY EVERYWHERE (owner)

> *"`CvDerivedCache` should be replaced by `ContextDict` virtually everywhere needed, and we just need to start
> taking one cluster at a time with event wiring."*

**The component and the dictionary are the two answers to one question, and the spine decides which is right.**
`CvDerivedCache` is *mark → recompute from sources*; `ContextDict` is *apply the delta the fact carries*. The
first is only ever necessary when the inputs arrive UNANNOUNCED — and under a saturated emit surface no input
does ([DEC-flag-is-fossil](decisions.md#dec-flag-is-fossil)). So the component's remaining niche is
not small, it is EMPTY BY CONSTRUCTION, and every tenant of it is a store waiting to be re-expressed as a keyed
accumulator ([DEC-keyed-accumulator](decisions.md#dec-keyed-accumulator)).

⛔ **A surviving `CvDerivedCache` tenant is therefore a MISSING EMIT wearing a component**, exactly as a staleness
flag is a missing emit wearing a flag. The disposition is never "keep the cache for this one": it is **name the
fact, wire it, and let the dictionary hold the answer** — after which there is nothing left for a recompute to
do.

**⚖ THE METHOD IS ONE CLUSTER AT A TIME, AND THE SIZE OF THE WHOLE IS IRRELEVANT (owner): *"this is one of
those times where how big it is is irrelevant — we have to start in a corner."*** A cluster is one entity's
FACTS plus the STORE those facts feed, converted together: the events re-cut to name their happenings
([event-spine.md](../specs/event-spine.md) § A FACT NAMES THE HAPPENING), the store converted from
mark-and-recompute to `id → count` fed ±1, and the recompute deleted in the same change. ⛔ Do NOT convert a
store's STORAGE without its MAINTENANCE or the reverse — a set-shaped store survives only while something
recomputes it whole, so the two halves land together or not at all (§ THE SEMIBOOLEAN STATE).
⚠ **Counting the remaining clusters is not a prerequisite for starting one**, and treating the total as a
decision to be taken first is the hesitation this ruling exists to remove.

⚑ **What is genuinely NOT a `ContextDict` — so "virtually everywhere" is not read as "everywhere".** The
dictionary holds a keyed COUNT; a summed MAGNITUDE belongs in the channel-indexed package
(§ EVERY DERIVED STORE IS ONE SHAPE — they share the maintenance rule and not the identity), and a genuine
per-object scalar that no key indexes stays a plain member. The question is what the slot HOLDS, never which
mechanism is fashionable.

**The component being removed** is `Sources/Infrastructure/CvDerivedCache.h` — a templated value-holder with the
recompute injected as a member-function-pointer, in three forms: the single-flag `CvDerivedCache<TOwner,T,N>`,
the partial-mask `CvDerivedCacheSet<TOwner>` (the mask protocol the packages and the team capabilities still
sit on), and the runtime-sized `CvDerivedCacheVec<TOwner,T>`. Recognise it by its shape: **a `markDirty` that
triggers a recompute over the owner's CURRENT state.** That recompute IS the calculation the fact was supposed
to make unnecessary.

⛔ **It is not extended, not given new tenants, and not "converted" in place.** A tenant leaves by the cluster
move above — its fact re-cut to name the happening, its state re-expressed as `id → count` or as a channel slot,
its recompute deleted — and the component disappears when the last tenant does. ⛔ A hand-rolled staleness-flag pair
beside it is not an improvement on it; it is the same defect without the shared type
([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape)).

⚑ **The legacy `CvCity` hand-rolled staleness caches** (`m_aiCommerceRate`, `m_aiBuildingCommerce100`,
squirrelBanana) are **demolition fodder rather than conversion targets** — they are cut when the channel that
replaces them lands, never polished or re-homed onto the component on the way out.

## ⚖ Refinements

- **⚖ THE CAPSTONE RULE: the cascade is built and kept current ENTIRELY from events — no blanket rebuild, ever,
  and no per-slot rebuild either.** On LOAD the cascade is stood up by the **event reseed** — the save read fires
  the DOMAIN events for every fact as it deserializes and each fact applies its source's deposits
  ([event-spine.md](../specs/event-spine.md) / [DEC-spine-reseed](decisions.md#dec-spine-reseed)); the old
  recompute-on-load / warm-up recalc (`playerSliceRebuild` + `worldRebuild`) was a stabilize-the-drift STOPGAP
  and is REMOVED. Post-load, a fact reaches exactly the slots its deposits feed and **nothing else runs at all** —
  no full per-player rebuild on `doTurn`, no mark-all, no per-slice blanket, no turn-roll self-heal
  ([DEC-no-self-heal](decisions.md#dec-no-self-heal)). A missed emit surfaces as a live divergence, never a
  silently self-healed cost — which is precisely why the event spine must be COMPLETE (every mutation emits) and
  is built proper and FIRST.
  ⚑ **Under the maintained sum that sentence hardens from a design preference into a PRECONDITION:** an
  unsaturated spine does not merely leave a value stale, it leaves the sum wrong with nothing that could ever
  correct it ([DEC-maintained-sum](decisions.md#dec-maintained-sum)).
  Reads are BARE NUMBER FETCHES during the turn (an ensure-per-read protocol on AI-hot paths measurably ground
  unit automation). ⚑ *"It's the percentage recalcs that hurt"* is answered at the root rather than mitigated:
  the compiled deposit carries its channel AND its unit, so a flat fact touches a flat slot and no percent stack
  is ever walked — there is no mask to split, because there is no recalc to narrow.
- **⚖ THE PER-SCOPE PACKAGE MODEL — the cascade's FOUNDING DESIGN ([modifier.md](../specs/modifier.md) §1), stated
  as cache architecture.** A package lives ON EVERY SCOPED ITEM, every level (world → team → player
  → city → plot); the cascade loads **yield packages in ONE UNIFORM FORMAT** (Σflat and Σpercent each their OWN
  package per channel; the unit is part of the slot key) into each scope's cache; each package is maintained
  from events at its OWN scope (a world-scope fact moves the world package while every other level stands). **The
  only live calculation is adding the ~5 packages together at read.**
  **⛔ EVERY scope carries packages — whether a given scope's packages are EMPTY is IRRELEVANT (owner ruling).**
  The uniformity IS the design: it is what makes "the only live calc is summing the packages" literally true and keeps
  the read path identical at every level. A scope is never skipped because its packages look empty — an absent package
  forces the read to source that scope some OTHER way, and both ways are defects: a per-read walk (the cost class this
  doc exists to prevent), or an upper scope's sum stored in a lower one (breaking the scope principle,
  [modifier.md §1](../specs/modifier.md), which forces downward invalidation fan-out).
  **⛔ THE ORIGIN RULE — THIS IS THE PURE CASCADE DESIGN (owner), not a constraint bolted onto it:**
  - **YIELDS come from exactly three sources: PLOT, SPECIALISTS, and BUILDINGS (city).** Nowhere else produces a
    yield. So the flat/yield side of a package exists at **plot** and **city** only.
  - **MODIFIERS come from everything BUT plot** — city, empire, team, world. So the percent side exists at
    every scope except plot.

  Plot and the upper scopes are therefore mirror images (yield-only vs percent-only), and **CITY is the single
  scope carrying both**. That is why "whether a scope's packages are empty is irrelevant" is not hand-waving: the
  shape is uniform, and the origin rule says which half any given scope ever fills.
  ⚖ **The rule governs the YIELD/RATE plane; for every other family the sides are the DATA's and the minted
  channel sets enforce them** (wellbeing authors empire flats; health/defense/property author plot percents)
  — [modifier.md §1](../specs/modifier.md). ⛔ Consequence for any read-side roll-up: **the channel set is the
  gate, never a hand-written per-scope filter.**

  **⛔ THE CONSOLIDATION REQUIREMENT (owner): every modifier/yield cache is ONE shape** — TWO DICTIONARIES per
  scope object, one flats and one percents, each an int keyed by channel. The drift it replaces is the ~33
  hand-named scalar fields (`scGpBaseBld`, `scDefense`, `scDefBombard`, `scMaintModCity`, `scTradeCity`,
  `brCityMilitary`, …): a hand-named field cannot be addressed uniformly, so it forces a bespoke invalidation
  path per field, which is how that many accumulated. A new scope or channel must be DATA, not a new struct.

  **⛔ KEYS ONLY WHERE THEY ARE NEEDED (owner) — the storage is NOT a global dense index.** The channel set is
  DATA-DEFINED (`PROPERTY_*` is one channel per property info) and no object uses more than a fraction of it, so
  a dense array over every channel on every object is mostly zeros — on 9,600 plots that is ~7 MB of nothing.
  Each scope carries ONLY the channels authored AT that scope, both the channel ids and the per-scope sets
  derived from the data at load (the `ClassificationRegistry` minting precedent), never hand-listed. Measured
  from `Assets/Data`: plot **13** · city **40** · empire **50** · team **3** · self 1 — the distinct non-unit
  channels, with no object carrying more than 50. ⚠ city and empire exceed a 32-bit derived mask, so the
  shared `CvDerivedCacheSet` mask widens to 64-bit (every existing user occupies few bits and is unaffected).

  **⛔ A SCOPE MUST BE UNAMBIGUOUSLY OWNABLE — WHICH IS WHY A LANDMASS IS NOT ONE (owner).** This is the test a
  candidate scope has to pass, and it explains the whole spine at once:
  - **WORLD passes by being UNIVERSAL** — *"game scope works, because it affects everyone, always"*, so the
    question of who owns the value never arises.
  - **team / empire / city / plot pass by being OWNED BY EXACTLY ONE PLAYER** up the chain, which is what lets a
    deposit roll DOWN and a target read one combined total.
  - **A LANDMASS passes NEITHER.** *"It knows no borders"* — one landmass spans several empires at once, so an
    effect on it *"affects individual players"* and is inherently a per-(landmass × player) **CROSS-PRODUCT**
    rather than a scope. Modelling it as one forces a bespoke slot into the MIDDLE of the containment spine, and
    that bespoke slot is the TELL, not the solution.

  So **there is no area scope**: `"area"` is not a scope token, no object carries an area package, and the
  containment spine is `world › team › empire › city › plot` ([json.md §3.2](../specs/json.md)). The legacy
  `iArea*` authorings were modders reading "area" as "player" — they author at **EMPIRE** — and the ONE genuine
  area concept is a PHYSICAL CONTIGUITY constraint (you cannot run power lines across an ocean), which is the
  engine-side clean-power counter and never a cascade channel.
  ⚑ **The area ID SURVIVES as a plain FACT**, and that is the whole of what an area is to the cascade: a bare id
  plus its tile count, forwarded by `CityContext` for the `AREA_SIZE` token and the coastal water-body read
  ([contexts.md](contexts.md)). ⛔ The city carries that ID, never a `CvArea*` or a per-read `area()` chase — a
  per-read `area()->getNumTiles()` dereferences a whole object to answer a counter an int already holds.
  **⚑ Areas are VIRTUALLY NEVER recalculated (owner)** — `CvMap::recalculateAreas` exists for the extreme case
  of terrain levelled to sea level (the WMD mechanic), plus map generation; a landmass does not otherwise split
  or merge in play. Treat a rebuild as RARE-but-real: it does `m_areas.removeAll()` and reassigns every id.
  **So the rebuild announces itself as a DOMAIN fact (owner): emit "areas recalculated" and force the recheck** —
  every holder of an area id re-reads, rather than each cache inventing its own staleness test. Being rare, the
  blanket costs nothing; and it is not the banned self-heal: a wholesale identity reassignment is not
  addressable per-source, so no finer route exists to derive ([DEC-no-self-heal](decisions.md#dec-no-self-heal)
  bans papering over a
  MISSED invalidation, not announcing a genuine wholesale one).

  **⛔ TWO SCOPES ARE DELIBERATELY NOT PACKAGES (owner):**
  - **WORLD is CONFIG** — cost multipliers and the like, carried by eras / gamespeeds / handicaps. It changes
    essentially never and is read from its sources, not cached behind a staleness protocol. A project granting
    something to every player is NOT world-scope state: it authors the plural TARGET `world.empires`
    ([json.md §3.3](../specs/json.md)) and lands in each PLAYER's package. The handful of `health.world` /
    `happiness.world` / `tradeRoutes.world` project authorings are mis-scoped data, a curator fix
    ([DEC-recurate-on-decision](decisions.md#dec-recurate-on-decision)).
  - **UNIT is RESOLVED VALUES, not a package** — "when the number is put on the unit, no more percentages or
    whatever is involved, the data just IS". The exact set of numbers a unit carries is known, so they are summed
    and stored individually, and they move on a different trigger from everything else: ONLY when a promotion or
    combat class changes. It is the most static plane in the engine.
    ⛔ **THE SUM WALKS WHAT THE UNIT HOLDS, NEVER THE REGISTRY.** The contributors are the unit's own type plus
    its held promotions and held combat classes, enumerated from the containers the unit already keys them in —
    not discovered by sweeping every promotion and every class asking "do I have this?". That sweep costs the
    DATABASE per gather to rediscover a handful, which is the O(registry) shape the event-built state exists to
    delete ([contexts.md](contexts.md): a read that walks per call is the efficiency defect to reject in review)
    and the own-data inversion [DEC-one-reverse-view](decisions.md#dec-one-reverse-view) bans one plane over.
    The unit's storage is therefore NOT a
    bespoke struct awaiting consolidation — it is correctly its own shape, and the 12 unit-only families
    (`strength`, `movement`, `withdrawal`, `firstStrike`, `capture`, `collateral`, `heal`, `bombard`, `air`,
    `cargo`, `range`, `pillage`, …) never enter a scope's channel set.
    ⚖ **STRENGTH'S BASE IS PER-UNIT STATE AND IS DELIBERATELY SERIALIZED (owner ruling).** Every other resolved
    slot takes the unit's own TYPE from the gather, because it is a pure function of that type. Base strength is
    not: **WorldBuilder edits an individual unit's strength**, and the WBS scenario format persists the result
    (`CombatStr=`, written only when it differs from the type). *"You want people to be able to do things in
    WorldBuilder."* So the base lives on `CvUnit` as the serialized `m_iBaseCombat`, the resolved plane carries
    the promotion / unit-combat **DELTA ONLY**, and the consumer adds the two. ⛔ This is the ONE carve-out in an
    otherwise uniform gather, and it is load-bearing: letting the type contribute to the strength slot as well
    silently DOUBLE-COUNTS every unit's authored base. ⚠ It is therefore NOT a
    [DEC-derived-never-trusted](decisions.md#dec-derived-never-trusted) violation — the value is genuine
    per-unit state that no amount of re-derivation can reconstruct, which is exactly why it is stored.
    ⚖ **A SECTION FOLDS BESIDE THE SLOT TABLE, ON THE SAME MARK — it does not become a slot, and it does not
    become a hand-named pair either.** The slot table addresses modifier-FAMILY entries by
    `(family, kind, scope, unit)`; a `hideAndSeek`-shaped SECTION ([json.md §9](../specs/json.md)) has no such
    address, so it cannot ride the table — and a scalar pair beside it would be the shape
    [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) calls a defect. It gets its own cached block
    on the SAME two facts, so ONE route maintains one unit's whole resolved state.
    ⚑ **What earns a section that block is the CARRIER SET, and it is the test to apply to the next one:** it
    folds over exactly the unit's own info ∪ held promotions ∪ held unit-combat classes — the same three the slot
    table folds over — so the two facts that move the table are precisely the two that move it, with no third
    trigger to find. ⛔ A per-read fold over those carriers is the O(registry) walk this plane exists to delete,
    and converting it to walk the HELD containers instead is NOT the fix — that is the same walk with a better
    receiver ([contexts.md](contexts.md)). The read becomes a bare fetch or nothing has been done.
  ⚠ Hand-maintained duplicates DRIFT — that is not theoretical: the maintenance decomposition and its cached fill
  duplicated five terms, and the L8 home/otherArea overlay landed in one and not the other, so `/computed`
  under-reported by 39 against the served value until the duplicate was replaced by a delegation.
  Full rebuild of everything = LOAD ONLY.
  **⛔ THE FIX IS NEVER "ADD ANOTHER STRUCT" — that is the failure mode this ruling exists to close.** The previous
  substrate grew ONE BESPOKE STRUCT PER SCOPE, each with hand-named per-channel members instead of channel-indexed
  Σflat/Σpercent; it is archived and must not be reconstructed ([superseded-ideas](superseded-ideas.md) #14).
  **A missing scope is a SYMPTOM of that, not the disease:** with one uniform package, giving a scope its packages
  is a single member; with bespoke structs every scope is its own project — which is exactly why a small scope
  (team, at three channels) never got one, and why its sums leaked into whichever neighbour already had
  a struct. So the package TYPE is unified FIRST (one owner-templated, channel-indexed package on
  `CvDerivedCacheSet<TOwner>`), after which every scope falls out of the same member. Adding a further per-scope
  struct deepens the divergence this closes.
- **⚖ THE KEY IS SAMENESS (owner ruling): every store is the SAME OBJECT TYPE everywhere, and they ALL MAINTAIN
  the SAME WAY.** That — not the per-scope layout — is the requirement the whole model rests on. One templated
  channel-indexed slot table on every owner, and ONE application path driving all of it, derived from the deposit
  index. What varies between scopes is only WHICH SLOTS carry a value; the type and the protocol never vary.
  - **A RECEIVER is not a different kind of cache — it is the same cache holding a different slot (owner).**
    Whatever scope CONSUMES a channel caches its realized sum as **one variable per channel**, in the same cache
    beside the packages: `CvPlayer` caches research / gold / culture / espionage; `CvCity` caches production /
    culture and the other sums it consumes. The city's realized yield rate (`yRate100[]`) is the general shape,
    not a special case — so there is no separate "receiver mechanism" to build.
  - **⛔ THIS IS WHY HAND-NAMED SCALAR FIELDS ARE THE DEFECT, not just untidy.** A named field cannot be addressed
    uniformly, so it forces its own bespoke maintenance path — which is precisely how 33 of them accumulated.
    Channel-indexed slots are reached by the deposit's own compiled address, with no per-field code.
  - **The receiving scope is NOT the storing scope.** A package never moves to its consumer (that breaks the scope
    principle); the consumer stores only its own realized TOTAL — one cheap variable per channel.
  - **⛔ A CROSS-SCOPE receiver total is the Σ of its MEMBERS' REALIZED values — and NOTHING beside that Σ.** The
    empire's gold / research / culture / espionage sums are Σ over the player's cities of each city's realized
    rate of that channel: exactly the quantity the retired per-read city walk answered, merely cached. The
    per-city quantity for a commerce channel is the whole [modifier.md §2a](../specs/modifier.md) split — the
    slider share of the city's COMMERCE yield, the channel's own deposits, and the process conversion — not the
    channel's deposits alone. ⛔ **An upper scope's own package is NEVER added on top of that Σ:** its deposits
    roll DOWN ([modifier.md §1](../specs/modifier.md)) and are therefore already inside every member's realized
    value, so adding them again at the receiving scope counts each empire-scope deposit once per city PLUS once
    more — a silent multiplication that compiles, runs, and simply reports wrong numbers.
  - **⛔ NOT a push accumulator, and NOT a per-read walk — the cached sum sits between those two failures.**
    Rejecting the legacy incremental accumulator does not license recomputing on every read: an empire-scope
    getter that re-walks every city per call (`CvPlayer::getCommerceRate`) is the per-read-walk cost class this
    doc exists to prevent, merely relocated one scope up.
  - **ONE EVENT REACHES BOTH LEVELS (owner).** The event names the packages it feeds **and** the sum slots those
    packages feed — one derivation from the deposit index, two targets (for a cross-scope aggregate, two owners).
    There is **no** dependency-ordered pass, because nothing is deferred at either level.
  - **⚖ THE CROSS-SCOPE RECEIVER — SUPPRESSION IS SETTLED; ONLY THE DELTA QUESTION IS OPEN.** A receiver total is
    the Σ of its members' **REALIZED** values (§ A CROSS-SCOPE receiver total), and a realized value is the §2a
    combine over the member's packages, not a stored deposit sum. Two of its apparent obstacles dissolve:
    - **⛔ DISORDER AND WLTKD ARE NOT TERMS IN THE COMBINE — they are a PARTICIPATION GATE ON THE Σ (owner):**
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
    **⚖ THE RECEIVER RE-SUMS ITS PARTICIPATING MEMBERS, AND NOTHING IS BUILT TO AVOID THAT (owner).** *"The
    summing is so trivial that it would cost more to try some efficiency shenanigans."* The read side is ~5 int
    adds for the cross-scope roll-up and one combine per participating member for a receiver Σ — against the
    per-source walk the maintained sum deletes, that is not a cost to design around.
    ⛔ **So do NOT build a per-source decomposition plane.** A `(scope × channel × SOURCE)` breakdown exists only
    to make WITHDRAWAL cheap, and withdrawal is only expensive if summing is. Its source axis dwarfs the channel
    axis that [KEYS ONLY WHERE NEEDED](#) already rejected as mostly-zeros, and the shape is the
    add-another-struct failure [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) names. The
    unconditioned plane re-applies its compiled constant; the conditioned tail re-resolves; nothing stores a
    per-source breakdown.
    ⛔ **And do NOT push the realized delta upward** — that is the shenanigan the triviality makes pointless, and
    it is a push up the chain. The third shape is barred outright: a member EMITTING *"my realized value
    changed"* ([event-spine.md](../specs/event-spine.md): *"yield is a computed RESULT, never an event"*).
    > **⚑ THIS IS NOT A DEFERRAL, and reading it as one is the misreading to prevent (owner): *"IF it shows that
    > the summing requires any kind of serious power, we deal with it then."*** [DEC-no-deferred](decisions.md#dec-no-deferred)
    > bans parking work KNOWN to be needed; this declines to build machinery for a cost nobody has demonstrated
    > exists — which is what [roadmap.md](../plans/structural-cleanup/roadmap.md) already requires (*"build the
    > base first; the most efficient way comes AFTER … do not build, investigate, or pre-shape it ahead of the
    > base"*) and what [triggers.md](../specs/triggers.md) requires of hypothetical machinery. You cannot defer
    > work whose necessity is unestablished.
    > ⚑ **The REVISIT TRIGGER is named and it is a MEASUREMENT, never an argument:** a turn-time cost on the
    > standing late-game save, attributed to the summing, on the wall clock
    > ([DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king)). ⛔ Until that exists, a proposal to optimize
    > the sum is speculative structure — and an AI loop asking a receiver Σ per candidate is answered by the
    > CALLER caching its own scores ([patterns.md](patterns.md), the sanctioned heuristic residual), never by
    > reshaping the machine.
  - **Which scope receives a channel is spec'd, not chosen per site:** one consuming scope per channel
    (food/production → city; gold/research/espionage/**maintenance** → empire), with **culture the lone
    dual-consumer** (the city sums it for plot culture + border expansion, the empire for civ culture + traits —
    two independent sums over the same packages).
    ⚑ **MAINTENANCE is the one NON-commerce receiver, and it is what makes the rule general rather than a
    commerce habit.** The empire's total maintenance is the Σ over its cities of each city's realized
    maintenance — precisely the cross-scope receiver shape above — so it is a SLOT in the empire's own package
    cache, never a hand-named cache beside it ([DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape):
    a named field cannot be addressed by a derived mask, so it forces its own bespoke invalidation path).
    ⚠ Its per-city quantity is the one a package cannot answer alone: a city's realized maintenance composes the
    three component KINDS (distance / numCities / colony) each against its own modifiers, takes the `amount`
    stack over the total, and declines wholesale under WLTKD/disorder ([economy.md](../reference/economy.md)).
    ⛔ **`MAINTENANCE_CORPORATION` is NOT one of them** — corporate maintenance is its own pre-inflation expense
    beside total maintenance, so the city total SKIPS that kind. Its deposit is a city-scope FLAT and therefore
    sits in the city's package like any other: a read that folded every maintenance kind would charge the same
    corporate gold twice in one expense total, plausibly and silently. So the Σ asks the CITY for its realized value —
    which is what "the Σ of its members' REALIZED values" already says — and the oracle recomputes both halves
    from source rather than reading either off the stored surface.
    ⚠ **A receiver read is therefore NOT interchangeable with a rolled-legs read on the same channel.** The
    cross-scope roll-up answers a receiver channel with its maintained SUM, so a consumer that wants the
    channel's percent STACK at that scope must read the legs directly — asking the roll-up would hand back the
    realized total instead, silently and plausibly.
- **⚖ THE TWO DERIVED PLANES SHARE ONE MECHANISM — what differs is their CONTENT, never their maintenance:**
  - **The yield + percent packages** are maintained by applying the moved source's deposits to the slot they
    feed — § THE MAINTAINED SUM. ⛔ The earlier framing called this "an INPUT/OUTPUT value cache: memoize,
    mark-invalidate on a source event, recompute from inputs" and pointed it at `CvDerivedCache`. That framing
    is RETIRED, and it is what let the modifier alone keep a staleness protocol while both of its siblings ran
    without one ([superseded-ideas](superseded-ideas.md) #30).
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
  when the fact arrived ([superseded-ideas](superseded-ideas.md) #30).
- **THE APPLICATION IS DERIVED FROM THE DATA, never hand-wired.** A DOMAIN event carries its SOURCE; the source's
  compiled deposits (the load-time strings→ints index, `Data/CvDepositIndex.{h,cpp}` — per-deposit interned
  segments + FK-resolved target id + the resolved channel/scope slot, compiled at readJson push-time) name exactly
  the channels × scopes × targets it feeds — **so what to apply, and where, falls out of the deposit addresses.**
  The routing is a pure function of the index; a hand-coded hook per event site is a per-site bespoke path of
  exactly the kind [DEC-uniform-cache-shape](decisions.md#dec-uniform-cache-shape) forbids. Derive it from the
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
  [DEC-turn-time-is-king](decisions.md#dec-turn-time-is-king).

This is the Clean-Architecture north-star applied to engine state: the repository **is** the contract, and it is the
lever for thinning the `Cv*` god-classes without touching the closed-EXE-bound `CvPlot`/`CvCity` layout. See
[north-star](north-star.md).
