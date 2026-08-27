# 4c. The ×100-space ADDENDS that LACK a `…100()` getter — the heuristic's blind spot

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

The "`*100` getters mark the scaled fields" rule is INCOMPLETE: some fields are added in ×100 space
*without* a `…100()` getter. These must be mapped at the consumption site, not by name. Verified against
`CvCity.cpp`:

| field | scale | evidence | curator action |
|---|---|---|---|
| `BonusCommercePercentChanges` (Building) | **×100, and FLAT** | added raw beside `100 * getBuildingCommerce` inside `getBuildingCommerce100` (`CvCity.cpp:12132`); the *rate* modifier is the separate `m_aiBonusCommerceRateModifier` | ÷100 de-scale **+ relabel `percent`→`flat`** (the name's "Percent" is a misnomer) |
| `YieldPerPopChange` / `CommercePerPopChange` (per-pop) | **×1 human, NOT ×100** | added raw into the ×100-space `getExtraYield100` / `getBuildingCommerce100` (`CvCity.cpp:11323` / `:12132`) — the legacy "latent /100 weakening" | **emit as-is; do NOT de-scale** (÷100 here corrupts `1/pop` → `0.01/pop`) |
| `YieldsProduced` / `CommercesProduced` (Corporation) | **×100** | `getCorporationYieldByCorporation` (`CvCity.cpp:12594-12602`): `produced × Σ getNumBonuses(prereqBonus) × worldCorpMaintPct / 100`, then the corp result `/100` — so `produced=75` ⇒ 0.75/bonus. NOT the genuinely-×1 `*Changes` twin (`getYieldChange × 100` in-formula) | ÷100 de-scale → human (`curate_corporation`). The dedicated corp pass also verifies + de-scales `iMaintenance` (`calculateCorporationMaintenanceTimes100`, ×100) |
| `iHealthPercent` / `iHappinessPercent` (Specialist) | **×100, and FLAT** | `processSpecialist` STORES them raw (`CvCity.cpp:5184/5192`, `change*Health/*Happiness(field × count)`) — the misleading part — but the REALIZED `goodHealth()`/`badHealth()`/`happyLevel()`/`unhappyLevel()` read them `/100` (`CvCity.cpp:5848/5876/5714/5654`). The `/100` is NOT AI-only weighting; it is the actual realized level. | ÷100 de-scale → human (FLAT; the "Percent" is a misnomer). `curate_specialist`. ⚠ Map at the CONSUMER, not the store — the raw `change*` store site is the trap that produces a wrong "it's FLAT ×1" correction |

> The per-pop row is the [the no-guessing rule](../../../../AGENTS.md#conduct) case in miniature:
> the scale was *mapped* at the consumption site, never guessed from the field name.

