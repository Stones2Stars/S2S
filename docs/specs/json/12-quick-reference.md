# 11. Quick reference

> Part of the **[json](../json.md)** spec.

**Top-level keys** — `type` · `identity` · `cost` · `ui` · `world` · `sound` · `ai` · `enables` · `obsoletes` ·
`replaces` · `disables` · `requires` · `allowed` · `grants` · `triggers` · `skills` · `tags` · `state` · `attributes` ·
`amenities` · `characteristics` · `capabilities` · `shrine` · `headquarters` · *(modifier families)* · *(auxiliary/bespoke, §9)*

**Scope (singular)** — `world › team › empire › city › plot{improvement|feature|terrain|route} › building|specialist|unit` · off-spine `self` = the entity's own build
**Target (plural)** — `plots · units · cities · areas · empires` = all of that kind in the scope, predicate-filtered
**Combinators** — `all` (AND `&&`) · `any` (OR `||`) · `noneOf` (NONE), each over its direct children (leaf or nested node); a recursive boolean tree, nestable to any depth
**Atom** — `{ type, scope, min?, max?, connection? }` · presence = `min:1`
**Predicate** — bare (`IS_*`/`HAS_*`/`VICINITY`/`IS_CAPITAL`…), `{PREDICATE: param}`, or membership `{terrain|feature|bonus:[…]}`
**Units** — `flat` (amount) · `percent` (+% delta) · `multiplier` (×, identity 100). Human-readable; ×100 is a bug.
**Entry** — `{ <payload>, scope?, per?, enabled?, disabled?, ai? }`
**`requires`** — `build` (greys) / `operate` (greys + dormancy)
**`allowed`** — `{scope:N}` self-cap, or `{worldWonders|teamWonders|nationalWonders:N}` per-city category cap

---

*The machines that consume this shape: [enabler](../enabler.md) (can I?) · [modifier](../../cascade.md) (how much?) ·
[tally](../tally.md) (how many?). The legacy XML→JSON field mapping is migration-transient and lives with the
migration, not here.*
