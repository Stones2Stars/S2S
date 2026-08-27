# Bonuses — what a resource is, how it is placed, obtained, and traded

> **The one place the whole bonus model lives.** A resource's behaviour was previously only assemblable from six
> machine docs — residency from [enabler.md §8](../specs/enabler.md), the atoms from
> [json.md §3.4](../specs/json.md), supply from [§5a](../specs/json.md), the vicinity stores from
> [contexts.md](../cascade.md), the network from [special-systems.md](special-systems.md), trading
> from `CvDeal` — so anyone asking *"how does a resource get here"* had to reassemble it and reliably got it
> wrong. This is the subject-side view; the machine docs keep owning their machines.

---

## 1. ⛔ ONE LIST OWNS THE NUMBER — the plot group, and nothing mirrors it

**`CvPlotGroup` is the only authoritative holder of what a player HAS**, a sparse `id → count` map maintained by
`CvPlotGroup::changeNumBonuses`. `CvCity::getNumBonuses` RELAYS through the city's plot-group pointer (applying
the three genuinely per-asker adjustments — `TechCityTrade` gate, minted-percent suppression, corporation
add-on) and `CityContext::tradedBonusCount` forwards to that read. Full ruling, the ownership funneling through
cities/forts, and the host table: [enabler.md §8](../specs/enabler.md) RESIDENCY.

