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
NON-LOCALLY when terrain or a route changes, so it is not a sum and the maintained-sum shape does not reach it;
[legacy-value-calc-map.md](../../reference/legacy-value-calc-map.md) already classifies `cultureDistance` as
**SPATIAL (#429-adjacent)**, a permanent carve-out. The mechanic stays.

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

**⛔ THE STORED-vs-ORACLE TRIPWIRE CANNOT CATCH A REGRESSION HERE.** The pair works only because its two sides are
independent derivations. The plot group is an **INPUT to both**: the oracle's operate fixpoint resolves `requires`
through `getNumBonuses`, which relays to the same group the stored side read. So a wrong network is INHERITED by
both sides and the diff stays GREEN — the same-derivation failure
([superseded-ideas #17](../../architecture/superseded-ideas.md)) arriving through the input rather than the
comparison. ⇒ **Any change here needs verification built for it FIRST.** "Cut it and let the tripwire catch it"
does not apply and must not be assumed.

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

## 8. The LIVE Python failures — what actually fires, as opposed to what the census counts

**Method (reproducible):** load the standing save with the game up and read
`Documents/My Games/Beyond the Sword/Logs/PythonErr.log`. ⚑ **This is a FLOOR, never a census.** The log records
only what has RUN, so a load with no turns played and no screens opened exercises almost nothing — the per-turn
handlers, advisors, pedia and city screen are all still unmeasured. Play turns and open screens to grow it.
⚑ **Why it beats the static count:** [python-read-map.md](../../reference/python-read-map.md) measures 2,070
unserved names / 21,279 call sites but states plainly that REACHABILITY is not provable from the Python tree (XML
callbacks and BUG config decide what executes). The log is the only thing that says which of them run.

| failing read | path | note |
|---|---|---|
| `CyCity.getName` (and `getPopulation` / `getFood` behind it) | `CvWBInterface.writeDesc` → `CvWBDesc.write` | WORLDBUILDER. ⛔ **NOT accepted breakage** — scope decision 1b is about sequencing and about what may constrain a cut, and the owner has ruled plainly: *"we cannot accept actually breaking worldbuilder stuff, we fix things we see."* ⚠ NOT one method: `CyCity` publishes only the IDENTITY SET (`getID`/`getOwner`/`getX`/`getY`), so the whole city writer goes onto `CyState` reads by ADDRESS |
| `CyGlobalContext.getBonusInfo` | `MoreCiv4lerts.buildBonusString` | the same defect as entry 3 |

**PROVEN — the shape of the demand is narrow.** Every live failure is one of two kinds: an **info-registry read**
(`getBuildingInfo` / `getBonusInfo`) or **basic object state** (`getOwner`). That is the demand-driven seed for
the replacement library — serve what actually fires, in the order it fires
([observability.md](../../reference/observability.md): the investigation names the read, not a sweep).

⛔ **The fix is to SERVE the read on the new surface, never to restore the binding**
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)). ⚠ A handler RAISING is the intended
interim state — it stays visible rather than being silenced ([roadmap.md](roadmap.md) § the mutating Python
handlers).

## 9. The tech tree draws too many connector links

**Observed:** the tech-tree screen renders more connector arrows than wanted. ⚠ **Not blocking** — the tech
advisor works: colours, queueing and links all render. This is the *which relation should the tree show*
question, deliberately deferred (owner: *"I want the layer working, before nitpicking"*).

**PROVEN — where the arrows come from now.** `CvTechChooser.interfaceScreen` draws from the **enables** edge:

```python
for iTechX in INFO.getEdgeIds("TECH_", iTech, EdgeFamily.EDGEF_ENABLED_BY, EdgeBucket.EDGEB_TECHS):
```

That is the right RELATION: `enables` is the sole authority on tree membership
([enabler.md](../../specs/enabler.md) §1/§2) while `requires` is only the GATE. It also disposes of the OR
problem structurally — an OR-group means "any ONE of these", so an arrow per member would read as "all of these
are required", the opposite of what the group says.

**PROVEN — the data, so density is not mistaken for double-drawing.** 944 techs carry 1,915 prereq edges,
mean 2.03 per tech, max 8 (277 techs with 1, 415 with 2, 197 with 3). The AND and OR lists do not overlap, so
nothing is drawn twice.

**RULED OUT — two earlier sources, so neither is re-tried.**
- `PYLIST_PREREQ_OR_TECHS` alone (the original): 934 of 944 techs carry NO OR-group, so the whole tree drew
  links for **five** techs. That is why the tree rendered with cells and no links at all.
- AND + OR together: correct data, wrong relation, and visually dense.

**NOT YET KNOWN — which SUBSET the visual tree wants.** Candidates, none decided: every `enables` edge (today);
a transitive reduction (drop an edge implied by a longer path); or a primary/spine edge per tech with the rest
shown only on the tech's own page. ⚑ This is a DISPLAY decision, not a data one — the edges are correct either
way. Worth tracing what the pre-rework tree actually drew before choosing.

⚑ Also mirrored as GitHub issue #454.

---

## 10. The finance advisor rebuilds the empire's commerce by walking every city and plot

**Observed:** `CvFinanceAdvisor.drawBase` raises `AttributeError: 'CyCity' object has no attribute
'getCityIndexPlot'`, and behind it `isWorkingPlot` / `getTradeRoutes` / `getTradeCity` /
`calculateTradeYield` / `getCorporationYield` — all on `CyCity`, which carries only the IDENTITY SET.
⚠ Its two `yCommerceSlider` errors are DOWNSTREAM of this: `interfaceScreen` aborts before the attribute is
set, so `update`/`onClose` then fail on it. They are not a second defect and should clear with this one.

**PROVEN — the walk computes nothing.** The panel takes the real total in ONE call
(`CyPlayer.calculateTotalYield(YIELD_COMMERCE)`) and the entire loop exists only to ATTRIBUTE that total
across five labels — worked tiles, domestic trade, foreign trade, corporations, specialists — with the sixth,
buildings, taken as the RESIDUAL (`iBuildings = iTotalCommerce - iCommerce`). No bucket is a number the
machine does not already hold.

**PROVEN — it is the cost class the model exists to delete.** The shape is every city × 21 plots × every
trade route × every specialist type, per draw. `getPlotYield` is named in
[state-repositories.md](../../architecture/state-repositories.md) as **a DELETION, not a value to re-home**,
measured there at *913M plot reads in one turn* for exactly this per-read walk.

**RULED OUT — it is NOT the per-source decomposition the oracle owns.** That rule governs
`(scope × channel × SOURCE)` accumulators — per building, per bonus. These five buckets are the
[modifier.md §2a](../../specs/modifier.md) BASE TERMS (plot yield · `tradeYield` · specialists · corporation ·
building flats), which is a different and legitimate question. ⛔ Do not close this by reviving per-source
getters, and do not cite the oracle rule to park it.

**NOT YET KNOWN — whether the five-way split survives at all.** The empire TOTAL is already served
(`calculateTotalYield`; `STATE.getCommerces(iPlayer, -1)`, which this same screen's income line already
uses). The SPLIT is stored nowhere — a package holds Σflat/Σpercent per channel, not per term — so it is not
a read that was cut, it never existed as a stored quantity. Either it collapses to the total, or it earns a
defined decomposition read against the §2a terms.
⚑ [patterns.md](../../architecture/patterns.md) makes display shape DEMAND-DRIVEN and END-STAGE, which is the
argument for collapsing it and letting a real request bring it back.

---

## 12. The WorldBuilder SCREENS mutate through the city handle, which now carries the identity set only

**Observed:** every `pCity.<mutator>` call under `Screens/Worldbuilder/` is dead. `CyCity` publishes exactly
four defs — `getOwner` / `getID` / `getX` / `getY` ([patterns.md](../../architecture/patterns.md) THE IDENTITY
SET) — so each of these raises `AttributeError` the moment its handler fires.

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

## 13a. The INVISIBILITY MEMBERSHIP TEST never asked whether the unit hides that way

**⛔ THE EARLIER HEADLINE HERE WAS WRONG AND IS RETRACTED.** It read *"`getBestDefender` returns NULL
everywhere — 64,485 of 64,485"*, on the argument that `defenderValue` cannot return 0 without an attacker, so a
zero score could only come from the three early returns. **`getDefenderScore` opens `int iValue = 0` and assigns
only INSIDE its gate**, so a zero needs no early return at all — and `getPreferredCenterUnit` calls
`getBestDefender` FIVE times, four of which return NULL in ordinary situations (call 1 tests `canMove()`, calls
3–5 exclude the active player's own units via `unitX->getOwner() != eAttackingPlayer`). The `[GFX] defenderScan`
emit fires exactly when a scan found nothing, so *"every emitted scan has `scoredPositive=0`"* is what CORRECT
code produces on the fall-through. **The measurement was not diagnostic; no defender defect is established.**

⚑ **What settles it is the reject histogram, not another scan count** — `[GFX] defenderReject` names which of
the three early returns fired, and `reason=ownerMismatch` is the normal case:
`grep -a defenderReject Graphics.log | grep -ao "reason=[a-zA-Z]*" | sort | uniq -c`.
`AI_getPredictedHitPoints()` resets to **-1**, not 0, so the predicted-dead clause is not a universal killer.

**⛔ WHAT WAS ACTUALLY FOUND, AND IS FIXED.** `CvUnit::hasInvisibilityType` was a pure NEGATION filter —
`!isNegatesInvisible(e) && !hasSkill(NO_INVISIBILITY) && getNoInvisibilityCount() < 1` — and never asked whether
the unit hides by that method at all, so it answered TRUE for all 14 methods on nearly every unit. `isInvisible`'s
hide-and-seek branch then returns invisible on the FIRST method no seer has registered against, before the contest
is reached. ⚠ **Byte-identical to `main`**, where the per-method `invisibilityIntensityTotal(eType)` supplied the
discrimination the collapse to a method-agnostic `concealment()` removed — so this is inherited breakage the
collapse exposed, not a branch regression, and it explains why `COMBAT_HIDE_SEEK` is unusable.
⇒ The method is a SKILL ([vision.md §4](../../specs/vision.md)), so holding it IS the membership question;
`hasInvisibilityType` now asks it and `setHasAnyInvisibility` asks the same one.

Fixed with it, same plane:
- **`getInvisibleType` lost `main`'s negation suppression** (`main:CvUnit.cpp:10733`) — the CLASSIC path, i.e.
  the live one with hide-and-seek off. A unit whose cover is stripped (the `WANTED` line) still reported a hiding
  method and read invisible to any team with no spotter for it. The suppression is restored.
- **Detection was filed under the wrong key** (`CvPlot::changeAdjacentSight`) — `detectionAgainst(eInvisible)`
  passed an `InvisibleTypes` INDEX into a parameter keyed by SKILL id. Every other call site passes
  `GC.getMethodSkill()`, including one thirty lines above in the same function.

**⛔ STILL OPEN — a promotion-granted method does not register.** Both reads above ask `getUnitInfo().hasSkill()`,
so a method granted by a PROMOTION or a unit-combat class is invisible to them — and **73 promotions author one**
([vision.md §4](../../specs/vision.md)). The fix is a resolved per-unit skill plane dirtied on promotion change
(the shape `UnitResolvedHideAndSeek` already uses for detection), never a per-read walk of every promotion inside
`isInvisible`, which is one of the hottest reads in the engine.

---

## 13b. IF a defender query does return NULL, this is the blast radius

⚠ **Conditional on 13a, whose premise is now RETRACTED — this is a verified CALLER inventory, not evidence that
any of it is happening.** Kept because the inventory is real work already done: if a defender query is ever found
returning NULL wrongly, these are the consequences to expect, and the crash rows are worth guarding regardless.

| site | what NULL does |
|---|---|
| `CvUnit.cpp` `updateCombat` | attacks resolve as MOVES — clears the attack plot and calls `groupMove` |
| `CvUnitAI.cpp` `AI_attackOddsAtPlot` | returns **100** on NULL ⇒ every AI attack evaluation reports certain victory |
| `CvUnit.cpp` `canEnterPlot` | the `!canAttack(*pDefender) → return false` legality veto never runs |
| `CvPlayerAI.cpp` `AI_convertUnitAITypesForCrush` | the don't-strip-the-defender veto never fires |
| `CvUnit.cpp` air strike | `canAirStrike` false ⇒ the air-attack subsystem is dead |
| `CvUnitAI.cpp` `AI_assaultSeaTransport` | `bCanCargoAllUnload=false` ⇒ amphibious invasions refused |
| **CRASHES — unguarded NULL deref** | `CvSelectionGroup.cpp:2089`, `CvUnitAI.cpp:26027`, `CvUnitAI.cpp:26195` |

`CvGameCoreDLL.def:44` aliases the EXE's `CvPlot::getBestDefender` onto `getBestDefenderExternal`, so the closed
EXE sees whatever the DLL does. ⚠ `getBestDefenderExternal`'s viewport / dummy-entity filter is **byte-identical
to `main`** — it is the EXE's drawable-unit query by design, not a branch regression.

---

## 13d. Graphics paging — the hypothesis is FALSIFIED, and most of the coupling list was too

**Owner hypothesis tested:** *"turning graphics paging off harms everything, because code expects paging on."*

⛔ **FALSIFIED AS STATED.** `CvPlot::isGraphicsVisible` is
`IsGraphicsInitialized() && (!isGraphicalPaging() || (m_visibleGraphics & graphics)) && isInViewport()` — with
paging **OFF the middle term short-circuits TRUE**, so every gate is *more* permissive, never less. Paging is a
BUG option (`MainInterface__EnableGraphicalPaging`, default True) read at three sites; nothing outside the
graphics layer reads its members, none is serialized, no RNG, so no OOS channel. **The paging surface is
byte-identical to `main`.**

⛔ **AND TWO ROWS OF THE ORIGINAL COUPLING TABLE WERE FALSE FINDINGS — retracted:**
- **`updateAirStrike`'s turn timer is NOT graphics-gated.** The gate is `pPlot->isVisibleToWatchingHuman()`,
  which is FOG-OF-WAR (`isVisible(team, false)`), not `isGraphicsVisible`. `incrementTurnTimer` extends the
  multiplayer timer to accommodate an ANIMATION; with nobody watching there is no animation to accommodate, and
  the strike itself has already resolved in `airStrike()`. It is the same `bQuick` idiom the combat path uses.
- **`getBestDefenderExternal` returning NULL on a dummy entity** is `main`'s design (13b).

**The one genuine paging-OFF defect, presentational and narrow:** `m_requiredVisibleGraphics` has a single writer
in the paging-off world — the one-shot sweep at `CvPlotPaging.cpp:291-299`, latched by
`g_bWasGraphicsPagingEnabled` over `GC.getMap()` only. Plots created after the latch flips — a second map under
Parallel Maps — keep `NONE` forever, so `showRequiredGraphics` computes nothing and features/rivers/routes never
appear on that map.

---

## 13h. THE CITY LAYOUT HAS FEWER SLOTS THAN A CITY HAS BUILDINGS (art data, not the DLL)

**MEASURED from `LSystem.log`** ([observability.md](../../reference/observability.md)): **1,684**
`Failed to place goal building <ART_DEF>` over **174 distinct buildings**, plus **147**
`Layout failed to complete while adding generic buildings!`. Every one of the 174 is a building that HAS a
model — `ASSEMBLY_PLANT`, `FACTORY`, `COLOSSEUM`, `HOSPITAL`, `COURTHOUSE` — so this is the layout engine
running out of room, never a missing-art gap.

⛔ **DISTINCT from the art-less flood (§13), and the two must not be conflated.** That one was the city offering
the engine buildings with NO model (`is not associated with a CvCityLSystem node`), and it is fixed by
`world.art.notShownInCity`. This one is the opposite condition: a real model with nowhere to put it. Checked
rather than assumed — **0 of the 174 are flagged `notShownInCity`**.

⚠ **Whether the §13 fix moves this number at all is UNKNOWN and is one measurement away.** It turns on whether
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

## 13. Unit graphics do not follow a moving unit, and units render stacked

**OBSERVED (owner):** moving a unit leaves its graphics behind — the walk animation plays **in place, on the
original tile** — units **render on top of each other**, animations never end, and **re-selecting the unit or
splitting the group fixes it**. Beside it: an FPS drop after end turn that **completely disappears** under bare
map (no fog of war, no city billboard bars, no units shown).

**⛔ THE MOVEMENT/STACKING HALF IS EXPLAINED AND FIXED — entity churn orphaned the queued move.**
`CvUnit::reloadEntity` destroyed any real entity unconditionally, and `CvSelectionGroup::groupMove` routes that
destroy into the worst possible window: inhibit centre-unit recalc on both plots → `setXY` **`QueueMove` pushes
onto entity E1** → the inhibit **nulls `m_pCenterUnit`** → lifting the inhibit makes
`newCenterUnit != m_pCenterUnit` **guaranteed true** → `reloadEntity(true)` → **E1 destroyed with the queued move
on it** → E2 created → `ExecuteMove` runs against E2, whose queue is empty. The DLL concedes the entity owns
mission state: `reloadEntity` calls `RemoveUnitFromBattle` — *"remove this unit from any active mission"* —
immediately before destroying, and the interface is pointer-keyed with no re-bind. `if (!IsSelected())` is why
re-selecting fixed it. `setupGraphical`'s own `ExecuteMove(0, false)` — commented *"forces multi-unit graphics to
update; if it isn't done then only 1 unit shows up"* — is the stacking half, spent on the empty queue.
**Fixed:** an entity that is already the kind the unit wants is KEPT; `rebuildEntityArt()` serves the one case
that genuinely needs a new scene node (a warlord attaching swaps the model).

⛔ **RULED OUT — the rendering CODE is not what changed. Do not re-tread any of this.** All 16 `CvPlot` render
functions, `CvUnit`'s entity management and dummy gating, `setXY`'s graphics half, `move`, `updateCenterUnit`
(body and all 27 call sites), `m_bInhibitCenterUnitCalculation`, `CvSelectionGroup`, `CvDLLEntity`,
`CvPlotPaging`, `CvMapExternal`, `CvViewport` and the `.def` export table are **identical to `main`** (the one
difference is where `updateSymbolsInternal` gets its yield-icon numbers, which cannot produce these symptoms).
⚠ A false lead, recorded so it is not re-derived: `setInfoDirty` drops from 51 call sites to 33. That is **not** a
repaint purge — all 17 host functions were deleted whole by the sanctioned accumulator cut
([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)), and restoring them
would revive the legacy accumulators.

**THE END-TURN FPS DROP IS THE ART GAP, ON TWO PLANES — the render engine hunting for graphics that are not
there.** Resolved art stays cached until turn end and is dropped there, so the re-search is what the player feels
AFTER ending a turn, and bare map removes it by rendering neither plane:

- **UNITS** — a collapsed mesh-group grid answered **0 group definitions**, so the EXE had no formation to lay a
  unit out from. This is the larger half, which is why bare map OFF (units drawn) lagged hardest.
- **BUILDINGS** — `CvCity::getVisibleBuildings` lost its skip, so a city offered the engine **every** building it
  holds, including the **4,674 of 5,180 (90%)** whose art define is scaled to nothing. `LSystem.log` measures the
  result directly: **26,273** `is not associated with a CvCityLSystem node` warnings over **260** distinct
  buildings, plus **4,168** `Art/Empty.nif ... SHADOW` complaints and **369** placement/layout failures. The
  restored skip catches **258 of those 260**.

⚠ The **2 it does not catch** (`BUILDING_PALACE`, `BUILDING_SNOWCASTLE_OF_KEMI`) carry real art at real scale and
are simply absent from `CIV4CityLSystem.xml` — an ART-XML gap ([roadmap.md](roadmap.md) scope decision 3), which
legacy warned about identically. Not a DLL defect.

⚑ **`LSystem.log` is the instrument for this whole class** ([observability.md](../../reference/observability.md))
— the EXE writes it itself, so it reports what the render engine did with what the DLL handed it.

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

**On dogs vs criminals — ⛔ the earlier reading here was wrong twice and is retracted.** It said the contest
*"is not yet evaluated by the engine"*, quoting a [vision.md §4](../../specs/vision.md) line that was itself
false: the contest is INHERITED FROM `main` AND RUNNING. Two live defects on that path are now fixed (13a) — the
detection registration was filed under an `InvisibleTypes` index instead of a SKILL id, and the membership test
never asked whether a unit hides by the method being contested.
⚠ **What remains for the dogs specifically is the promotion-granted-method gap (13a):** the reads ask the unit's
INFO, so a method a promotion grants does not register. The War Dog's detection role is
`SeeInvisible INVISIBLE_CAMOUFLAGE`, so confirm against a live run before assuming either fix reached it.

⚠ **The whole-map-scan half is untouched and is the same work item issue 13's remaining FPS drop points at** —
the owner's ruling is that `updateSight` is **reviewed, not reverted**, because reverting it would bury the
scan bugs rather than fix them.

---

## The new surface, wired wrong — fix these first

> These are not legacy and not gaps: they are built machines connected to nothing, or connected wrongly. Each
> compiles, runs, and produces a plausible wrong answer, which is why none of them surfaced on its own.
> ⛔ Symbols, never line numbers — a symbol survives an edit and a line number does not.

- **⛔ CONVERT THE PACKAGES TO THE MAINTAINED SUM — the THREE PLANES**
  ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum);
  [state-repositories.md](../../architecture/state-repositories.md) § THE MAINTAINED SUM; the retired protocol is
  [superseded-ideas](../../architecture/superseded-ideas.md) #30). The addressing half already exists —
  `DepositIndex::depositsFor` IS "the fact names the source, the index names its deposits" — so what is missing is
  the WRITER and two of the three ROUTES.
  **⛔ CUT FIRST, THEN FILL — do NOT build to keep the current numbers alive.** `CascadeGather::refresh*` is a
  whole-registry sweep (it asks every building / tech / civic / trait / bonus / religion / corporation whether the
  owner has it, per mark), which contradicts the spec, so it is cut EARLY and the packages bind CLEAN. Every
  deposit without a route then reads **ZERO, visibly**, and that census is what drives the rest of this item.
  `gather*Into` survives as the ORACLE and nothing else.
  1. **`CvCascadePackage::apply(channel, unit, ±value)`** — a pure add. Today the only writer into a slot is the
     gather's zero-then-refold; `sourceFlat`/`sourcePercent`/`sourceSum` are rebuild-path READS, not writers.
  2. **Plane A — the SOURCE route.** `±value` on the source arriving or leaving. ⚑ There is no withdrawal input to
     audit any more: every fact's direction IS its id, so the route is picked by which event arrived and the payload
     is read only for HOW MANY ([DEC-facts-name-happenings](../../architecture/decisions.md#dec-facts-name-happenings);
     [event-spine.md](../../specs/event-spine.md)). ⛔ So a consumer must never re-derive a direction from a payload
     sign, an old-vs-new id pair or a presence bool — those conventions are gone from the surface, and reintroducing
     one at a consumer rebuilds the branch the split deleted.
  3. **Plane B — the COUNT route.** A `count-key → the deposits it scales` reverse index off the compiled deposit
     index, so a `ContextDict::add(id, ±1)` applies `±value × Δcount` to every deposit scaled on it whose SOURCE is
     live at that owner (an O(1) `has()` test — this is what keeps the two arrival orders convergent).
  4. **Plane C — the ATOM route.** `±value` on a condition atom's verdict crossing, over the deposits that atom
     gates. ⛔ **B and C land TOGETHER or neither** — C is delta-able only because B guarantees no count moves
     unannounced ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum)).
  5. **Retire the staleness protocol** — the mask derivation, the banked-marks bracket, and `CvCascadePackage`'s
     `CvDerivedCacheSet` member.
  ⚠ **THE RIDER THE CUT DROPPED, owed by the apply path.** The four `refreshCascadePackage` delegates carried a
  side effect past their one-line delegation: a MAINTENANCE channel moving at city / empire / team scope also
  moved the empire's maintenance TOTAL. That obligation is real ([save.md §6](../../specs/save.md): audit a
  deleted body for riders) and it is exactly "ONE EVENT REACHES BOTH LEVELS" — an apply into a maintenance
  channel must also apply to the empire's maintenance RECEIVER SUM. ⛔ It does NOT come back as a hand-named
  cache: the empire total is a receiver SLOT in the player's own package
  ([state-repositories.md](../../architecture/state-repositories.md) § THE CROSS-SCOPE RECEIVER), which is also
  what retires `markMaintenanceDirty` and its banned spelling.
  ⛔ **Build NO per-source decomposition plane and NO upward push** — the receiver re-sums its participating
  members, and the summing is trivial enough that avoiding it costs more than it saves
  ([state-repositories.md](../../architecture/state-repositories.md) § THE CROSS-SCOPE RECEIVER). ⚠ That is a
  measurement question if it ever reopens, never an argument: a turn-time cost on the standing save, attributed
  to the summing.
  ⚑ **Start in the corner that is the PLOT package**: the smallest channel set, no percent side (the origin rule),
  five sources that all announce with old values, no receiver sums, and it exercises the `substrateFlat` segment.
  Then city, then empire/team.
