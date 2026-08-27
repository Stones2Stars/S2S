# Unit lifecycle — birth, the death sequence, and why deletion is delayed

> How a `CvUnit` comes into existence, how it stops existing, and the constraints the death path is built
> around. Engine behaviour as it is today. The death fact's place on the spine is
> [spine.md](../spine.md); this page is the mechanism the fact comes from.

## The object

A unit is an entry in its owner's `FFreeListTrashArray` (`CvPlayer::m_units`), reached by id
(`CvPlayer::getUnit`) and iterated by `CvPlayer::units()`. Its map position is **not a pointer** — it is the
`m_iX` / `m_iY` pair, and `CvUnit::plot()` resolves them through `CvMap::plotSorenINLINE` on every call.

⛔ **`plot()` reports NULL for exactly one coordinate pair: `INVALID_PLOT_COORD`.** Any other out-of-range
coordinate (a stored `-1`) resolves to a WRONG plot rather than to NULL. A check for "is this unit on the map"
must therefore test the coordinate range too, never `plot() != NULL` alone.

Two independent lists hold a unit besides the owner's array — the **plot's** unit list and the **selection
group's** unit list. Leaving the map (`setXY`) removes it from the first; leaving the group (`joinGroup(NULL)`)
removes it from the second. Neither removes it from the owner's array; only `CvPlayer::deleteUnit` does, and
that is what frees the object.

## Birth

`CvPlayer::initUnit` → `CvUnit::init`, which emits `SEVT_UNIT_CREATED`. A LOADED unit never runs `init`, so
`CvUnit::read` emits the same fact from inside the save read (the reseed, [the load reseed](../spine/05-the-load-reseed.md#5-the-load-reseed)).

## The death sequence — one job per operation

Five operations in `CvUnit`, each with exactly one job. **The names are load-bearing: only `die()` kills.**

| operation | its ONE job |
|---|---|
| **`kill(bDelay, ePlayer, bMessaged)`** | the dispatcher every caller uses; owns the recursion brake and nothing else |
| **`killUnconditional(...)`** | the same, minus the brake — what the delayed-death pass calls to reap an already-scheduled death |
| **`scheduleDeath(...)`** | every effect of the KILLING BLOW, before the outcome is known. Never kills |
| **`resolveScheduledDeath()`** | the reaper: asks the survival question once, dispatches to exactly one outcome |
| **`evacuateToCapital(city)`** / **`surviveLastStand()`** | the two NON-death outcomes — a relocate + damage set, and a damage set |
| **`die()`** | the ONE terminal: announce, detach, decrement, delete |

`die()` carries **no early return and no conditional deletion**. `emitUnitKilled` is its first line and
`CvPlayer::deleteUnit` its last, so `SEVT_UNIT_KILLED` is true **by construction** rather than by being placed
past a run of survival branches. The `if (plot != NULL)` guards inside it govern what the unit is DETACHED
FROM, never whether it dies.

⛔ **A new outcome that leaves a unit alive is a branch in `resolveScheduledDeath`, never an early return in
`die()`.** That is the whole point of the split: a function named for killing only ever kills.

### What `scheduleDeath` runs — and for units that survive

`scheduleDeath` runs before the outcome is decided, so its effects land on units that go on to live:
deselection, the worker plot-claim release, the cargo's fate (each cargo unit rolls to escape to an adjacent
plot or drowns — a recursive kill), `CvEventReporter::unitKilled` to Python, the global "a leader was killed"
broadcast, and clearing the worker's city assignment. **This is deliberate: they are consequences of the
killing BLOW, not of the death.**

### The two survival outcomes

Both are asked only of a unit still on the map, and both are nested under the owner's **capital city** — the
evacuation needs it as a destination, and the survivor roll shares that nesting, so a player without a capital
has neither available.

- **`evacuateToCapital`** — `isCanRespawn()` and the unit is not already at the capital: teleport there, set
  damage to 90%, spend an `oneUpCount`.
- **`surviveLastStand`** — `isSurvivor()`: leave the unit one hit from death and clear the survivor flag (it
  applies to THIS combat only; the unit can be attacked again the same turn).

Neither emits a death fact — nothing died. Both clear the death schedule.

## Delayed death — the schedule, and why DELETION is the point

`m_bDeathDelay` is a **serialized** boolean meaning *"this unit's death is decided but not yet performed."*
`isDelayedDeath()` returns it, and `isDead()` is `isDelayedDeath() || getDamage() >= getMaxHP()` — so a
scheduled unit reads as dead everywhere while still being a live object.

⛔ **`isDead()` is `DllExport`: the closed Firaxis `.exe` calls it.** Its semantics, and the member's name and
type, are fixed.

**Delayed DELETION is the real constraint.** Combat resolution holds raw `CvUnit*` pointers across the whole
exchange (attacker, defender, the animation/entity layer), so a unit that dies mid-combat must not be freed
until the exchange is over. `kill(true, …)` therefore performs the pre-death bookkeeping, sets the schedule,
and returns with the object intact. The reaper runs later:

- `CvSelectionGroup::doDelayedDeath` walks its units and calls `CvUnit::doDelayedDeath` on each whose death is
  scheduled; that calls `killUnconditional(false, …)` **only when `!isInBattle()`**. A unit still in battle is
  simply skipped and reaped on a later pass.
- The group-level pass runs from `CvPlayer::doTurn` and from the group's own mission/activity choke points.

**Consequence for callers:** ~8 call sites dereference a unit after `kill` returns, and are correct only
because they pass `bDelay = true`. Two shapes: an explicit follow-up read
(`CvPlayer.cpp` golden-age `pBestUnit->plot()` / `->getGroup()` after the kill; `CvMap::moveUnitToMap`'s
`TravelingUnit` record), and **iterator validity** — `CvPlot.cpp`'s nuke and terrain-change loops call
`kill(true)` from inside `foreach_(… units())`, and the unit staying in the plot's list is what keeps the
iterator valid.

### The recursion brake

Both survival outcomes call `setDamage` while the death is still scheduled. `setDamage` ends with
`if (isDead()) kill(true, …)`, and `isDelayedDeath()` alone makes `isDead()` true — so that is an immediate
re-entry. **`CvUnit::kill`'s `if (bDelay && m_bDeathDelay) return;` is what absorbs it.** It is load-bearing,
not defensive. `killUnconditional` deliberately skips it: reaping a death that is already on the books is
exactly what it is for.

## The OFF-MAP unit

A unit whose `plot()` is NULL is a real state, not only a save defect:

- **The death path's own window.** In `die()`, `setXY(INVALID_PLOT_COORD, …)` severs the plot link and
  `CvPlayer::deleteUnit` runs several statements later. In between, the unit is **live but off-map** — off the
  plot's unit list, `plot()` NULL, yet still yielded by `CvPlayer::units()`. The capture block inside that
  window calls Python (`unitCaptured`) and can run a nested kill, so re-entry is reachable, not theoretical.
- **Python.** Every Python kill passes `bDelay = False`, and several iterate `player.units()` rather than a
  plot's units (`CvEventManager.onBuildingBuilt`'s nanite defuser, `CvRandomEventInterface.doNuclearProtest1`,
  the WorldBuilder delete-by-id screens) — so they can reach an off-map unit directly.
