# Empire capabilities — glossary

The catalogue of **empire/team-wide, grantor-provided abilities** — the **empire counterpart to unit
[skills](skills.md)**. This is the **glossary** (the namings); the **system** is the [json spec](json.md) §8.
Sibling of skills.md.

> **⚖ THE MECHANIC — a derived-on-query system, enabler-style; nothing is "granted".**
> Capabilities are not handed out per se — no grant event, no application moment, no stored team state. The system
> behaves like the [enabler](enabler.md): the empire's ACTIVE capability set is **derived where consumed**, as the
> union over the **currently live sources** — the same HAVE axis the enabler generates from (team techs + adopted
> civics + active buildings). A source's `capabilities` block is pure data direction ("this source carries the key");
> liveness does the rest — a capability is active iff SOME live source carries it, and it lapses the moment its last
> source does (no lifetime bookkeeping exists or is needed). **In practice no capability is ever disabled today**:
> every shipped grant is tech-side and techs are never lost — the lapse-with-source semantic is **headroom the model
> carries for free**. Grantor breadth: **any source kind on the HAVE axis participates** (tech / civic / building);
> "monotonic" holds for TECH sources only, never for the system.

**Curator mechanics:** the tech `enabler` channels fold into the `capabilities` block
(`enabler_block="capabilities"` in `curate_common.apply_channel` — `{cap: true}`, scope implied), so a tech reads
`"capabilities": {techTrading: true, …}` instead of a top-level family. The **civic** `enabler` channels are the
sibling case — **policies enacted by a civic** → the `policies` block (emitted by `curate_civic`). Entity-level
boolean gates that are neither (a building's `damageAllAttackers`, a wonder's `buildingOnlyHealthy`) stay as-is.

