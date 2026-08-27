# 3. The data reads cold

> Part of the **[overview](../overview.md)** spec.

Every entity is **one JSON object in its own file** under `Assets/Data/<type>/`.
→ [specs/json.md](../specs/json.md)

The promise the format is held to:

> A well-authored file is understandable with zero engine knowledge. Keys say what they mean; values say what
> they are. If a shape only makes sense once you know the C++, it is wrong — the engine is built to fit the
> data.

The same Forge, abridged to the parts that answer the same questions:

**Before — 144 lines of XML**

```xml
<PrereqTech>TECH_METAL_CASTING</PrereqTech>
<ObsoleteTech>TECH_NANOMINING</ObsoleteTech>
<ObsoletesToBuilding>BUILDING_FOUNDRY</ObsoletesToBuilding>
<ReplacementBuildings>
  <BuildingType>BUILDING_FOUNDRY</BuildingType>
</ReplacementBuildings>
<PrereqBonuses>
  <Bonus>BONUS_CHARCOAL</Bonus>
  <Bonus>BONUS_COAL</Bonus>
</PrereqBonuses>
<ConstructCondition>
  <Or>
    <Has><GOMType>GOM_BONUS</GOMType><ID>BONUS_COPPER_INGOTS</ID></Has>
    ... 11 more ...
  </Or>
</ConstructCondition>
<YieldModifiers>
  <iYield>0</iYield><iYield>15</iYield>
</YieldModifiers>
<ExtraFreeBonuses>
  <ExtraFreeBonus>
    <FreeBonus>BONUS_TOOLS</FreeBonus><iNumFreeBonuses>1</iNumFreeBonuses>
  </ExtraFreeBonus>
</ExtraFreeBonuses>

<!-- what it unlocks: not stored at all -->
```

**After — one file, one entity**

```jsonc
"enables": {
  "buildings": ["BUILDING_ARMOURER", ...19 of them, stated forward...]
},
"obsoletedBy": { "techs": ["TECH_NANOMINING"] },
"requires": {
  "build": { "all": [
    "TECH_METAL_CASTING",
    { "any": [
      { "type": "BONUS_COPPER_INGOTS", "scope": "city", "connection": "trade" },
      ...11 more...
    ] }
  ] },
  "operate": {
    "all": [ { "any": [ { "type": "BONUS_CHARCOAL", ... },
                        { "type": "BONUS_COAL", ... } ] } ],
    "dormant": ["BUILDING_FOUNDRY"]
  }
},
"production": { "city": { "percent": 15 } },
"provides":   { "bonuses": ["BONUS_TOOLS"] }
```

The same facts are on both sides; each now says what it is. The positional `<iYield>` pair became a named
channel. Two unrelated requirement mechanisms became one `requires` with two timings and the same condition
vocabulary in both. `ReplacementBuildings` stopped pretending to be a removal and is filed as `dormant`.
`ExtraFreeBonuses` became `provides`.

One tag moves somewhere non-obvious: `ObsoletesToBuilding` was a swap *destination*, never a cause. So the
obsolescence is carried by the tech alone, and the Foundry appears twice for two different reasons — as the
upgrade the Forge becomes (`whenObsolete.becomes`), and as the thing whose mere presence parks it (`dormant`).
Those are the two halves of an **upgrade chain**: building the successor parks the predecessor reversibly; the
tech landing turns it into the successor, one-way ([enabler.md §2](../specs/enabler.md),
[json.md §4.2](../specs/json.md)).

And `enables` — the forward edge that did not exist in the source data at all — is now stated on the thing
doing the enabling. That inversion is what lets the availability machine stop scanning the database.

### The shared vocabulary

| | |
|---|---|
| **Scope** (singular) | `world › team › empire › city › plot` — where an effect applies, or where a count is taken |
| **Target** (plural) | `plots · units · cities · empires` — all objects of that kind in the scope. Grammatical number is the whole differentiator |
| **Combinators** | `all` (AND) · `any` (OR) · `noneOf` — a recursive tree, nestable to any depth |
| **Units** | `flat` (+N) · `percent` (+%) · `multiplier` (×) |
| **Predicates** | `IS_CAPITAL`, `HAS_POWER`, `HAS_RIVER`, `{existedFor: {min: 1000}}` — an extensible registry |

The same atoms compose `requires`, a deposit's `enabled` condition, a count-scaler and a grant. There is
deliberately **no expression syntax** — a composite formula is a *list* of entries that sum, because a list is
inspectable and an expression language is a second engine nobody asked for.

Values are human-readable: `7`, `25`, `1.5`. Internally everything is integer fixed-point at ×100 — two
decimals without floats, which matters when multiplayer is deterministic lockstep and a float divergence is a
desync. The conversion happens once, in the loader. **A ×100 value in a JSON file is a bug.**

---

