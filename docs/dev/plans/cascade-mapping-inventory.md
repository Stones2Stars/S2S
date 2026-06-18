# Pre-hard-switch MAPPING INVENTORY — everything we need to understand + observe before the cutover

**Purpose (owner 2026-06-18): the design-table artifact — a living inventory of every system/behaviour we must
MAP (understand + make observable) before the #428/#430 hard switch**, so the demolition (enabler-spec §14) can't
silently break anything. The completeness bar (owner's metric): *"it should be theoretically possible to completely
render the game purely by reading data from the APIs"* — read-only by design (no commands via API, the OOS-safety
guarantee). If a system can't be reconstructed from the API + logs/events, it's an unmapped gap.

Companions: enabler-spec **§14 H** (the state-maintainer demolition list), **cascade-known-discrepancies.md** (the
cascade-vs-legacy divergences), the **http-server** diagnostics (the read surface), and the **event spine** (the
live stream). This doc is the *superset* — the full "what must be mapped" list those feed into.

---

## A. OPAQUE GAMEPLAY SYSTEMS — owner flagged as "no clue how they work" (need investigation + observability FIRST)

These are the systems whose mechanics we do NOT currently understand well enough to map/mimic. Each needs: (1) a
read of the live code to document how it actually works → `docs/dev/reference/`, and (2) observability (logs/events/
API) so its per-turn behaviour is visible. (owner 2026-06-18, "for when we map it all".)

| System | Note | Status |
|---|---|---|
| **Food calculation — WASTAGE especially** | how surplus/consumption/wastage actually compute per city | ❓ unmapped |
| **Espionage** | the whole espionage economy/missions/points | ❓ unmapped |
| **Culture** | equilibrium model is KNOWN (owner helped design it); the rest of culture accrual/borders/flips is not | ◐ partial |
| **Religion spread** | how religion propagates between cities | ❓ unmapped |
| **Corporations — ADVANCED corporations especially** | spread, resource consumption, the advanced-corp rules | ❓ unmapped |

*(This list is a SEED, not exhaustive — add systems as they surface. A system the owner can't explain off-hand is
prima-facie unmapped and a priority for the "render-from-API" bar.)*

## B. STATE MAINTAINERS (from enabler-spec §14 H)

The per-turn/per-event "decide a building's state" quirks (religiously-limited, `checkPropertyBuildings`, autobuild,
SpecialBuilding group, `hasAllReligionsActive`, resource dormancy) — each must be mapped + shadowed before deletion.
See enabler-spec §14 H for the list + the over-reach / map-before-delete rulings.

## C. CASCADE DIVERGENCES (from cascade-known-discrepancies.md)

Every place the cascade shadow differs from the live game, cause-tagged via the `/diagnostic` reason-reporters. See
that doc; the diagnostics make each one a one-query diagnosis.

---

*Process: each entry gets a `docs/dev/reference/` page documenting how it ACTUALLY works (read the code, don't guess
— the trust-but-verify rule) plus an observability hook (gated log + event + API field) so it meets the
render-from-API bar. The hard switch is "ready" on a system only when it is both understood and observable.*
