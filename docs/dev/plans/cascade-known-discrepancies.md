# Cascade — KNOWN DISCREPANCIES (shadow vs. live game)

**Purpose (owner 2026-06-18): a living list of every place we KNOW the #430 cascade shadow differs from the live
(legacy) game** — so each is a deliberate DECISION, never a surprise at cutover. For each: decide to **CHANGE** (the
cascade's behaviour is the one we want — accept/keep the divergence as a correction) or **ALIGN** (the cascade must
match legacy — fix it, at latest by the hard switch). Nothing here is "discovered late": it's tracked the moment we
see it.

This is the companion to two other artifacts:

- the **`/diagnostic/sweep`** (buildability map: cascade verdict vs `canConstruct`/`canTrain`) — its *open* divergences
  feed §A below;
- the **§14 H state-maintainer demolition list** (enabler-cascade-spec) — the runtime behaviours the sweep can't see
  feed §B.

**Status legend:** ✅ verified-MATCH (no divergence, recorded so we don't re-investigate) · ⚠ KNOWN-GAP (real
divergence, disposition set) · ❓ UNDIAGNOSED (sweep shows it, cause not yet pinned) · 🔭 UNSHADOWED (no tool exercises
it yet).

---

## A. Buildability divergences (from the sweep — `type=buildings&player=0`, 2026-06-18: 48 diverge = 38 over + 10 under)

