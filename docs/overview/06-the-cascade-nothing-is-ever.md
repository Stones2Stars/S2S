# 6. The cascade: nothing is ever recalculated

> Part of the **[overview](../overview.md)** spec.

→ [cascade.md](../cascade.md)

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

