# ⚖ EVERY DERIVED STORE IS ONE SHAPE — a KEYED ACCUMULATOR maintained by a delta

> Part of the **[cascade](../cascade.md)** spec.

**A count is a sum.** The possession plane and the magnitude plane are not two mechanisms — they are one
structure over different payloads, and the only things that vary are the key space and the value type:

| store | key → value | the delta arrives from |
|---|---|---|
| the plot group's bonuses | `id → count` | a member plot/city joining or leaving |
| `CityContext.amenities` | `id → count` | a grantor starting or stopping conferring |
| `CityContext`'s vicinity tiers (all/owned/foreign/worked/onSite) | `id → count` | a radius plot's bonus, ownership or served-resource verdict moving |
| `EmpireContext.policies` | `id → count` | a civic / trait / project / wonder |
| the enabler's membership planes | `id → (enable, remove)` | a HAVE-change |
| `OperatingBuildings::providedCount` | `id → count` | an active flip |
| **the cascade packages** | `channel → Σvalue` | a source's compiled deposits |

⇒ **`ContextDict` and `CvCascadePackage` share a MAINTENANCE RULE**, so
[the maintained sum](05-three-planes.md#-the-maintained-sum--three-planes-one-slot-and-nothing-is-ever-recomputed) is the MAGNITUDE case of one general rule, never a
cascade-only one.

> **⛔ THEY BEHAVE SIMILARLY AND ARE NOT THE SAME — sharing a mechanism is not sharing an identity.**
> The rule above governs HOW a derived store stays current. It says NOTHING about which store a value belongs
> in, and reading it as licence to merge them is the conflation this callout exists to stop:
>
> | | context dictionary | package channel |
> |---|---|---|
> | the KEY is | a minted **classification** id — a named FEATURE | a minted **cascade channel** — a named QUANTITY |
> | the VALUE is | grantors present, or a held strength | a summed magnitude in a unit |
> | the SCALE rule | none — a count is a count | [the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries): flats ×100, percents unscaled |
> | READ by | gates, conditions, `per` scalers | the combine, the realized value |
> | AUTHORED in | the `amenities` / classification block ([json.md §8](../specs/json.md)) | a family address `<family>.<scope>.<unit>` |
>
> ⚠ **The SCALE row is what bites silently if they merge** — ×100 semantics landing on a refcount, or dropping
> off a magnitude, both staying entirely plausible.
> ⚑ **The worked case: AIRLIFT CAPACITY.** A building's airlift is a NUMBER it carries and the city's total is a
> SUM of numbers — so it is a modifier-family CHANNEL and retires onto the city's PACKAGE, exactly like the other
> hand-named scalars ([every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)). ⛔ It is NOT an amenity,
> however volumetric it looks: putting it in the dictionary would make an `AMENITY_*` id carry a magnitude and
> break what that registry means.
> ⚠ **Consequence for the volumetric headroom, stated so it is not mis-planned:** power becoming a CAPACITY a
> city draws against would not be an amenity carrying a magnitude — it would be power CHANGING PLANES, from a
> classification key to a channel. Do not "future-proof" the dictionary for a change that would relocate the
> value. ⛔ [every derived cache is one shape](#-every-derived-store-is-one-shape--a-keyed-accumulator-maintained-by-a-delta)'s scope of *"every derived
cache on the cascade plane"* was drawn too narrowly: the plane boundary is not real, and the one store that
drifted onto a different mechanism is the one that boundary excluded.

### ⛔ THE SEMIBOOLEAN STATE — the read is BOOLEAN, the storage is NOT

**That mismatch IS the trap: storing the thing as what it READS like is the whole error.** The contract:

- **STORED** `id → count`, an int.
- **READ** `has(id)` ≡ `count > 0`.
- **WRITTEN** ±1 as a grantor starts or stops participating — never `set`, never `clear`, never a recount.
- **ZEROED at owner reset** — a delta store is correct only from a known zero, and `CvCity` is recycled out of an
  `FFreeListTrashArray`, so a reused slot inherits the previous occupant's counts and **no later delta can ever
  correct them** (§ THE CONTEXTS, below).

⛔ **THE READ SURFACE IS A BOOLEAN GETTER, AND CONSUMERS NEVER SEE THE INT.** *"The dictionary literally
needs to have a boolean getter that says whether it's there."* `has(id)` ≡ `count > 0` IS the contract; the count
exists so MAINTENANCE can be correct, not so a reader can inspect it. ⚠ The moment a consumer reads the number
the representation leaks — `count == 1` / `count > 2` logic appears, and then **volumetric can never land**,
because changing what the number MEANS breaks readers that were never meant to see it. The one legitimate reader
of the int is the genuinely volumetric one.
⇒ **The surface: `has(id)` → bool for every consumer · `add(id, ±1)` for maintenance · `count(id)` reserved for
a volumetric reader · and NO `set`.**
⛔ **`set(id, n)` IS THE FOOTGUN AND DOES NOT BELONG ON THIS TYPE** — it overwrites a refcount, so a key that
several grantors confer is cleared by the first one to leave. The live case is the THIRD RING
(`CLS_AMENITY_ADDS_3RD_RING`, read through `CvCity::hasThirdRing`): several buildings confer it, so an assignment
would shrink a city's workable radius the moment it lost ONE of TWO grantors, where the refcount keeps the ring.
A type that PERMITS the banned move forces the rule to be remembered; removing the verb makes it
unsayable, which is the enforcement model this project keeps choosing
([patterns.md](../architecture/patterns.md): a contract, not a prohibition).

⛔ **ALWAYS A COUNT, NEVER A BIT — and the deciding argument is not "some keys have several grantors."** It is
that **you can never safely answer NO**: these registries are OPEN by design
([the classification-infos registry](../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)), so a key with one grantor today gains a
second the moment someone AUTHORS data, with no engine change. A bitset breaks silently on a data edit, in a
build nobody touched. The count is not a concession to the multi-grantor cases; it is the only representation
that survives an open registry.
⚑ Two properties fall out free, and both are already ruled for the amenity instance: **VOLUMETRIC needs no
reshape** (the slot is already an int, so a state that becomes a QUANTITY only changes what the number means),
and the **REMOVAL-WINS trap is structurally absent**.
⚠ **The masking to recognise:** a set-shaped store survives only while something RECOMPUTES it whole. Convert
such a store to delta maintenance without converting its STORAGE and it breaks immediately — so the two halves
land together or not at all.

