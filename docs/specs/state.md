# Unit states — glossary

The catalogue of a unit's **transient states** — fired → counted down → over. This is the **glossary** (the
namings); the **system** is the [json spec](json.md) §8. Sibling of [skills.md](skills.md).

> **Greenfield — open by design.** `state` was **never a first-class concept**: it's been faked via
> **pseudo-promotions** and **Python event handlers**, and this glossary formalizes it. Like its sibling
> registries ([json.md §8](json.md)) the member set grows as states are identified from the data — an ongoing
> activity, not a gap to close.

## What a state is (recap)
- **Transient** — fired, then **counts down and is over** (a timer) — unlike a *mutable* skill (persists until
  changed) or an *immutable* tag (set at creation).
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
