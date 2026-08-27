# The cascade — deposits, the maintained sum, and the live-state contexts

The cascade machine that computes **per-turn magnitudes** — a city's food, a unit's strength, a property's
level. Sources **deposit** values; a target reads the **combined total**. It reads the modifier families
authored per the [json spec](specs/json.md) §6; everything here is downstream of that authoring: the deposit
flow and combine arithmetic, the conditioning (dormancy) model, the ownership rule that decides *where* a
cross-entity modifier is authored, the **maintained-sum** mechanism that keeps every derived slot correct with
nothing ever marked or recomputed, and the per-scope **contexts** a reader goes to for an entity's live state.

**At heart, a modifier is a [`requires`](specs/enabler.md) gate plus an output.** It uses the *exact same*
condition vocabulary as the enabler — `all`/`any`/`noneOf`, atoms, predicates, scopes — so once `requires` is
nailed the modifier follows for free; the only thing it adds is a **magnitude** to deposit when the gate holds.
So the cascade leans on that shared vocabulary (defined in [enabler](specs/enabler.md) / [json](specs/json.md))
and spends its effort on the **output half**: how a magnitude deposits, accumulates, combines, stays current,
and is read back.

**One owner, one design, stated once.** The deposit machine, the maintenance mechanism that keeps every derived
slot correct, and the per-scope live-state read surface are ONE concept. They are paginated below for reading,
never separated: every part lives in `cascade/`, under this page, and no part of this concept is filed under
another tier.

⛔ **The pages below ARE the spec — this page is a map and carries no ruling of its own.** Read the parts your
work touches END TO END; the count of parts that apply is something you FIND, not something you decide
([AGENTS.md](../AGENTS.md)).

## The parts

| part | what it settles |
|---|---|
| **[deposit and read](cascade/01-deposit-and-read.md)** | The one step: deposit DOWN, accumulate, read O(1). |
| **[maintained sum](cascade/02-maintained-sum.md)** | A package is never dirtied and recalculated - the problem and the model. |
| **[no staleness no selfheal](cascade/03-no-staleness-no-selfheal.md)** | Why a staleness flag or a self-heal is the fossil of a missing emit; the legacy-accumulator cut. |
| **[derived stores](cascade/04-derived-stores.md)** | Every derived store is one shape: a keyed accumulator maintained by a delta. |
| **[three planes](cascade/05-three-planes.md)** | Three planes, one slot, nothing ever recomputed - the invariant and why delta-deriving works now. |
| **[spatial and contextdict](cascade/06-spatial-and-contextdict.md)** | The spatial carve-out (a path IS a legitimate cache) and CvDerivedCache -> ContextDict. |
| **[combine arithmetic](cascade/07-combine-arithmetic.md)** | The combine arithmetic: flats sum, percents sum then multiply once. |
| **[realized rate](cascade/08-realized-rate.md)** | The realized RATE - TIER 1 BASE vs TIER 2 EXTRA, and the one additive percent stack. |
| **[wellbeing channels](cascade/09-wellbeing-channels.md)** | The WELLBEING channels - health + happiness, signed-split. |
| **[contexts](cascade/10-contexts.md)** | The contexts: the per-scope live-state read surface, and the one idea behind it. |
| **[context stores vs forwards](cascade/11-context-stores-vs-forwards.md)** | What a context STORES vs FORWARDS - an event-built store, not a forwarding facade. |
| **[eval ctx and counts](cascade/12-eval-ctx-and-counts.md)** | The eval ctx carries contexts, not game objects; counts, not objects. |
| **[context maintained events](cascade/13-context-maintained-events.md)** | Contexts are maintained EVENT-DRIVEN - never a per-turn recompute. |
| **[context scope set](cascade/14-context-scope-set.md)** | The scope set: plot / city / player now; units future; no AreaContext. |
| **[read path](cascade/15-read-path.md)** | The read: the cascade PROVIDES, the game object SUMS - and the capstone rule. |
| **[package model](cascade/16-package-model.md)** | The per-scope PACKAGE model - the cascade's founding design stated as cache architecture. |
| **[conditioning](cascade/17-conditioning.md)** | Conditioning - re-applied when its own dependency moves (the dormancy model). |
| **[ownership](cascade/18-ownership.md)** | Ownership - the deliveryguy rule. |
| **[targets](cascade/19-targets.md)** | Targets - scope-wide, object-plural, or keyed. |
| **[unit plane](cascade/20-unit-plane.md)** | The unit plane - a self-accumulator, and specialist counts. |

## See also

- [json.md](specs/json.md) — the data this machine reads: the modifier-family address, `flat`/`percent`/`multiplier`
  units, `enabled`/`disabled`/`per` conditioning, `plots`/`units` targets, and the `buildRate` vs `production`
  split (§3, §6).
- [enabler.md](specs/enabler.md) — the "can I?" machine. Availability is upstream of magnitude: an unavailable or
  dormant entity deposits nothing.
- [tally.md](specs/tally.md) — the count machine a `per` scaler reads at cross-city scopes.
- [naming.md](specs/naming.md) — the `INFOTYPE_NAME` ids used as deposit keys and condition atoms.
- [patterns.md](architecture/patterns.md) — the INFO DATA-OUT contract + the per-group valuation surface that
  reads the contexts.
- [spine.md](spine.md) — the event dispatch primitive every consumer on this page (the modifier consumer, every
  context dictionary, the package apply path) draws its facts from.

