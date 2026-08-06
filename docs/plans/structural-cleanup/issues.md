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

## 1. HARD CRASH — attacking a unit dies in a boost::python to_python conversion

**Repro:** reliable. Attack any unit with a combat unit; observed attacking a **Wombat** (an animal, so the
kill-outcome path is in play) with Modern Infantry. The game dies.

**PROVEN — the failure mode.** An unhandled C++ exception (`e06d7363`), from a boost::python conversion that
has no registered `to_python` converter for the type being pushed. Symbolized stack:

```
KERNELBASE!RaiseException
msvcr71!CxxThrowException
boost_python!boost::python::throw_error_already_set+0x18
boost_python!boost::python::converter::registration::to_python+0x62
boost_python!boost::python::converter::detail::arg_to_python_base::arg_to_python_base+0x11
```

The last engine output before the throw is mission traffic on both units:

```
Modern Infantry startMission endish mission=0...
Modern Infantry part 1 continueMission 0...
Wombat clearMissionQueue...
```

⚑ This is the failure class [patterns.md](../../architecture/patterns.md) predicts by name: an engine→Python
push of a type whose `class_<>` registration is absent — *"publishing the accessor without registering what it
returns yields a def that resolves and then raises at conversion."*

**RULED OUT — do not re-tread these.** Both were checked against the tree, with evidence:

- **A missing `Cy*` wrapper trait.** `CvUnit` / `CvCity` do NOT use `DECLARE_PY_WRAPPER`; they use
  `DECLARE_PY_IDENTITY`, crossing as an `(owner, id)` INT PAIR by design (CyCity carries zero defs, so a handed
  handle could be asked nothing). That path needs no converter and cannot be the throw.
- **`CvGameObject::createPythonWrapper`.** Every type it builds — `CyGame` / `CyTeam` / `CyPlayer` / `CyCity` /
  `CyUnit` / `CyPlot` — has a live `class_<>` registration.

**NOT YET KNOWN.** WHICH type, and WHICH push site. The minidump cannot answer it: a minidump's stack is
truncated (`Stack unwind information not available`), so it shows the boost frames but not the frame in our DLL
that initiated the conversion. `!analyze -v` is useless here — it needs OS symbols we deliberately do not fetch
offline and derails into `WRONG_SYMBOLS`.

**THE NEXT STEP, and it is one word.** Arm cdb for **`eh`**, not just `av`:

```
sxe -c "kp 60;.dump /ma <path>;qd" eh
```

`av` was armed and this is a C++ EH exception, so cdb fell through to its default break — which is what froze
the game and why no dump was written. `eh` breaks at **FIRST CHANCE**, at the throw, with the live process
intact and the full stack present, which names the push site and the type in one shot.
⚠ Run it against a **Release** build with `-y` pointing at `Assets` + `Build/Release` + `Build/Assert`; our DLL
frames resolve with line numbers, EXE frames stay unresolved and that is expected
([external-tools-and-workflows.md](../../reference/external-tools-and-workflows.md)).

---

## 2. A stale comment names two wrappers that do not exist

`Sources/Infrastructure/CvDLLPython.cpp:154` states that `DECLARE_PY_WRAPPER` *"exists for exactly four types
(CyCity / CyUnit / CySelectionGroup / CyPlot)"*. It exists for **two** — `CyPlot` and `CySelectionGroup`; the
city and unit entries moved to `DECLARE_PY_IDENTITY`.

⚑ Recorded because it is not cosmetic: it is what sent the crash investigation above down its first wrong path.
A comment naming a mechanism that is no longer there is exactly the bait
[DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence) describes.

---

## 3. `MoreCiv4lerts` dies on a cut binding

`PythonErr.log`, every active-player turn:

```
File "MoreCiv4lerts", line 357, in buildBonusString
AttributeError: 'CyGlobalContext' object has no attribute 'getBonusInfo'
```

A stage-4 casualty: the read was cut and this consumer was not moved. Belongs to the **Python-visuals barrier**,
not the events one. ⛔ The fix is to serve the read on the new library, never to restore the binding
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)).
⚠ The handler RAISING is the intended state meanwhile — it stays visible rather than being silenced
([roadmap.md](roadmap.md) § the mutating Python handlers).

