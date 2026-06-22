# Calc live-parity — ⛔ SUPERSEDED (v1 `dry_calc.py`, pre-v3-rebuild)

> # ⛔⛔ SUPERSEDED / DO NOT TRUST THE NUMBERS — this documents the V1 `dry_calc.py` MONOLITH (owner ruling 2026-06-22).
> This doc predates the **v3 `Tools/ModifierCalc/drycalc/` package rebuild (2026-06-22)** — it references the old
> `dry_calc.py` (the ride-in monolith `calc-emulator-spec.md §2a.2` calls "the v1 mistake", the calc that coined "kraken").
> Its headline claim — "the enabler active-set is validated, ~0.05% divergence" — is a **V1 number and is FALSE for v3.**
> The CURRENT v3 ground truth is the live `python -m drycalc.verify_enabler` harness: **NOT validated** — 28 missed-dormant
> (bar violations, the zero-bar) + 216 over-dormants as of 2026-06-22, tracing to the PESTS/CRIME data-completeness gap.
> **Do not cite this doc's numbers, and do NOT conclude "validated" from it** (that is the exact trap it set — and that
> validity is measured per-mechanic against the zero-bar, NEVER by a percentage — [DEC-per-mechanic-parity](../architecture/decisions.md#dec-per-mechanic-parity)).
> What is STILL accurate below: the engine-grounded **dormancy mechanism** table (`isActiveBuilding`/`isDisabledBuilding`
> + the three `setDisabledBuilding` sources) — independently re-verified against the live engine. Read ONLY that section.
> **TODO: fold the still-accurate mechanism table into `reference/cascade/enabler.md` §8 and RETIRE this doc.**
>
> **Status:** SUPERSEDED working notes (v1)   ·   **Date:** 2026-06-21 (pre-v3-rebuild)
> **Scope (HISTORIC):** live-parity validation of the V1 `Tools/ModifierCalc/dry_calc.py` (the monolith) against the
> running game — NOT the v3 `drycalc/` package.
> **Grounding:** mapped against the engine verdict above and the dormancy mechanism against the named
> `CvCity`/`CvPlayer`/`CvCascadeCondition` functions. Re-confirm a `file:line` before relying on it.

## How the live pass is wired

- **`load_raw_state`** (in `dry_calc.py`) builds each city's `ScopeContext` from
  `Tools/RawStateExtractor/game_state.json` — the extractor dump produced by the DLL emitter in
  `Sources/Tools/CvHttpServer.cpp`. The pure calc functions consume only raw STATE; `load_raw_state` is the
  adapter that fills that state from real facts. The calc COMPUTES every value (zero ride-in; the lone exception
  is `distanceFromCapital`).
- **Ground truth** for values is `/diagnostic/cityInput?player=P&city=C` — the legacy per-channel decomposition
  computed on demand via the mailbox (no log dependence, no `gPlayerLogLevel` gate).
- **Ground truth** for the active-set is the per-city **`dormantBuildings`** field (buildings present but not
  active = `!hasFullyActiveBuilding`) — the engine's own dormancy verdict, used to validate dry's computed set.
- Relevant extractor state fields: `corporations` (`isActiveCorporation`, feeds corp OUTPUT) and
  `presentCorporations` (`isHasCorporation`, feeds the corp PREREQ — a branch can be present-but-inactive);
  per-building `goldBld100`/`resBld100`/`culBld100`/`espBld100` = `100 * getBuildingCommerceByBuilding(eC,b,
  false,false)`, the per-building commerce BASE that reconciles to `bldgCommercePure100`.

## The dormancy / active machine (grounded against the engine)

| function | definition | file:line |
|---|---|---|
| `isActiveBuilding(e)` | `!isDisabledBuilding(e) && hasBuilding(e)` | CvCity.cpp:14375 |
| `hasFullyActiveBuilding(e)` | `!isReligiouslyLimitedBuilding(e) && isActiveBuilding(e)` | CvCity.cpp:14381 |
| `isDisabledBuilding` | membership in `m_vDisabledBuildings` | CvCity.cpp:21363 |

`setDisabledBuilding` (dormancy) is called ONLY by three sources, and dry's `requires.operate` mirrors exactly
these:

- **Replacement** — a present successor disables the predecessor (CvCity.cpp:14468; curated `replaces.buildings`,
  derived from legacy `ReplacementBuildings`).
- **Religion-prereq** — the building's `PrereqReligion` is absent from the city (`applyReligionModifiers`
  ~14999; curated `requires.operate` religion atom).
- **Corp-prereq** — the building's `PrereqCorporation` is not **PRESENT** in the city. The gate is
  `isHasCorporation` (`applyCorporationModifiers` off `setHasCorporation` ~15198/15226), **not**
  `isActiveCorporation` — so dry reads `presentCorporations`, and the corp output set (`corporations`) is
  reserved for output only.

**Build gates are NOT dormancy.** Culture-level (`getPrereqCultureLevel`, CvCity.cpp:2788) and state-religion
(`getPrereqStateReligion` CvPlayer.cpp:6676; `needStateReligionInCity` CvCity.cpp:2604) are checked only in
`canConstruct` — they gate BUILDING (greying, `requires.build`); a built building does not go dormant when the
city later loses them. Civic prereqs are build gates **unless** `bRequiresActiveCivics` is set, in which case
they are dormancy (CvCity.cpp:20882).

**Religiously-limited** (`isReligiouslyLimitedBuilding`) strips a building's **yields AND commerce**
(`processBuilding` ~4509 runs both loops unconditionally), keeping only GP-rate-modifier and religion-influence.
It fires ONLY under `GAMEOPTION_RELIGION_DISABLING` (`checkReligiousDisabling` CvCity.cpp:14935); that option is
OFF in the test save, so it is inert there.

