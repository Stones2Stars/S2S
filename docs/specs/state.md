# Unit states — glossary

The catalogue of a unit's **transient states** — fired → counted down → over. This is the **glossary** (the
namings); the **system** is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Greenfield — open by design.** `state` was **never a first-class concept**: it's been faked via
> **pseudo-promotions** and **Python event handlers**, and this glossary formalizes it. Like its sibling
> registries ([json.md §8](json.md)) the member set grows as states are identified from the data — an ongoing
> activity, not a gap to close.

> **⚖ STATUS IS A SCOPE CONCEPT, NOT A UNIT ONE (owner).** A unit is PARALYZED, a PLAYER is in a GOLDEN AGE, a
> CITY is CELEBRATING — all three are the same mechanic: applied, ticking down every turn, over at zero. So each
> scope that carries statuses gets its own enum and the identical store / accessor / tick shape on its owner
> (`Engine/CvStatus.h`), instead of every timer being a hand-named member with its own getter, setter,
> decrement and save field. ⚑ That is the whole value: the legacy engine wrote this mechanic out longhand once
> per timer.
> ⚠ **A status change is not always a bare decrement.** Where crossing zero has CONSEQUENCES — a golden age
> starting cancels anarchy, announces its fact and re-yields — the crossing keeps its side-effect surface; the
> store replaces the hand-named counter, never the crossing logic ([save.md §6](save.md): audit a changer's
> whole body before cutting it).

> ⚖ **A DURATION-1 STATUS IS THE NATURAL SHAPE FOR "WHILE X HOLDS" (owner).** We Love the King/Emperor Day is a
> ONE-TURN status re-applied every turn by a trigger while its conditions match — so it lapses by simply not
> being re-applied, and needs no separate clear. The trigger owns the TEST; the counter owns the ENDING.
> ⛔ **Its legacy trigger wiring STAYS (owner):** re-homing that per-turn condition test is funky, so the status
> owns the storage and the read while the existing code owns deciding whether the conditions match. That is a
> ruled carve-out, not a half-migration to finish opportunistically.

## What a state is (recap)
- **A SPECIFIC COUNTER, DECREMENTED EVERY TURN (owner)** — applied to the unit, ticking down, over at zero.
  Unlike a *mutable* skill (persists until changed) or an *immutable* tag (set at creation).
- ⚑ **The block's name is `status`** ([json.md §8](json.md)); this file is its glossary.
- ⛔ **A status is NOT a skill**, and mis-filing one as a skill is a recurring error the owner has rejected more
  than once: a skill is an ability the unit HAS, a status is a condition something PUT ON it for N turns. The
  curator therefore maps no status tag into `skills` — an unmapped tag reports loudly instead.
- **The read is `count > 0` (owner)** — a status HOLDS while its value is above zero, the ordinary
  `ContextDict` semantic ([contexts.md](../architecture/contexts.md)). Expiry IS the counter reaching 0; there
  is no separate present/absent plane beside it.
- ⚠ **It is id→COUNT like a city's `amenities`, but the COUNT MEANS SOMETHING ELSE** — an amenity's count is a
  refcount of live grantors (moved when events add or repeal one), a status's is TURNS REMAINING and moves on
  its own. Same shape, different model; do not merge the mechanisms.
- Historically NOT a data block — a pseudo-promotion or a Python event stands in for it.

> **⚖ OPEN BY DESIGN — when we find more, we add more (owner).** The `UnitStatus` enum is a HAND-MAINTAINED
> list and identifying new statuses is an ongoing activity for the life of the mod, exactly as it is for
> [tags](tags.md): a new member is a one-line addition, and **more arriving is the normal state, never a gap to
> close**. ⛔ So this glossary is never "incomplete" against a finish line, and the short list is not a backlog.
> ⚑ That nothing but an EVENT applies one today, and that no data authors one, is likewise BY DESIGN — the
> standardization is the deliverable; the empty authoring surface is the model working.

## States

**UNIT** (`UnitStatus`)

| state | meaning | legacy mechanism |
|---|---|---|
| `paralyze` | immobilises the unit for the turn (`setImmobileTimer(1)`) | a promotion granted by an event |
| … | *(to identify)* | pseudo-promotions / Python event handlers |

**CITY** (`CityStatus`)

| state | meaning | legacy mechanism |
|---|---|---|
| `weLoveTheKingDay` | the celebration; a DURATION-1 status re-applied every turn while its conditions hold | its own bool + timer |
| `powerDisabled` | a blackout — the city's power is out for N turns and comes back on its own | `m_iDisabledPowerTimer`, longhand |

**PLAYER** (`PlayerStatus`) — `goldenAge`.

> **⚖ THE STORE IS SERIALIZED; WHAT IS NOT CARRIED IS THE CONVERSION (owner).** Turns-remaining is genuine
> NON-DERIVABLE state — nothing reconstructs *"three turns of blackout left"* from anything else — so it is
> exactly the class [save.md §5](save.md) keeps a serialized store for, and
> [DEC-derived-never-trusted](../architecture/decisions.md#dec-derived-never-trusted) does not reach it: that
> rule bans serializing DERIVED data.
> ⛔ **What is deliberately dropped is the MIGRATION of a legacy timer into the store.** Re-homing one deletes
> its old save field, so an existing save's in-flight value is lost — *"we just don't convert the old statuses
> to the new object for virtually no real gain"*. **The blackout is the worked case:** a save taken mid-blackout
> loads with the power already back on. The old tag is named in `Assets/savemigration.txt` and drains
> ([save.md §3](save.md)).
> ⚑ **The recipe generalizes to every status that follows:** re-home onto the store, name the old tag, take the
> one-time loss. There is no per-status migration to design, and none is worth designing.
> ⚠ Its PLAYER-ALERT ("power restored") died with the per-turn maintainer, as those alerts do — it comes back
> as a CONSUMER of the fact ([event-spine.md](event-spine.md)), and is on the owed list in
> [todo.md](../plans/structural-cleanup/todo.md).

> **⚖ THE CROSSING IS ANNOUNCED, AND THE FACT IS GENERIC OVER THE STATUS.** `CvCity::setStatus` is the ONE write
> path — the per-turn tick and the LOAD both come through it — and it emits `SEVT_CITY_STATUS_ADDED` /
> `_REMOVED` at the 0-crossing only, carrying WHICH status in `iType`.
> ⚠ **The load therefore LANDS through it, never straight into the array.** The store deserializes wholesale, so
> a status written directly into the slot announces nothing and every consumer gating on it reads a holder that
> is not held — the same hole the plot substrate had. ⛔ That id is not the discriminator
> [DEC-facts-name-happenings](../architecture/decisions.md#dec-facts-name-happenings) bans: it names which
> member of an OPEN registry moved, exactly as a religion or property id does, and the direction is in the event
> name. A fact per status would mean an engine change per authored status — the very thing the open registry and
> the no-named-accessor rule exist to avoid.

## Open
- **Identify the faked states** — catalogue everything currently implemented as a pseudo-promotion or a Python
  event handler that is really a transient state, and list each here.
- The **`state` data shape** — the formal model (timer / trigger / effect / expiry).

## See also
- [json.md](json.md) §8 — the system. · [skills.md](skills.md) · [tags.md](tags.md) · [capabilities.md](capabilities.md).