| # | What | Status | Disposition |
|---|---|---|---|
| A1 | **OVER (25): obsolete/replace-chain buildings.** The sweep's cascade verdict (CvHttpServer.cpp:527-530) checks obsoletion but **NOT `replaces`** — a building whose successor exists isn't subtracted from CAN GET. AND legacy's *effective* obsolete-tech can differ from the XML `ObsoleteTech` the cascade reads (canonical: `COAL_PLANT` — legacy runtime `TECH_ARMORED_CAVALRY` ≠ XML `TECH_FUSION`; a module-merge / replace-chain quirk). Reclamation plants, subdued-animal/herd builds, burial traditions, `COAL_PLANT`/`BUNKER`, … | ⚠ DIAGNOSED | ALIGN — add a `replaces` subtraction to the cascade verdict + reconcile obsolete runtime-vs-XML (data quirk; may be a real base-data bug) |
| A2 | **OVER (12): cause NOT pinned — needs LIVE per-clause diagnosis (game went down mid-dig).** RULED OUT (static): civic (cascade `requires.operate` exactly matches `hasValidCivics`, CvPlayer.cpp:26218); map-category (`MAPCATEGORY_EARTH` on 84% of buildings — not distinguishing on an Earth map); `MaxStartEra` (none); **and the special-building-waiver mismatch I first suspected — DISPROVEN: NONE of the A2 prereqs are special-building-group members, so that waiver never fires, and the obsolete-prereq waiver MATCHES legacy (CvCity.cpp:2858).** 6 of the 12 (`GARDEN_CITY`/`EXILE_PRACTICES`/`SHRINE`/`INTERROGATION_BUILDING`/`GRAND_VILLA`/`JUNKYARD`) have NO in-city prereqs at all → no prereq angle can explain them. **One real-but-LATENT difference noted (not the current cause):** the cascade building atom tests `hasBuilding`; legacy uses `isActiveBuilding` (CvCity.cpp:2859) — a *disabled* prereq would diverge, but none are disabled now. | ❓ NOT PINNED | LIVE per-clause diagnosis when the game is up; do NOT speculative-fix |
| A3 | **UNDER (~9): `bWater` mis-mapped to `IS_WATER`.** The cascade evaluated a building's water requirement as `pl->isWater()` (the city PLOT is water — always false, cities sit on land), so every coastal/naval building was hidden. Legacy `isValidBuildingLocation` (CvCity.cpp:18500-18506) gates `bWater` on **`isCoastal()`**. `NAVAL_YARD`, `DESALINATION_PLANT`, `FACTORY_SAILS`, `PIRATES_COVE`, `MONTREAL_BIODOME`, `CULTURE_DAHOMEY/KUSHITE/NUMIDIAN`, … | ✅ DIAGNOSED + FIXED + VERIFIED (curator `bWater→IS_COASTAL`, incl. water-OR-river): under-offers 10→1. NB unmasked 2 coastal over-offers the buggy always-false `IS_WATER` had been hiding — always divergent, now visible in A1/A2 | CHANGE — curator fix (done) |
| A4 | **UNDER (`LEECH_CATCHER` etc.): the cascade OVER-obsoletes.** Its obsolete index holds the XML `ObsoleteTech` (a future tech the player has), so it hides the building — but legacy still offers it (legacy's *effective* obsolete differs). The inverse of A1's runtime-vs-XML obsolete mismatch | ⚠ DIAGNOSED | ALIGN with A1 (one obsolete reconciliation) |
| A5 | **BONUS-connection primitive** `hasBonus`/`hasVicinityBonus` — flagged "fuzzy/suspect" in CvCascadeCondition.cpp:7-10; the cascade's connection test may differ from legacy's | ⚠ KNOWN-GAP | the cascade's own trade-network model is deferred; ALIGN or replace at #430 — watch for it behind A2 |

## B. Runtime-maintainer behaviours (the sweep CANNOT see — it tests buildability, not already-built things)

These are the §14 H state maintainers. The buildability sweep excludes built things (`!hasBuilding`), so a maintainer
matching/​diverging is **unverified until it has its own behaviour shadow.**

| # | What | Status | Disposition |
|---|---|---|---|
| B1 | **Religiously-limited dormancy** — a built state-religion wonder under no matching state religion. Legacy `setReligiouslyLimitedBuilding` (`processBuilding ∓1`, reversible) ↔ cascade `requires.operate` dormancy | ✅ MATCH (verified 2026-06-18) | keep; recorded so we don't re-litigate |
| B2 | **`hasAllReligionsActive()` exemption** — under an all-religions-active civic/trait, legacy keeps a built religious wonder ACTIVE (CvCity.cpp:14975); the cascade operate gate would dormant it | ⚠ KNOWN-GAP | currently MOOT (no current civic — Secularism/Free Church/Egalitarian all `AllReligionsActive=None` — grants it). ALIGN at switch via a `hasAllReligionsActive` waiver clause IF any civic/trait ever sets it |
| B3 | **Property-band placement** (`checkPropertyBuildings`, crime/disease/pollution) — per-turn add/remove on a value band ↔ cascade end-state = `requires.operate` property-in-band dormancy | 🔭 UNSHADOWED | property gate **replaced at the hard switch** (PropertyEffect, enabler-spec §3). Build a maintainer shadow before deletion |
| B4 | **autoBuild placement** (per-turn autobuild loop) ↔ cascade end-state = `enables` + `autoBuild` placement marker | 🔭 UNSHADOWED | wrangle at switch (data-model §4.2b). Needs a placement shadow |
| B5 | **Resource dormancy** (`PrereqBonuses` via `isActiveBuilding`) — losing a continuous resource ↔ cascade `requires.operate` | 🔭 UNSHADOWED | many bonus prereqs currently sit in `requires.build` (grey-only) but should `operate` (Phase-F build-vs-operate). Needs a shadow |

## C. Intentional / interim divergences (cascade differs BY DESIGN — recorded so they're not mistaken for bugs)

| # | What | Status | Disposition |
|---|---|---|---|
| C1 | **`notConstructible`** (cost==-1) — the cascade hides these from the player build list; the property/autobuild/outcome systems still PLACE them via `canConstruct(bIgnoreCost=true)` | ✅ MATCH (interim) | correct interim; effect buildings formalize OUT to `PropertyEffect` at the switch (data-model §4.2b) |
| C2 | **State-religion gate in `requires.operate`** (adds dormancy) — matches legacy's religiously-limited (B1), not a build-only gate | ✅ MATCH | keep |

---

*Process: when the sweep surfaces a divergence, diagnose it; if it's a real behaviour difference, it lands here with a
disposition (CHANGE / ALIGN) rather than being silently "fixed to match." A maintainer (§B) is not "done" until it has a
behaviour shadow AND its row is resolved. The hard-switch demolition (enabler-spec §14) is complete only when every row
here is CHANGE-accepted or ALIGN-done.*
