# 4. The `allowed` cap

> Part of the **[enabler](../enabler.md)** spec.

`allowed` ([json](../json.md) §4.4) is a separate gate from `requires` — "how many of **me** may exist," not "what
I need." A build is permitted while **`count(me, scope) < allowed`**; the count comes from the [tally](../tally.md).
The engine owns ignoring caps under game options / era-scaling — the machine just compares.

**The two cap shapes gate in DIFFERENT places, because they have different scopes.** A **self-cap**
(`world`/`team`/`empire`) is player-scoped and gates in `allowedOk`. A **category count-cap** — how many
world/team/national wonders one CITY may hold, set by its `CultureLevel` — is per-CITY, which `allowedOk` cannot
see, so it gates in the building domain's own gate beside the SpecialBuilding group cap. A building's CATEGORY is
derived from **WHICH self-cap it authors** ([json.md §4.4](../json.md): the cap's scope is what makes it a world /
team / national wonder), never from an `isWorldWonder` mirror, and the comparison uses the city's RAW category
counts — never the engine's `isWorldWondersMaxed()` verdict, which is a computed output a gate must not ride in on
([the pollution guardrail](../validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)).

⚠ **Its two gate INPUTS name the candidate NOWHERE, so neither is reachable through the candidate's own
`EDGEF_REQUIRED_BY` set** — the city's CULTURE LEVEL (which sets the max) and another wonder of the same category
ARRIVING here (which moves the count). Both therefore re-gate the whole capped set: on the culture-level fact, and
in this city on the building-changed fact beside the existing cap-scope fan. An unrouted gate input is a
permanently stale verdict ([self-heal is not a backstop](../../cascade/03-no-staleness-no-selfheal.md#-a-self-heal-is-the-fossil-of-a-missing-emit--so-it-is-a-search-not-just-a-ban)).

⚖ **TWO GAME OPTIONS REMOVE THE CATEGORY CAP OUTRIGHT, and the gate must honour BOTH.**
**`GAMEOPTION_NO_WONDER_LIMIT`** is the player asking for no limit — removing it is the whole point of the option —
and **`GAMEOPTION_CHALLENGE_ONE_CITY`** = NO wonder limits; OCC remains an UNSUPPORTED mode, but it is an
ordinary game option like any other and needs no special machinery. While either is on, the category cap simply
does not apply.
⛔ There is deliberately **no curated cap variant** for either — neither RESCALES the limit, they REMOVE it, so the
legacy per-culture-level OCC cap field is not migrated. The gate reads the options at the CONSUMING system (here,
the enabler) while the info keeps serving ungated data ([json.md §9](../json.md)).
⚠ **The enabler computes this verdict itself and must therefore carry the carve-outs itself.** Reading
`CvCity::isWorldWondersMaxed()` is banned for the same reason as the raw-count rule above
([the pollution guardrail](../validation.md#the-pollution-guardrail--engine-computed-data-never-rides-in)), so the option checks stay with the
count — an omitted one silently enforces a limit the player switched off. Re-deriving a verdict means re-deriving
every carve-out on it, not just its arithmetic.

---