---

## 4. AREA CLEAN POWER is dead end to end — the Hoover Dam's mechanic reaches nothing

**What it is (owner):** area clean power is **how the Hoover Dam project/wonder functions** — it powers every
city on the landmass. Not a curiosity; a shipped wonder's entire effect.

**⚠ PRIORITY (owner): nowhere near the top — it waits until the machinery actually functions.** Recorded so the
evidence is here when it is picked up, not to be picked up.

**PROVEN — every link in the chain is broken, independently.**

| link | state |
|---|---|
| the DATA | `curate_building.py` emits **no** `cleanPower` key; **nothing under `Assets/Data/` carries one**. The legacy schema tag `bAreaCleanPower` exists (`Assets/XML/Schema/C2C_CIV4BuildingsSchema.xml:364`) and never became JSON |
| the ENGINE GRANTOR | **nothing in the DLL calls `CvArea::changeCleanPowerCount`.** Its only caller is the `CyArea` binding (`Sources/Python/CyArea.cpp:152`, `.def` at `:201`) |
| the live CALLER | one Python random event — `Assets/Python/EntryPoints/CvRandomEventInterface.py:5607` — and nothing else in the tree |
| the CONSUMER ROUTE | `SEVT_AREA_CLEAN_POWER_ADDED / _REMOVED` is emitted (`CvArea.cpp:808/812`) and **consumed by no consumer at all** |

