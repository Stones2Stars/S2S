# The enabler getter cutover — per-domain ENABLE-SIDE-FIRST stages

> **Owner direction (2026-07-14):** bring the availability getters onto the enabler,
> **enable-side FIRST, then requires**, one stage per domain. Pass-1 (enable-side)
> deliberately **OVER-SHOWS** — too many techs researchable, buildings buildable, etc.
> That is EXPECTED and fine; it is cleaner to finish the enable side across the domains
> before adding the `requires` gate back per domain. The enabler is "just enabler" — the
> `enables` forward walk + the `requires` gate ([DEC-enabler-not-cascade](../../architecture/decisions.md#dec-enabler-not-cascade),
> [enabler.md](../../specs/enabler.md) §1-2). Event-driven only: no poll, no slice self-heal
> ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).

## The per-stage recipe

For a domain X (tech / building / unit / improvement-build / civic / project / …):

1. **Enable-side CAN GET only.** The enabler's frontier fill (`Cv<X>Enabler` / `EnablerKernel::gateSet`)
   produces `union(enables) − (disables ∪ obsoletes ∪ replaces)` over HAVE, minus already-held/built.
   **DROP the `requires.build`/`requires.operate` GATE and the `allowed` caps** — those are LATER
   stages ([enabler.md](../../specs/enabler.md) §1-2).
2. **Getter reads ONLY the enabler.** `canResearch`/`canConstruct`/`canTrain`/… read the enabler set;
   the legacy fallback is **removed** (commented out).
3. **Event-driven.** The frontier recomputes on a HAVE-change event ([enabler.md](../../specs/enabler.md) §7
   recompute-on-HAVE-change) — no poll, no slice self-heal.
4. **VERIFY LIVE (load-only, endpoint-only — logs are held open by the running game).** The domain's
   available set must be the COMPLETE enable-side set (0 missing / 0 extraneous vs "≥1 held prereq,
   not held") AND OVER-INCLUSIVE (entries whose full `requires` the legacy gate would block). Commit
   the stage only once endpoint-verified ([validation.md](../../specs/validation.md) done-is-observable).

## Stages (owner's illustrative numbering; NOT exhaustive)

1. **Enabled techs — ✅ DONE (commit `307937de1`).** `canResearch` reads only the enabler
   (`CvPlayer.cpp` — legacy removed); `TechEnabler::available` = enable-side (`generate()`'s
   `cand["techs"]` − held − disabled; requires gate dropped). Verified live (player 1, turn 1337,
   446 held / 944 techs): `availableTechs` = 63 = the COMPLETE enable-side set (0 missing, 0 extraneous),
   of which **17 over-inclusive** (available with unheld `requires.build.all` — legacy would block).

2. **Enabled buildings — NEXT.** `CvCity::canConstruct` (`CvCity.cpp:2542`) already routes
   default/post-init to `enConstruct`/`enConstructVisible` → `enBuildable`; **remove the
   `canConstructLegacy` fallback** (mirror the tech change).
   ⚠ **Buildings do NOT use an `enables`-frontier** — `BuildingEnabler::buildable`
   (`CvBuildingEnabler.cpp:201`) frontier is **ALL buildings** (an `enables`-frontier under-offers
   no-enabler buildings like PALACE — see the code comment). So the enable-side extraction = modify
   the **shared** primitive `bc_isBuildable` (`CvBuildingEnabler.cpp` ~151-199, used by BOTH
   `buildable()` and the incremental `bc_recheckBuildings`) to **KEEP** the enable-side prunes
   (tech-obsolete, already-built, in-queue, `notConstructible`) and **DROP** the pass-2 gates
   (instance-`allowed`-cap, special-building group-cap, dormancy `requires.operate`, prereq-AMOUNT,
   `requires.build`, `requires.operate`). Over-inclusion here will be LARGE (all-buildings minus a few).

3. **Enabled units** — `canTrain` → `enTrainable` / `UnitEnabler::trainable`. Same recipe (units carry
   `build` only; drop the requires gate + the `allowed`/superseder/dormant-upgrade gates for pass 1).

4. **Enabled improvement builds** — `canBuild` → the enabler build-unlock set. Same recipe.

5+. **requires per domain (pass 2)** — re-add the `requires.build`/`operate` gate over the enable-side
   frontier, per domain; then the `allowed` caps. This is where the over-inclusion is narrowed back to
   the real buildable/researchable set, now enabler-owned end-to-end.

## See also
[enabler.md](../../specs/enabler.md) §1-2 (GENERATE then GATE) · §6 (greyed frontier) · §7
(recompute-on-HAVE) · [enabler-event-fed.md](enabler-event-fed.md) (the HAVE-from-events axis, distinct).