**Current state:** readJson maps the `capabilities` block onto the entity's `CvJsonInfo` — each key realized as a
runtime-generated **`CAPABILITY_*` info** ([DEC-classification-infos](../architecture/decisions.md#dec-classification-infos),
[json.md §8](json.md)) with the grantor-side getters reading O(1) id bitsets — and the CvTeam capability
getters **run on the cascade**: `CascadeCapabilities` (per-team cached derived-on-query union — O(1) precomputed
flags + a per-terrain bit vector, invalidated at `setHasTech`/`reset`) is the **sole** union; the legacy counters
are deleted, their serialization retired by the soft-remove — the read + write dropped and the tags named in
`Assets/savemigration.txt` ([save.md §3](save.md)). NPC guards + game-option compositions
live in the getters; the side effects the deleted changers carried (the trade-network recompute
`updatePlotGroups` + `MarkBridgesDirty`, the improvement-validity cache round, `updateYield`) survive in
`processTech`. **The shadow phase has ended** ([validation.md](validation.md)) — the `[CAPSHADOW]` net no longer
gates anything.

## What a capability is (recap)

- **Team / empire scope** — applies to the whole civilization, not one unit (the section name carries the scope).
- **Source-derived, never granted** — active iff some **live** HAVE source (tech / civic / building) carries the
  key; derived on query, enabler-style. Monotonic only insofar as the source is (a tech is; a building isn't).
- The empire analogue of a unit `skill` (a `skill` is the *unit* ability; a `capability` is the *empire* one).

> **⚖ PARAMETERIZED abilities.** Two shapes, decided by the value's shape (json.md §2):
>
> - **Per-commerce sliders — discrete capability keys** (`setScienceRate`/`setCultureRate`/`setEspionageRate`):
>   after the split each is a genuine bare-bool ability, so the flat set carries them.
> - **Per-terrain trade — NOT a capability: the root `canTradeOn` block** (named `canTradeOn`, not `canTrade`, to
>   avoid confusion with trading RESOURCES). Flat `canTradeOnX` booleans *"will end up having to be individual
>   hard-code gates, with 0 modularity"* — every key needs its own C++ gate + a hardcoded terrain→key table. Instead
>   the grantor (tech) carries a bespoke **`canTradeOn`** block with REAL `TERRAIN_` references (FK-resolved by
>   readJson), **which the trade-route system goes through**: the empire's tradable-terrain set is the
>   derived-on-query union over live sources' `canTradeOn.terrains` (same mechanic as capabilities), and the
>   consumer (`CvPlot.cpp:5641` `CvTeam::isTerrainTrade`) asks generic set-membership — new tradable terrains are
>   pure data, zero code. Live data: raft-building (lake-shore), sailing (coasts+lake), seafaring (seas),
>   navigation (oceans+trenches). **The COMMON (baseline) tradable terrains are homed on `TECH_GAME_START`'s
>   `canTradeOn` block** — the universal start node every civ holds (it already carries `setScienceRate:true` the
>   same way), so the from-game-start tradable set rides the SAME union mechanic, no engine special-case for
>   "always tradable". **`riverTrade` is semantically DISTINCT:** it defines whether a river can be used as a
>   "trade ROAD" (a connectivity conduit, like routes) — NOT whether you can trade on a river tile — so it is not
>   terrain-list data; it **IS a capability** (a river-interaction ability like `bridgeBuilding`), outside
>   `canTradeOn`, which stays purely "which plot types carry trade".
>
> **⚖ The `canTrade` block — the whole `-Trading` family re-homes out of flat capabilities.**
> The semantic model first: `openBorders` is FULLY open — a civilization-to-civilization **"tradeable
> pact"** (all units pass); `limitedBorders` means only CIVILIAN units (merchants and such) can pass — **in-game
> name: "Right of Passage"** (verified: `TXT_KEY_MISC_LIMITED_BORDERS`). Each is a capability only in the sense that
> **you can trade FOR it** — the unlock is the *ability to negotiate that pact type*; actually *having* it with
> another civ is a **traded agreement** (diplomatic state, outside this system). That model generalizes into a root
> **`canTrade`** block — *"what may appear on your trade table"* — booleans for items AND agreements: `techs`,
> `openBorders`, `rightOfPassage` (the player-facing name, not limitedBorders), `embassy`, `bonuses`,
> `freeTradeAgreement` *"and so on"* — plus the grounded legacy re-homes `gold`, `maps`,
> `defensivePact`, `vassals`, `permanentAlliance`. The diplomacy/deal system (`CvPlayer::canTradeItem` + the
> per-item gates) goes through it **generically** — no per-key hardcoded gate — via the same derived-on-query union
> over live sources. Sibling of `canTradeOn` (above); the flat capability set keeps only the non-trading abilities.
> Data consequence: the curator emits BOTH `canTrade.openBorders` AND `canTrade.rightOfPassage` from the single
> legacy `isOpenBordersTrading` flag (legacy couples them in one `processTech` branch) — two keys so the coupling
> lives in DATA, not a hardcoded engine implication.
>
> **The grounded tradeability map (locust pass — the block is additive, a miss is easy to add after).** Legacy has
> exactly **8 tech-side flags** (`CvTechInfo.h:57-74`):
> `bMapTrading`→`maps` · `bTechTrading`→`techs` · `bGoldTrading`→`gold` · `bOpenBordersTrading`→`openBorders`+
> `rightOfPassage` · `bDefensivePactTrading`→`defensivePact` · `bPermanentAllianceTrading`→`permanentAlliance` ·
> `bVassalStateTrading`→`vassals` · `bEmbassyTrading`→`embassy`. (`CvTeam::isLimitedBordersTrading` is the coupled
> team counter with NO own XML flag — the double-emit covers it.) The wider trade-TABLE item space
> (`TradeableItems`, `CvEnums.h:2156` — resources/bonuses, cities, workers, military units, contacts, corporations,
> votes, `TRADE_FREE_TRADE_ZONE`, `TRADE_RITE_OF_PASSAGE`, war/peace/embargo/civic/religion) is game-option/
> state-gated today, NOT tech-flagged — each becomes a `canTrade` key (`bonuses`, `freeTradeAgreement`, …) as/when
> its gate goes data-driven.
>
> **Engine-side option compositions stay at the consumer:** `canTrade.vassals` / `canTrade.permanentAlliance`
> compose GAME OPTIONS engine-side (`CvTeam.cpp:3262/3279`: the flag getter = capability ∧
> `GAMEOPTION_ENABLE_PERMANENT_ALLIANCES` / ¬`GAMEOPTION_NO_VASSAL_STATES`) — the capability DATA is the unlock,
> like era-scaling on `allowed` caps. Any spec-check must fold the options. `isCommerceFlexible` additionally
> gates espionage on met-civs and everything on founded-first-city — runtime UI conditions, not capability data.
> `canSetScienceRate` + `canSetEspionageRate` are UNIVERSAL defaults (`CIV4CommerceInfo.xml` `bFlexiblePercent=1`,
> a system global with no grantor) — their data home is `TECH_GAME_START`'s `capabilities`; culture stays
> tech-gated (TECH_DRAMA); gold has no slider.
>
> **⚖ The `canWorkOn` block.** *Which plot classes a city's citizens may WORK.*
> Deliberately **coarse plot classes, not terrain lists** — in essence
> **`water` · `ocean` · `peaks` · `space`**. The `CvCity::canWork` gate queries the block generically (derived
> union over live sources). Grounded legacy sources:
>
> - **water/ocean** — `bWaterWork` (`TECH_TRAP_FISHING` → `CvTeam::isWaterWork`, the `canWork` `isWater()` gate,
>   `CvCity.cpp:1753`) is the ONE direct work gate found. The owner half-remembers a separate ocean (and
>   deepOcean) tech requirement — NOT found in `canWork` (all water terrains carry positive base yield,
>   so it is not the `hasYield` gate either); **trace the actual ocean-working realization at port time**, do not
>   assume the single-flag model.
> - **peaks** — need **`TECH_MOUNTAINEERING`** (grounded: `bCanPassPeaks` → `CvTeam::isCanPassPeaks`). The
>   legacy realization is INDIRECT — peaks are impassable without it (`CvPlot::isImpassable`, `CvPlot.cpp:5785`);
>   there is no direct `canWork` peak test — trace the exact hop at port time.
> - **space** — **semi-modelled today / to be modelled in the future**; the block is its ready home.
> Same magically-free modularity as `canTrade`/`canTradeOn`: a new workable plot class is data, not a new
> hardcoded gate. **If terrain-level explicitness is ever needed here, rework it THEN** — the
> coarse classes are the model until a real need says otherwise.
>
> **⚖ Dual-plane abilities — same name on both planes.** An ability can exist as BOTH a
> unit **skill** and an empire **capability**, under the **same clean name**. The exemplar: **`canPassPeaks`** — a
> promotion grants the unit skill (legacy `bCanMovePeaks`) to a specific unit, and `TECH_MOUNTAINEERING` makes it
> universal as the empire capability (legacy `bCanPassPeaks`) — "everyone can do it at mountaineering". The
> effective check is the OR of the planes: unit-has-skill ∪ empire-has-capability (legacy AI already treats them as
> one mechanic — it zero-values the promotion once the team flag is up, `CvPlayerAI.cpp:28313`). The distinct
> `canLeadThroughPeaks` (lead a whole stack through) stays its own skill.

## Capabilities — the CANONICAL list (clear-semantics names — the name says what it does)

> Naming convention: **`can<Verb><Object>` / `has<Thing>`** — e.g. `canSetScienceRate`, `hasRiverTrade`,
> `canIgnoreIrrigation`, `canSpreadIrrigation`. Grounded from the shipped data + the engine
> flags; the `-Trading` family lives in `canTrade`, terrain trade in `canTradeOn`, workability in `canWorkOn`
> (rulings above). `moveOnWater` is DROPPED (exists in neither data nor engine; `canWorkOn.water` is the
> water-working ability, a different thing).

| capability (canonical) | was (emitted) | legacy source | meaning |
|---|---|---|---|
| `canFoundOnPeaks` | `canFoundOnPeaks` | `bCanFoundOnPeaks` (TECH_ALGEBRA) | can found cities on peak tiles |
| `canPassPeaks` | `canPassPeaks` | `bCanPassPeaks` (TECH_MOUNTAINEERING) | move through peaks — **dual-plane** with the unit skill (ruling above) |
| `canMoveFastOnPeaks` | `moveFastPeaks` | `bMoveFastPeaks` (TECH_COLONIALISM) | faster movement over peaks |
| `canFarmDesert` | `desertFarming` | `bEnablesDesertFarming` | can farm desert tiles |
| `canSpreadIrrigation` | `irrigation` | `bIrrigation` | irrigation spreads / chains from fresh water |
| `canIgnoreIrrigation` | `ignoreIrrigation` | `bIgnoreIrrigation` | farms work without an irrigation chain |
| `canBuildBridges` | `bridgeBuilding` | `bBridgeBuilding` | roads cross rivers |
| `hasRiverTrade` | `riverTrade` | `bRiverTrade` | a river acts as a trade ROAD (conduit — ruling above) |
| `canRebaseAnywhere` | `rebaseAnywhere` | tech flag | air units may rebase to any friendly plot |
| `canSeeFurtherFromWater` | `extraWaterSeeFrom` | `bExtraWaterSeeFrom` | see FROM water plots one level higher (`CvPlot::seeFromLevel`) |
| `hasCenteredMap` | `mapCentering` | `bMapCentering` (TECH_GEOMETRY — the SOLE authoring; the building-side tag is schema-only, data-dead) | minimap centered on your civ + round-globe view; arrive-and-stay latch ≡ derived (tech-only grantor) |
| `hasWholeMapRevealed` | `mapVisible` | `bMapVisible` | reveals the ENTIRE map on acquire (`setRevealedPlots`, `CvTeam.cpp:5292`) |
| `hasLanguage` | `language` | `bLanguage` (TECH_LANGUAGE) | civ has developed language — gates `needLanguage` heritages (`CvPlayer.cpp:30970`) |
| `canSetScienceRate` | `setScienceRate` | commerce-flexible (TECH_GAME_START) | the science slider |
| `canSetCultureRate` | `setCultureRate` | commerce-flexible (TECH_DRAMA) | the culture slider |
| `canSetEspionageRate` | `setEspionageRate` | commerce-flexible (TECH_GAME_START) | the espionage slider |
| ~~`dcmAirBomb1` / `dcmAirBomb2`~~ | same | `bDCMAirBombTech1/2` | DCM air-bomb target tiers — **MOOT: DCM air bombing is slated for whole-system REMOVAL** ([structural-cleanup.md](../plans/structural-cleanup/structural-cleanup.md) Tier 2); the channels drop with it |

## Grounded meanings

- **`canWorkOn.water`** (was `waterWork`) — cities may WORK water tiles at all (`CvCity::canWork` gate,
  `CvCity.cpp:1753`); granted by `TECH_TRAP_FISHING`.
- **`canSeeFurtherFromWater`** — see FROM water plots one level higher (`CvPlot::seeFromLevel`, `CvPlot.cpp:2562`) —
  ships stop being nearsighted; second consumer in AI settle scoring (`CvCity.cpp:6327`). It *could* be modelled in
  the vision system proper, but **stays a capability — solve if/when it is a problem**; expected surfacing point:
  when the visibility system is modelled properly during the cascade rebuild — whoever builds that system revisits this
  key then (it sits next to the BLOCKED unit visibility/invisibility accumulators and the building `lineOfSight`
  channel).
- **`hasCenteredMap`** — *"when it arrives, map gets centered, and stays centered"* — an arrive-and-stay latch in
  practice, equivalent to the derived union because the sole grantor is a tech and techs are never lost. It *could*
  technically behave as a grant (a one-shot pulse), but no special grant type is minted for a thing like this — do
  not reclassify. *(Pre-named future path: if we ever need to grant a player ONE-TIME EFFECTS, mapCentering lands
  cleanly in that grant type — until then it stays here. And if a building-conditioned centering is ever wanted:
  a tech-conditioned entry on the always-present palace, riding the ordinary condition vocabulary — latch behaviour
  falls out for free.)* Pure presentation, no map reveal: minimap renders centered on your civ
  (`CvGameInterface.cpp:2907`) + globe view goes round-with-stars (`CvGame.cpp:2760`).

## Open

- **Ocean-working trace** — the half-remembered ocean/deepOcean requirement (see the `canWorkOn` ruling).
- **Stale code comment** — `CvJsonTechInfo.h:7` still claims "techs are the ONLY grantor"; fix when the
  capability union widens from techs-only to the full HAVE axis (civic/building grantors).

## See also

- [json.md](json.md) §8 — the system. · [skills.md](skills.md) — the unit counterpart. · [tags.md](tags.md) ·
  [state.md](state.md). · [engine.md](../reference/engine.md) — the save-field retirement mechanism + the
  deleted-changer side-effect audit rule the capability cut proved.