⇒ So the wonder's effect is not merely unrouted, it is **unauthored**: even if the route existed there is no
grantor to move the counter. The one thing that can move it today is a `.def` on the kill list
([DEC-cy-not-fixed](../../architecture/decisions.md#dec-cy-not-fixed)), so cutting that binding leaves the
counter permanently 0 with no other writer.

**PROVEN — the live consequence while it stays a leg.** `CvCity::hasPowerSource()` reads
`isAreaCleanPower()`, so an area gaining or losing clean power moves `CvCity::isPowered()` for every city of
that team **silently**: no `SEVT_CITY_POWER_*` crossing is announced, so plane C keeps deposits gated on
`HAS_POWER` that nothing withdraws, and the enabler's power gate goes stale with nothing to re-derive it
([DEC-maintained-sum](../../architecture/decisions.md#dec-maintained-sum)).

**RULED OUT — it is NOT a cascade channel, and authoring `cleanPower` as a modifier family is the wrong fix.**
[state-repositories.md](../../architecture/state-repositories.md) already rules this exact case: *"the ONE
genuine area concept is a PHYSICAL CONTIGUITY constraint (you cannot run power lines across an ocean), which is
the engine-side clean-power counter and never a cascade channel."* A landmass is not an ownable scope, so there
is no `area` scope to author into.
⚠ `Tools/Migration/migrate_buildings.py:55` maps `bAreaCleanPower` → `("area", "cleanPower")` — a scope that
does not exist in the model. That script is **not** the live curator (`curate_building.py` is), so it is a dead
mapping, but it is exactly the wrong shape for anyone who finds it first.

**RULED OUT — it is not a second amenity (owner):** *"area clean power is just power to all cities, it's not a
separate amenity."* It grants the SAME `AMENITY_PROVIDES_POWER`, with area-wide reach — the ordinary
wider-reach grantor shape [contexts.md](../../architecture/contexts.md) specifies for civic/trait/tech
amenities, where the grantor fact fans over the cities that already stand and a city starting to exist folds
what its area already holds.

**RULED OUT — there is NO clean-vs-dirty axis to model (owner): *"power is power, clean or otherwise."*** The
word "clean" describes the absence of the UNHEALTH a dirty plant carries, never a different kind of power — so
that belongs entirely to the unhealth the dirty grantor authors, and a coal plant is simply a power grantor
that also deposits its own `unhealth`. ⛔ So do NOT mint a second amenity key, a qualifier on the existing one,
or a clean/dirty discriminator anywhere: the store holds ONE key and knows nothing about provenance.
⚑ **The name is INHERITED, not designed (owner)** — someone called it that while implementing the Hoover Dam
(the tag comes down from the legacy schema and is believed to be vanilla Civ4's own). So it names ONE building's
implementation, never a concept, and there is nothing in the spelling to preserve or reason from.
⚠ `CvArea::isCleanPower` / `m_aiCleanPowerCount` keep it today; they are renamed with the fix, and until then
the word is read as "power", full stop.

**NOT YET KNOWN — the two ends, and they are separable.**
- Which entity carries the grant, and how a building/project reaches an (area × team) counter from the DLL side
  once the Python binding is gone.
- Whether the fold takes it as an amenity grantor per city (one `applyKey` per city of the area, so the
  existing `announcePowerCrossing` covers it for free) or the area fact routes into the fold per affected city.
  The first keeps one mechanism; the second keeps the counter authoritative. Both need the grantor first.

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
| `CyCity.getName` (and `getPopulation` / `getFood` behind it) | `CvWBInterface.writeDesc` → `CvWBDesc.write` (`:1819`/`:1483`/`:1067`) | WORLDBUILDER — accepted breakage ([roadmap.md](roadmap.md) § scope decision 1b) but recorded here because that ruling requires a knowingly-broken WB path to be SAID, not left silent. ⚠ NOT one method: `CyCity` publishes only the IDENTITY SET (`getID`/`getOwner`/`getX`/`getY`), so the whole city writer wants `CyState` reads (`getCityName` / `getCityPopulation` / `getGrowth`) |
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
- **Nothing can enumerate the GREYED tier.** `EnablerDomain::inTreeIds` (LISTED + GREYED — the visible tri-state)
  has no caller, while its sibling `listedIds` has many. The "go get copper / adopt this civic" greyed build-list
  entries that [enabler.md §6](../../specs/enabler.md) specifies cannot be rendered by anyone.
- **The enabler's validation-oracle surface is unrunnable** — `BuildingEnabler::verifyCity`, `TechEnabler::available`
  and `UnitEnabler::explain` all have zero callers, and no route reaches them. ⛔ This is the load-bearing one:
  `CvTechEnabler`'s header states the design contract that the enabler consumes ONLY events *precisely so* a
  missed emit surfaces as a visibly wrong enabler, with the oracle diff as the tripwire. **The event-only design
  is resting on a tripwire nothing pulls.**
- **Emit the load-pipeline diagnostic.** `emitLoadPipeline` is the ONE spine endpoint with no emitter anywhere —
  the kind, the renderer and the field decode are all built. So load-stage timings, fixpoint pass/flip/converge
  counts and the verify-catch count reach neither the log nor `/events`, and load time is the currency that pays
  for turn time ([DEC-turn-time-is-king](../../architecture/decisions.md#dec-turn-time-is-king)).
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
- **Wire the operating-building seed and its maintenance hooks.** `EnablerKernel::seedOperatingBuildings` has no
  caller, and the caller its own comment names — `CvCity::refreshOperatingBuildings` — does not exist. The six
  `on*Active` hooks are equally unreached. `operatingBuildings()` is a bare fetch by ruling, so nothing rescues
  it: the set is empty for every city, so every building reads DORMANT, `wireOperatingBuildings` fills the eval
  ctx with empty active/provided sets, `cityHasVicinityBonus` answers false, and the trigger plane grants
  nothing. ⚑ Diff `/computed/enabler/operating/{stored,oracle}` to confirm — that pair exists for exactly this.
- **⛔ Stop reading a NEGATIVE band bound as "no bound" — this is the landmine directly behind the item above.**
  `CvJsonConditionParse` defaults an unauthored `min`/`max` to **-1**, and the PROPERTY band atom in
  `CvConditionEval` then treats *any* negative as absent (`a->min < 0 || …`). Its own comment four lines up says a
  `PROPERTY_*` value can be legitimately negative, so the parse sentinel and the atom contradict each other. Every
  `PROPERTY_EDUCATION` low-education tier is authored with a negative min **and** max, so BOTH bounds are dropped
  and the band clause is unconditionally true; the positive-side bands are unaffected, which is exactly why it
  reads as working. Those tiers are `notConstructible`, so `CvCity::placeSystemBuildings` puts them in every city
  at founding, and the `dormant` successor ladder then leaves the DEEPEST tier — the one with no successors —
  permanently active regardless of the city's real education. ⚑ The seed item above is what hides it — deposits
  only flow for buildings in the operating set. **Fix both in the same change, or wiring the seed lights up a
  crippling city-wide penalty and takes the blame for it.** The fix is a real absent-sentinel (a has-bound flag,
  or `INT_MIN`/`INT_MAX`), never a `< 0` test — see [enabler.md](../../specs/enabler.md) §3.
- **Delete `CvPropertyInfo::getPropertyBuildings`, `m_aPropertyBuildings` and their CURATOR-GAP comment.** Nothing
  reads the getter, and the comment promises a resolution the band model supersedes
  ([enabler.md](../../specs/enabler.md) §3).
- **Give `CityContext::amenities` a load path and a working removal.** Its feeder is the operate crossing, which
  the dead seed above is what fires on load — so the fold is empty after every load and `isGovernmentCenter`,
  `getPowerCount`, `isNoUnhappiness` and the health flags all read false. The removal leg is gated on the same
  empty operating set, so amenities never decrement while additions fire: the dictionary grows monotonically for
  the life of a game.
  ⛔ **The building leg RIDES THE ENABLER'S CROSSING, and that is correct — do not "fix" it by moving the leg onto
  a fact the city emits from its own read.** The enabler is a SOURCE OF FACTS, never the home of this answer, and
  the dictionary is the final stopping place ([contexts.md](../../architecture/contexts.md)). The ordering ban
  applies to a store that RE-DERIVES by reading another system's built set; a store that CONSUMES a delta has no
  such dependency and builds identically whenever the facts arrive.
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
- **Retire `CvDerivedCache` by emptying it, one tenant at a time**
  ([DEC-contextdict-replaces-derivedcache](../../architecture/decisions.md#dec-contextdict-replaces-derivedcache)).
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
- **Delete `CvCity::doVicinityBonus`** — a per-turn blanket clear plus lazy recompute-on-read beside the
  event-built vicinity store ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)). Nothing is
  missing behind it: the maintained store already exists, and this cache's own comment claims a reader that has none.
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

- **`SEVT_PROPERTY_ADDED / _REMOVED` is emitted into the void.** The fact fires from the `CvProperties` mutation choke
  points, but repo-wide the id exists ONLY as an enum entry, a log spelling, and the emitter — **no consumer
  carries a case for it**. So a property value moving never re-checks the bands it crosses.
- **The receiving machine is built and has zero callers**: `EnablerKernel::onPropertyBandHitActive` and
  `propertyBandThresholds` (the threshold union the watermark was to read) are reached by nobody, and the
  `s_operateNeedsLiveState` bucket that would otherwise catch these is a DEAD STORE — pushed to at load, never
  read. The kernel opted out of the dynamic path in a comment promising "the watermark emits a targeted band-hit
  on a threshold crossing". The watermark does not exist.
  ⚑ **The un-announced fact is: "property P crossed one of its registered band boundaries in city C."** The
  threshold table is already built; emit the crossing and route it — do not add a per-turn re-scan
  ([DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal)).
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
- **⛔ Delete `CvDerivedData` — it is a REVIVED KILLED IDEA.** `superseded-ideas.md` #1 records the derived-data
  repository (`TLazy`/version/staleness aggregation) as killed with an explicit *"don't revive the repository"*, and
  the file is back: four empty repositories, members on four game objects, zero tenants, and doc-comments
  teaching ensure-on-read plus a "bounded staleness" periodic rebuild as the sanctioned architecture. It also
  cites a plan doc that does not exist. ⚑ It executes nothing, which is exactly why it is dangerous — it is what
  the next agent reads when asking how derived caches work here.
- **The city's vicinity fact is derived THREE times.** `CityContext` stores it properly, while `CvCity` keeps a
  hand-rolled ensure-on-read memo gated on a multiplayer option AND a per-turn blanket wipe over every bonus in
  every city — under a comment claiming no stored context copy exists, which the live `CityContext` dictionaries
  refute. ⚠ The `had*VicinityBonus` arrays it feeds are SERIALIZED derived state
  ([DEC-derived-never-trusted](../../architecture/decisions.md#dec-derived-never-trusted)) and have **zero**
  readers in the whole tree — a per-turn radius scan and a save payload feeding nothing.
- **Give `capabilities` its generated classification ids.** The team caps carry three runtime
  `std::set<std::string>` rebuilt by heap-copying strings per team per mark, to serve three accessors with zero
  call sites — while the string-keyed source getters ARE called ~80 times from AI diplomacy with string literals.
  [DEC-classification-infos](../../architecture/decisions.md#dec-classification-infos) makes the union a bitset OR
  and deletes the key table, the three sets and the flag mirror together.
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
- **Rename or delete `clearCanConstructCache`** — both overloads ignore their parameters, have identical bodies,
  and say in their own text that there is no cache to clear. A name advertising a dead cache is the bait.
- **Spell out the bare single-letter identifiers** in the enabler/capability code (`j` for the info, `c`, `r`,
  `s`, `sb`, `jg`, `a`, `mem`, `wcap`). Per Sources/AGENTS.md this is a review-blocker on sight, and it is NOT
  the sanctioned exception — that covers a file-anchored PREFIX, never a bare parameter name.

## ⛔ COMMENTS THAT CONTRADICT THEIR OWN CODE

> The highest-signal rollerskate: the right intent written down, something else implemented, and the comment
> reassuring every subsequent reader. ⚑ Two cite a `DEC-*` id that does not exist in the ledger at all.

- **A phantom `DEC-json-not-cascade` is cited in seven places and defined nowhere.** A second citation invokes
  `DEC-mirror-then-redesign`, which is not a ruling either — it is in `superseded-ideas.md` as DEAD, under
  "never re-argue that a shape must be preserved because it is what the engine does today". Both are load-bearing
  justifications resting on authority that does not exist.
- **A false "verified" claim, refuted 35 lines below it in the same file** — the reverse pass asserts no
  corporation headquarters registry exists and nothing asks for one; the registry, its feeder and four consumers
  are all there. ⛔ The word "verified" is what pre-empts the check, so an agent trusting it builds a second
  registry.
- **A cached-read contract on an uncached read** — the city maintenance getter's header promises "a BARE FETCH of
  the derived cache — never a gate test, never a recompute", while the body gates and loops every kind through
  the cross-scope legs, and concedes in its own text that nothing is cached. It misdirects the turn-time hunt.
- **A live enum documented as dead** — the GREYED tri-state is marked "unused until it lands" while it is
  assigned and wired across eight domains, with the same stale claim repeated on two sibling headers.
- **A component and a hook that never existed** — the enabler header attributes trait maintenance to a
  `TraitEnabler` with an `onTraitChanged` hook; neither has ever existed in the tree. Every sibling line around it
  resolves, which is what makes it invisible.
- **The evaluator is described as a class in three files**; it is a free function. And the promotion
  negative-effects derivation is blocked by a comment saying it "waits on the firstStrike.chance vocabulary row"
  — that row is live and already read elsewhere. Only the comment blocks it.
- **The likely SEED of the ×100 cluster**: the JSON parse header claims "a BLANKET ×100 at every magnitude leaf".
  It is not blanket — percent leaves are skipped. Fix this one first; it is what the next agent reads before
  writing another divide.

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
- **Delete `CvDerivedData` — a rival cache framework with zero tenants.** `TLazy<>` is declared on four owners and
  `reset()` on all four, and not one datum has ever been declared in it. ⛔ It is a rollerskate GENERATOR: it is
  what the next agent finds when asking how derived caches work here, and it documents recompute-on-read plus a
  "bounded staleness" periodic rebuild as the sanctioned architecture — exactly what
  [DEC-no-self-heal](../../architecture/decisions.md#dec-no-self-heal) and
  [DEC-uniform-cache-shape](../../architecture/decisions.md#dec-uniform-cache-shape) refuse. It also points twice
  at a plan doc that does not exist.
- **The per-turn victory-city recount** wipes and refills two hand-named scalars whose only inputs are victory
  validity and immutable info data. Cheap, same shape, same missing emit.
- **Owner call needed on the AI turn-scoped memo clears** (tech values, mission targets, civic values, build
  values, unit counts, trade routes, resource consumption). They memoize AI *valuations* rather than derived game
  state, so whether the no-self-heal rule binds them is a ruling, not an agent's call. ⚑ Start with the mission
  target cache — its own comment says it is force-recalculated "for reliabilty reasons (more robust to bugs)".

## ⛔ VALUE CORRUPTION — the ×100 cluster's REMAINDER

> ⚑ The contract, so it is never re-guessed: `mod_valueForUnit` returns a `CASC_UNIT_PERCENT` read as a PLAIN
> HUMAN PERCENT; a FLAT is ×100 and reduces at its point of use. **Ask the KIND's unit, never the family's** — a
> family-wide blanket on a per-kind-split family produced every defect in this cluster
> ([DEC-fixedpoint-x100](../../architecture/decisions.md#dec-fixedpoint-x100)).

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
- **Trade routes — counted twice, and one leg is at the wrong scope.** `m_iTradeRoutes` is fed from building
  deposits at empire scope AND from tech deposits read at CITY scope into an empire accumulator, while the city
  read already rolls team+empire+city. Serve from the cascade read alone and soft-remove the member.
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
- **Rule on the revolution mirror.** It is plausibly the sanctioned Python-authoritative mirror, but one leg
  reads CITY scope into an empire accumulator and, unlike every sibling, applies no `/100`. Even if the mirror
  stays, that leg is wrong.

## Rollerskates — the abandoned path, still in the tree

> Evidence of a path someone tried and left behind
> ([DEC-no-rollerskate-evidence](../../architecture/decisions.md#dec-no-rollerskate-evidence)). It holds the NAMES
> of dead things, which is what sends the next agent re-treading them — and being preprocessor-skipped or
> commented, none of it is visible to the compiler census.

- **Delete the `#ifdef` attic.** Each guard below is defined NOWHERE — not in `Sources/`, not in `fbuild.bff`, and
  not even as a commented-out `#define` — so no switch ever existed and the block is an abandoned alternate:
  `USE_BOTH_TECHBUILDING_EVALUATIONS` (a whole second, older tech-building valuation parked inside
  `AI_techBuildingValue` — the very function the enablement-valuation ruling governs), `USE_OLD_PATH_GENERATOR`
  (the old pathfinder, across `CvUnitAI`/`CvGameCoreUtils`/`CvSelectionGroup`/`CvUnit`, with live `#else`
  branches), `VALIDITY_CHECK_NEW_ATTACK_SEARCH` (a migration-era harness that re-runs the old brute-force search
  and diffs it), `TEMP_DEBUGGING_SUPPORT` (a parked `StreamWrapper : FDataStreamBase`), `EXTREME_PAGING`,
  `EXPERIMENTAL_FEATURE_ON_PEAK`, `DEBUG_TECH_CHOICES`.
  ⛔ **Do NOT sweep the OFF-SWITCHES with them.** A guard that HAS a commented-out `#define` is un-killed forward
  intent and the disposition is the owner's ([DEC-keep-unkilled-ideas](../../architecture/decisions.md#dec-keep-unkilled-ideas)):
  `ENABLE_FOGWAR_DECAY`, `USE_MEMMANAGER`, `GLOBAL_WARMING`, `THE_GREAT_WALL`, `USE_INTERNAL_PROFILER`,
  `NO_RANDOM`, `VALIDATION_FOR_PLOT_GROUPS`, `VERIFY_CAN_BUILD_CACHE_RESULTS`, `VERIFY_PLOT_DANGER_CACHE_RESULTS`,
  `VERIFY_YIELD_CACHE_RESULTS`, `DYNAMIC_PATH_STRUCTURE_VALIDATION`, `LIGHT_VALIDATION`. The mechanical test is
  the commented `#define`, never the guard's name.
- **Record WHY each off-switch is off, in its subsystem's reference doc.** Only `ENABLE_FOGWAR_DECAY` has its
  reason written down; the rest do not, and an unexplained off-switch is what the next sweep eats.
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

