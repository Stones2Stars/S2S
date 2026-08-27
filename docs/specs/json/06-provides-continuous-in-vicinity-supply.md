# 5a. `provides` — continuous in-vicinity supply

> Part of the **[json](../json.md)** spec.

What an entity makes AVAILABLE in its city *while active* — distinct from `grants` (a one-shot/recurring handout).
The canonical case is a building or map bonus that supplies a `BONUS_*`: a tamed-animal herd / industrial farm
supplies its animal bonus, and a map bonus on a workable plot supplies itself. One uniform surface, so a
`connection:"onSite"` requirement is satisfied by *any* provider in the city — plot bonus **or** active building.

```jsonc
"provides": { "bonuses": ["BONUS_CAMEL", { "BONUS_MOVIE": 6 }] }
```

- **`bonuses`** — `BONUS_*` ids this supplies in-vicinity. A consumer's vicinity check unions, over the city radius,
  every provider's `provides.bonuses` (active buildings; map bonuses providing themselves). **Active only** — a
  building that is dormant/obsolete supplies nothing.
- **Supply QUANTITY** — an entry is a bare `BONUS_*` string (count **inferred 1**, the common case) **or** a single-key
  object `{ BONUS_X: N }` carrying an explicit supply count. The count is the tradeable-supply amount (`getNumBonuses`
  += N), NOT vicinity presence — vicinity is presence-only, so it reads the *keys*. The canonical count case is a
  wonder that supplies several copies of a luxury (HOLLYWOOD → 6 movies; the legacy `iNumFreeBonuses`).

---

