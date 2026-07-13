# Enabler → event-fed (assessment + plan)

> **Task (owner 2026-07-13):** "ensure the enabler only gets data from the events — it really really does not now."
> This maps how the enabler sources data today, what "event-fed" requires, and the design forks that need an owner
> call before the rearchitecture. Companion to the modifier-side event work ([f0-eventspine-invalidation.md](f0-eventspine-invalidation.md),
> [scope-packages.md](scope-packages.md)) — the enabler is the SAME event-driven model applied to the "can I?"
> machine. Naming per [DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade)
> (`EnablerKernel`/`BuildingEnabler`/`UnitEnabler`/`TechEnabler`, files `CvEnablerKernel`/`Cv*Enabler`).

## How the enabler sources data TODAY (it reads live state; events only TRIGGER re-reads)

- **GENERATE reads live HAVE.** `EnablerKernel::generate` (`CvEnablerKernel.h:60`): *"HAVE = team techs + adopted
  civics (+ the city's buildings)"* — pulled straight off `GET_TEAM`/`GET_PLAYER`/`CvCity`, not an event-maintained set.
- **The gate reads live predicates.** `requiresMet`/`gateSet` evaluate `requires` through the shared condition
  evaluator (`cascadeEvalCondition`, `CvCascadeConditionEval` — used by BOTH machines), whose predicates
  (`hasTech`/`hasBonus`/`isCapital`/`STATE_RELIGION_IN_CITY`/…) are **live game-state queries**.
- **The operating-building set re-reads live state.** `recomputeOperatingBuildingsInto` reads `city.getHasBuildings`
  each recompute; the targeted `on*Active` hooks are event-*triggered* but each **re-reads** live state to update the set.
- **Counts read objects.** `allowedOk` + count atoms read the tally, which by design **reads the object-owned counts**
  ([DEC-tally-serializes-nothing](../../architecture/decisions.md#dec-tally-serializes-nothing)) — not events.

So: events currently *trigger recomputes that re-read the game objects*. The enabler's DATA is the live game state.

## What "event-fed" means for the enabler (the target)

The enabler's derived sets — **HAVE**, the **operating-building set**, the **frontier** — built by the **reseed on
load** (the in-read DOMAIN events populate them, [DEC-spine-reseed](../../architecture/decisions.md#dec-spine-reseed))
and **maintained incrementally by play-time events** (building built → HAVE.buildings += ; tech researched →
HAVE.techs += ; bonus network shift → operate re-check), **never re-reading live game objects and never a per-turn
blanket re-check**. Exactly the modifier caches' model, applied to the availability machine.

## The gaps + design forks (need an owner call — do NOT guess these)

1. **⚠ The orphaned dynamic re-check (a REGRESSION from the self-heal removal, fix first).** `scanCondDeps`'s
   `bMarkDynamic` marks bonus/vicinity/connectivity operate-atoms as *"live state no event carries"* and routes their
   buildings to `onSliceRebuildActive` — the **bounded per-turn re-check**. That was called ONLY from
   `playerSliceRebuild`, which is now deleted → **`onSliceRebuildActive` has no caller**, so those dynamic operate
   conditions never re-check. This is almost certainly part of "canConstruct lost requires." Event-feeding replaces
   the poll with events — but until it does, that state is stale. **Options:** (a) the bonus-network events already
   wired (`SEVT_PLOTGROUP_BONUS_CHANGED`/`SEVT_CITY_NETWORK_CHANGED`, F0 R4) carry the connectivity axis → route them
   into the operate re-check and the poll is mostly unneeded; (b) the **vicinity (radius)** axis still has "no discrete
   event" (the pending city plot-gain/loss hook) — either emit that event (culture-expansion / radius-change) or keep
   a scoped re-check for it alone.

2. **HAVE from events vs. the tally-reads-objects rule.** The tally deliberately does NOT store — it reads object
   counts. Does the enabler maintain its OWN event-fed HAVE set (presence, not counts), or keep reading object HAVE
   like the tally? Presence and counts are different questions; an event-fed HAVE presence-set can coexist with the
   read-only tally, but this is a model call. **Owner fork.**

3. **The shared condition evaluator.** `cascadeEvalCondition` reads live predicates and is shared with the modifier.
   Event-feeding the enabler means its eval context reads event-maintained state instead of `GET_TEAM.isHasTech` — but
   the evaluator is ONE implementation ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
   Does the eval ctx get pointed at the event-fed HAVE set, or do raw-state predicate reads stay (they're raw saved
   state, not computed outputs — arguably legitimate live reads, like the tally)? **Owner fork — this decides how deep
   the change cuts.**

4. **The load build (reseed).** The enabler's sets currently warm via `buildFrontierIndices` + ensure-on-read at
   `onFinalInitialized` (still present — I did NOT remove those). Event-feeding means the reseed's in-read events
   populate them instead. This is the enabler half of the same reseed the modifier side needs (both consume the same
   in-read DOMAIN events).

## Proposed sequence (for the when-back session)

0. **Decide forks 2 + 3** (HAVE-set model; evaluator event-ctx depth) — everything else follows from these.
1. **Close the orphaned dynamic re-check** (fork 1): route the wired bonus-network events into the operate re-check;
   decide the vicinity-axis event vs. a scoped re-check. This also repairs the canConstruct-requires regression.
2. **Event-maintain the operating-building set incrementally** (add/remove on building/have events, no re-read) —
   the frontier-perf targeted-propagation completion.
3. **Reseed the enabler sets** from the in-read events (shared with the modifier reseed).
4. **Point the eval ctx at the event-fed HAVE** per fork 3.
5. Then the when-back breaking step (canConstruct/canResearch read ONLY the enabler) stands on an event-correct enabler.

## Status

Rename to "the enabler" — **DONE + Assert-green** (this session). The event-feeding rearchitecture — **mapped, not
started**: it has the 3 owner-forks above and touches the shared evaluator, so it is surfaced for a decision rather
than skated blind. The one clear regression (orphaned `onSliceRebuildActive`) is documented as step 1.
