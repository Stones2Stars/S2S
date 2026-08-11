# SURROUND_DESTROY removal map (deferred — turnkey for circle-back)

`GAMEOPTION_COMBAT_SURROUND_DESTROY` is the "S&D Extended" family: **surround + enclose +
lunge + unnerve + dynamicDefense** (unit) and **LocalDynamicDefense** (city/building).
Unlike BATTLEWORN it is a **LIVE runtime option** (no `#define` gate) — it actually changes
combat when enabled. User deferred it (2026-06: "least bad of the bad options"). Concept
captured in combat-simplification-scope.md good-ideas list.

## The combat application site

`CvUnit::getDefenderCombatValues` still applies exactly the shape this map originally
described:

```
const int iSurround = pAttacker->surroundedDefenseModifier(pAttackedPlot, this);
iTempModifier -= std::max(0, iSurround - iSurround * dynamicDefenseTotal() / 100);
```

`CvUnit::surroundedDefenseModifier` and `CvUnit::dynamicDefenseTotal` (plus
`unnerveTotal`/`encloseTotal`/`lungeTotal`) are still the live functions computing it, gated
on `kPromotion.getCombatModifier(COMBAT_LUNGE, CASC_SCOPE_UNIT)` /
`COMBAT_DYNAMIC_DEFENSE` — i.e. the per-promotion values now come off the **cascade combat
modifier channels**, not flat Info fields.

## The data layout has moved — re-derive it, don't reuse this map's old line numbers

**None of the per-field line numbers this map originally cited are current**, and the
underlying storage shape has changed structurally, not just moved lines:

- The Info-class side (`CvUnitInfo`, `CvPromotionInfo`, `CvUnitCombatInfo`,
  `CvBuildingInfo`) no longer carries raw `m_iUnnerve`/`m_iEnclose`/`m_iLunge`/
  `m_iDynamicDefense`/`m_iLocalDynamicDefense` members at all — those classes are rebuilt
  JSON pocos now (see `docs/specs/json.md`), and the promotion-side values feed the cascade
  `COMBAT_*` modifier channels referenced above instead of a flat Info getter.
- On the city side, `CvCity::getExtraLocalDynamicDefense()` is now a cascade lookup
  (`cascadeDefense(DEFENSE_DYNAMIC)`, `docs/specs/enabler.md`), not a flat accumulated
  member — so there is no single `m_iExtraLocalDynamicDefense` field to delete; removal
  means retiring the `DEFENSE_DYNAMIC` cascade channel and its data-side producers.

A future removal pass needs to re-map both sides against the **current** cascade/JSON
surface (grep the symbols above, not this doc's old numbers) before writing a new
line-by-line recipe. The AI/UI/GameText/schema/data touch points this map originally
enumerated (promotion-value multipliers in `CvPlayerAI`/`CvCityAI`, building-help and
promotion-help text, the option's XML/schema/GameText definition) are still the right
*categories* to sweep — only their exact locations need re-deriving.

## Procedure (when circling back)

1. Re-derive the current Info/cascade field map (see above) rather than trusting old line numbers.
2. Delete the combat application (`getDefenderCombatValues`) + the whole `*Total`/
   `surroundedDefenseModifier` functions, then the cascade channel(s) and their data-side producers.
3. Remove the AI valuation, UI/help-text, option, schema and XML data touch points.
4. Build (Assert) + XML-validate after each phase.