- **`m_pTempUnit`**, the per-player pathing anchor, is permanently off-map. It is EXCLUDED from
  `CvPlayer::units()` iteration and from every death sweep; it is not a unit in the game.
- **Saves** occasionally contain units with no plot or with out-of-range coordinates.

**An off-map death is a real outcome, not a skipped one.** `scheduleDeath` never defers it — the delayed-death
pass reaps through the selection groups standing on the map, so a deferral it cannot see would strand the unit
alive forever — and `die()` deletes the unit whether or not it held a plot. **One mechanism removes a unit,
and it is `die()`.** `CvPlayer::read`'s save repair therefore forces the invalid coordinates (so the unit is
genuinely off-map rather than pointing at a wrong plot) and calls `kill`; it does not delete anything itself.

## ⚠ Re-entrancy — the single-threaded "race"

The death path re-enters itself by several routes, on ONE call stack, against a half-updated object. Known
routes, all live:

- `setDamage` → `kill(true)` (braked, see above).
- `scheduleDeath`'s cargo loop → `unitX->kill(bDelay, …)` — a nested full death sequence per cargo unit.
- `die()`'s capture block → `pkCapturedUnit->kill(false, …)` and the Python `unitCaptured` handler.

⚠ **The cargo loop iterates `pPlot->units()` — the unmutated view — while its nested kills can delete units
from that very list** when `bDelay` is false (which is every Python kill, `doDelayedDeath`'s reap, and the
flanking kill). `units_safe()` is the snapshot variant used elsewhere for exactly this
(`CvUnit::setXY`, `CvSelectionGroup::doDelayedDeath`). This is a known hazard in the cargo path, not a
statement that it is safe.

## See also
- [spine.md](../spine.md) — `SEVT_UNIT_KILLED` / `SEVT_UNIT_DEATH_SCHEDULE_ADDED / _REMOVED` and the reseed.
- [engine.md](engine.md) — the save-load and pointer-lifetime constraints the toolchain imposes.
