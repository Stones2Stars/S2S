# 3.7 `per` — count-scaling

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

```jsonc
"per": "SPECIALIST"                                              // bare-string sugar: × count, each:1, own scope
"per": { "type": "POPULATION", "each": 5, "scope": "city" }      // value × (count / 5)
"per": { "anyOf": ["BONUS_COW","BONUS_PIG"], "scope": "city" }   // value × (summed count of any listed)
```

A bare string is the common case collapsed (the §3.4 bare-atom sugar, applied here): `"per": "SPECIALIST"` ≡
`{ "type": "SPECIALIST", "each": 1 }` at the deposit's own scope. Keep the object form only when a real `each`
quantum or a non-default scope forces it.

**`above:` — the over-threshold scaler.** `"per": { "type": "CITY", "above": N }` scales by the count
EXCEEDING the threshold — `value × max(0, count − above)` — for the "per city over the limit" formula class.
The threshold is a literal or a **token** (the §3.3 threshold-token rule): `"above": "CITY_LIMIT"` reads the
depositing civic's own resolved limit. Composes with `each` (`(count − above) / each`) and the §3.9 gates.

`each` is the quantum ("per 5 population" → `each: 5`); state it explicitly. `scope` defaults to the deposit's own
scope; cross-city scopes (empire/team/world) resolve via the [tally](../../tally.md), `city`/`plot` are local.

**`unit: <predicate>`** qualifies a deposit by a **unit predicate** — the *same* `unit:` qualifier cargo uses
(`cargo.space.{unit: IS_AIR, …}`, [modifier](../../../cascade.md) §6). On a count-scaling family it reads **per unit
matching**: `happiness.empire.cities.{unit: IS_MILITARY, flat: N}` = "N happiness per *military unit* stationed" —
the unit-presence effect lives on the civic/trait that grants it, targeting each city.
The qualifier generalizes by counted kind — the field NAMES what is counted and holds the filtering predicate:
`happiness.empire.cities.{religion: "!IS_STATE_RELIGION", flat: N}` = "N happiness per city religion matching"
(here: per non-state religion present in the city).

> ⛔ **THE QUALIFIER MAY BE A CONDITION OBJECT, NOT ONLY A BARE STRING — `{unit: {all: [IS_LAND, !IS_VTOL]}}` is
> the ordinary compound form, and it is the MAJORITY form in the cargo data.** ⚠ `unit` and `religion` are also
> SCOPE / keyed-map segment names, so shape alone cannot say which one an object-valued key is. What settles it
> is that **a qualifier is a SIBLING OF A MAGNITUDE LEAF** (`flat`/`percent`/…) and a scope hop never has one —
> which is exactly how `CvModifiers::walk` discriminates them.
> ⚑ Getting that wrong is SILENT and total: a compound qualifier read as an address segment turns the entry into
> the unkinded member `cargo.space.unit.all` and **drops it**, so the deposit simply does not exist. It cost 56
> of the 90 authored carriers their entire hold — every aircraft carrier among them — while the data read as
> perfectly well authored. The `[READJSON] unkinded-member` census names it on every load
> ([validation](../../validation.md)); a non-zero count is dropped authored data, never cosmetic.

> **Predicates vs tags.** `IS_*` predicates are **independent queries**, *not* tag-membership: `IS_LAND`
> (used by cargo above) matches an intrinsic *domain*, not a `tag`. But a predicate **may be defined to encompass
> tags** (e.g. `IS_MILITARY` set up to match the `military` tag + similar) — predicates have **definitions**.
> **Post-migration:** make predicates **definable as JSON objects** and support **predicate groups** (compose
> them); for **migration they are HARDCODED** (`IS_MILITARY`/`IS_LAND`/`IS_AIR` baked into the curator).

