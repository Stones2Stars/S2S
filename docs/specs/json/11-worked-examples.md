# 10. Worked examples

> Part of the **[json](../json.md)** spec.

### A building

```jsonc
{
  "type": "BUILDING_FORGE",
  "identity": { "description": "TXT_KEY_BUILDING_FORGE" },
  "enables": { "units": ["UNIT_CROSSBOWMAN"] },
  "requires": { "operate": { "all": [ {"type":"BONUS_IRON","scope":"city","connection":"trade"} ] } },
  "production": { "city": { "percent": 25 } },
  "happiness":  { "city": { "flat": 1, "enabled": "HAS_POWER" } },
  "cost": { "production": 120 }
}
```

*Unlocks the Crossbowman; needs connected iron to keep operating; +25% production and (while powered) +1 happiness
in its city; costs 120 hammers.*

### A world wonder (a cap + a conditional bonus)

```jsonc
{
  "type": "BUILDING_VERSAILLES",
  "allowed": { "world": 1 },
  "requires": { "build": { "disabled": "IS_CAPITAL" } },
  "buildRate": { "self": { "percent": 100, "enabled": { "type": "BONUS_MARBLE", "scope": "city", "min": 1 } } },
  "culture": { "city": { "flat": [ 10, { "value": 10, "enabled": { "existedFor": { "min": 1000 } } } ] } }
}
```

*Only one may exist in the world; can't be built where a capital already sits; builds twice as fast with connected
marble; +10 culture, doubling after it has stood 1000 years.*

### A culture level (per-city wonder caps)

```jsonc
{
  "type": "CULTURELEVEL_DEVELOPING",
  "enables": { "buildings": ["BUILDING_TOWN_HALL"] },
  "allowed": { "worldWonders": 2, "teamWonders": 2, "nationalWonders": 8 },
  "defense": { "city": { "amount": { "percent": 12 } } }
}
```

*A city at this level may hold up to 2 world / 2 team / 8 national wonders, and gets +12% defense.*

---