- **⛔ SWEEP THE BANNED TERM OUT OF THE CODE** ([DEC-no-staleness-vocabulary](../../architecture/decisions.md#dec-no-staleness-vocabulary)).
  KEEP only the graphics/interface repaint vocabulary the closed EXE needs and BUG resolves by name
  (`InterfaceDirtyBits` and the `setDirty`/`setLayoutDirty`/`setFlagDirty`/`setInfoDirty` helpers over it).
  Everything derived-state goes with its mechanism: `CvDerivedCacheSet`'s `markDirty`/`isDirty`/`markAllDirty`,
  `markMaintenanceDirty`, `setCommerceDirty`, and the AI re-evaluation flags `AI_setAssignWorkDirty` /
  `AI_makeAssignWorkDirty` / `AI_setChooseProductionDirty`. ⛔ A surviving derived-state site gets NO synonym —
  name it for the job it does, or delete it with the mechanism.
- **⛔ Route every domain through `EnablerKernel::gateSet` — the declared GENERATE→GATE primitive has ZERO
  callers while SIX file-static copies of it exist**, one per domain (`bd_`/`bl_`/`ce_`/`pc_`/`pj_`/`ud_gateSet`).
  The kernel's own header calls these "the single-implementation enabler primitives"; the gate ORDER
  (obsolete-check → `requires` → `allowed` cap) now lives in six independent bodies free to drift apart
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)). This is the
  reinvented-machine signature at the centre of the enabler.
