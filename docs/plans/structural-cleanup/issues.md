# Issues — one defect per entry, with its evidence

> # ⛔ DO NOT WORK ANYTHING IN THIS FILE UNLESS THE OWNER NAMES IT (owner)
>
> **This file is NOT session-start reading, NOT a queue, and NOT an invitation.** It is a RECORD, opened when
> the owner points at a specific entry — and it is deliberately absent from the docs index for exactly that
> reason: *a list of defects compels agents to run after them without being asked.*
>
> ⛔ Finding an entry here is not authorization to fix it. Reading one while working on something else is not a
> reason to widen that work. The owner decides what is picked up and when; an agent that self-assigns from this
> file has chosen its own priorities over the owner's, which is the failure the whole file is written around.
> ⚑ **Why it is written down at all, then:** so that when an issue IS picked up, its evidence and its dead ends
> are already there — not so anyone goes looking for work.

> **⚖ THIS REPLACES THE TODO MODEL (owner): *"the todo file does not work as I want it, so we change it up and
> create issues."*** A single long worklist stopped being usable — it grew past the point where anyone reads it
> end to end, and an entry in it carries an INSTRUCTION without the EVIDENCE that justified it, so the next
> session cannot tell a live finding from a stale assumption without re-deriving both.
>
> **An ISSUE is therefore self-contained and individually closeable**: what was OBSERVED, how to REPRODUCE it,
> what is PROVEN, what is RULED OUT (so the eliminations are never re-tread), and what is NOT YET KNOWN. It is
> deleted whole when fixed, and anything durable it established moves into the owning spec
> ([DEC-docs-current-truth](../../architecture/decisions.md#dec-docs-current-truth)).
>
> **This file CARRIES STATE ON PURPOSE** — stacks, `file:line`, measurements — exactly as
> [property-audit.md](property-audit.md) and [stub-census.md](stub-census.md) beside it do. That is the
> difference from a todo and the reason the state ban does not reach it: an issue without its evidence is the
> thing that failed.
>
> ⚠ **Keep PROVEN / RULED OUT / NOT YET KNOWN separated in every entry.** Blurring them is how a guess becomes
> a fact the next reader builds on — which is the failure this whole file exists to stop.

---

## 5. Culture distance RECOMPUTES ON READ — the tombstoned ensure protocol, still live

**⚠ PRIORITY / DISPOSITION (owner): it has to be fixed, but it needs PROPER PLANNING — and it WORKS.** *"I have
never noticed any performance issues from it."* ⛔ So this is a SHAPE defect, not a perf incident: do not open it
as an optimization, and do not cite turn time as the reason to take it. It waits for a designed fix.

**What it is.** `CvCity::cultureDistance(const CvPlot&)` is the shortest **weighted** path from the city to a
plot (terrain / feature / route / bonus each cost more), live ONLY under
`GAMEOPTION_CULTURE_REALISTIC_SPREAD`; without that option it is plain `plotDistance` with no cache and no
recompute at all. ⚑ The option gate is the likely reason no cost has ever been felt.

**PROVEN — the read triggers the recompute.** On a cache miss (`m_aCultureDistances`, a
`std::map<const CvPlot*,int>`) the const read calls `recalculateCultureDistances(getCultureLevel())`, which is a
`while (bHasChanged)` **relaxation re-sweeping the whole `rect(iMaxDistance, iMaxDistance)`** until the
distances converge. That is [superseded-ideas](../../architecture/superseded-ideas.md) **#14** — the
`ensure()`-on-read protocol — verbatim, and the shape [state-repositories.md](../../architecture/state-repositories.md)
bans outright (*"a read is a BARE FETCH, unconditionally"*).
⚑ The code states its own cost: *"rather brute-force and inefficient"* and *"This happens ~iMaxDistance times per
city per turn."*

**PROVEN — the cache is cleared EVERY TURN, per city.** `CvCity::doPlotCulture` clears it, so the first read
after each turn's culture spread pays the full radius fixpoint. Its own comment says so: *"need to recompute
cache each turn because many things can change distance."*

**PROVEN — invalidation EXISTS and covers capture.** Four sites, so this is NOT a stale-forever cache:

| site | when |
|---|---|
| `CvPlayer::acquireCity` (`CvPlayer.cpp:2578`) | a city is TAKEN |
| `CvCity::kill` (`CvCity.cpp:1321`) | the city dies |
| `CvCity::doPlotCulture` (`CvCity.cpp:12072`) | every turn |
| `CvCity::readBody` (`CvCity.cpp:13099`) | the load |

**RULED OUT — it is not deletable, and not delta-maintainable like a package.** A shortest path moves
NON-LOCALLY when terrain or a route changes, so it is not a sum and the maintained-sum shape does not reach it.
`cultureDistance` is **SPATIAL**, a permanent carve-out. The mechanic stays.

**NOT YET KNOWN — the designed fix.** The obvious direction is to move the recompute OFF the read and onto the
invalidation (rebuild where it is cleared, so the read becomes the bare fetch it is specified to be) — but that
turns a lazy per-city cost into an eager one at four sites, including a per-turn one, and whether that is the
right trade has not been established. ⚠ Do not assume the eager form is cheaper; nothing here has been measured,
and the owner's standing observation is that the current form costs nothing noticeable.

---

## 6. `CvPlotGroup::recalculatePlots` — the trade network recomputes to find out whether it had to

> **⛔⛔ DO NOT TOUCH THIS OPPORTUNISTICALLY — AND THE USUAL SAFETY NET DOES NOT COVER IT.** This entry exists to
> STOP the cut, not to invite it. The owner's standing position: *"I am scared to deal with that, specifically
> because of the traderoute recalcing."* That caution is correct and is part of the finding.

**Why the blast radius is the worst in the tree.** `CvPlotGroup` is **the ONLY authoritative list for trade
resources** ([enabler.md §8](../../specs/enabler.md) RESIDENCY) — every `requires` gate, every
`connection:"trade"` atom and `CvCity::getNumBonuses` relay through it. A wrong cut does not move a number; it
silently changes what is BUILDABLE, in every city, with no loud symptom.

**⛔ NO COMPARISON CAN CATCH A REGRESSION HERE, WHICH IS WHY THIS ONE NEEDS ITS OWN VERIFICATION.** The plot group
is an **INPUT to everything that could be compared**: the operate fixpoint resolves `requires` through
`getNumBonuses`, which relays to the same group every other reader saw. So a wrong network is INHERITED
uniformly and every surface agrees — the same-derivation failure
([superseded-ideas #17](../../architecture/superseded-ideas.md)) arriving through the input rather than the
comparison, and the reason the THREE-LEG check's third leg (what STATE expects) is the only one with any
purchase here. ⇒ **Any change here needs verification built for it FIRST**, never assumed.

**PROVEN — what it actually does.** It computes the answer in order to decide whether it needed to:
1. Runs a full `FAStar` pathfind over the connected region to build two **Zobrist hashes** (all nodes, resource
   nodes).
2. `allNodesHash` unchanged ⇒ early return.
3. Only `resourceNodesHash` unchanged ⇒ cheap path: drop the plots that left the group and re-colour them.
4. Otherwise ⇒ full rebuild of the plot list and bonus counts.

**PROVEN — three retired shapes, stacked.**
- **Hash-based change detection** — the inverse of *the fact names the source*: it pays the pathfind first and
  then asks whether anything moved.
- **Session SEQUENCE counters** (`m_bulkRecalcStartSeq` / `m_sessionRecalcSeq` / `m_recalcSeqForSession`) — the
  epoch class [DEC-flag-is-fossil](../../architecture/decisions.md#dec-flag-is-fossil) names outright.
- **A blanket sweep** — `algo::for_each(plot_groups(), CvPlotGroup::fn::recalculatePlots())`
  (`CvPlayer.cpp:4243`) over every group of a player.

**Call sites (none is the sanctioned load-end rebuild):** `CvPlayer.cpp:4243` (whole-player sweep) ·
`CvCity.cpp:1438` and `CvPlayer.cpp:2893` (via `originalTradeNetworkConnectivity`, on city kill and
`acquireCity`) · `CvPlot.cpp:8656` (a plot change) · `CvGame.cpp:496` → `RecalculatePlotGroupHashes`.

**⚖ RULED — THIS IS A *KEEP*, NOT CASCADE WORK (owner): *"it is somewhat out of scope of what the
enabler/cascade setup is supposed to do."*** [north-star.md](../../architecture/north-star.md) names the
**trade-route network calculation** as one of the three legitimate KEEPs — work that is *"none of the four
systems' job"* — which is precisely why route yield is FOLDED IN as an input rather than derived. The enabler
reading the group is *"the boundary working, not a KEEP of the reading system's own work."*
⇒ **So the shapes above are observations about ENGINE-OWNED code, never a conversion worklist.** Do not schedule
this as cascade/enabler work, and do not read the retired-shape list as a to-do.

**⚖ AND IT IS NOT A HOT PATH (owner)** — so no performance argument reaches it either. Between that and the KEEP
ruling, the ONLY thing that would justify opening it is a demonstrated correctness defect in the network itself.

⚠ A load-end rebuild is separately sanctioned and must survive anything done here
([enabler.md §8](../../specs/enabler.md)): the deserialized groups are drained and membership re-coloured from
current state.

**NOT YET KNOWN — and deliberately not pursued.** What verification would make a change safe (the tripwire does
not), and whether the connectivity facts to maintain membership as a delta exist. Both are open questions about a
KEEP, so neither is owed an answer by this rework.

---

## 7. `recalculateAllResourceConsumption` — a per-turn sweep, KEPT

**⚖ DISPOSITION (owner): KEPT — resource depletion needs it**, *"even if the thing is buggy afaik."* So the
suspected bugginess is recorded, NOT diagnosed here, and this is not an invitation to rewrite it.

**PROVEN.** `CvPlayer::doTurn` calls it every turn, gated on `MODDERGAMEOPTION_RESOURCE_DEPLETION`; it walks
every city × every bonus to build a consumption vector, and its ONLY consumer is the depletion-odds scaling in
`CvPlot::doBonusDepletion`. ⚑ The option gate is why a per-turn O(cities × bonuses) walk has not been felt.

---

## 12. The WorldBuilder SCREENS mutate through the city handle, which now carries the identity set only

**Observed:** every `pCity.<mutator>` call under `Screens/Worldbuilder/` is dead. `CyCity`'s published surface
is READS only — the identity set plus the coherent group reads — and carries **no mutator at all**, so each of
these raises `AttributeError` the moment its handler fires.
⚠ The read half has grown since this was written; the WRITE half has not, which is what this entry is about.

**PROVEN — it is a BLOCK, not a set of call sites.** `WBCityEditScreen.handleInput` is one `elif` chain of
~20 structurally identical handlers (`CityFoodPlus/Minus`, `CityDefensePlus/Minus`,
`CityTradeRoutePlus/Minus`, `CityChangeCulture`, …), each a `pCity.change*` pair; `WorldBuilder.copyCityStats`
is a ~60-line run in which every line is one of these. Fixing one branch leaves the chain it sits in dead, so
a per-branch repair is the partial fix [DEC-WF-surface-sprawl](../../architecture/decisions.md#dec-wf-surface-sprawl)
bans rather than progress.

⚑ **The scenario serializer is the worked precedent and is DONE:** `pyWB/CvWBDesc.py`'s `CvCityDesc.apply` /
`postApply` are wired onto `CyAct` by (owner, id), which is the shape the screens take too.

⛔ **What blocks the screens is a STRUCTURE call, not effort: WHICH mutators the WB write surface carries.**
The verbs `CvWBDesc` needed already exist; the screens additionally reach for ~40 more
(`setProgressOnBuilding`, `changeReligionInfluence`, `setForceSpecialistCount`, `setGreatPeopleUnitProgress`,
`changeSpecialistCommerce`, `changeFreeBonus`, and a run of anger/espionage timers). Minting that set is the
owner's call on what WorldBuilder may edit at all — several of those values are now DERIVED and have no setter
to publish.

⚠ **`ExtraTrade` is the case that proves the last point and is already resolved on the data side:** a city's
extra trade routes is a derived cascade read (`cascadeValue(MODFAM_TRADE_ROUTES, …)`), so there is no stored
value to write. The scenario format's `ExtraTrade` field, its parse and its apply are REMOVED; the screens'
`CityTradeRoutePlus/Minus` control edits a value that cannot exist and goes with the pass rather than being
given a setter ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)).

⚖ This is the WorldBuilder pass [roadmap](roadmap.md) § scope decision 1b names — *"we cannot accept actually
breaking worldbuilder stuff, we fix things we see"* — and it is deferred here only on the ground that ruling
allows: the replacement MACHINE (the verb set) does not exist yet, and it is NAMED.


---
# Migrated from the todo

> Everything below was the defect half of `todo.md` and is moved VERBATIM — the wording, the evidence and the
> `file:line` anchors are exactly as they were written when each was found. ⛔ It is deliberately NOT reshaped
> into the observed/proven/ruled-out form above: rewriting a finding is how its evidence gets lost, and these
> were recorded by whoever had the trace in front of them. Reshape an entry WHEN IT IS PICKED UP, not in bulk.
>
> ⚠ Verify any claim here against the tree before acting on it — several predate the current build and name
> symbols that may have moved ([docs README](../../README.md): the cheap check is to grep one or two of the
> symbols an entry is anchored on).

## 13h. THE CITY LAYOUT HAS FEWER SLOTS THAN A CITY HAS BUILDINGS (art data, not the DLL)

**MEASURED from `LSystem.log`** ([observability.md](../../reference/observability.md)): **1,684**
`Failed to place goal building <ART_DEF>` over **174 distinct buildings**, plus **147**
`Layout failed to complete while adding generic buildings!`. Every one of the 174 is a building that HAS a
model — `ASSEMBLY_PLANT`, `FACTORY`, `COLOSSEUM`, `HOSPITAL`, `COURTHOUSE` — so this is the layout engine
running out of room, never a missing-art gap.

⛔ **DISTINCT from the art-less flood, and the two must not be conflated.** That one was the city offering
the engine buildings with NO model (`is not associated with a CvCityLSystem node`), and it is fixed and landed
(`world.art.notShownInCity`). This one is the opposite condition: a real model with nowhere to put it. Checked
rather than assumed — **0 of the 174 are flagged `notShownInCity`**.

⚠ **Whether that fix moves this number at all is UNKNOWN and is one measurement away.** It turns on whether
the art-less buildings consumed layout slots before being rejected or never reached placement — the
*"not associated with a node"* wording suggests the latter, in which case removing them frees nothing and the
1,684 stand. `LSystem.log` is rewritten per session, so re-counting after a run on the new DLL answers it.

⇒ **THE ASK IS TUNED DOWN — the two knobs are `GlobalDefines.xml`, and neither is a DLL change.**
`CvCity::getVisibleBuildings` offers `10 + 2*pop^GAME_CITY_SIZE_EXP_MODIFIER` objects, of which
`GAME_CITY_SIZE_MAX_PERCENT_UNIQUE` are distinct models. At the inherited **1.05 / 0.7** a population-40 city
asked for **106** objects (74 unique); at **0.9 / 0.6** it asks for **64** (38 unique). ⚑ The `* 2` is hardcoded
engine-side, so the exponent is the only curve knob and the percentage merely splits the total.
⚠ The remaining lever, if the overflow survives that, is `Assets/XML/Buildings/CIV4CityLSystem.xml` carrying more
or larger nodes — ART DATA ([roadmap.md](roadmap.md) scope decision 3). ⚑ Inherited rather than introduced: a
layout grid sized for vanilla's building count against this mod's **5,180** is the ratio that produces it.

---

## 13g. THE PER-CIV UNIT ART STYLE OVERRIDE IS NOT CARRIED

`CvUnitArtStyleTypeInfo` is an identity-only info: it holds its `Type`/`Description` and nothing else, so a
civilization's art STYLE contributes no art. `CvCivilizationInfo::getUnitArtStyleType()` still resolves a civ's
style against the registry and `CvUnit::getArtInfo` still passes it down, so the parameter arrives and is
declined — every civ therefore renders a unit with that unit's own art.

The data is `Assets/XML/Civilizations/CIV4UnitArtStyleTypeInfos.xml` — **6,012 `<StyleUnit>` entries**, each
keyed by unit and carrying its own Early/Middle/Late tags (~20× the unit-side mesh-group data). The consuming
shape already exists and is specified: the style is tried FIRST and falls back to the unit's own per-era tag
([json.md §7](../../specs/json.md)), which is the leg that is live today — so this is additive, and nothing
renders wrongly meanwhile, it merely renders un-styled.

⚠ **NOT the long-standing simple-unit-graphics / no-animation defect (owner: it has persisted for years)** —
that predates the rebuild and is a separate, inherited bug.

---

## 14. Vision: `updateSight` scans most of the map, and detection no longer works

**OBSERVED (owner):** *"more than 1 bug, where `updateSight` virtually scans the entire map, when a unit checks
its vision radius."* Separately: **dogs no longer see criminals at all.**

**PROVEN — the shape of the cost.** `CvPlot::changeAdjacentSight` derives `iRange = iSight / VISION_OPEN_GROUND_COST`
and then walks a `(2·iRange+1)²` box calling `canSeePlot` per tile. So any caller handing it an oversized
`iSight` turns one unit's move into a map-wide walk, and the per-tile obstruction charge is inherently more
work than `main`'s flat radius test. `updateSight`'s city leg is the one that changed shape most —
`changeAdjacentSight(team, 1, …)` became `changeAdjacentSight(team, pCity->sight(), …)`.

**⛔ RULED OUT:** the ×100 scale is NOT mismatched at `changeAdjacentSight` — it divides by
`VISION_OPEN_GROUND_COST` itself, and the owned-territory leg converts `1` → `VISION_OPEN_GROUND_COST`
correctly.

**On dogs vs criminals.** The contest is inherited from `main` and RUNNING; two live defects on that path are
fixed and landed (the detection registration was filed under an `InvisibleTypes` index instead of a SKILL id,
and the membership test never asked whether a unit hides by the method being contested).
⚠ **What remains is the promotion-granted-method gap:** `hasInvisibilityType` and `getInvisibleType` both ask
`getUnitInfo().hasSkill(...)`, so a method a PROMOTION grants does not register — and 73 promotions author one
([vision.md §4](../../specs/vision.md)). Wants a resolved per-unit skill plane, never a per-read promotion walk
inside `isInvisible`. The War Dog's role is `SeeInvisible INVISIBLE_CAMOUFLAGE`; confirm on a live run.

⚠ **The whole-map-scan half is untouched** —
the owner's ruling is that `updateSight` is **reviewed, not reverted**, because reverting it would bury the
scan bugs rather than fix them.

---

## The new surface, wired wrong — fix these first

> These are not legacy and not gaps: they are built machines connected to nothing, or connected wrongly. Each
> compiles, runs, and produces a plausible wrong answer, which is why none of them surfaced on its own.
> ⛔ Symbols, never line numbers — a symbol survives an edit and a line number does not.

- **⛔ SWEEP THE BANNED TERM OUT OF THE CODE** ([DEC-no-staleness-vocabulary](../../architecture/decisions.md#dec-no-staleness-vocabulary)).
  KEEP only the graphics/interface repaint vocabulary the closed EXE needs and BUG resolves by name
  (`InterfaceDirtyBits` and the `setDirty`/`setLayoutDirty`/`setFlagDirty`/`setInfoDirty` helpers over it).
  What is left on the derived-state side is the AI re-evaluation flags — `AI_setAssignWorkDirty` /
  `AI_makeAssignWorkDirty` / `AI_setChooseProductionDirty` — and `CvDerivedCacheSet`'s own
  `markDirty`/`isDirty`, whose one remaining tenant is the UNIT RESOLVED plane (`CvUnitResolved`,
  `CvUnit::markResolvedValuesDirty`). ⛔ A surviving derived-state site gets NO synonym — name it for the job it
  does, or delete it with the mechanism.
- **Split the vicinity BAND axis from the `worked`/`onSite` predicates** so a vicinity-named read cannot answer
  the on-site verdict ([contexts.md](../../architecture/contexts.md) § THE VICINITY SPLIT — the storage is
  already right, the ADDRESSING is what conflates them). ⚠ Reaches the authored `vicinity:` key, `CvCondition`
  and the evaluator, so it is a STRUCTURE call, not a sweep — owner input first.
- **Give `ev_countCore` a `TAG_` branch** so `CvCascadeTally::countUnitsWithTag` is reachable. The count-atom
  dispatcher branches on specialist/building/tech/unit only, so an authored `{TAG_X, min:N}` count silently falls
  through to presence and answers 0/1. ⚠ Latent — no data authors one yet.
- **Wire the `EnablerOverlay` DELTA reads** (`unlocks` / `unlockedIds`) or delete them. The overlay's `addHave`
  half is live in the civic what-if; the delta half — the header's "deliberately the primitive rather than
  something each caller re-derives" — has no consumer because the tech-picking one does not exist yet.
- **Re-route `governmentCenterDistance` and delete the fact pair nobody emits.** Its interest set names the
  government-centre crossing, whose emitters have no callers at all — the counter that raised them became the
  amenity fold and the interest set did not follow, so the distance is frozen at whatever founding or load
  computed while `DISTANCE_TO_GOVERNMENT_CENTER` is read live. ⚠ It also re-derives by reading the amenity fold,
  which is the ordering dependency [contexts.md](../../architecture/contexts.md) bans — settle the registration
  order in the same change, and correct contexts.md, which still documents the retired counter as live.
- **`HAS_CORPORATION` on plane C needs a VERDICT to announce — the fact that exists is the wrong one.** A
  conditioned deposit is withdrawn by its ATOM's verdict crossing
  ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum)), and this predicate has no arm in
  the modifier consumer's routed set, so its deposits apply when the corporation arrives (plane A) and are never
  re-resolved. *(`IS_HEADQUARTERS` was the other half and is now routed off
  `SEVT_CITY_HEADQUARTERS_ADDED / _REMOVED`, whose designation crossing IS that predicate's verdict.)*
  **PROVEN — the open question is ANSWERED, and the answer forbids the obvious route.** `{HAS_CORPORATION}` means
  ACTIVE ([json.md §3.5](../../specs/json.md)), and `CvCity::isActiveCorporation` computes that LIVE over four
  legs — presence, the player-level active state, the obsoleting tech, and a prereq bonus being held. The only
  fact on the surface, `SEVT_CITY_CORPORATION_ADDED / _REMOVED`, is emitted from `setHasCorporationInternal`,
  i.e. **PRESENCE — one of those four legs**. Routing plane C on it would apply and withdraw on a crossing that
  is not the predicate's, leaving a present-but-dormant corporation depositing.
  ⇒ **So this wants the THRESHOLD-CROSSING shape, not a case arm** ([event-spine.md](../../specs/event-spine.md)
  § THE THRESHOLD CROSSING IS ITS OWN FACT): the holder announces when the VERDICT moves, exactly as the amenity
  fold announces `isPowered` rather than its refcount. ⚠ That is a STRUCTURE call — where the verdict is held
  and what drives the other three legs — so it is owner input before code, not a sweep.
  ⚑ **The PLOT plane is the shape to copy**: `SEVT_PLOT_PREDICATE_ADDED / _REMOVED` carries the predicate id, so
  ONE route covers every plot bit and a new bit needs no case
  ([contexts.md](../../architecture/contexts.md): derive the routing, never hand-write it).
  ⛔ **RULED OUT — do not re-tread these.** `HAS_POWER` is correctly served by ONE route: the amenity fold
  announces the VERDICT (`AmenityContext::announcePowerCrossing` → `CvCity::isPowered`, grantor ∧ no blackout),
  and the blackout status routes INTO that fold, exactly as [event-spine.md](../../specs/event-spine.md)
  requires — there is no "three legs" and no clean-power flag in the tree. `IS_ANARCHY` gates no deposit at all
  (its 17 authored uses are `outcomes` gates on the captive units); the empire-status gap it really points at is
  recorded in [state.md](../../specs/state.md). `HAS_RELIGION` and `IS_TAG` have ZERO authored uses — latent, not
  defects.
- **Fill or delete the empty departed-owner branch** in the modifier consumer's `SEVT_PLOT_OWNER_ADDED /
  _REMOVED` case. Its own comment states the obligation — a plot's yield has just left the OLD empire, whose
  plot-fed receiver sums go stale "marked here or never" — and the `if (kEvent.iA >= 0 && kEvent.iA != NO_PLAYER)`
  body is **empty**. Either the delta is applied for the departed owner or the branch and its comment go; an
  empty guard reads as handled.
- **Route the wellbeing sign-split in the APPLY path.** `modifier.md` §2b routes a negative deposit to the
  opposing channel AT FILL, and only the VALUATION fill does it — so the event-built side never populates `anger`
  or `unhealth` and folds every negative into `happiness`/`health` instead. ⚠ The two fills also disagree on
  GRANULARITY (the valuation routes a collapsed sum by its net sign per scope; an apply can only route per entry,
  which is the delta-able form) — settle which is canonical in the same change, or the tripwire keeps reporting a
  divergence that is really two different aggregations.
- **Apply the SLIDER TEST to the rest of the assign-work fans.** `AI_makeAssignWorkDirty` re-runs the FULL citizen
  assignment over a player's cities — and through `CvGame`, over EVERY player's — and the commerce slider was only
  one of its callers. Each remaining one answers the question the owner settled there: *does this actually move an
  input a citizen decision reads?* One that does not is a flag asserting a change that did not happen
  ([DEC-flag-is-fossil](../../architecture/decisions.md#dec-flag-is-fossil)) and goes with it. ⚠ The per-city
  `AI_setAssignWorkDirty` sites are the same question one scope down, and the game-wide fans are the expensive end.
  ⚑ The instrument answers this directly and needs nothing built: a DIRECT setter call names its caller on the
  per-city `[CIT/assign/dirty]` line, and a whole-scope fan names its own caller on `[CIT/assign/fan]`. Both land
  in **`CityAI.log`** — the `[CIT]` domain's file, NOT `Cascade.log`. Resolve an RVA against the PDB
  (`ln CvGameCoreDLL+<callerRva>`), and the histogram of those values IS the worklist, ranked by cost.
- **Give the vicinity store its `CASC_PRED_*`-keyed twin** — the vicinity counterpart of `plotAttrs` (river / coast
  / hills / peak / fresh water over the radius tiles), beside the `BONUS_*`-keyed one that now exists
  ([contexts.md](../../architecture/contexts.md), owner). ⛔ The two are NOT merged: the key spaces are disjoint
  registries both starting at 0, so one store re-opens the cross-registry id collision the `CLS_` prefix closed
  (`ContextDict.h`). Nothing reads it yet, so it lands when a consumer wants it.
- **Declare `HAS_FRESHWATER`'s missing axes.** Its row enumerates most of the legs the live engine predicate
  reads but omits the impassability leg, which branches on the OWNING TEAM's pass-peaks capability — so neither
  an ownership change nor that tech re-derives the bit. ⚑ This is the standing cost of the
  [DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation) carve-out: the one row
  that calls a live engine predicate instead of reading stored bits has to transcribe its dependency set BY HAND,
  and a hand-transcribed set can silently under-declare. Any future row of that kind carries the same hazard.
- **Route the BUILDING grantor into the ability union when data authors one.** The building info already carries
  the block and the union folds only the tech leg, so a building-authored capability would be silently dropped.
  ⚠ No data authors one today — this lands WITH that data, never speculatively.
- **Collapse the per-dictionary plumbing into one mechanism** — owner resolution, dispatch, consumer class and
  registration are hand-written per family.
- **Emit the RECEIVED line** so the whole event flow is auditable live off `/events`
  ([event-spine.md](../../specs/event-spine.md) § THE RECEIVED LINE): a consumer announces that it ACTED on a
  fact, and a DOMAIN fact with no matching received line names a missing consumer route — the one gap form with
  no other observable signature. ⛔ `DIAGNOSTIC`, never `DOMAIN`.
- **Collapse the TWO disjoint segment interners** — `modSegmentIntern`/`s_modSegments` and
  `DepositIndex::internSegment`/`s_segs` intern the same concept in different id-spaces with different slot
  counts, only one carrying spell-back, and `di_pushFamilies` round-trips ints → string → ints between them
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
  ⚠ **Neither is the SEGMENT of "a package is made up of its segments"** — those two intern ADDRESS FRAGMENTS,
  while a package segment is a named SLICE of a channel total (`CvCascadePackage::substrateFlat` is the one
  realized instance). One word, two unrelated concepts; do not converge them by name.
- **Delete `te_applyDelta`'s hand-rolled copy of the membership fold** and route the TECH domain through
  `EnablerKernel::applyEdges` like every other domain — techs are the largest HAVE axis, so the one domain that
  bypasses the single writer is the one that matters most
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- **Clamp the enabler's membership refcounts, or make the guard survive the shipped build.** The negative-refcount
  `FAssertMsg` compiles out of `Release`/`FinalRelease` (`FASSERT_ENABLE` is Assert/Debug/Testing only) and
  neither changer clamps, so in a shipping build an over-release leaves the enable count negative and the next
  legitimate acquisition lands at zero — silently absenting the candidate.
- **Repair the `savemigration.txt` obligations naming `CascadeWellbeing`** — that class lives only in
  `SourceArchive/`, so those cut fields have no source at all ([save.md §3](../../specs/save.md)).
- **Stop handing `CvPlot*` out of `CityContext`** (`cityPlot`, `radiusPlot`) — the evaluator uses that hole to
  reach a second context, while the eval ctx already carries one. The isolation is meant to be structural.
- **De-instantiate `CvCascadeTally`** — a calculator is a static-methods holder, never an instance
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- **Reset the enabler's build-once latches**, or make them re-entrant: `rj_clearAllRepos` re-maps every info on
  the postmenu pass while `buildActiveIndex`, `bd_buildGateClasses`, `ud_buildClasses`, `bd_sbMembers` and
  `bd_cappedBuildings` latch permanently. `di_ensureDependencies` is a literal ensure-on-read behind four reads.
- **Announce the property bridge's fail-closed SKIPS.** ⚠ **The earlier disposition here — "collapse the second
  condition-evaluation surface" — is RETRACTED: it contradicts the LOCKED property spec**, which APPROVES this
  translator explicitly (owner-decisions #1/#2, [property-audit.md](property-audit.md): a small scoped
  `CvCondition` → legacy `BoolExpr` bridge, because the KEEP-legacy solver evaluates `BoolExpr` and the engine
  math is not to be rewritten). Collapsing it would be remodelling the property engine, which is banned.
  **PROVEN — and the fail-closed half is CORRECT, not the defect.** `condToBoolExpr` refuses what it cannot
  faithfully translate — a count threshold (`min > 1` / `max`), a `connection` qualifier, an unmapped predicate —
  returning NULL rather than applying the source under a wrong condition, and `entryActiveExpr` raises an
  explicit `bUntranslatable`.
  **PROVEN — what IS the defect is that the caller then drops it in SILENCE:** `bridgeFamilies` does
  `if (bUntranslatable) continue;` with no report, and the same for a non-`POPULATION` `per`. That is exactly
  what [triggers.md](../../specs/triggers.md) rules out — *"being fail-closed is right; being fail-closed AND
  silent is not: authored data that loads, never applies, and reports nothing is invisible on both axes at
  once"* — so each skip belongs on the ONE load-time census beside readJson's coverage counts, never a second
  reporting path.

## ⛔ THE PROPERTY-BAND ECOSYSTEM IS BROKEN FOUR INDEPENDENT WAYS

> The crime / disease / pollution / education building ecosystem — the largest `requires.operate` population in
> the data — is severed at four points at once. Fix them together; any one alone leaves it dead, and fixing the
> seed alone lights up the negative-band defect (below) as a crippling city-wide penalty.

- ⚠ **It fails INVISIBLY, not cleanly.** The re-check re-evaluates the WHOLE operate condition for whatever
  buildings some other event happens to seed — so a crime building that also requires a tech gets its crime band
  re-read incidentally the next time any tech lands. The verdict is right at unpredictable moments and wrong in
  between, per city.
- **A second band on the same property in one `requires.operate` tree silently overwrites the first** — the band
  map is filled by assignment, not accumulation.
- **`-1` as "no bound" recurs in the band THRESHOLD table**, same shape as the atom's `< 0` test. Fix both with
  one real absent marker (a has-bound flag, or the `MIN_INT` shape already used correctly for build-year).

## ⛔ SHAPE VIOLATIONS — the bespoke thing forcing bespoke machinery

- **⛔ `CvCondition` still carries RUNTIME STRINGS.** `type` and `param` survive as `std::string` MEMBERS into the
  runtime, read on both the apply path and the per-decision `expected*` read
  ([DEC-materialize-at-mapfrom](../../architecture/decisions.md#dec-materialize-at-mapfrom),
  [DEC-one-json-reader](../../architecture/decisions.md#dec-one-json-reader): nothing string-shaped survives load).
  The condition already carries the FK-resolved `id` and a `predKind` beside them, so the members are the residue,
  not the addressing.
  ⚠ **The earlier claim that "the evaluator routes every atom through prefix compares — dozens of them in one
  file" is RETRACTED — the PREDICATE dispatch is already interned:** the evaluator switches on `predKind` across
  **39** `case CASC_PRED_*` arms and carries exactly **ONE** `compare(0, …)`, in a single `en_starts` helper. So
  what is left is the string members and that one helper, never a per-atom string walk.
  ⚑ Why it is still worth closing: `CvCapabilities` records that a per-call `std::string` construction on the
  pathfinder *"4x'd the turn"*, and a string member on a structure read at decision cadence is the same shape
  waiting to be constructed.
- **⛔ `EnAllowedCap` fuses KIND × SCOPE into one enum, and it has already produced a WRONG GATE.** Two ladders
  collapse the team and empire arms into one branch, so a project's `empire:N` cap is enforced against a TEAM
  count while a tech's `team:N` cap is enforced against an EMPIRE count. That is
  [DEC-scope-is-an-axis](../../architecture/decisions.md#dec-scope-is-an-axis) failing as a live behavioural
  defect, not a naming preference. Re-cut as `cap(ALLOWEDCAP_SELF, <scope>)` — the tally already takes a scope, so
  all four hand-written kind→scope ladders delete.
- **Materialize the remaining per-call string reads in info getters** — the hide-and-seek detection row resolving
  an infotype by string at read time (on every visibility check), the deposit-address `lookupSegment` built fresh
  at its call site (the DEC's own worked example, reconstructed literally), and the trigger-promotion scans
  comparing `happening` strings per call.
- **Retire the `Global*` / `National*` scope fragments in names** — including newly-written `mapFrom` code that
  had the scope in hand as a parameter and threw it into the member name instead.
- **Re-home the condition-as-kind survivors** — kinds that answer WHERE/WHEN rather than WHAT (in-border
  experience, the three territory heal kinds, hills work-rate, the holy-city and NPC-peace scalars). Each has an
  existing predicate, and json.md rules a hill is `plots {HAS_HILLS}`, never its own kind.
- **Kill the `getX`/`getX100` pairs** — the maintenance getter reduces `Times100` internally, which is the exact
  pair [DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100) forbids, plus the saved-maintenance
  and unit-upkeep twins.
- **Spell out the bare single-letter identifiers** in the enabler/capability code (`j` for the info, `c`, `r`,
  `s`, `sb`, `jg`, `a`, `mem`, `wcap`). Per Sources/AGENTS.md this is a review-blocker on sight, and it is NOT
  the sanctioned exception — that covers a file-anchored PREFIX, never a bare parameter name.

## ⛔ SELF-HEAL FOSSILS — each one is a missing emit wearing a per-turn sweep

- **The whole map's visibility is wiped and re-applied EVERY TURN** in `CvGame::doTurn`, under a comment that
  says outright it is "a stickytape - can't find where it's skewing visibility counts". ⚑ The comment names the
  fix: an unpaired visibility increment/decrement leaves the per-plot counter outside its stated 0–1 invariant.
  Find the unpaired mutation; the sweep then deletes itself. Until then it costs a full-map clear plus a whole-map
  re-sight per turn ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
  **PROVEN — the unpaired mutations, mapped by the bracket census** (every sight add/remove goes through
  `changeAdjacentSight`; a skew needs a sight INPUT moving outside a remove/re-add bracket). Two are FIXED:
  `setTerrainType` carried no bracket while terrain authors elevation/obstruction (hill/peak — its three sibling
  setters all bracket; it now does), and a promotion moving `URS_VISION` on a STANDING unit re-brackets around
  the `setHasPromotion` commit (the lost BTS `changeExtraVisibilityRange` shape restored at the new commit
  point; the gate also covers a promotion providing/removing a vision-authoring combat class —
  RECON/EXPLORER/AIR_RECON author `vision.unit`).
  **⚖ RULED — THE SWEEP IS A KEEP; THE VISIBILITY ENGINE IS NOT REBUILT IN #430 (owner: "I am concerned about
  rebuilding the entire visibility engine").** The per-turn rebuild stays as the visibility engine's wholesale
  maintainer — it is LOAD-BEARING today, not just tape: it is the only thing applying the 29 vision-authoring
  buildings' city sight (the city's brackets exist only at init / kill / capital-move, so an activation never
  re-brackets in place). The CITY leg therefore stays unwired behind it, and no sight-record structure (the
  per-team sight-as-added exactness record the pairing fix would want) is built in this rework. A proper
  visibility pass is its own deliberate future work item — it naturally rides the vision / hide-and-seek ground.
  ⚠ The two landed brackets stay: they mirror existing sibling shapes, fix real immediacy bugs (a vision
  promotion / terraform now applies at once instead of at end of turn), and the sweep rebuilds over them
  harmlessly.
  ⚠ `doSetUnitCombats` (the era attach) reaches `setHasUnitCombat` outside the promotion funnel and is
  unbracketed by shape — inert today (no era class authors vision), covered by the sweep regardless.

## ⛔ A LEGACY PLANE ALIVE ONLY FOR PYTHON — and a value silently lost

- **An event's yield modifier is stored and LOST — it is event-granted persisted state that nothing folds.**
  `CvCity::m_aiYieldRateModifier` has exactly ONE writer, `applyEvent` (`CvCity.cpp`), and is serialized;
  `getBaseYieldRateModifier` is a pure cascade read (`rolledLegsAtCity`) and does not consult it; its only
  reader, `CyCity::getYieldRateModifier`, is declared but unpublished. So the value reaches nobody.
  ⛔ **NOT a legacy plane to cut, and not a package channel either** — a one-shot event grant has no live source
  to withdraw against, which is exactly what makes an accumulator unmaintainable
  ([state-repositories.md](../../architecture/state-repositories.md) § WHY DELTA-DERIVING FAILED BEFORE). It is
  the class [modifier.md §2b](../../specs/modifier.md) already sanctions for `extraHappiness`/`extraHealth`:
  serialized event state that the REALIZED read folds on top. The fold is what is missing.
- **The player twin is a serialized plane maintained solely to feed a binding.** `CvPlayer`'s
  `m_aiYieldRateModifier` is fed from building deposits and has no engine consumer at all — its only reader is
  `CyPlayer`. Exactly the shape [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) and
  [DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface) ban.
- **Cascade computes it, legacy is what consumers read** — the cascade slot beside each is dead and therefore
  unverified: `CvTeam::m_iEnemyWarWearinessModifier`, `CvCity::m_aiProductionToCommerceModifier` (fed from the
  rebuilt process info by a legacy push) and `CvCity::m_aBuildingCommerceChange` (fed from
  `CvPlayer::getBuildingCommerceChange`). Each is a `change*` push beside a live cascade read.
## Rollerskates — the abandoned path, still in the tree

> Evidence of a path someone tried and left behind
> ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)). It holds the NAMES
> of dead things, which is what sends the next agent re-treading them — and being preprocessor-skipped or
> commented, none of it is visible to the compiler census.

- **Record WHY each surviving `#ifdef` off-switch is off, in its subsystem's reference doc.** The abandoned
  alternates are gone; what is left are legitimate switches, and an UNEXPLAINED one is what the next sweep eats
  — the mechanical test cannot tell a deliberate off-switch from dead code ([AGENTS.md](../../../AGENTS.md)
  Conventions §Design: what is BEHIND the guard decides). Two are written down —
  `ENABLE_FOGWAR_DECAY` (broke hotseat) and `THE_GREAT_WALL` (rendering it caused CTDs), both in
  [special-systems.md](../../reference/special-systems.md). The rest are not.
  ⚖ **The PATHFINDING caches are NOT in that class and are wanted (owner)** — a path is the most expensive
  thing the engine does and is unmaintainable as derived state, *"it has to scan plots by its very
  definition"*. See [state-repositories.md](../../architecture/state-repositories.md) § THE SPATIAL CARVE-OUT.
  What is still worth asking of `PLOT_DANGER_CACHING` and `YIELD_VALUE_CACHING` is only whether a compile
  SWITCH belongs on them at all, not whether the cache should exist.

- **Strip the comment trails that name a DEAD symbol.** Keep the forward statement, delete the dead name — naming
  it is the bait. Worst offenders: `CvCity`
  (narrates a removed generic-citizen loop AND spells out how to revive it), `CvUnitAI` (`AI_bestCityBuild`
  "retained for any external callers but no longer used"), `CvPlayerAI` ×3, `CyTeam`, `CvPlayer`,
  `CvTriggerEngine`, `CvHttpServer`.
- **Rewrite the `CyGameTextMgr` half-state comment** — it says the composer bodies "were cut and are being
  rebuilt, so a method here answers empty until its composer lands." A doc/comment describing a half-state reads
  as a sanctioned shape; state what IS, and put the missing composer in this list instead.
- **Sweep the commented-out code**, worst in `CvUnitAI`, `CvPlayerAI`, `CvUnit`, `CvPlayer`. ⚑ Start with
  `CvPlayerAI`'s commented-out `AI_getHealthWeight` + its `getAdditionalHealthByCivic` calls — it sits directly on
  the wellbeing surface just cut, so it is the segment most likely to send someone after a getter that is gone.
  Two entire commented-out `AI_Potential*` functions kept "just in case" are the other exemplar. Most of the bulk
  is inherited C2C rot rather than #430 residue, so it is its own pass.

## ⛔ FOUND IN PLAY — the rendered surface, once the screens came back up

> These are owner observations from a live turn, i.e. the first look at the rebuilt composers with real data
> behind them. ⚑ They are grouped because they share a cause-class rather than a screen: the entry renderer is
> now the ONE surface every entity's effect lines go through ([patterns.md] category 5), so a defect in it is
> visible on every screen at once — which is also why each is cheap to fix in one place.

- **⚖ A CIVIC'S ENTRIES FILL THE SCREEN, SO OLD-vs-NEW CANNOT BE COMPARED (owner).** The civic screen renders
  the whole entry list as one block per civic, and a swap decision is inherently a COMPARISON — *"it's impossible
  to compare old and new civic when entries of 1 civic take the full screen"*. They need to sit **side by side**.
  ⚑ This is a LAYOUT question, not a renderer one: the per-entry lines are already tagged by family
  ([pedia-read-map] need-class 2 — entries arrive tagged so a page can group and lay out by family), so the
  comparison view is a consumer of the same rendered lines, never a second render path.
  ⚠ It also sharpens what the renderer owes: a side-by-side view wants the entries GROUPED, so a flat
  newline-joined blob is the shape to move away from.
- **⛔ A POLICY SCREEN NOBODY RECOGNISES, AND CLICKING IT DOES NOTHING (owner).** A screen reachable from the
  civics UI that the owner has not seen before and that is inert on click. ⚠ Do NOT assume it is dead and sweep
  it — `Forgetful` is the standing exhibit for a screen that reads exactly like abandoned code and is wanted
  ([python-read-map] §3.6). Establish what registers it and whether its handler ever bound, THEN decide.
- **⛔ ENTERING A CITY IS FAR SLOWER THAN IT SHOULD BE — "like it tries to calculate something upon entering"
  (owner).** The city screen is the densest read surface in the game, so it is exactly where a per-read walk
  that should be a bare fetch shows up ([contexts.md]: a read that walks per call is the efficiency defect to
  reject in review). ⚑ The suspects to check before theorising: a composer or screen asking a valuation
  per (building × candidate) on open, and the `expected*` what-if being called inside a loop rather than once
  per decision ([patterns.md]: `expected*` is a per-DECISION read; an AI caching its own scores is the sanctioned
  shape, re-asking it in an inner loop is not).
- **⚠ A CPP_EXCEPTION CRASH ON END TURN.** `code=0xE06D7363` (a thrown C++ exception, not an access violation)
  with `lastCyRead=CyState::canUnitAcquirePromotion` and a minidump beside it. ⚑ The breadcrumb names the last
  PYTHON→C++ read, which is not necessarily the faulting frame — the wrapper itself guards both ids and cannot
  throw, so the throw is inside the engine's own `canAcquirePromotion` or somewhere after it. Symbolize the dump
  before theorising ([external-tools-and-workflows.md](../../reference/external-tools-and-workflows.md) carries
  the cdb recipe; use the **x86** debugger — the game is 32-bit).
  ⚠ **RE-TEST BEFORE INVESTIGATING.** `CombatDetails` was being pushed to the `combatLogCalc` / `combatLogHit`
  events with no `class_<>` registration, which threw this same code out of boost::python's to_python on every
  fight involving a human — the player attacking, and the AI attacking the player during its end-turn. That
  converter is now registered. Whether any end-turn throw SURVIVES it is unestablished; the breadcrumb above is
  consistent with either, so do not assume this entry is separate until a fight-free end turn still crashes.

## AUTOMATED POPULATION PLACEMENT IS SLOW -- ~200ms PER CITY

**OBSERVED (owner, in play):** toggling automated population placement ON and OFF across **26 cities takes
5-6 seconds** -- on the order of **200ms per city per toggle**. That is the cost of a re-assignment, not of a
decision anyone is waiting on.

**PROVEN: it is a COST problem, not a placement-QUALITY one.** The owner's measurement is wall clock on a
toggle, so whatever the scorer picks is not the complaint -- do NOT open this by auditing `CvCityAI`'s tile
valuation.

⚑ **The standing suspect is the same one already measured on the slider path**
([modifier.md §2a](../../specs/modifier.md)): a setter flagged every city for re-assignment and ONE slider tick
stalled ~15s across the empire, over a decision whose inputs had not moved. Same shape, smaller n -- so the
first question is not "why is one assignment slow" but **how many assignments does one toggle run, and how much
does each re-derive that nothing invalidated.**

**PROVEN -- WHAT THE TIME IS SPENT ON (owner): it RECALCULATES THE CURRENT AI VALUE before reassigning.** So the
200ms is a per-city re-valuation, not the placement step.

⛔ **AND THAT RECALCULATION IS NOT THE DEFECT -- making the AI value derivable state is OUT OF SCOPE, not
impossible (owner).** It could be done; it is simply not this work. Recording the distinction because the two
read the same in a hurry and are opposite: an impossibility closes the option, a scope ruling parks it
([DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas) -- un-killed forward intent
is kept, so do not retire the idea, and equally do not start it here).
⚑ Today an AI score is a heuristic the asking side owns, not a cascade quantity: `expected*` is specified as
a per-DECISION read and an AI that wants repeated access caches its OWN scores, the sanctioned AI-heuristic
residual ([patterns.md](../../architecture/patterns.md); [superseded-ideas #1](../../architecture/superseded-ideas.md)).
So "make it a maintained store" is NOT the fix available in THIS issue -- an agent opening this one by
event-maintaining the AI value has widened the work rather than closed the defect.

⇒ **What is therefore actually in question is narrower, and it is two things:**
- **Does the valuation's own INPUT surface do per-read work it should not?** The scoring is legitimately
  recomputed; what must still be O(1) is everything it READS
  ([contexts.md](../../architecture/contexts.md): *"a predicate that walks plots/units per call is the
  efficiency defect to reject in review"*). A re-valuation is only as expensive as its reads.
- **How many times does one toggle run it, and does toggling OFF need it at all?**

⚠ **NOT YET KNOWN:** whether the 200ms is one expensive pass per city or many cheap ones; and whether toggling
automation OFF should re-assign at all -- turning it off hands control back to the player and need not re-decide
anything. Establish the CALL COUNT before optimising any single pass.

⚑ Behaviour as it stands today is described in [citizen-assignment.md](../../reference/citizen-assignment.md);
the per-read-scan class this most likely belongs to is [contexts.md](../../architecture/contexts.md)
(*"every evaluator predicate is an O(1) CONTEXT fetch -- a predicate that walks plots/units per call is the
efficiency defect to reject in review"*).


## THE PYTHON HALF OF THE Cy DISCONNECT IS NOT DONE

**PROVEN — measured:** `GC.get*Info` accessors — **215** call sites, **30** distinct accessors, **39** files
(`grep -rhoi '\bgc\.get[A-Za-z]*Info(' Assets/Python`, counting both the `GC.` and `gc.` spellings).

⛔ **The `CyCity` call-site figure this entry used to carry is WITHDRAWN, not updated — it cannot be measured
that way at all.** It came from matching `CyCity` METHOD NAMES across the Python tree, and those names collide
with other receivers: the top hits are `getPlayer`, `getTeam`, `getCity`, `plot` and `area`, which are
`CyGlobalContext` / `CyPlayer` / `CyPlot` reads far more often than city ones. A name match therefore returns
thousands of sites that are not city calls, which is precisely what
[python-read-map.md §1.1](../../reference/python-read-map.md) says of its own method — *receiver-name heuristics
are useless here*. ⇒ Re-derive with `python Tools/census-python-boundary.py`, never by grepping a method name.

⚠ **`CyCity` publishes far more than the identity set, and an entry naming a read as dead is stale on sight.**
Beside `getOwner`/`getID`/`getX`/`getY` it carries the coherent GROUP reads (`getYields`, `getCommerces`,
`getWellbeing`, `getYieldTerms`, …) and a run of named concepts (`getName`, `getPopulation`, `getTradeRoutes`,
`isProductionUnit`). ⛔ Check the published surface — **and the LOADERS, not just `CyCity.cpp`** — before
working any row that calls a named city read dead.

⛔ **THE FAILURE MODE IS WHAT MAKES THIS DANGEROUS TO ESTIMATE FROM SYMPTOMS.** Python takes the surface with
`from CvPythonExtensions import *` — 169 files star-import it against 6 with an explicit list — so **nothing
declares a dependency and nothing fails at import**. Each dead call surfaces only when its code path runs, i.e.
one panel at a time, as a player happens to open it. A screen that opens today is not a screen that works: it
means you have not yet pressed the button that reaches its dead call.
⚑ And a raising `__init__` leaves `None`, so every later `handleInput`/`onClose` on that screen fails too — one
dead binding produced five distinct tracebacks in the espionage advisor.

⚠ **A file with zero `GC.get*Info` is NOT fixed.** They are two independent populations. `CvDomesticAdvisor` had
all 15 of its `GC.get*Info` calls cleared and still will not open, because 108 dead `CyCity` calls remain. Do
not read a clean grep for one as progress on the other.

**⚖ THE RULE FOR EACH SITE IS SETTLED (owner, worked repeatedly): the binding does not come back
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).** Check whether the read already exists
on `INFO`/`STATE` — several did, and `CvAdvisorUtils` was already calling one of them correctly; grow a slot
when it does not; and where no read exists and the value only feeds a heuristic, **the rewrite does not need to
be faithful — it needs to show the info as it is today (owner)**, so an honest simplification beats a
fabricated number.

⚑ Worked precedent for the biggest case: `RawYields` was hand-computing a yield breakdown through this API. It
was not re-bound — it now reads `CyState::getCityYieldTerms`, the SAME decomposition the `/computed` census
renders, because a tooltip IS a census and two computations of one number drift.


## THE `CyCity` WRAPPER STILL DECLARES ~280 UNPUBLISHED METHODS — the C++ half of the same disconnect

**PROVEN — measured.** `CyCity.h` declares **372** methods; `CyCity::pythonPublish` emits **90** `.def`s — the
identity set plus the coherent group reads and named concepts. The remaining **~282** are **unbound outlaws**:
each compiles, forwards to a live `CvCity` method, and no script can ever reach it.
⚑ The published half has grown substantially as reads were re-pointed; the DECLARED half has not shrunk with
it, which is the whole of this entry. ⛔ Re-count both before quoting either — the gap closes from one side.

⚠ **This is the OTHER half of the entry above and must not be conflated with it.** That one counts dead PYTHON
call sites; this counts dead C++ WRAPPER methods. Clearing either leaves the other standing, and a clean grep
for one says nothing about the other.

⛔ **The disposition is settled and needs no ruling: DELETE.** *"A dead legacy Python getter is an OUTLAW, shot
on sight"*, and for a `Cy` binding the ONLY fix is deletion — never re-pointing or widening
([roadmap.md](roadmap.md); [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).

**⛔ WHAT MUST SURVIVE THE CUT, verified — deleting the file wholesale would break the KEPT direction.**
- **The `class_<CyCity>` registration itself**, carrying its four defs. It is the type IDENTITY that lets a city
  cross the boundary at all; the marshaller throws at runtime without a registered converter
  ([patterns.md](../../architecture/patterns.md)).
- **The constructor and the underlying-pointer accessor.** `CyCity` is CONSTRUCTED in six engine/wrapper sites —
  `CvGameObject::createPythonWrapper`, `CyGame::getHolyCity`/`getHeadquarters`, `CyPlayer::initCity`/`getCity`/
  `getCapitalCity`/`firstCity`/`nextCity`, `CyPlot::getPlotCity`/`getWorkingCity`, `cyGetCity` — and several
  wrappers take a `CyCity*` PARAMETER and unwrap it (`CyPlayer::acquireCity`,
  `CyPlayer::getAdvancedStart*Cost`, `CyPlot::isConnectedTo`, `CyGame::cityPushOrder`,
  `cyPlotCityXYFromCity`). Those callers are the KEPT engine→Python direction.
⇒ **So the cut is "everything but the identity set, the ctor and the unwrap accessor", not the file.**

⚑ **Two were already taken as a worked precedent** (`getImprovementFreeSpecialists` /
`changeImprovementFreeSpecialists`) — no `.def`, so unbound; their one Python caller sits in
`WorldBuilder.copyCityStats`, which raises today and is covered by §12's block. Nothing that worked stopped
working.
⚠ **`CyUnit` is the same shape and wants the same census** before either is called done.

## Engine→Python IDENTITY conversion left 46+ handlers dereferencing a tuple

**OBSERVED.** `PythonDbg.log`, every turn with combat: 193 BUG-caught handler errors, 82% of them
`combatResult`, reading `'tuple' object has no attribute 'getOwner' / 'isMadeAttack' / 'getUnitType'`.
⚠ These do NOT appear in `PythonErr.log` — BUG catches them inside its own dispatch
([external-tools-and-workflows.md](../../reference/external-tools-and-workflows.md)).

**PROVEN — it is not a defect, it is an unconverted consumer set.** `CvUnit*` and `CvCity*` now cross to
Python as their `(owner, id)` IDENTITY rather than as a `Cy*` handle:

```
Sources/Python/CyUnit.h:203   DECLARE_PY_IDENTITY(CvUnit*, getOwner(), getID());
Sources/Python/CyCity.h:420   DECLARE_PY_IDENTITY(CvCity*, getOwner(), getID());
```

`CvPlot*` and `CvSelectionGroup*` still cross as wrappers. The handlers still do
`CyUnitW, CyUnitL = argsList` — the unpack SUCCEEDS and the elements are tuples, which is why the failure
reads as a mystery rather than as a missing binding.

- **39 engine events pass a unit or a city** (`CvDllPythonEvents.cpp`, every `report*` taking `CvUnit*`/`CvCity*`).
- **85 Python handlers subscribe to them; at least 46 dereference the passed object** across 11 files —
  `Contrib/autologEventManager.py` 17 · `CvEventManager.py` 15 · `Revolution/RevEvents.py` 4 ·
  `CaptureSlaves.py` 2 · `Partisan.py` 2 · six files with one each.
- ⚠ **46 is a FLOOR.** The census keyed on `= argsList` at end-of-line and therefore MISSED any unpack with a
  trailing comment (`autologEventManager.py` is exactly that). Re-run it with that fixed before trusting a total.
- ⚑ **The `combatResult` slice is CONVERTED and is the worked precedent for the rest.** All four of its handlers
  (`CvEventManager` · `autologEventManager` · `CaptureSlaves` · `Partisan`) unpack the identity pair and read
  through `STATE.getUnitRead` / `getUnitFlags`, so captives and unit capture run again. It was the largest
  contributor to the log above; the remaining worklist below is what is left.

**⚖ THE LOGS ARE THE PRIORITY ORDER; THE CENSUS IS THE UPPER BOUND.** A session's `PythonDbg` shows what FIRED,
so it is a far shorter list than the census and is the right thing to work down — measured over one late-game
session, the 278 deref errors were only SEVEN events (`combatResult` 189 · `unitBuilt` 36 · `buildingBuilt` 33 ·
`cityDoTurn` 17 · `unitSpreadReligionAttempt`/`techAcquired` 1 each, plus `CvGameUtils.cannotMaintain`, which is
a `BugGameUtils` callback rather than an event handler and which no handler census counts).
⛔ **But a clean log is NOT a finished surface, and must not be read as one:** a session exercises a narrow
slice, so the next one that opens the victory screen, runs WorldBuilder or generates a map surfaces a fresh
batch. **424 `GC.get*Info(` call sites survive across the tree** — every one an `AttributeError` when its path
runs ([python-read-map.md](../../reference/python-read-map.md) is the standing census). Treat log-clean as a
checkpoint that is reachable and verifiable, never as completion.
⚑ **Re-measured since: 215 sites / 35 distinct accessors / 39 files** (`grep -rhoi '\bgc\.get[A-Za-z]*Info(' Assets/Python`,
counting BOTH the `GC.` and `gc.` spellings). Roughly half the population above has already gone, which is the
work landing — ⛔ but re-run the grep rather than quoting either number, exactly as
[python-read-map.md §1.1](../../reference/python-read-map.md) says of its own totals.

**⚠ THERE ARE TWO ERROR CLASSES IN THOSE LOGS AND THEY NEED DIFFERENT FIXES — reading them as one is why a
first pass under-scopes.** The tuple deref above is one; the other is a read the new surface does not publish
(`GC.get<X>Info` is published NOWHERE by ruling, so it is an `AttributeError` at the moment its handler fires,
not a slow read). ⛔ The second class is never fixed by re-adding the legacy binding
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)) — it converts onto `CyInfo`/`CyState`,
or, where the engine hands a TYPE across, by restoring the `class_<>` registration alone with zero `.def`s
([patterns.md](../../architecture/patterns.md)).
⚑ **A logged failure is reliably a floor on its own site count**: each of that class's seven logged items was
between 1 and 23 actual sites in the tree, because only the paths that ran reported.

**RULED OUT.** Not the contract broker, and not a regression from any recent change — the identity conversion is
the deliberate IDENTITY SET ruling ([patterns.md](../../architecture/patterns.md)) landing ahead of its consumers.

**The conversion PATTERN is settled** — `pyWB/CvWBDesc.py` is the worked precedent: unpack the identity, read
through `STATE.xxx(iOwner, iId)`, write through `CyAct`. `.getOwner()` becomes free (element 0).

**WHAT MAKES THIS BIGGER THAN A PYTHON SWEEP — a converted handler drags reads with it.** `combatResult` is the
measured instance: serving its four handlers took the union below, roughly half of which had to be ADDED to the
surface rather than re-pointed. Expect the same of each remaining handler — the unpack is the cheap half.

| already served | had to be ADDED |
|---|---|
| `getOwner` (free) · `getUnitPosition` · `getUnitRead[UNIT_READ_TYPE\|_DOMAIN]` · `hasUnitPromotion` · `hasUnitCombat` · `getCaptureKinds` | `isMadeAttack` · `isAnimal` · `getCaptureUnitType` · `getUnitCombatType` · `getExperience` · `isHuman`/`isNPC` · a position→`CyPlot` path for `plot()` · a **CyAct** route for `setDamage` and `changeExperience` |

**⛔ A PROMOTION GRANT NEVER CHECKS ACQUIRABILITY, AND ADDING THAT CHECK BREAKS IT (owner).** Events grant
promotions that sit OUTSIDE the normal promotion list — *"they cannot be taken by xp, they can just be
granted"* — so `canAcquirePromotion` (the LEVEL-UP question, which `CyState::canUnitAcquirePromotion` serves
with `bForFree` defaulted false) refuses them BY CONSTRUCTION. ⚑ The mechanism is the event free-promotion
registry keyed by UNITCOMBAT (`CvPlayer::isFreePromotion`, written only by `applyEvent`): the event names the
combat classes it grants to and then fans over every matching unit
([legacy-grant-apply-sites.md](../../reference/legacy-grant-apply-sites.md) §4 marks that leg event-owned and
out of scope).
⇒ **GRANT path ⇒ `ACT.setUnitPromotion` and nothing else** (which is what the already-converted sites do);
**LEVEL-UP/offer path ⇒ `STATE.canUnitAcquirePromotion`.** ⛔ A conversion PRESERVES whichever the original
had — adding a gate silently drops event promotions, and removing one grants promotions a unit cannot hold,
since `ACT.setUnitPromotion` does NOT validate (`setHasPromotionInternal` applies unconditionally).

**⛔ GREP THE WHOLE PUBLISHED SURFACE BEFORE ADDING A READ — one class is not the surface.** `CyEnabler` was
given a `canAcquirePromotion` that `CyState::canUnitAcquirePromotion` already served, identical body — the
does-the-same-thing failure [patterns.md](../../architecture/patterns.md) names, reached by checking the class
the concept *sounded* like it belonged to instead of every class. The check is one command and it is the FIRST
move, not a review step: `grep -rn '\.def("[^"]*<Concept>' Sources/Python/ Sources/Infrastructure/CvPython*Loader.cpp`.
⚑ A live per-object question lands on `CyState` even when it reads as availability, which is exactly why a
single-class grep misses it.

**⛔ A LITERAL `.def("` GREP UNDER-REPORTS THE PUBLISHED SURFACE, IN TWO DIFFERENT WAYS — and believing one is
how a read gets added that already exists, or a working read gets "fixed".**
- **The surface lives in the LOADERS, not in the `Cy*.cpp` files.** `CyPlayer.cpp`, `CyPlot.cpp` and
  `CyGlobalContext.cpp` publish NOTHING; their defs are in `Sources/Infrastructure/CvPython{Player,Plot,GlobalContext}Loader.cpp`.
  Grepping the type's own file concludes the whole wrapper is dead when it is richly published.
- **Some defs are MACRO-GENERATED and carry no literal name at all.** `DO_FOR_EACH_EXPOSED_INFO_TYPE` /
  `DO_FOR_EACH_EXPOSED_INT_GLOBAL_DEFINE` expand to one def per entry, so `getUNIT_WORKER` and its kin are
  published while no `.def("getUNIT_WORKER"` string exists anywhere.
⇒ **Search the loaders too, and expand the macro tables before calling a read unserved.** ⚠ The failure is
one-directional and quiet: it never reports a read as missing that is present, it reports a PRESENT read as
missing — so the wasted work looks like ordinary work.

**⛔ A CITY REACHES PYTHON IN TWO DIFFERENT REPRESENTATIONS, BOTH LIVE, AND WHICH ONE A SITE HOLDS DECIDES THE
CONVERSION.** They are easy to conflate because both are "the identity":
- **An identity TUPLE** — anything the engine PUSHES (`Cy::Args() << pCity`, so every `CvEventReporter` argument)
  crosses through `DECLARE_PY_IDENTITY` as a plain `(owner, id)` pair. It has no methods at all; every `.getX()`
  on one is the tuple-deref failure.
- **An identity HANDLE** — anything a published accessor RETURNS (`CyPlayer.cities()`, `getCapitalCity()`,
  `CyPlot.getPlotCity()`) is a real `CyCity` carrying exactly four defs: `getOwner` / `getID` / `getX` / `getY`.
⇒ **A handler argument is unpacked; a handle is ASKED for its address and then read through `STATE`/`ACT` by that
address.** ⛔ Do not "convert" a handle's `getID()` into an unpack — it works — and do not leave a pushed
argument's `.getID()` standing because a handle elsewhere in the same function has one.

**⚠ CONVERTING A HANDLER CAN INTRODUCE AN `UnboundLocalError` THAT WAS NOT THERE BEFORE — the `MAP` shape.**
Several long handlers rebind a module global inside one branch (`MAP = GC.getMap()`), which makes that name a
FUNCTION-LOCAL for the WHOLE body. Adding a use of it EARLIER — which the conversion does constantly, since a
city's plot is now reached through the map — raises at runtime in branches that never touched it. ⇒ When a
conversion starts using such a name earlier than its first assignment, bind it ONCE at the top and drop the
per-branch rebinds; the rebinds were there for per-call freshness, which one top-level binding preserves.

⛔ **Do NOT half-convert.** A handler whose args are ids while its body still calls a method that does not exist
RELOCATES the failure ([roadmap.md](roadmap.md) § scope decision 6) — finish a handler or leave it untouched.
⛔ And do not borrow a legacy read to fill a gap: ADD the read to the library
([DEC-no-legacy-masking](../../architecture/decisions.md#dec-no-legacy-masking)).
⚠ `Assets/` is the live game, so Python edits ship the instant they are saved — never edit while the game runs.

**⚖ EACH FIXED HANDLER ALSO OWES ITS SPINE FACT (owner).** A handler that calls back to C++ rides a happening
that today reaches Python and nothing else, so `CvEventReporter` emits a spine fact beside its Python call in
the same work item — an ADDITION, never a conversion of the reporter. The rule, the reporter-minus-existing-facts
subtraction, the per-method KIND test and the raw-not-formalized bar live at
[event-spine.md § `CvEventReporter` EMITS SPINE FACTS](../../specs/event-spine.md); they are not restated here.
⚑ For `combatResult` that is `SEVT_COMBAT_RESULT` beside the ~10 surface additions above — and it is what makes
the conversion verifiable at all, since a fixed handler is then observable on `/events` rather than inferred
from the ABSENCE of a `PythonDbg` traceback.

## The identity conversion is NOT closed — the log names the FIRST failure, never the handler

**⛔ A `PythonDbg` line reports the FIRST exception in a handler invocation, so fixing it EXPOSES the next one.**
An event is closed only when every one of its handlers has been read END TO END — never when its logged error
stops appearing. ⚑ Two events looked closed on exactly that reasoning and are not: `cityDoTurn` (its logged
error was a handler whose BODY was still unconverted) and `BeginGameTurn` (its logged error
was the `CvRandom` marshalling, with five dead calls after it in the same handler).

**⛔ AND BUG FANS ONE EVENT TO EVERY REGISTERED HANDLER.** The log names the EVENT, so a second handler for the
same event is invisible in it — `autologEventManager` alone carried seven broken handlers where the log could
only ever show one. ⇒ Enumerate `def on<Event>` across the whole tree before calling an event done.

**⚠ A HALF-CONVERSION LEAVES A DANGLING NAME, which fails differently.** A handler that reads its
head through `STATE` and then drives a `CyCity` that is **never bound anywhere in the function** — so it raises
`NameError`, not the `AttributeError` the tuple-deref class produces. Grepping for the deref shapes alone does
not find it ([roadmap.md](roadmap.md) § scope decision 6: half-converting RELOCATES the failure).

### The remaining worklist, by handler

The receivers below are city/unit HANDLES or pushed IDENTITIES.

⚠ **The rows are STALE ON THE CITY SIDE and must be re-checked per method, not trusted.** They were written when
`CyCity` carried the identity set alone; it now publishes 90 reads, so several names here — `getName`,
`getPopulation` among them — ARE served, while the mutators (`change*`/`set*`) and the unit-side reads are not
(`CyUnit` still publishes exactly four). ⇒ Check each method against the published surface before working a
row; a row is not a worklist entry merely because it is written down.

| file · handler | dead calls |
|---|---|
| `Contrib/WoodlandCycle.onBeginGameTurn` | `canFight` · `changeDamage` · `getHP` · `getName` · `kill` |
| `CvEventManager.onNukeExplosion` | `getName` · `getUnitType` · `kill` |
| `CvEventManager.onCityBuilt` | `changeFood` · `changeHasBuilding` · `changePopulation` · `getExperience` · `getUnitType` · `growthThreshold` · `isHasPromotion` |
| `CvEventManager.onCityRazed` | `findHighestCulture` · `getConscriptUnit` · `getCulture` · `getName` · `getNumWorldWonders` · `hasBuilding` · `isRevealed` |
| `CvEventManager.onGreatPersonBorn` | `changeCulture` · `getAddedFreeSpecialistCount` · `getCultureThreshold` |
| `CvEventManager.onChangeWar` | `addProductionExperience` · `changeHappinessTimer` · `getConscriptUnit` |
| `CvEventManager.onCityLost` | `plot` |
| `CvEventManager.onModNetMessage` · `freePromotions` · the two name-popup callbacks | `changeHasBuilding` · `changeHurryAngerTimer` · `flatHurryAngerLength` · `kill` · `maxMoves` · `setExperience` · `setHasPromotion` · `setMoves` · `isFound` · `getName` · `getNameNoDesc` |
| `Contrib/autologEventManager` — `onCityAcquired` · `onCityBuilt` · `onCityRazed` · `onCorporationRemove` · `onCorporationSpread` · `onImprovementBuilt` · `onImprovementDestroyed` · `onReligionRemove` · `onReligionSpread` · `onUnitPillage` · `getUnitLocation` | `getName` · `plot` |
| `DancingHoskuld/CaptureSlaves.onCityRazed` | `changeFreeSpecialistCount` · `getFreeSpecialistCount` · `getName` · `getPopulation` · `getSpecialistCount` |
| `DancingHoskuld/Partisan.onCityAcquired` | `changePopulation` · `getName` · `getPopulation` · `isOccupation` · `plot` |
| `CvGameUtils.getWidgetHelp` | the widest single site — ~30 city reads (`foodDifference` · `getCultureLevel` · `getProductionNeeded` · `isHolyCityByType` · …) |

⚠ **`Revolution/RevEvents.onBuildingBuilt` is broken and is deliberately NOT on this list.** Nothing registers
it — `RevolutionInit` imports the module and binds only `kbdEvent` / `GameStart` / `OnLoad` — so it cannot fire,
and Revolution is a carve-out owed its own rework. ⛔ Do not "fix" it into the live set.

