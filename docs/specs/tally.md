# The tally — "how many?"

The cascade machine that answers **"how many of X do I have?"** — presence (`≥ 1`) and thresholds (`≥ / ≤ N`) of
any [Type](naming.md) at any scope. It is the **count sibling of the [modifier](modifier.md)** over the same scope
spine — where the modifier flows **DOWN** (summed deposits), the tally rolls **UP** (counts). But unlike the
modifier it is **not a store**: the game objects already own their counts, so the tally is the standardized
*accessor* that reads them and rolls up (§1).

It is **engine machinery, never authored in JSON**. The JSON only carries the clauses that *read* it — an
[enabler](enabler.md) `requires` count-atom, an [enabler](enabler.md) `allowed` cap, a [modifier](modifier.md)
`per` scaler. This doc is that machine.

---

## 1. NOT a store — a standardized aggregate ACCESSOR over object-owned counts

⛔ **The tally stores nothing and accumulates nothing.** The game OBJECTS already own and
maintain their own counts, O(1): `CvPlayer::getBuildingCount` is `m_paiBuildingCount[i]`, maintained incrementally by
`changeBuildingCount` (which **already emits** the `DOMAIN` count event); `getUnitCount` likewise; techs on `CvTeam`.
A second accumulator that re-summed `±1` per building would **duplicate authoritative state**, risk OOS drift, and
make any cascade-vs-legacy count shadow **tautological** (the same number twice — exactly the false-confirmation trap,
[validation.md](validation.md)).

So the tally is the **one standardized, predictable surface** for "how many of TYPE at SCOPE": it **reads** the
object-owned count and **rolls it UP** the scope spine. Its whole value is that — a single access point with the
per-domain object accessor + the roll-up in **one place**, collapsing the scattered `getNum*` count loops
(collapsing the scattered `getNum*` count loops into one surface) — **not** a re-store of data the
objects already hold ("creating something new when we already have it is pointless").

Reading the object-owned count is reading a **raw INPUT** (saved presence), never a computed output, so it is **not**
the pollution anti-pattern ([validation.md](validation.md)). The read is authoritative and exact (the object is the
source), which is precisely what the [enabler](enabler.md) needs to gate buildability.

---

## 2. Counts roll UP; the OBJECT owns the count

Counts **originate per city** (a building is built *in* a city), and the engine already **rolls them up to the
player** — `CvPlayer::getBuildingCount` IS the empire aggregate, maintained as cities gain/lose the building. The
tally reads that and supplies the higher roll-ups:

- **empire** read → the player's own object aggregate (`getBuildingCount`/`getUnitCount`), O(1).
- **team** / **world** read → **summed over the relevant alive players on read** (no stored team/world total).
- **city** / **plot** reads do **not** go through the tally at all — they read the live `CvCity`/`CvPlot`
  directly (a local count needs no roll-up).

There is **no tally-owned store at any leaf** — empire is the object's own aggregate; team/world are summed on read.
If a domain has no object-side aggregate yet, the fix is to give the **OBJECT** one (it "cares about itself"), never
to add a tally side-store.

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

## 4. It serializes NOTHING — and maintains nothing (it reads)

**The tally saves nothing and stores nothing.** It reads the object-owned counts on demand, so there is **no
load-time seed, no incremental maintenance, no rebuild, and no shadow**:

- **Derived, not source.** The count already lives in the saved object (`m_paiBuildingCount`, unit counts, …). The
  tally is a pure read + roll-up; duplicating it would only risk drift.
- **OOS-safe by construction.** With no separate store there is nothing that *can* diverge — the tally IS the
  object's count. (A tally-owned duplicate store is a retired idea — [superseded-ideas](../architecture/superseded-ideas.md).)
- **No save surface, no rebuild, no shadow** — nothing to version, `@SAVEBREAK`, seed, or shadow-verify.

> **The standardized event EMITTERS stay; the in-engine tally just doesn't consume them.**
> The objects emit `DOMAIN` count events on change (already wired: `CvPlayer::changeBuildingCount` / unit-count →
> `eventSpine().emit`). Those serve **observability** (the Orwell bar), **cache-invalidation** (the
> modifier/enabler mark triggers), and the **out-of-process replay** — a consumer with no engine objects rebuilds
> its model from the event stream + engine state ([event-spine.md](event-spine.md) KIND table). The **in-engine** tally
> needs none of it: it reads the live
> objects ("let an object care about itself, and standardize the accessors + event emitters"). **Genuine historical counters** (e.g.
> "units of type X ever created") are *not* the tally's — they live on their owning object and are saved there; the
> tally only reads/rolls them up.

> **The `allowed`-cap exception — lifetime-created, not currently-alive.** A world-unique *unit* cap counts
> **lifetime-created** (a hero "born once, does its thing, then poofs" still consumes its world slot), so that
> one case reads the engine's persisted created-count; everything else (buildings, all other scopes) reads the
> live count. The tally owns the *job* without duplicating the *state*.

---

## 5. Status

The standardized accessor reads **buildings + units + techs + specialists + unit-tags** at **empire / team /
world**. Buildings/units read the player's own O(1) aggregate at empire and sum over alive players above it.

**TECHS are the worked case for "give the OBJECT the aggregate, then READ it".** A tech is TEAM-held, so its
count is over TEAMS, never players: **world** reads the engine's existing `CvGame::countKnownTechNumTeams`
(ever-alive teams holding it — techs are monotonic, so held == ever-held), and **team/empire** are the asking
side's held FLAG (0 or 1), empire resolving through its player's team. ⚑ Nothing new was stored to wire it: the
aggregate already existed on `CvGame` and the tally simply reads it — which is the whole read-not-store rule
working, and why "the domain is not wired" is never the same statement as "no counter exists". ⛔ Check for the
engine's own aggregate BEFORE concluding a domain needs building.

The remaining count domains (civic / religion / bonus / project) are not yet wired — a `requires`/`per` atom over
those reads 0 until its domain is added. *(Cross-scope RELIGION_X already answers from `countReligionLevels` at
the evaluator, ahead of a tally domain of its own.)* Under the read-not-store model a **domain is now
just: the object-side accessor it reads + its type-prefix routing + the roll-up** — **no** side-store, emit-driven
maintenance, rebuild scan, or shadow id (those were the duplicate-store model's burden). Where an object lacks the
aggregate, give the object the accessor (it "cares about itself"). City/plot reads go direct to the live object regardless.

**The tally READS the object-owned count; it never re-stores it.** The object already maintains its own count O(1)
(`getBuildingCount`, `getUnitCount`, …), so the tally's job is to be the standardized accessor over that count and
its roll-up — a second store would only risk drift. That read-not-store invariant is the design; it is **not** a
licence to leave a count LOGIC bespoke. Counting is the tally's job ([north-star.md](../architecture/north-star.md)
EACH IS ITS OWN SYSTEM), so a bespoke engine count-loop a cascade/enabler consumer needs — `CvPlayer::countNumBuildings`
(the cities-having, ≤1/city semantic), `CvTeam::getHasReligionCount`/`getHasCorporationCount` — is an **UNWIRED
TALLY DOMAIN (open), not a KEEP**: it goes through the tally, which reads the object's own count. Where the object
lacks the aggregate the read needs, give the OBJECT the aggregate (it "cares about itself"); the tally never grows
a side-store to compensate.

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