- **The enabler's validation-oracle surface is unrunnable** — `BuildingEnabler::verifyCity`, `TechEnabler::available`
  and `UnitEnabler::explain` all have zero callers, and no route reaches them. ⛔ This is the load-bearing one:
  `CvTechEnabler`'s header states the design contract that the enabler consumes ONLY events *precisely so* a
  missed emit surfaces as a visibly wrong enabler, with the oracle diff as the tripwire. **The event-only design
  is resting on a tripwire nothing pulls.**
- **Consult `EnablerKernel::cityHasVicinityBonus` instead of re-deriving it.** Its header names it the ONE home
  for "is this bonus in vicinity here?"; it has no caller, and the evaluator re-implements the same two-half union
  inline. Behaviour agrees today — the divergence risk is structural.
- **Wire the `InfoValuation` fold seams or delete them**: `tradeRouteChannelYield`, `combinedGroupSum` and
  `netUpkeepAfterFree` are the declared canonical math "so the package rebuild and the `expected*` endpoints call
  the SAME math", and all three have no caller while the arithmetic is hand-written at the consumers. ⚑ Knock-on:
  the combine-floor metadata table is reachable ONLY from the dead `combinedGroupSum`, so adding a floored
  (family, kind) row today would have zero effect, silently.
- **Wire `DepositIndex::segIdFor*`** — a five-entry string-elimination optimizer whose header says it "kills all
  string handling in the per-plot keyed walks". It has no caller and the per-read `lookupSegment(std::string(...))`
  it was built to remove is still on the per-plot turn path.
