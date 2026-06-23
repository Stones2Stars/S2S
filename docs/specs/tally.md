# The tally — "how many?"

The cascade machine that answers **"how many of X do I have?"** — presence (`≥ 1`) and thresholds (`≥ / ≤ N`) of
any [Type](naming.md) at any scope. It is the **count sibling of the [modifier](modifier.md)**: the same
additive scope substrate, but summing **counts** instead of magnitudes — and where the modifier flows **DOWN**,
the tally rolls **UP**.

It is **engine machinery, never authored in JSON**. The JSON only carries the clauses that *read* it — an
[enabler](enabler.md) `requires` count-atom, an [enabler](enabler.md) `allowed` cap, a [modifier](modifier.md)
`per` scaler. This doc is that machine.

---

## 1. One substrate, two machines

The tally and the modifier are **one primitive, two instantiations** of a scope-keyed additive accumulator:
`deposit(key, ±delta)` folds a delta into a key's running sum, `get(key)` reads it. The modifier sums effect
*magnitudes*; the tally sums presence *counts* (each building/unit deposits `±1`). That shared foundation is why
the tally is a first-class machine, not a bolt-on.

The tally's accumulator is **authoritative and exact** — deliberately *not* lazy or stale-tolerant — because the
[enabler](enabler.md) gates buildability off it: a wrong or stale count would let the wrong things be built.

---

## 2. Counts roll UP; the stored leaf is the PLAYER

Counts **originate per city** (a building is built *in* a city) and **roll up** to empire / team / world. But
the stored leaf is the **player** — one accumulator per `(domain, player)`, where a **count domain** is the
bucket for one kind of Type (buildings, units, techs, … — one domain per [Type](naming.md) kind); there is no
per-city count table.

- **empire** read → the player's own accumulator.
- **team** / **world** read → summed over the relevant players **on read** (no stored team/world total).
- **city** / **plot** reads do **not** go through the tally at all — they read the live `CvCity`/`CvPlot`
  directly (a local count needs no roll-up).

Player-leaf suffices because every current cross-city consumer (§3) is empire/team/world; widening to a city
leaf later, if a real per-city consumer appears, is a contained, no-save-break change.

---

## 3. Who reads it

One module, several readers ([enabler](enabler.md) / [modifier](modifier.md) / engine):

1. **`requires` count-thresholds** — `min(TYPE,N)` / `max(TYPE,N)` at empire/team/world (city = local read).
2. **`allowed` cap enforcement** — a build is permitted while `count(me, scope) < allowed`.
3. **`per` count-scaler** — a deposit scaled by `count(TYPE)/each` at a cross-city scope.
4. **demographics / UI / AI / score** — current counts, wanted independent of the cascade.

Routing is by **Type prefix** ([naming.md](naming.md)): `BUILDING_`/`UNIT_`/… selects which count domain a read
lands in. Presence is just the `min:1` case — authoring presence as a count means going volumetric later is a
value change, not a model change.

---

## 4. It serializes NOTHING — rebuilt on load

**The tally saves nothing.** On game load (and new-game init) it is **rebuilt** from the authoritative loaded
objects, then maintained incrementally by events during play:

- **Derived, not source.** The tally is a pure roll-up of counts that already live in the saved objects
  (`m_paiBuildingCount`, unit counts, …). Persisting it would duplicate the save and risk drift.
- **OOS-safe.** The tally is the authoritative synced gate, so a stale or divergent tally *is* a desync. A
  deterministic rebuild from the loaded objects makes every client's tally match actual state; a *saved* tally
  could drift.
- **No save surface** — nothing to version or `@SAVEBREAK` as the model evolves.

The rebuild is one deterministic scan (clear, then walk the loaded objects and deposit their counts) — used both
to seed on load and to shadow-verify the event-maintained tally against ground truth, so the two paths can't
diverge. **Genuine historical counters** (e.g. "units of type X ever created") are *not* the tally's — they live
on their owning object and are saved there; the tally only reads/rolls them up.

> **The `allowed`-cap exception — lifetime-created, not currently-alive.** A world-unique *unit* cap counts
> **lifetime-created** (a hero "born once, does its thing, then poofs" still consumes its world slot), so that
> one case reads the engine's persisted created-count; everything else (buildings, all other scopes) reads the
> live count. The tally owns the *job* without duplicating the *state*.

---

## 5. Status

Built for **buildings + units** only. The other count domains (tech / civic / religion / bonus / project /
specialist) are not yet wired — a `requires`/`per` atom over those reads 0 until its domain is added (a domain =
one new count-domain value + its emit site + its rebuild-scan contribution + a shadow id). City/plot reads go
direct to the live object regardless.

The tally's `specialist` count domain (counting specialists, e.g. for `per:specialist` scaling) is DISTINCT from
[modifier](modifier.md) §6's `freeSpecialists`/`allowedSpecialists` (which GRANT / CAP specialists — a deposit,
not a count). No conflict — different mechanisms.

---

## See also
- [enabler.md](enabler.md) — the biggest reader: `requires` count-thresholds and the `allowed` cap both resolve
  through this machine at cross-city scopes.
- [modifier.md](modifier.md) — the magnitude sibling; its `per` scaler reads the tally at cross-city scopes. Same
  accumulator substrate, opposite flow direction.
- [json.md](json.md) — the count vocabulary that reads the tally (`min`/`max` atoms §3.4, `per` §3.7, `allowed`
  §4.4). The tally itself is never authored there.
- [naming.md](naming.md) — the `INFOTYPE_NAME` prefix that routes a count to its domain.
