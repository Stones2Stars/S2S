# 4b. The CLOSED per-100 set — ÷100 to humanize

> Part of the **[fixed-point-and-scales](../fixed-point-and-scales.md)** spec.

Verified exhaustive: `grep -rE "get[A-Za-z_]+100 *\(" SourceArchive/Infos/*.h` returns **exactly six** `…100()`
accessors across the legacy Info headers (curator input only — the classes were moved to `SourceArchive/`
per the red-ratchet; see [AGENTS.md](../../../../AGENTS.md)). That set IS the de-scale list:

| field | accessor | scale | curator action |
|---|---|---|---|
| `TechYieldChanges` (Building) | `getTechYieldChanges100` | ×100 | ÷100 → human (FLAT) |
| `TechCommerceChanges` (Building) | `getTechCommerceChanges100` | ×100 | ÷100 → human; it is **FLAT** (`changeBuildingCommerceTechChange`→`getBaseCommerceRate100`, `CvCity.cpp:12136`); the XML "CommercePercents" sub-tag is a misnomer |
| `EraCommerceChanges` (Heritage) | `getEraCommerceChanges100` | ×100 | ÷100 → human |
| `iExtraUpkeep100` (Promotion / UnitCombat) | `getExtraUpkeep100` | ×100 | ÷100 → human |
| `getTotalModifiedCombatStrength100` (CvUnit) | — | ×100 | **computed**, not an XML field — nothing to de-scale |