- **Give `ev_countCore` a `TAG_` branch** so `CvCascadeTally::countUnitsWithTag` is reachable. The count-atom
  dispatcher branches on specialist/building/tech/unit only, so an authored `{TAG_X, min:N}` count silently falls
  through to presence and answers 0/1. ⚠ Latent — no data authors one yet.
- **Wire the `EnablerOverlay` DELTA reads** (`unlocks` / `unlockedIds`) or delete them. The overlay's `addHave`
  half is live in the civic what-if; the delta half — the header's "deliberately the primitive rather than
  something each caller re-derives" — has no consumer because the tech-picking one does not exist yet.
- **Fix the stale comment pointing at `CvPlot::setWorkingCity`** on the working-city event — no such method
  exists (only the override variant), and naming a dead site is the bait
  ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)).
- **Re-route `governmentCenterDistance` and delete the fact pair nobody emits.** Its interest set names the
  government-centre crossing, whose emitters have no callers at all — the counter that raised them became the
  amenity fold and the interest set did not follow, so the distance is frozen at whatever founding or load
  computed while `DISTANCE_TO_GOVERNMENT_CENTER` is read live. ⚠ It also re-derives by reading the amenity fold,
  which is the ordering dependency [contexts.md](../../architecture/contexts.md) bans — settle the registration
  order in the same change, and correct contexts.md, which still documents the retired counter as live.
