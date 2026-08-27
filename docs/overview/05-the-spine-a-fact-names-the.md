# 5. The spine: a fact names the happening

> Part of the **[overview](../overview.md)** spec.

→ [spine.md](../spine.md)

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

