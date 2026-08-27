# RESIDENCY — the network count lives on the PLOT GROUP, and only there

> Part of the **[08-the-machines-shape-components-host](../08-the-machines-shape-components-host.md)** spec.

> **⛔ A PLOT GROUP IS A PURE OWNERSHIP QUESTION, AND IT IS ALWAYS FUNNELED THROUGH THE CITIES / FORTS THAT
> PARTICIPATE IN IT — NEVER THROUGH THE PLOT.** It answers *"does this city HAVE this bonus"* — feeding
> `requires` gates, the `connection:"trade"` atom and any deposit conditioned on `HAS_BONUS`. It never
> contributes a MAGNITUDE to anything, and it never answers for a tile: the city is the asker
> (`CvCity::getNumBonuses` relays through the city's plot-group pointer), a fort participates as a city-like
> member via the `actsAsCity` characteristic ([json.md §8](../../json.md)), and the plot is merely where the resource
> sits.
> ⛔ **THE ROLLERSKATE THIS EXISTS TO STOP — CONFLATING THE PLOT GROUP WITH THE LOCAL PLOT SCOPE.** Both say
> "plot", and they are unrelated: a plot GROUP is a connectivity object spanning the map answering possession;
> plot SCOPE is one tile's own output. ⚑ **The measured consequence when they were conflated:** the connection /
> vicinity / network facts were routed into the PLOT package plane, where — carrying no plot — they fanned a mark
> over every plot of every city of the owner, dominating the entire load bracket. A connection fact moves no
> tile's output at all: the resource was already on its tile producing it.
> ⚑ **And a bonus's own yield reaches ONE tile — its own.** A resource changing a NEIGHBOURING tile's output is
> the deliveryguy's ([the deliveryguy ownership rule](../../../cascade/18-ownership.md#4-ownership--the-deliveryguy-rule)) and is authored on that
> tile's IMPROVEMENT, conditioned on the bonus — never on the bonus. ⇒ A plot-scope deposit is authored only by a
> PLOT-RESIDENT source, so a plot-scope route with no named plot has no target by construction, and declining to
> fan drops nothing.

> **⛔ NO BONUS LIST IS SERIALIZED, ANYWHERE — the plot group's, `onSite`, any of them — and the plot group is
> populated EXCLUSIVELY BY EVENTS ON LOAD.** A resource list is DERIVED at every scope it appears at, so
> it answers to [derived data is never trusted from a save](../../save.md#5-derived-data-serializes-nothing-) with no
> per-list judgement to make.
> ⚖ **THE ONE EXCEPTION IS A TRADE, AND IT IS THE DEAL THAT PERSISTS, NEVER THE LIST.** Bonuses traded away
> must survive a save or the trade is lost — so the current trade DEAL is serialized, never the list. An agreement between two players is genuine non-derivable state (the event-store
> class, [save.md §5](../../save.md)); the per-bonus import/export COUNTS that follow from it are derived and are
> re-derived from the held deals on load, exactly as the network is.
> ⚑ **The test the exception gives you is general:** ask whether the thing is an AGREEMENT or a CONSEQUENCE of
> one. The agreement is state; every count downstream of it is derived.

**⛔ The `CvPlotGroup` is the ONLY authoritative list for trade resources, and NOTHING mirrors it.** Its content
is placed by the member CITIES (and `actsAsCity` forts) — never by a plot, which only holds the resource — so the
group is where the number is formed; every reader below it RELAYS. A `connection:"trade"` gate reads that list
and nothing else.

- **`CvCity::getNumBonuses` is a relay**, not a stored count: it reads the group through the city's plot-group
  pointer and applies the three things that are genuinely per-asker — the bonus's `TechCityTrade` gate, the
  player's minted-percent suppression, and the city's own corporation add-on. **The city declares no
  bonus-count member.**
- **`CityContext::tradedBonusCount` FORWARDS to that read** — it is the object's own O(1) data, so the
  STORES-vs-FORWARDS rule ([contexts.md](../../../cascade.md)) puts it on the forward side. A stored
  copy re-swept every bonus on every fact that could move one, for a number a pointer hop already answers.
- **What the crossing fan-out is FOR.** `CvPlotGroup::changeNumBonuses` still fans into its member cities, and
  the city's plot-group moves still announce — but only to fire the **presence CROSSING** (`processBonus` + the
  corporation re-check), never to maintain a value. A count moving between two non-zero values announces
  nothing, by ruling ([spine.md](../../../spine.md)).

⚑ **Why a per-city mirror is the wrong answer even though the read is hot.** Three copies of one number
(group → city → context) is duplicated authoritative state with only drift to gain — the read-not-store rule
([tally.md](../../tally.md): creating something new when it already exists is pointless). And the cost that
argued for it is gone: the group maintains its holdings as a sparse `id → count` map, so the relay is a pointer
hop and a lookup, not the group SUM the mirror was built to avoid.

⚖ **VICINITY belongs to the CITY and is a plain local-presence fact:** it satisfies `connection:"onSite"`
atoms and NOTHING else — it never adds a second owned count (one pasture is ONE horse, not vicinity+network=2).

