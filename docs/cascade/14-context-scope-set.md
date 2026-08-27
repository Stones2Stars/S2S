# Scope set — plot / city / player now; units FUTURE (role-specific); no AreaContext

> Part of the **[cascade](../cascade.md)** spec.

Contexts exist today on **plot, city, and player**. There is deliberately **no `AreaContext`**, and the reason
generalizes: **an area is not a scope at all.** A scope must be unambiguously OWNABLE — universal (world) or
owned by exactly one player up the chain — and a landmass is shared by several empires at once, so anything on it
is a per-(landmass × player) cross-product rather than a scope
(§ THE READ PATH, below). An area is therefore a bare **id**, "a really big plot" to
reference, and an area-shaped effect authors at **empire**.

> **⛔ AND TEAM IS NOT A CONTEXT EITHER — `CvTeam` IS THE TECH BRIDGE; `CvPlayer` HOLDS THE CONTEXT.**
> A team's job on this plane is to hold the shared TECH/war facts and hand them across its members. It owns no
> live-state surface, so **every team fact a reader needs is asked of the PLAYER** — team-held techs through
> `EmpireContext::teamHasTech`, and everything else forwarded the same way.
> ⇒ The team carries the unified TECH and PROJECT lists as MEMBERSHIP; anything DERIVED off them — the capability
> union, the skill planes — is the PLAYER's. A derived store landing on `CvTeam` is misplaced by construction,
> whatever maintains it.
> ⛔ **Consequence, and it is structural: `CvCascadeEvalCtx` carries NO `CvTeam*`**, and no getter, evaluator,
> predicate or valuation reaches a team to answer a state question. A player that cannot answer one is a
> **CONTEXT GAP to close by adding the forward** (§ THE EVAL CTX, above), never a reason to reach a team.
> ⚠ **Do NOT read the DEPOSIT spine as licence.** `world › team › empire › city › plot` is the containment spine
> for MAGNITUDES, and a team genuinely carries a package (§ THE READ PATH, below: three
> channels) — so "team is a scope" is true of deposits and false of state. Conflating the two is what puts a
> `CvTeam*` back in a reader's hands; the same distinction is stated on `CvTeam` itself.

⚑ **What the contexts DO carry is the area FACT** — the city's area id, its tile count and the coastal
water-body size, forwarded by `CityContext` for the `AREA_SIZE` token and the adjacency reads. *"We rather use
the area id"*: the id is a fact a city reads, never a place state lives.

**Units are a deliberate FUTURE scope, held off on purpose.** A unit context must be **ROLE-SPECIFIC**: the
goal is that a unit no longer carries ALL the data (the ~247-field fat-unit problem) — each unit holds only the state
its role needs. Working out that role-partitioning is *why* it waits, rather than wiring a fat unit context now.

**⚖ IDENTIFIED MEMBER — the UPGRADE resolution belongs to the UNIT CONTEXT.** *"Upgrade should live in the
unit context."* **The DIRECTION is the ruling: the UNIT asks.** *"When a unit asks if they can do their
upgrade in a city somewhere, then the unit has to check if a city has whatever requirement it needs."* This is
built: `CvUnit::getUpgradeCity` drives the search and fans out to `GET_PLAYER(...).cities()`, asking each
candidate's own `getUnitAvailability(eUnit)` — a city is a place the query LOOKS, never the owner of the
question.

⚑ **AND IT IS PURELY AN AI-LOOP CONCERN** — the AI deciding whether, and where, to send a unit to
upgrade. That settles its cost class: any caching this resolution earns is **AI-heuristic caching**, the
sanctioned residual ([superseded-ideas #1](../architecture/superseded-ideas.md)), NOT engine state and NOT a derived cache on
the cascade plane — it would carry no staleness protocol and answer to no invalidation contract.

---

