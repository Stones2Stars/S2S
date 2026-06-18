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

## B. STATE MAINTAINERS (enabler-spec §14 H) — investigation 2026-06-18

The per-turn/per-event "decide a building's state" quirks. The buildability sweep does NOT exercise these (they act on
already-built/auto-placed things) → each needs its OWN behaviour shadow before deletion. Grouped by what they DO, with
current `file:line`, the cascade replacement, and the shadow the cutover needs. (Over-reach bias + map-before-delete: §14 H.)

### B-i. AUTO-PLACEMENT — per-turn `changeHasBuilding(true/false)` that MUTATES the building set (the riskiest)
Both run in `CvCity::doTurn`'s building-maintenance block (~CvCity.cpp:1455-1488) and both call the to-be-replaced `canConstruct`.
- **Autobuild loop** (CvCity.cpp:1459-1487) — over `BuildingsRepo::get().autoBuildings()`: if absent + `canConstruct(…,bIgnoreCost)`
  → ADD (+"auto-build" message); if present + non-wonder + a `PrereqNumOfBuildings` modifier dropped below threshold → REMOVE
  (the adopted-cultures rule; wonders `getMaxGlobalInstances()==-1` exempt from removal). → cascade **`autoBuild` placement marker
  + `requires`** (place when `requires` hold, drop when they go false; the owner's "autoBuild ≡ enables/requires slots" ruling).
- **`checkPropertyBuildings`** (CvCity.cpp:1490-1518, non-NPC only, called 1457) — for each PropertyType × each `PropertyBuilding`
  `{eBuilding,[iMinValue,iMaxValue]}`: ADD when the city's property value is IN-band + `canConstruct`, REMOVE when out-of-band or
  not constructible. These are the `BUILDING_EFFECT_*` "really effects, not buildings". → cascade **`requires.operate`
  property-in-band dormancy (the §3 PropertyEffect reverse-enabler; data-model §4.2b)** + autoBuild placement; formalizes OUT to
  `PropertyEffect`/`BaseEffect` (#429-adjacent).
- **SHADOW:** per city per turn, does the cascade's placed/removed set EQUAL these loops' `changeHasBuilding` actions? (presence diff)

### B-ii. DORMANCY — a BUILT building goes inactive-but-PRESENT when a condition fails
- **Religiously-limited** — `isReligiouslyLimitedBuilding` / `m_pabReligiouslyDisabledBuilding` / `setReligiouslyLimitedBuilding`
  (CvCity.cpp:21279-21319; the 14940-ish trigger), with the `hasAllReligionsActive` exemption; reversible `processBuilding ∓1`.
  → `requires.operate` (`STATE_RELIGION`/`STATE_RELIGION_IN_CITY` + a `hasAllReligionsActive` waiver clause). **B1 in
  known-discrepancies: MATCH verified 2026-06-18** (currently moot — no civic sets AllReligionsActive).
- **Resource dormancy** — `isActiveBuilding` (CvCity.cpp:14364) folding `PrereqBonuses` / `isDisabledBuilding` (`setDisabledBuilding`
  21239-21269) → `requires.operate` resource dormancy. The `setDisabledBuilding` replace/re-enable chain (§14 F) is the imperative
  twin to derive away.
- **SHADOW:** per city per turn, does the cascade's active/dormant set EQUAL `isActiveBuilding`?

### B-iii. GROUP GATE — `isSpecialBuildingNotRequired` (CvPlayer.cpp:13927; civic-driven count 18239)
The SpecialBuilding group cap/tech/obsolete/waiver. → uniform group-gate inheritance (data-model §7, the building-group deliverable;
the cap half + TechPrereq/ObsoleteTech inheritance already shipped this session). **SHADOW:** group membership active/waived parity.

### B-iv. WAIVER — `hasAllReligionsActive` (CvPlayer.cpp:30299; civic `isAllReligionsActive`)
The religion-exemption → a DECLARED `requires` waiver clause, not a buried `if`. Currently moot (no civic sets it) but must be a
defined fact pre-switch. Folds into B-ii's religion dormancy.

**⚠ The shadows above are the honest scope of "map every current behaviour" — a runtime twin of the buildability sweep, NOT yet built.**

## C. CASCADE DIVERGENCES (from cascade-known-discrepancies.md)

Every place the cascade shadow differs from the live game, cause-tagged via the `/diagnostic` reason-reporters. See
that doc; the diagnostics make each one a one-query diagnosis.

---

*Process: each entry gets a `docs/dev/reference/` page documenting how it ACTUALLY works (read the code, don't guess
— the trust-but-verify rule) plus an observability hook (gated log + event + API field) so it meets the
render-from-API bar. The hard switch is "ready" on a system only when it is both understood and observable.*
