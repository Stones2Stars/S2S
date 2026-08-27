# 2. Anatomy of an entity

> Part of the **[json](../json.md)** spec.

| group | sections | what they are |
|---|---|---|
| **Availability** | `enables` · `obsoletes` · `replaces` · `disables` · `requires` · `allowed` | what this unlocks/removes; what it needs to be built & to keep running; the cap on how many may exist |
| **Provisions** | `grants` · `triggers` · `provides` | `grants` = pure payload on the source's considered action; `triggers` = trigger → chance → action (happening-fired / rolled / state-conditioned effects); `provides` = a continuous in-vicinity SUPPLY while active (e.g. a building or map bonus that makes a `BONUS_*` available in the city) |
| **Effects** | every **modifier family** key (`food`, `production`, `happiness`, …, one per `PROPERTY_*`) | per-turn magnitudes this deposits onto targets |
| **Trade routes** | `tradeRoutes` | **its own BASE SECTION** — the route COUNTS and the modifiers TO routes, together. See below |
| **Intrinsic** ("what am I") | `identity` (incl. all TEXT) · `cost` · `ui` · `world` · `sound` · `ai` | empire-agnostic self-description, art, audio, AI metadata |
| **Classification** | `skills` (UNIT, mutable abilities) · `tags` (UNIT, immutable type membership) · `status` (UNIT, a per-turn counter -- applied, ticks down, over) · `attributes` (BUILDING, what the building itself is/does) · `amenities` (CITY-held, grantor-provided) · `characteristics` (PLOT SUBSTRATE, held plot-scope intrinsics) · `capabilities` (TEAM, grantor-provided) | §8 — the classification model; scope carried by the section name |
| **Applicability** | entity-level `enabled` · `disabled` | the whole entity applies only while `enabled` holds and `disabled` does not (the §3.9 pair at entity level) — the canonical whole-entity game-option gate: `"enabled": "GAMEOPTION_X"` |
| **Auxiliary / bespoke** | `policies` · `excludes` · `produces` · `condition` · `effect` · `outcomes` · `mapGeneration` · `replacedBy` · `promotionLine` · `buildUp` · `shrine` · `headquarters` · `properties` · `voteSource` · `threshold` · `role` · `victory` · `targetLevel` · `conversion` · `unitCapability` · `canTrade` (tech → the trade-table/deal system: tradeable items + agreements — `techs`/`openBorders`/`rightOfPassage`/`embassy`/`bonuses`/…) · `canTradeOn` (tech → trade-route system; terrain refs) · `canWorkOn` (tech → the city `canWork` gate; workable plot classes — `water`/`peaks`/…) — all three [capabilities.md](../capabilities.md) | data read by their own systems, not the cascade |

`type` (the entity's own id, e.g. `"BUILDING_FORGE"`) and the TEXT fields are present where relevant.

> **⚖ `tradeRoutes` IS ITS OWN BASE SECTION — the COUNTS and the MODIFIERS TO ROUTES live together in it.**
> *"Trade route counts, and the modifier to trade routes, should be in the base, because trade routes
> fundamentally is its own section."*
>
> ⛔ **A per-yield trade modifier is NOT a member of the yield family** — a trait reducing a route's food authors
> under `tradeRoutes`, never `food.<scope>.tradeRoute` — the yield families carry what an entity DEPOSITS, and
> "what a route is worth" is a property of the route, not of food.
> ⚑ Getting this wrong is silent: an unkinded member parses, logs once as `[READJSON] unkinded-member
> <family>.<member>`, and produces NOTHING — the trade-route per-yield and `coastal`/`foreign` variants bite
> hardest here.
>
> ⚖ **Two axes, neither the other's member:** the **COUNT** axis (how many routes, and the cap) and the
> **MODIFIER** axis (the route-PROFIT percentage by route kind, plus the per-channel percentage on the yield a
> route delivers). ⛔ **Route KIND is a CONDITION, never a kind of its own** —
> `coastal`/`foreign`/`sharedCivic` are predicates on the entry ([§3.5](03-the-shared-vocabulary/05-predicates-a-systems-runtime-state.md#35-predicates--a-systems-runtime-state-query)):
> `IS_FOREIGN`/`SHARES_CIVIC` evaluate against the route's partner city. ⚠ Not the same predicate shape:
> `coastal` is a verdict about the CITY; `foreign`/`sharedCivic` about the ROUTE.
>
> **⛔ WHY ITS OWN SECTION: trade routes are an isolated system we don't fully control, so we feed it
> what we have BEFORE it reaches base.** The engine owns the network calculation (which cities pair, what a route
> is worth) and the cascade cannot re-derive it; the cascade owns only the INPUTS, assembled as one complete
> package before the engine runs, because afterwards there is nothing left to attach them to. ⇒ That is what
> makes it a SECTION rather than a family — a modifier family deposits into a scope's package and is read at
> combine; this one has to be COMPLETE at a specific upstream moment.
> ⚑ **A route therefore takes percentages TWICE, which is not double-counting:** once on the route itself
> (profit + per-channel, inside the engine stage), again when the yield lands in the city's TIER-1 BASE and takes
> the ordinary percent stack ([modifier.md §2a](../../cascade.md)) — the same two-stack shape a SPECIALIST's own
> percent layer uses before joining BASE. ⚠ A trade-route modifier authored anywhere else arrives too late to
> reach the system that consumes it.

---