**Obsolescence** (tech-obsolete / `ObsoletesToBuilding`) does NOT feed `isDisabledBuilding`. Obsolete buildings —
**wonders included** — are REMOVED via `hasBuilding` (`changeObsoleteBuildingCount` → `changeHasBuilding(false)`,
CvTeam.cpp ~4660; CvCity.cpp ~22468), so they are already absent from the extractor's `buildings` list. The calc
needs no obsolescence handling.

**`any` conditions are AND-of-OR-groups** (`CvCascadeCondition.cpp` `cascadeEvalCondition`: each OR-group must
contain at least one true leaf, and the groups are AND-ed). So `any:[[A,B,C,D]]` (one group) = "any one of A–D";
`any:[[A],[B]]` (two groups) = A AND B. This is the shape the curators emit (one OR-group per source-prereq).

## Active-set divergence (dry vs engine `dormantBuildings`, all 185 cities)

~0.05% over-drop, **one-directional** (UNDER = 0). The residual is two non-systematic causes:

- **Engine event-driven-dormancy quirk** — the dormancy sources above fire on ADD/REMOVE *events*, not as a
  continuous check, so a corp or religion that left a city can leave a dependent building active (the removal
  event did not re-disable it). dry's continuous check cannot replay that history, so it dormants where the
  engine left the building active. Small, scattered (MOBBY/ZOROASTRIANISM-style buildings).
- **Education property-band pseudobuildings** — an EXPECTED, value-neutral divergence: the curator moved them to
  the cumulative END-STATE (incremental deltas; `enabler.md §3.3`) while the engine still lists the
  replacement-chain bands. Values reconcile (deltas sum to the top band); only the active-set listing differs.

## Methodology note (owner)

Validate the **enabler active-set first**, then values. Micro-tuning a few channels on one empire while the
active-set underneath is unvalidated produces an over-fit that does not generalize — the active-set error flows
into every channel. When pulled toward the structure, do not glide back into value-tweaking.

## Value parity (modifier dryrun) — state (2026-06-21, all verified against the engine)

`/diagnostic/cityInput` emits a **three-way** decomposition per channel: **legacy**, the **in-DLL cascade**
(`cascadeFlat`/`cascadePercent`/`cascade`), and dry computes its own. The combine math is identical on all
three — `(base)×(1+pct) + flat`, `specialist=0` (this mod's `getSpecialistYieldTotal=0`) — verified by
reconstruction (legacy/cascade/dry each reproduce their own realized). So a dry−legacy gap is a **named INPUT
gap**, never a math difference.

Whole-game realized gap (52 cities / 11 empires): **food −5.8%, production −21.4%, commerce −4.5%**, one-
directional under. Production decomposes (London, residual-0 both sides) to:

- **`flat` (the non-building `m_aiExtraYield`)** — `getExtraYield100 = m_buildingExtraYield100 + m_aiExtraYield×100`;
  the non-building part is fed ONLY by its `changeExtraYield` callers: **corporation yield** (`updateCorporationYield`)
  and **`BuildingYieldChange`** (`setBuildingYieldChange`). NOT specialists (that's an old-save comment).
- **`base.plot` −50** — dry's worked-tile yield calc (the cascade rides legacy base, so it has no such gap).
- **`modifier` −30pp** — the 7-way `%` (legacy carries `power 140`, `building 311`…).

**Corp yield — REPRESENTED (dry).** `_corp_yield100` is the yield twin of `_corp_output100`
(`getCorporationYieldByCorporation` CvCity.cpp:12584): `Σ over active corps of ceil((yieldChange×100 + Σ
yieldProduced·getNumBonuses(prereqBonus)·worldCorpMaintPct/100)/100)`, into the flat bucket outside the multiply.
Reads only raw inputs (corp JSON `value` = raw `getYieldProduced`, `bonusCounts`, `corpMaintPct`, active-corp set)
— **no calculated ride-in**. Verified: London CORP_1 = 2 food (matches the engine by hand; fits within the 85-food
non-building flat). **Corps are the only volumetric mechanic** — the `per`-over-bonus-COUNT is the resource-volume
scaling, unique to corps; nothing else scales on resource volume. Effect: food −6.6%→−5.8% (small; corp yields are
small). `isDisorder` (revolt/anarchy → whole-city 0 output) is a city-level gate not yet on the wire — rare.

**Still to represent (production, in size order):** `BuildingYieldChange` (the bulk of London's production flat),
the `−30pp` modifier source, the `base.plot −50` worked-tile calc. Each gets traced to its mechanic and
represented — never skipped to fit ([DEC-represent-dont-fit]).

## Value findings (already modeled or understood)

- **Vicinity/produced-bonus commerce buildings** (PHARMACY, HIGHWAYMANS_HIDEOUT, SMUGGLERS_SHANTY): their
  per-produced-bonus commerce is `YIELD_COMMERCE` (the bonuses are produced, not map-placed — "vicinity" is a
  misnomer; the curated condition fires on the city HAVING the produced bonus). dry counts it in the commerce
  YIELD; a gold "building base" apparent-under was largely legacy routing commerce-yield into gold.
- The per-building base reconciles (`goldBld100` Σ == `bldgCommercePure100`), so attribution is grounded
  per-building.

## See also

- [`../reference/cascade/enabler.md`](../reference/cascade/enabler.md) — the enabler design (generate-then-gate,
  `requires.build` vs `requires.operate`, `any` semantics, the §8 maintainer list).
- [`../reference/cascade/shadow.md`](../reference/cascade/shadow.md) — the shadow/map-before-delete methodology.
- `Tools/ModifierCalc/dry_calc.py` — the calc; its header carries the same enabler summary.