- **Derive the plane-C dependency routing instead of hand-listing it.** A conditioned deposit is withdrawn by its
  ATOM's verdict crossing ([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum)), and the
  modifier consumer wires those crossings as a hand-written `case SEVT_X → gatedByPredicate(CASC_PRED_Y)` switch.
  Most `CASC_PRED_*` values have no case at all, so a deposit gated on one is applied when its SOURCE arrives and
  never re-resolved — the phantom contribution nothing clears, compounding exactly as
  [state-repositories.md](../../architecture/state-repositories.md) predicts. ⚑ **The PLOT plane already shows the
  answer**: `SEVT_PLOT_PREDICATE_ADDED / _REMOVED` carries the predicate id in its payload, so ONE route covers
  every plot bit and a new bit needs no new case. The city/empire predicates want the same treatment — a per-
  predicate declaration of the facts that feed it, never a switch arm someone has to remember
  ([contexts.md](../../architecture/contexts.md): derive the routing, never hand-write it).
  ⚠ Two named instances to close with it: **`HAS_POWER` has THREE legs and only one is routed** — the power count
  re-gates, while the blackout status and the area clean-power flag announce and nothing re-resolves, so a
  blacked-out city keeps every powered deposit and keeps power-gated buildings operating; and **`IS_ANARCHY`'s
  fact reaches no consumer at all**.
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
- **Give `NO_PLAYER`-attributed tech grants a consumer route.** The game's own free/start-era techs announce with
  no owning player, so the capability union, the enabler and the modifier consumer all drop them alike. The fact
  identifies the TEAM's acquisition; the consumers key on a player.
