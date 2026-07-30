# Unit states — glossary

The catalogue of a unit's **transient states** — fired → counted down → over. This is the **glossary** (the
namings); the **system** is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Greenfield — open by design.** `state` was **never a first-class concept**: it's been faked via
> **pseudo-promotions** and **Python event handlers**, and this glossary formalizes it. Like its sibling
> registries ([json.md §8](json.md)) the member set grows as states are identified from the data — an ongoing
> activity, not a gap to close.

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

## States
| state | meaning | legacy mechanism |
|---|---|---|
| `paralyze` | immobilises the unit for the turn (`setImmobileTimer(1)`) | a promotion granted by an event |
| … | *(to identify)* | pseudo-promotions / Python event handlers |

## Open
- **Identify the faked states** — catalogue everything currently implemented as a pseudo-promotion or a Python
  event handler that is really a transient state, and list each here.
- The **`state` data shape** — the formal model (timer / trigger / effect / expiry).

## See also
- [json.md](json.md) §8 — the system. · [skills.md](skills.md) · [tags.md](tags.md) · [capabilities.md](capabilities.md).
