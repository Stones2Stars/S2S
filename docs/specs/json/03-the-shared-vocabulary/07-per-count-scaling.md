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

> **Predicates vs tags.** `IS_*` predicates are **independent queries**, *not* tag-membership: `IS_LAND`
> (used by cargo above) matches an intrinsic *domain*, not a `tag`. But a predicate **may be defined to encompass
> tags** (e.g. `IS_MILITARY` set up to match the `military` tag + similar) — predicates have **definitions**.
> **Post-migration:** make predicates **definable as JSON objects** and support **predicate groups** (compose
> them); for **migration they are HARDCODED** (`IS_MILITARY`/`IS_LAND`/`IS_AIR` baked into the curator).

