# Tally — formal spec (consolidated 2026-06-16)

**Status: CONSOLIDATION, not new design.** The tally is the third foundational machine of the #428/#430 cascade (the
count sibling of the modifier), but its design was only ever written SCATTERED across `enabler-cascade-spec.md` (§7 count/
threshold/waive, §8 scopes, §13.2/§13.7 the count atoms) + `modifier-cascade-spec.md` (§4 the `per` count-scaler). This doc
gathers it into one place so the engine (#430) builds against it like the other two specs. Anything here that would EXTEND
the design (vs restate it) is flagged ⚑ for an owner call — none is intended.

---

## 1. What the tally IS — the additive COUNT machine (sibling of the modifier)

"How many of X do I have, and where." The tally answers **presence** (`∈ HAS` = `count ≥ 1`) and **counts** (`count ≥/≤ N`)
of any data Type at any scope. It is **the same additive-scope substrate as the modifier, summing a different thing**
(enabler-spec §8): the **modifier** sums effect-MAGNITUDES and flows DOWN the containment spine; the **tally** sums
presence-COUNTS and rolls UP it. One primitive, two instantiations — which is why it's a foundational machine, not a
bolt-on.

- **Scope spine (shared):** `world → team → empire → area → city → plot{…} → building | specialist | unit`.
- **City is the leaf / bottom-out (enabler-spec §8).** Each city holds its own counts; empire/team/world counts are the
  additive roll-up of the per-city reports. So the tally never crosses city isolation — it aggregates reports.

## 2. The readers (one module, many consumers — wanted regardless of the cascade)

1. **Enabler `requires` count-thresholds** — `min(TYPE,N)` / `max(TYPE,N)` at empire/team/world scope read the tally; the
   higher-scope **HAS sets themselves ARE the tally** (enabler-spec §8: "Tally IS the higher-scope HAS"). City-scope `requires`
   reads the local city count directly, NOT the tally.
2. **Enabler `allowed` CAP enforcement** (owner 2026-06-17; enabler-spec §5a) — a build is permitted only while
   `tally.count(X, scope) < allowed`, where X is SELF (self-cap, scope `world`/`team`/`empire`) or a wonder-category counted
   **per city** (`worldWonders`/…). So the declarative `allowed` ceiling is just another tally read; the engine, not the parser,
   does the `count < cap` check (and ignores it under the no-limit game options).
3. **Modifier `per` count-scaler** (modifier-spec §4) — `per:{type,each,scope}` at cross-city scopes (empire/team/world)
   resolves via the tally; `city`/`plot` = the local count. "One module, two readers" (modifier-spec §4).
4. **Demographics / UI / AI / score** — current counts AND lifetime/historical facts (§6). Wanted independent of the
   cascade, which is part of why it's its own module.

## 3. Structure — per-`(type, scope)` count, additive roll-up

A tally entry is a count keyed by `(Type, scope)`. The leaf (city) records its local counts; each higher scope is the
additive sum of its contained scopes (`Σ` cities → empire/team; → world). **Reuses the modifier machinery, multi-level +
additive** (enabler-spec §8) — a "had X" at a city is an additive deposit that rolls up, exactly as a modifier sums down.

## 4. The report mechanism — the HAS-builder feeds the tally (enabler-spec §7/§8)

The tally lives OUTSIDE the per-city HAS-builder. As the HAS-builder gathers each city's HAS (pass 1), it emits a
side-channel report — **"X is had / produced here"** — to the tally; the tally aggregates the empire/team/world totals.
The HAS-builder stays per-city and isolated (it only emits reports); the cross-city aggregate lives in the tally. So
`requires`/`per` at a higher scope consult the tally and **never themselves cross city isolation**.

## 5. The count vocabulary — presence is the N=1 degenerate case

- **Presence ⇒ `min(X,1)`** (enabler-spec §13.7 VOLUMETRIC-READY): author resource/thing presence as the count form with
  N=1, so going volumetric later (resources gain amounts) is a value change, zero model rework. Unifies all count-capable
  kinds under one `min`/`max`.
- **Threshold ⇒ `min(X,N)` / `max(X,N)`**; exact-N = `min ∧ max` (no separate primitive). `min(BUILDING_BARRACKS,12)` =
  West Point; `max(UNIT_COMMANDER,5)` = an instance CAP (`getMaxPlayerInstances` family).
- **Routing by Type PREFIX** (enabler-spec §13.7): the id prefix (`BUILDING_`/`UNIT_`/`UNITGROUP_`/`BONUS_`/…) selects the
  tally bucket — no separate `kind` field; the namespace self-routes. Relies on every Type id carrying its prefix.

## 6. Current-state vs lifetime/historical (enabler-spec §8 "fun-fact boundary")

Because the leaf is the city, the city-level tally is the well-defined home for per-city facts — not only the **current**
counts the enabler reads, but **lifetime/historical** ones ("how many knights has this city produced in its lifetime?",
buildings ever built, …). The enabler reads the *current* counts; demographics/UI read the lifetime facts; the additive
roll-up yields empire/team/world totals for free. **⚑→ persistence RESOLVED (owner 2026-06-17, §9): the tally never
persists anything.** Current counts are REBUILT from the objects on load (§9); a genuine lifetime/historical counter —
one that is NOT recomputable from current state — is therefore **not owned by the tally**: it lives on its owning object
and is saved *there*, with the tally only reading/rolling it up. The remaining detail (how a city stores a current count
alongside any lifetime counter) is an implementation choice pinned at the #430 build; the ownership/persistence question
is settled — nothing unrecoverable ever lives only in the tally.

## 7. GROUNDED — the count-threshold inventory it must serve (sweep 2026-06-14, enabler-spec §13.2)

SIX count-threshold types, **ALL cross-city, ZERO per-city** — empirically validating the separate module (a city's
set-HAS answers none): `PrereqNumOfBuildings` / `getNumCitiesPrereq` / `getUnitLevelPrereq` / `getNumTeamsPrereq` /
`ProjectInfo PrereqProjects/iNeeded` / the CIVIC city-limit (CvPlayer.cpp:6707/6754/6763/6685/6872/8466) + the
**SpecialBuilding group-cap waive** (`specialBuildingsWaived`, enabler-spec §7). These are the consumers the tally exists
to answer; the per-`per`-scaler demand (Corporation per-bonus output, population-scaled deposits) adds the modifier-side
readers.

## 8. BOUNDARY — engine machinery, NOT info data (modifier-spec §0.6)

The tally is **engine machinery** — it is never authored in the JSON. The info data carries only the **clauses that READ
it**: a `requires` count atom (`{type, scope, min/max}`, enabler-spec §6.1 — full/explicit/self-describing) and a modifier
`per` scaler (`{type, each, scope}`). The tally itself — the report side-channel, the additive roll-up, the current/lifetime
storage — is built at #430 (the additive-scope substrate the modifier shares). It is also the "we want it anyway" module
(demographics/AI/score), so it is built once and read three ways.

---

## 9. Save handling — REBUILT on load, never serialized (owner ruling 2026-06-17, LOCKED)

The tally serializes **nothing**. On game load (and new-game init) it is **rebuilt** from the authoritative loaded
objects, then maintained incrementally by DOMAIN events during play.

**Why rebuilt, not saved:**

- **Derived, not source.** The tally is a pure additive roll-up of counts that already live in the saved objects
  (per-city buildings, `CvPlayer` counts, units). It holds no source-of-truth state; persisting it would duplicate what
  the save already has.
- **OOS / determinism.** The tally is the authoritative synced gate (`requires` / `allowed` / `per` read it), so a stale
  or divergent tally **is** a desync. A deterministic rebuild from the loaded objects makes every client's tally match
  actual state. A *saved* tally could drift from the objects (version skew, out-of-band mutation) — the exact failure the
  DOMAIN/DIAGNOSTIC KIND firewall exists to prevent (`event-spine-spec.md`).
- **Save-format hygiene.** Not serializing = no save surface, no versioning, no `@SAVEBREAK`, no migration as the tally
  shape evolves (and it is still evolving through #428/#430).
- **Authoritative ≠ persisted.** Authoritative at *runtime* (the gates read it, not `m_pai*Count` post-cutover), but its
  contents are fully recomputable from the base objects — authoritative like a *computed index*, not source state. (The
  engine serializes `m_paiBuildingCount`; the tally need not — it rebuilds from that saved per-player state.)

**The HOW — one deterministic scan, used two ways.** During play the tally rides DOMAIN events (deltas:
`changeBuildingCount` → `deposit(±1)`). On load there are no deltas — objects deserialize fully-formed — so the seed is
the **bulk** form: **clear the tally, then scan the loaded objects and deposit their current counts.** That bulk scan is
exactly the recompute the slice-1 self-test already performs (`CvCascadeSelfTest`: walk the objects, compute the count),
with `deposit()` where the self-test does its compare. So there is **one** "enumerate this domain's counts from the
objects" routine per tracked domain, used for BOTH:

1. **Load-seed** → deposit into the cleared tally.
2. **Shadow oracle** → compare against the event-maintained tally to catch drift (the self-test's current job).

One routine = one definition of "what counts" = the load-seed and the live event path **cannot diverge**.

**Mechanics:**

- **Granularity = the city leaf (§1).** The scan deposits per city (each city pushes its buildings / specialists / units);
  the additive roll-up yields empire/team/world for free. (This is the "every loaded object's count gets pushed in".)
- **Per-domain, co-located.** Each tracked domain (buildings, units, techs, …) owns its incremental event hook AND its
  seed-scan contribution side by side — adding a domain adds both in one place, so they can't silently drift apart.
- **Idempotent + deterministic.** `rebuild()` = CLEAR-then-repopulate (so a second load in the same EXE session can't
  double-count — cf. the known load-after-load stale-state issue), and identical on every client (OOS-safe).
- **Hook.** Runs after object deserialization completes and before the first gate read; new-game init runs the same seed
  over the starting objects. Events are **never** replayed on load (there are none — the loaded objects ARE the state).
  Exact hook site pinned at the #430 build.

Same stance as the derived-data repository (rebuild/lazy, never foundational-persisted —
`derived-data-repository.md`) and the project's save-load principle that **derived/repository state serializes nothing**.
(Save-format *mechanics* — the name-keyed tagged wrapper, soft-vs-hard changes, `@SAVEBREAK` discipline — are in
[`../reference/save-load-format.md`](../reference/save-load-format.md) [DRAFT, branch-only until the cascade save
handling settles].)

---

*Build note (#430, cascade-engine-430.md §1.1): the tally is built FIRST of the three machines — the enabler depends on it
(higher-scope HAS + count thresholds) and the modifier's cross-city `per` reads it. It and the shared scope-accumulator
substrate are the dependency root.*
