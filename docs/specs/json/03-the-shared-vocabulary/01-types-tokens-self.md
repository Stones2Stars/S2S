# 3.1 Types, tokens, `SELF`

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

- **Data Types** — `INFOTYPE_NAME` ids (`BONUS_COAL`, `UNIT_AXEMAN`, `BUILDING_FORGE`). The leading **infotype**
  prefix identifies the kind and routes the reference; the full prefix glossary (`UNIT_` = unit, `TRAIT_` =
  simple trait, `TRAIT_COMPLEX_` = complex, …) is the **[naming spec](../../naming.md)**.
- **Catch-all tokens** — engine concepts that aren't data Types: `TURN`, `POPULATION`, `MILITARY`, `CITY`,
  `TEAM`, `UNIT_LEVEL`, `AREA_SIZE`, **`ERA`** (the player's current era as a plain **counter 1…X** — the era
  sequence; eras are ordered data defined in `Assets/Data/eras/`), **the commerce slider rates** `GOLD_RATE` /
  `RESEARCH_RATE` / `CULTURE_RATE` / `ESPIONAGE_RATE` (the player's current slider percents as plain counters —
  a wellbeing deposit per-scaled on one is "happiness per 10% culture rate" / "anger per gold rate"),
  **`CULTURE_PERCENTAGE`** (the city's OWN-culture percent of its plot — the city-scope culture-share driver;
  foreign share authors as the `100 −` telescoping pair, never an inverse unit),
  **`CITY_LIMIT`** (SOURCE-resolved: the depositing civic's own city limit — its base-limit config × the
  world-size scale percent; the `per.above` threshold for the over-limit unhappiness class),
  **`DISTANCE_TO_GOVERNMENT_CENTER`** (plot distance from the city to its owner's NEAREST government centre,
  0 in one — the driver of distance maintenance; served from a maintained `CityContext` store, never a
  per-read walk),
  **`TARGET_NUM_CITIES`** (the world-size's
  expected city count — `CvWorldInfo.getTargetNumCities`, e.g. 2/3/4/6/8/11/14/18 per world size; a runtime-resolved
  world constant, used as a `max:` in ranked selection §3.3), **`WORLD_WONDER`** / **`NATIONAL_WONDER`** /
  **`TEAM_WONDER`** (the count of wonders of that category in the scope — `city` = `CvCity::getNum{World,National,Team}Wonders`;
  the existing engine terms, pedia display names TBD; used as a `per` count-scaler, e.g. a trait's free-specialist-per-
  wonder), … (an engine-resolved, extensible registry).
- **`SELF`** — "this entity's own type," resolved per-entity. Used only in a `per` count-scaler ("per how many of
  me exist"). It is **not** used in `requires` — a "one of me" cap is [`allowed`](../04-availability.md#44-allowed--caps), not a
  condition.