- **Make the repeat-tech emit symmetric.** Play announces on every count increment while the read announces once
  per held tech, so a counted tech's folded state differs across a save/load round-trip AND from the oracle's
  from-source walk — which makes the missed-emit tripwire report a divergence that is not one.
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
- **Consume the withdrawal halves the unit plane already announces.** The unit-killed and left-city facts reach
  no consumer while their created / entered-city twins do, so whatever folds on the arrival never retires it —
  a compounding magnitude, not a stale gate ([state-repositories.md](../../architecture/state-repositories.md)).
  ⛔ Acting on one half of an ADDED/REMOVED pair is the defect; find the folds, not just the facts.
- **Emit the global-define WITHDRAWAL.** The removal helper exists with no caller while the addition fires, so a
  live-option slot REPLACEMENT is announced half-open and the old value is never taken back
  ([DEC-facts-name-happenings]: a slot replacement is two facts, and the withdrawal must be announced while the
  old state still holds).
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
- **Sweep the writerless serialized accumulators** by the two-grep test: serialized and read, with no caller on
  the changer. Beyond the two already named, `CvPlayer::changeHappyPerMilitaryUnit` and
  `CvCity::changeImprovementFreeSpecialists` are the same shape.
- **Stop handing `CvPlot*` out of `CityContext`** (`cityPlot`, `radiusPlot`) — the evaluator uses that hole to
  reach a second context, while the eval ctx already carries one. The isolation is meant to be structural.
