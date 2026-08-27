# 3.4 Conditions — `all` / `any` / `noneOf`

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

A **recursive boolean tree**, identical wherever a condition is needed (`requires`, and a deposit's `enabled`/`disabled`). Three combinators;
each holds a list of **children**, and a child is **either a leaf** (a count/presence atom or a predicate, §3.5)
**or another combinator node** — nesting is allowed to any depth:

- **`all`** = **AND** (`&&`) — every child must hold.
- **`any`** = **OR** (`||`) — at least one child must hold. A plain OR over its **direct children** — *not*
  "OR-groups AND-ed together".
- **`noneOf`** = **NONE** — no child may hold.
- **`!` prefix** = **NOT** on a single leaf-string — `"!IS_STATE_RELIGION"` negates the
  predicate inline, so `all: ["IS_HOLY_CITY", "!IS_STATE_RELIGION"]` reads naturally ("a holy city that is NOT the
  state religion"). It is pure **shorthand for `noneOf:[X]`** on one leaf (the parser rewrites `"!X"` → `noneOf:[X]`),
  reusing the boolean tree — for negating a *group*, use `noneOf` with a nested node. (Less obvious at a glance than
  `noneOf`, but the standardized terse form; documented here.)

```jsonc
{ "all": [ leafA, { "any": [ leafB, leafC ] }, { "noneOf": [ leafD ] } ] }
//  ≡  leafA && (leafB || leafC) && !leafD
{ "all": [ "IS_HOLY_CITY", "!IS_STATE_RELIGION" ] }   // ≡ all:[ "IS_HOLY_CITY", {noneOf:["IS_STATE_RELIGION"]} ]
```

So `any` is exactly `||` on what is directly below it:

- `any: [BONUS_COPPER, BONUS_IRON]` = `copper || iron`.
- `any: [ {all:[STONE,IRON]}, {any:[COPPER,WOOD]} ]` = `(stone && iron) || (copper || wood)`.

To require BOTH "(copper or iron)" AND "(forge or foundry)", **nest two `any` nodes under an `all`** —
`all: [ {any:[COPPER,IRON]}, {any:[FORGE,FOUNDRY]} ]`. `any` never means AND — it is a plain recursive boolean
tree (`all`/`any`/`noneOf`, nestable to any depth).
Each leaf is **either** a count/presence **atom** or a **predicate** (§3.5):

```jsonc
{ "type": "BONUS_IRON", "scope": "city", "connection": "trade" }   // an atom
```

An **atom** is `{ type, scope?, min?, max?, connection? }`. **Scope is IMPLIED from the type's domain** (derived from
the ID prefix) — TECH→`team`, civic/heritage→`empire`, building/bonus/religion/corporation→`city`. One data-driven
override: a building carrying `identity.empireLevel` (§7) has EMPIRE as its domain, so an atom naming one implies
`empire` — the player-held set is the only place its presence exists
([enabler.md §2](../../enabler.md), [empire-level buildings](../../enabler/02-pass-1-generate-the-frontier-the.md#2-pass-1--generate-the-frontier-the-enables-family)).
State `scope` explicitly ONLY when it differs from that default (e.g. a `world`-scope victory, a `player`-scope tech).
So a **plain default-scope presence collapses to a bare type-string** — author the common case as a simple string
array: `"all": ["BUILDING_FORGE", "TECH_ASTRONOMY"]` ≡ `[{type:"BUILDING_FORGE"},{type:"TECH_ASTRONOMY",scope:"team"}]`.
Keep the object form only when a special case forces it: a `connection`, a count (`min`/`max`), or a non-default
scope. **Forcing a redundant `{type, scope}` only invites authoring bugs.** *(Plot-substrate
`{type:"TERRAIN_…"/"FEATURE_…"/"IMPROVEMENT_…"}` and `{type:"MAPCATEGORY_…"}` stay object-form — they are plot predicates, §3.5.)*

- **presence** = `min: 1` ("have ≥ 1"). Authoring presence this way keeps it future-proof if a resource later
  gains amounts.
- **count thresholds** — `min: N` (≥ N) and/or `max: N` (≤ N), both inclusive. Exact-N = `min` and `max` together.
- `connection` (resources only) ∈ `"trade"` | `"onSite"`. **The two are MUTUALLY EXCLUSIVE** — a gate wanting
  either states TWO atoms under an `any`, never one combined selector. `vicinity` is a separate field, not a
  `connection` value. What each means: [bonuses.md](../../../reference/bonuses.md).
- **`vicinity`** — a separate field, carried with or without a `connection`: which tiles of
  the city's workable radius count. A radius tile's
  ownership is one of three — and the distinction is load-bearing: **owned** (the city's team), **neutral** (unowned,
  `NO_TEAM`), or **foreign** (another team). The ownership selectors nest `owned ⊂ owned+neutral ⊂ owned+neutral+foreign`:
  - **absent** = **owned + neutral** — the **DEFAULT**: the city's own tiles plus unclaimed land,
    but NOT another team's. This mirrors the engine's vicinity (feature prereqs count neutral tiles too — `neutral`
    flag, `CvHttpServer.cpp`; terrain/improvement/peak/hill are `owned`-only via the next selector).
  - `"owned"` = **owned only** — strictly the city's own tiles (centre or owned radius tile; **no** connection or
    improvement needed), excluding even neutral. A raw owned-presence.
  - `"crossBorder"` = owned + neutral + **foreign** (any ownership) — the opt-in that ADDS foreign tiles, counting
    beyond the city's borders. **No current use-case, kept for completeness.** Name avoids the
    `all`/`any`/`noneOf` combinators (§3.4). A foreign tile's bonus is revealed per its OWN team, so it can read
    differently per asking city — exactly why foreign is gated behind this explicit opt-in rather than the default.
  - `"worked"` = a tile a citizen **works** this turn (implies owned).
  - `"onSite"` = the resource is **actually available AT this city** — however it got there. Improving a resource
    on a workable plot puts it here, and so does a building in the city that supplies it (a herd, a factory —
    `provides.bonuses`, §5a): those are the SAME act as far as this list is concerned, and the list cares only
    about what is there, never about provenance.
    > **⛔ IT IS NAMED `onSite` BECAUSE "vicinity" AND "connected" BOTH MISLEAD.** The two sets are
    > ORTHOGONAL: **onSite** = the resource is here; **`connection:"trade"`** = the plot group reaches it. A
    > resource can be either without the other — a mounted unit needs horses ON SITE, a swordsman only needs
    > iron wares in the NETWORK.
    > ⚠ The retired spelling was `"connected"`, which took the trade side's word for a local question and is
    > what made the two read as one thing. `owned` (raw presence, improved or not) stays its own tier and is
    > strictly weaker than `onSite`.
    >
    > **⛔⛔ `onSite` IS AN ENABLER-SIDE CONCEPT ONLY. NO MODIFIER GATES ON IT — NOT ONE.** A DEPOSIT
    > conditioned on a resource asks whether the CITY HAS IT, which is the TRADED question and is spelled as the
    > bare `{type, scope:"city", min:1}` atom. `onSite` belongs to `requires` GATES and is *"almost purely a
    > concept that creating mounted units have to deal with, very little else"* — horses must be physically here;
    > the mine building is the other explicit case. Both are enabler-side.
    > ⚠ **This has been stated repeatedly and re-derived wrongly anyway**, because the tier list above reads as a
    > menu any atom may pick from. It is not: a modifier picks the bare atom, full stop. The measured cost of
    > getting it wrong is silent and total — the curator mapped legacy `VicinityBonusYieldChanges` to
    > `vicinity:"onSite"` on the strength of its XML name (its engine read, `hasVicinityBonus`, actually means
    > *obtained* in vicinity, i.e. connected), and every one of those deposits then refused against a resource the
    > city demonstrably held: London carried 96 resources in trade and 14 on site, so Cannery's apple, crab,
    > lemons and olives were each refused while the city was trading all four.
    > ⚑ The refusal is invisible in every total — a deposit that never applies leaves no trace — which is why this
    > survives review and why the yield tooltip and `/computed/city/yield` now list the refused deposits WITH the
    > atom that refused them.

  ```jsonc
  { "type": "BONUS_SHRIMP",   "scope": "city", "connection": "vicinity", "vicinity": "owned" }      // raw presence on an owned tile
  { "type": "BONUS_GOLD_ORE", "scope": "city", "connection": "vicinity", "vicinity": "onSite" }     // must be available here
  ```

- **`PROPERTY_*` band atom** `{type:PROPERTY_X, scope, min?, max?}` — its "count" is the city's property value;
  **absent `min` = no lower bound** (a max-only band), the one exception to the presence=`min:1` convention.
  Authored in `requires.operate`.

> **Counts vs caps.** `min`/`max` express what you **need** (a count of *some other* type, e.g. "≥12 Barracks").
> "How many of THIS may exist" is **not** a condition — it is the [`allowed`](../04-availability.md#44-allowed--caps) cap.

