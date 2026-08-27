# 3.9 The one entry shape

> Part of the **[03-the-shared-vocabulary](../03-the-shared-vocabulary.md)** spec.

Every deposit, grant, or conditioned value is the same shape:

```jsonc
{ <payload>, "scope"?, "per"?, "enabled"?, "disabled"?, "ai"? }
```

- **payload** — a unit magnitude: a **bare number**, or `{ "value": N, … }` when conditioned or in a list — the
  **unit** (`flat`/`percent`/`multiplier`) is the key *above* the entry, and `value` carries the magnitude inside
  it. OR a grant (`type`+`count`), OR a predicate.
- **`scope`** default = the containing scope · **`per`** default ×1 · **`enabled`** default true (applies only
  while the condition holds) · **`disabled`** default false (suppressed while it holds) · **`ai`** an optional
  sibling block applied for AI players only (same inner shape).
- **Target qualifiers ride the ENTRY too** — the §3.3 ranked-subset qualifiers (`max:` / `orderedBy` /
  `orderedByDescending`) and a counted-kind filter (`religion:` / `unit:` §3.7) may sit on an individual entry;
  a qualifier written at the target-node level is shorthand applying to every entry that carries none of its
  own. This is what lets ONE plural-target node hold differently-qualified deposits side by side (a largest-
  cities entry beside an every-city per-religion entry on the same `cities` node) — the entry is the universal
  carrier.
- **`enabled` is read before `disabled`** — the enable is evaluated first, the disable second; a `disabled`
  that holds **overrides** (the deposit is suppressed even if `enabled` was satisfied). Author them in that
  order: `enabled` first in the list, then `disabled`.
- **There is no `enabled: false`** — to conditionally SUPPRESS a deposit use `disabled` (its twin); an absent
  `enabled` means always-on.
- A leaf is a single entry **or a LIST of entries** (several conditioned values into one slot). **The list IS
  the formula mechanism:** a composite formula authors as the SUM of its entries — a base term, `per`
  terms, negative companions, threshold-gated bands — composed side by side; there is deliberately NO
  expression syntax. The telescoping pair is the canonical idiom: `V × (count − N)` = `{V, per: <counter>}` +
  the flat companion `{−V×N}`, both under the same gate. What entry sums cannot express is a separate
  MULTIPLICATIVE stage — that is an engine-formula parameter (a config value), never forced into entries:

```jsonc
"production": { "city": { "percent": [
    25,                                                            // always +25% (a bare number)
    { "value": 25, "enabled": { "type": "BONUS_COAL", "min": 1 } }  // +25% more while coal is connected
] } }
```

---