- **De-instantiate `CvCascadeTally`** — a calculator is a static-methods holder, never an instance
  ([DEC-single-implementation](../../architecture/decisions.md#dec-single-implementation)).
- **Reset the enabler's build-once latches**, or make them re-entrant: `rj_clearAllRepos` re-maps every info on
  the postmenu pass while `buildActiveIndex`, `bd_buildGateClasses`, `ud_buildClasses`, `bd_sbMembers` and
  `bd_cappedBuildings` latch permanently. `di_ensureDependencies` is a literal ensure-on-read behind four reads.
- **Collapse the second condition-evaluation surface.** `CvPropertyBridge` translates the cascade's own
  `CvCondition` trees back into `BoolExpr` for the legacy solver to evaluate, with different semantics — count
  thresholds and connections drop silently.

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

- **⛔ The ONE condition evaluator dispatches on RUNTIME STRINGS.** `CvCondition` keeps `type`/`param` as
  `std::string` into the runtime, and the evaluator routes every atom through prefix compares — dozens of them in
  one file, on both the package-rebuild path and the per-decision `expected*` read.
  ⚑ **This project already MEASURED this exact defect and fixed it elsewhere**: `CvCapabilities` records that a
  per-call `std::string` construction on the pathfinder "4x'd the turn", and precomputes flags for it. The same
  shape survives in the evaluator. `CvCondition` already carries the FK-resolved `id` and a `predKind` — route on
  an interned kind, and let no string survive load
  ([DEC-materialize-at-mapfrom](../../architecture/decisions.md#dec-materialize-at-mapfrom),
  [DEC-one-json-reader](../../architecture/decisions.md#dec-one-json-reader)).
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

- **A negative empire commerce total is rewritten to ~2 billion.** A legacy wrapped-int "false accumulate"
  detector (`< -9999` → `MAX_COMMERCE_RATE_VALUE`) survives on sums that are now accumulated in `int64_t` and
  cannot wrap — so the only thing it still discriminates is a GENUINELY negative total, which the realized city
  rate can legitimately produce (neither `cityRate` nor `commerceSplit` floors the flat/deposit legs). When it
  fires, the AI's production and tech weighting, the demographics history, and the Python surface all read
  positive two billion. Delete the detector; it can no longer detect what it was for.
- **The whole map's visibility is wiped and re-applied EVERY TURN** in `CvGame::doTurn`, under a comment that
  says outright it is "a stickytape - can't find where it's skewing visibility counts". ⚑ The comment names the
  fix: an unpaired visibility increment/decrement leaves the per-plot counter outside its stated 0–1 invariant.
  Find the unpaired mutation; the sweep then deletes itself. Until then it costs a full-map clear plus a whole-map
  re-sight per turn ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
- **The empire commerce total is an `ensure()`-on-read behind a `MAX_INT` sentinel, cleared by a per-turn
  blanket** — the tombstoned read-side rebuild, a hand-named scalar array, and a blanket standing in for the
  missing mark, all on one value, with the comment citing `state-repositories.md` while implementing what it bans.
  Wire the mark that a member city's realized commerce moved to also mark the empire's receiver slot. ⚠ Its
  sibling total-yield read has NO cache at all and re-walks every city per call, inside a players×yields double
  loop.
- **The per-turn victory-city recount** wipes and refills two hand-named scalars whose only inputs are victory
  validity and immutable info data. Cheap, same shape, same missing emit.
- **Owner call needed on the AI turn-scoped memo clears** (tech values, mission targets, civic values, build
  values, unit counts, trade routes, resource consumption). They memoize AI *valuations* rather than derived game
  state, so whether the no-self-heal rule binds them is a ruling, not an agent's call. ⚑ Start with the mission
  target cache — its own comment says it is force-recalculated "for reliabilty reasons (more robust to bugs)".

## ⛔ DOUBLE-COUNTED VALUES — the cascade folds it, then legacy adds it again

> One shape: a `process*` function reads a rebuilt info's compiled deposits and pushes them into the legacy
> accumulator the cascade replaced — while the gather folds the SAME entries into the scope package, and one
> consumer sums both. In three of four an in-tree comment asserts the legacy leg is sanctioned, which is exactly
> why they survived ([DEC-accumulator-cut-uniform](../../architecture/decisions.md#dec-accumulator-cut-uniform)).

- **City over-limit anger — counted twice, and the magnitude is wrong.** The ruling is written FIVE LINES ABOVE
  the feeder: the over-limit member is a PRESENCE COUNT and "the anger MAGNITUDE now lives in the cascade's
  CITY_LIMIT `per.above` deposits, never here." Every other consumer obeys it (`==0` / `>0` / `<1`). The
  wellbeing verdict multiplies the presence count as a per-city anger amount, on top of the cascade's own
  deposit — as does the UI attribution. Serve it from the wellbeing read alone.
- **Tech health and happiness — counted twice behind a camouflaging comment.** `processTech` pushes the tech's
  compiled empire-scope wellbeing into `m_iExtraHealth`/`m_iExtraHappiness`, which the city then adds on top of
  the cascade fold of the same entries. ⛔ The comment calling these "genuine one-shot event state" is TRUE of the
  random-event feeders (the owner-ruled carve-out) and FALSE since `processTech` started writing the same member.
  Cut the `processTech` lines; keep the member for the event grants.
- **Building-keyed happiness — the reverse pass lands it, then `processBuilding` lands it again.** The keyed
  happiness deposit is reverse-landed onto the TARGET building and folded with that building's own output; the
  player-side keyed accumulator adds it a second time. ⛔ **Do NOT cut the neighbouring
  `changeBuildingProductionModifier` on the same logic** — `buildRate` is excluded from the output-channel
  landing and keyed entries are skipped by the fold outside plot scope, so its keyed accumulator is its only home.

## ⛔ A LEGACY PLANE ALIVE ONLY FOR PYTHON — and a value silently lost

- **Random-event yield modifiers reach nothing.** `CvCity`'s `m_aiYieldRateModifier` is written ONLY by random
  events and read ONLY by the `Cy*` binding — `getBaseYieldRateModifier` is a pure cascade read that never
  consults it. So event yield modifiers are stored, serialized, shown to Python, and have ZERO gameplay effect.
  This is legacy-left-breathing masking a real functional loss, not merely a duplicate.
- **The player twin is a serialized plane maintained solely to feed a binding.** `CvPlayer`'s
  `m_aiYieldRateModifier` is fed from building deposits and has no engine consumer at all — its only reader is
  `CyPlayer`. Exactly the shape [DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed) and
  [DEC-new-getter-surface](../../architecture/decisions.md#dec-new-getter-surface) ban.
- **Cascade computes it, legacy is what consumers read** — the cascade slot beside each is dead and therefore
  unverified: the space-production modifier, the team enemy-war-weariness modifier (whose own comment concedes it
  is "what the legacy accumulator held"), the city production-to-commerce modifier, and the building commerce
  change.
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

## Whole-database scans where the forward edge already answers

> [DEC-one-reverse-view](../../architecture/decisions.md#dec-one-reverse-view) + [enabler.md §6](../../specs/enabler.md):
> ask the edge or the maintained frontier, never the registry. These are RESIDUE — in every case the same file
> already does it right somewhere else, which is the tell that the conversion stopped short rather than skipped.

- **`CvUnit::canAcquirePromotionAny` sweeps every promotion, on the hottest path there is.** `testPromotionReady`
  calls it TWICE (the second is redundant once the first sets the flag), and it fires on every XP change, every
  unit produced, and from a dozen other sites. ⚑ `CvUnitAI::AI_promote` sitting beside it already iterates the
  player's maintained UNLOCKED-promotion set and says so in its own comment — then its entry guard
  `isPromotionReady` sweeps the database anyway. Convert the guard onto `getUnlockedPromotions`.
- **`CvTeam::processTech` asks four questions BACKWARDS** — `ObsoletePromotions` and `ObsoleteCorporations` scan
  every promotion/corporation testing `getObsoleteTech()`, and two more scans test every build's `getTechPrereq`
  and every improvement's `getPrereqTech`. All four answers are compiled onto the TECH
  (`EDGEF_OBSOLETES`/`EDGEF_ENABLES`/`EDGEF_RELATED`) and are already read forward from `CvPlayerAI`.
  ⚑ The tell: the SAME function already reads `enables.specialBuildings` off the tech's own edge a few lines
  below, citing the ruling.

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
- **⛔ THE AI'S PLOT-vs-SPECIALIST VALUATION IS WILDLY OFF, AND A ×100 SEAM IS THE PRIME SUSPECT (owner).**
  Plot and specialist yields are compared against each other in the citizen-assignment decision, so a scale
  mismatch on ONE side does not merely mis-tune it — it makes one option dominate absolutely.
  ⚑ Why the suspicion is well-founded rather than a guess: a specialist's yield carries its OWN percent layer
  before it joins BASE and takes the city modifier ([legacy-value-calc-map] §1.5 — two distinct percent stacks),
  and the per-specialist "all" source is EMPIRE-wide, so the two sides of this comparison genuinely travel
  different paths to the same unit.
  ⚠ The failure signature to look for is [fixed-point-and-scales] §5's: a decision that never varies, or one
  side always winning, is a truncated-to-zero input — not an AI-logic problem. Read the scale at the boundary
  where the two are compared, against each side's DECLARED unit, before touching any weighting.
- **⛔ TURN TIME IS FAR LONGER THAN EXPECTED — AND THE REAL ISSUE IS THAT LEGACY IS RUNNING ON A HOT PATH AT ALL,
  WHEN IT EXPLICITLY SHOULD NOT (owner).** ⚠ Do NOT read this as the poisoned-measurement caveat and stop there.
  [DEC-legacy-decache-poisons-perf](../../architecture/decisions.md#dec-legacy-decache-poisons-perf) says a
  number taken with legacy on a read path measures legacy's decache penalty — true, and it is the SMALLER point.
  The larger one is that a surviving legacy calc on the turn path is a DEFECT to DELETE, not a condition to
  measure around ([roadmap.md](roadmap.md) § LEGACY STILL BREATHING: surviving legacy is never a gap, a stage or
  a transitional shape).
  ⇒ **So the slowness is not the problem to solve — it is the INSTRUMENT that reveals which legacy is still
  breathing.** Follow it to the surviving caller and cut that; the time comes back as a consequence. ⛔ What is
  banned is the inverse move: treating the number as the deliverable and optimising around a path that should
  not execute.
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

## ⛔ THE MAINTENANCE RECEIVER — its participation gate is missing a side

`InfoValuation::realizedAtEmpire`'s receiver Σ gates participation on `isDisorder()` alone. That is correct for
the four COMMERCE channels, whose per-city rate is already 0 under disorder. It is **incomplete for
MAINTENANCE**, the one non-commerce receiver: [economy.md](../../reference/economy.md) states a city emits 0
instead of its maintenance package under **WE LOVE THE KING DAY as well as disorder**, and WLTKD is the sole
gameplay effect that status has. So an empire currently pays maintenance for every celebrating city.
⚑ The gate belongs at the Σ and nowhere else — WLTKD is a duration-1 status re-applied every turn, so a
maintained membership delta would flip a member in and out every turn over a number that never moved.

⚠ **UNVERIFIED, and deliberately not asserted here:** whether `cityReceiverRate` is the right MEMBER quantity for
maintenance at all. economy.md composes a city's realized maintenance from the three component KINDS, each
against its own modifiers, with the `amount` stack over the total and `MAINTENANCE_CORPORATION` skipped — which
is not what a single-channel receiver read answers. Establish that before changing it; do not infer the
component set from the enum.


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


## THE PYTHON HALF OF THE Cy DISCONNECT IS NOT DONE — ~2000 DEAD CALL SITES

**PROVEN — measured, not estimated:**

| dead surface | call sites | spread |
|---|--:|---|
| `CyCity` methods | **1,586** | 43 files |
| `GC.get*Info` accessors | **~450** | 44 distinct accessors |

`CyCity` retains **four** defs — `getID`, `getOwner`, `getX`, `getY`. Everything else a screen asks a city
(`happyLevel`, `getYieldRate`, `canConstruct`, `isProductionUnit`, `getCultureThreshold`, …) raises
`AttributeError`. Worst files: `Revolution.py` 478, `RevEvents.py` 141, `CvRandomEventInterface.py` 124,
`WBCityEditScreen.py` 121, `CvEventManager.py` 117, `CvDomesticAdvisor.py` 108.

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
- **Live gameplay is silently off, not merely noisy:** all three `combatResult` handlers raise every combat, so
  `CaptureSlaves` (captives) and `Partisan` (unit capture) never run.

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

**NOT YET KNOWN / what makes this bigger than a Python sweep.** The `combatResult` slice alone needs reads the
new surface does not publish. Union of what its four handlers do to the two units:

| served today | NOT served — must be ADDED |
|---|---|
| `getOwner` (free) · `getUnitPosition` · `getUnitRead[UNIT_READ_TYPE\|_DOMAIN]` · `hasUnitPromotion` · `hasUnitCombat` · probably `getCaptureKinds` | `isMadeAttack` · `isAnimal` · `getCaptureUnitType` · `getUnitCombatType` · `getExperience` · `isHuman`/`isNPC` · a position→`CyPlot` path for `plot()` · a **CyAct** route for `setDamage` and `changeExperience` |

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

The receivers below are city/unit HANDLES (identity set only) or pushed IDENTITIES; every method named is
outside `getOwner`/`getID`/`getX`/`getY` and therefore dead.

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

## THE FREE-SPECIALIST READ REBUILDS AN EVAL CTX AND WALKS THE EMPIRE, ~40x PER CITIZEN

The hoisted segment lookup was the frame the sampler caught, not the cost. The shape underneath it:

```
CvCityAI::AI_assignWorkingPlots  (per CITY)
  -> AI_fillCitizensByPriority   (per CITIZEN)
    -> AI_foodAvailable
      -> for iI in 0..getNumSpecialistInfos()      <- 40 specialists, the registry count
        -> CvCity::getFreeSpecialistCount(iI)
             fillEvalCtx(...)                       <- a FRESH CvCascadeEvalCtx, per call
             for each ACTIVE building in the city: keyedTargetSum(...)
             -> CvPlayer::getFreeSpecialistCount(iI)
                  for each building the PLAYER HAS: collectKeyedTarget(...)
                  for each civic option, each held trait: the same again
```

So the per-turn cost carries a factor of `cities x citizens x 40 x (city buildings + empire buildings + civics
+ traits)`, for an answer that does not vary with the citizen being placed -- and `AI_foodAvailable`'s own
`iExtra` parameter does not reach the specialist term at all, so the whole block is INVARIANT across the loop
it sits in.

Y It is [DEC-legacy-decache-poisons-perf] exactly: the shape was always this, and every inner read used to hit
a serialized accumulator and cost O(1). Stripping the accumulators is what turned it from wasteful into a
stall -- and the same ruling says the decache is an INSTRUMENT, so this is the hot path announcing itself.

**BOTH HALVES ARE THE FIX, and neither alone is** (the same ruling):

1. **The READ.** A per-specialist scalar forces one full walk PER SPECIALIST. The grammar already answers this:
   ONE GETTER PER GROUP, filling a caller-owned array ([patterns.md] THE TWO READ ROLES rule 1 and rule 7) --
   one eval ctx, one walk of the operating set, one walk of the player's sources, all 40 slots filled. That is
   a ~40x constant factor and it is the spec'd shape rather than an optimization.
   V The scalar has ~30 call sites, so it stays -- re-bodied onto the group read so there is ONE implementation
   ([DEC-single-implementation]), never two that drift.
2. **The CALLER.** Even at one walk per call it is still asked per CITIZEN for a value that moves only when the
   city's sources move. The invariant term lifts out of the loop.

X NOT to be answered with a new cache. [DEC-legacy-decache-poisons-perf] sequences it: run uncached, let the
hot paths announce themselves, **fix the reads that should never have computed**, and only then let the AI
plane cache its own scores. A cache added while a wrong-shaped read is still underneath it hides the read
instead of fixing it.

Y The keyed walk itself is spec-correct and is NOT the defect: a keyed deposit is an entry-list read over the
live sources ([modifier.md] par.5), cheap "because it iterates the handful an entity AUTHORED" -- which holds
only if discovering the live sources is itself cheap. Here it is rediscovered 40 times per citizen.