⛔ **A per-city mirror is a retired idea** ([superseded-ideas #34](../architecture/superseded-ideas.md)): it
answered a DIFFERENT number than the engine, because it carried none of the three per-asker adjustments.

---

## 2. THE TWO ORIGINS — `trade` and `onSite`, mutually exclusive

**⛔ A resource atom's `connection` names WHERE IT ORIGINATES, and the two values may never be combined
.**

| value | means |
|---|---|
| **`trade`** | the NETWORK has it — the asking city's plot group holds it |
| **`onSite`** | it originates from the city ITSELF |

**A gate wanting either states TWO ATOMS under an `any`**, deliberately. There is no combined selector.

⚑ **Why they may never be one selector:** a gate satisfied by both keeps a city operating on ore it has TRADED
AWAY — the export leaves the plot group, but the ore is still in the ground — and it makes every network
question silently satisfiable by a local resource, which is the conflation the split exists to end.

⛔ **`vicinity` is NOT a connection value.** It is the PLOT-SET axis — WHICH plots count (`owned` / `worked` /
`crossBorder`) — and naming it an origin is exactly what made the two read as interchangeable. Vicinity means
plots in the city's control, and only that.

⚖ **The worked pair:** a mounted unit needs horses **on site**; a swordsman needs iron in the **network**
([json.md §3.4](../specs/json.md)).

---

## 3. ⚖ A MANUFACTURED BONUS IS THE SAME CLASS AS EVERY OTHER BONUS

**This is why the relationship was changed, and it is what makes §2 expressible at all.** A resource a building
PRODUCES enters the plot group through the operate/provides fixpoint exactly as a mined or traded one does, so
nothing downstream knows it was manufactured and `connection:"trade"` finds it.

⛔ **The union this retired is the thing never to re-add.** While manufactured bonuses sat in a class of their
own — reachable only through a vicinity leg — a gate HAD to ask *trade OR vicinity* to see them, which is
precisely the combined selector §2 bans. **Normalizing the class removed the need for the union**; the ban is
not a tightening applied on top of it.

⚠ **So a gate that appears to miss a manufactured resource is NEVER fixed by widening its `connection`.** The
supply reaches the group through the enabler's supply crossings; if it is absent, the defect is in THAT path — a
dormant provider, or a crossing that did not announce — and is fixed there
([enabler.md §3.2](../specs/enabler.md)).

---

## 4. HOW A RESOURCE IS PLACED AND OBTAINED — every route IN

| route | mechanism |
|---|---|
| **on a tile** | map generation / `doBonusDiscovery` places it; `CvPlot::updatePlotGroupBonus` folds the plot's own extracted resource into its group once improved and connected |
| **manufactured by a building** | `provides.bonuses` ([json.md §5a](../specs/json.md)) — an ACTIVE building's supply, pushed into the group through the operate/provides fixpoint (`EnablerKernel`'s supply crossings). **Active only**: a dormant or obsolete building supplies nothing |
| **imported by trade** | `changeBonusImport` on the receiving player → `changeNumBonuses(+N)` on the **capital's** plot group (§6) |
| **a city's free bonuses** | `CvCity::getFreeBonus`, folded by `updatePlotGroupBonus` |
| **a corporation** | the city's corporation add-on, applied inside the `getNumBonuses` relay |

⚑ **The supply COUNT and vicinity PRESENCE are different reads of one entry.** `provides.bonuses` may carry an
explicit count (`{BONUS_MOVIE: 6}`) which is the tradeable-supply amount; vicinity is presence-only and reads the
KEYS ([json.md §5a](../specs/json.md)).

### Every route OUT

Exported by trade (`changeBonusExport` subtracts from your OWN group) · a deal ending · a supplying building
going dormant or obsolete · the tile losing its improvement or its owner · depletion · the bonus's own
obsoleting tech.

---

## 5. THE CROSSOVER — traded ore does NOT become an on-site resource

A traded resource joins the **network**. It lands on no tile and is not available *at* any city, so it never
reaches the vicinity dictionaries. Where a traded input must become a local resource, that is a **two-step
`enables` chain through real buildings**, not a property of trade:

```
BONUS_COPPER_ORE                 traded → the capital's plot group (network)
  → BUILDING_NATIONAL_SMELTER_COPPER    requires.operate: the ore; allowed {empire:1}
  → enables BUILDING_RESOURCES_COPPER_INGOTS
        notConstructible + autoBuild — placed everywhere, operate: a smelter in this city
  → provides.bonuses: [BONUS_COPPER_INGOTS]          ← the on-site resource
```

The ore never becomes on-site. It satisfies an `operate` gate; the smelter's **output is a different bonus**
(ore → ingots), supplied by a separate system-placed building. The **national** variant is what lets a traded
input serve — it needs no local mine, unlike the plain smelter which requires `BUILDING_MINE_COPPER`. The
breeder line mirrors it.

⚑ This is [enabler.md §3.2](../specs/enabler.md)'s least-fixpoint: an operating building's `operate` can consume
a bonus another operating building provides.

---

## 6. TRADING — the deal is the state, the counts are derived

A trade is an entry in a **`CvDeal`**, which serializes the two players, the start turn, and a list of
`TradeData` per side.

- **`TradeData` carries NO quantity** (`m_eItemType`, `m_iData`, two display bools). **Multiplicity is node
  count** — three iron is three nodes, three applications, three `−1`s when it ends.
- **`CvDeal::startTrade` / `endTrade`** are the per-item choke points: `changeBonusExport(±1)` on the giver,
  `changeBonusImport(±1)` on the receiver.
- **Each of those reaches the network through `getCapitalCity()`** — the capital's plot group is where a traded
  resource lands, so with no capital there is no group to receive into. Note the sign: **export subtracts from
  your own group** (`changeNumBonuses(-iChange)`), which is what makes trading a resource away genuinely remove
  it from your network.
- **Only items with a DURATION are stored.** `startTrade` returns whether the item is held; the 10 held types
  are `RESOURCES` · `GOLD_PER_TURN` · `SURRENDER` · `VASSAL` · `EMBASSY` · `OPEN_BORDERS` · `RITE_OF_PASSAGE` ·
  `FREE_TRADE_ZONE` · `DEFENSIVE_PACT` · `PEACE_TREATY`. One-shots (a tech, a lump of gold, a city, maps) are
  applied and not stored, so there is nothing to unwind.
- **A deal is not always reciprocal.** A demand/tribute has an empty side; `CvDeal::doTurn` distinguishes them,
  charging peacetime TRADE value when both sides carry items and peacetime GRANT value when only one does.
- **The timer is DERIVED**, not a counter: `turnsToCancel() = initialGameTurn + treatyLength − currentTurn`.
  Reaching zero does not end the deal — it only makes it cancelable, after which the AI decides whether to keep
  it.
- **Every ending funnels through `CvDeal::kill`**, which unwinds each held item and deletes the deal: war,
  player elimination, an AI cancel, a broken resource (`verify`), and Python.

⚑ **The agreement is the state; the counts follow from it.** Per [enabler.md §8](../specs/enabler.md), a deal is
the ONE serialized exception in the bonus plane — the per-bonus import/export counts are derived from the held
deals ([derived data is never trusted from a save](../specs/save.md#5-derived-data-serializes-nothing-)).

---

## 7. THE FACTS — and the trap of three

⛔ **Three facts describe one resource reaching one city, and only ONE of them is a crossing a value may be
applied on** — this is bonuses' own worked instance of the general fact-vs-count ruling; full table + reasoning:
[spine.md](../spine.md) § A FACT NAMES THE HAPPENING (the counter-case). In short: the
has-verdict (`SEVT_CITY_BONUS_ADDED`/`_REMOVED`, 0 ⇄ non-zero only) is the one a value may ride; the vicinity and
plot-group facts carry a count and must never drive a deposit — only a gate re-check, which is idempotent.

Beside those three, the plot substrate announces `SEVT_PLOT_BONUS_ADDED` / `_REMOVED` (what the TILE carries) and
`SEVT_PLOT_SERVED_BONUS_ADDED` / `_REMOVED` (the plot's own served-resource verdict, which the city's on-site
store folds) — neither appears in the event-spine table, since neither is a city-scope fact.

---

## 8. LOAD

Neither the counts nor plot-group MEMBERSHIP are trusted from a save; a load-end rebuild RE-COLORS membership
from current state and re-folds the counts as a genuine crossing per plot, before the `GAME_LOAD_FINISHED` gate
pass. ⚠ **That re-color re-folds the TILE half only** — every resource an ACTIVE BUILDING supplies must be
re-pushed behind it separately, or a whole CLASS of resource goes invisible while tile-supplied ones beside it
are unaffected. Full mechanism + the fix: [enabler.md §8](../specs/enabler.md) Load-end reconciliation.

---

## See also

- [enabler.md §8](../specs/enabler.md) RESIDENCY — the plot group's ownership of the number; §3.2 the
  operate/provides fixpoint that supplies manufactured resources.
- [json.md §3.4](../specs/json.md) — the atom vocabulary (`connection`, `vicinity`); [§5a](../specs/json.md) —
  `provides`.
- [contexts.md](../cascade.md) — the city's vicinity dictionaries and the on-site store.
- [special-systems.md](special-systems.md) — trade routes and the plot group as the connection oracle.
