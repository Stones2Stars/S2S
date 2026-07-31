# Python read-map census (stage-4 input)

> Evidence base for the stage-4 Python library ([patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md)) — the evidence base for the stage-4
> Python surface, everything OUTSIDE the pedia. The pedia slice is mapped in
> [pedia-map.md](pedia-read-map.md) and is excluded from the detailed work here (§1 reconciles the totals).
>
> Per [DEC-cy-not-fixed](../architecture/decisions.md#dec-cy-not-fixed) and
> [DEC-new-getter-surface](../architecture/decisions.md#dec-new-getter-surface) this maps **NEEDS, not getters
> to port**. The legacy `Cy*` read bindings are GONE: the composition root
> (`DLLPublishToPython`, `Infrastructure/CvDLLPython.cpp`) publishes the enum int-conversions, the vector +
> `IDValueMap` container interfaces, the debug/Win32 helpers, and **`CyEnabler` — 17 availability reads, the only
> info/state surface Python can reach**. The `Cy*` WRAPPER classes stay for the engine→Python direction; a wrapper
> with no binding is the correct end state ([patterns.md](../architecture/patterns.md)).
>
> So every count below is **DEMAND** — what a consumer must be SERVED by the one data-fetching library — never
> what call survives. Counts are script-derived from the current tree; the method is in §1 so every number can be
> re-derived.

## 1. The surface, counted

### 1.1 Method (reproducible)

> **Re-derive with `python Tools/census-python-boundary.py`** — it measures both directions and emits the tables
> below ready to paste. These numbers describe a tree that is actively being cut, so they drift by nature: when
> they matter, RE-RUN rather than trust. `--demand` adds the full per-name demand list.

Receiver-name heuristics are useless here — `CvBuildingInfo`, `CyCity` and `CyPlayer` are all used as ordinary
*local variable names* in this tree, so "what does `GC.` call" undercounts and "what does `X.get*()` call"
overcounts by thousands. And the published surface no longer answers reads, so a call cannot be keyed on
"does this name resolve to a live binding". The census therefore keys on **UNSERVED DEMAND**:

1. Parse every `.def("<name>"` under `Sources/` → the **published** surface. The read half is
   `Sources/Python/CyEnabler.cpp` (17 names); the remainder are container/debug/util publishes carrying no
   entity data.
2. Scan every `.py` under `Assets/Python`, skipping full-line comments, for `<receiver>.<method>(`; drop
   `self.` receivers.
3. Keep the call when the method is **engine-shaped** (`get`/`is`/`can`/`set`/`change`/`calculate`/`find`/`AI_`/
   `do`/`create`/`init`/`has`), is **not published**, and is **not defined anywhere in Python**. That call has no
   answer in the tree, so it is demand the library must serve.
4. Bucket by RECEIVER into a read KIND (§4).

**Two distortions to hold onto, both one-directional so the totals are a FLOOR:**

- **A name Python also defines itself is dropped wholesale.** `getText` is the case that matters — `BugUtil.py`
  defines one, so all **3,168** `.getText(` sites fall out of the tables below. TEXT is therefore *not* sized here;
  it is a separate plane the library does not own (§4.1).
- **Receiver bucketing is heuristic**, so the split between STATE/COMPUTED/INFO moves at the margin; the
  totals do not depend on it.

### 1.2 Headline

| Measure | Value |
|---|---|
| Python files | **206** |
| Lines | **107,650** |
| Distinct methods called on any receiver | 3,286 |
| Published `.def` names (read half — `CyEnabler`) | **17** |
| **UNSERVED engine-shaped reads** | **2,070 names / 21,279 call sites** |

That second row is the whole Python→C++ read surface today, and the third is the size of the gap it leaves.
The library is built toward answering the third; **it is an END STATE, never a gate on cutting**
([roadmap.md](../plans/structural-cleanup/roadmap.md)).

### 1.3 By directory

Unserved engine-shaped reads, per §1.1. This is the demand each family places on the library.

| Directory | Files | Unserved sites |
|---|--:|--:|
| `EntryPoints/` | 14 | 3622 |
| `Screens/Worldbuilder/` | 19 | 3444 |
| `Screens/` | 23 | 2674 |
| *(repo root)* | 10 | 2510 |
| `Revolution/Gameready/` | 3 | 2010 |
| `Screens/Advisors/` | 11 | 1856 |
| `Contrib/` | 23 | 1287 |
| **`Screens/Pedia/`** | 21 | 855 |
| `Revolution/` | 7 | 798 |
| `pyWB/` | 1 | 421 |
| `BUG/` | 28 | 371 |
| `Afforess/` | 3 | 339 |
| `DancingHoskuld/` | 4 | 290 |
| `Screens/Debug/` | 2 | 180 |
| `PitBoss/` | 2 | 126 |
| `Utilities/` | 4 | 120 |
| `Screens/SimpleScreens/` | 3 | 105 |
| `Platyping/` | 2 | 73 |
| `Revolution/Development/` | 1 | 50 |
| `BUG/Tabs/` | 16 | 42 |
| `EnhancedTechConquestUtils/` | 2 | 33 |
| `Screens/ExtensionScreens/` | 1 | 30 |
| **`Screens/Sevopedia/`** | 2 | 26 |
| `DataStorage/` | 3 | 9 |
| `Sparth/` | 1 | 8 |

### 1.4 Reconciliation with the pedia slice

The pedia set (`Screens/Pedia/` 21 + `Screens/Sevopedia/` 2 + `Contrib/UnitUpgradesGraph.py`) is **24 files /
934 unserved sites** (INFO 742 · other-UI 182 · STATE 9 · COMPUTED 0 · MUTATION 0).

[pedia-map.md](pedia-read-map.md) reports **~1,283 static call sites** for the same set. The two are consistent and
measure different things: pedia-map counts *every non-UI `get*/is/has/parse*` call in the file* — including calls
Python defines itself and `CyGInterfaceScreen` accessors like `getXResolution` — while this census counts only
reads nothing in the tree answers. **Both stand; use pedia-map's for the pedia's own work and this one when
reconciling against the 21,279 total.**

**Excluding the pedia, this census covers 20,345 unserved sites.**

## 2. ⚑ The owner's hypothesis: is the pedia a completeness oracle?

> *"Build the reader surface for the pedia and we have everything."*

**VERDICT: TRUE-WITH-A-LISTED-APPENDIX — for the INFO plane only, and the appendix is not small.**

The hypothesis is right about the *shapes* and wrong about the *coverage*, and it is silent about three whole
read kinds. Both halves are load-bearing.

### 2.1 Where the hypothesis holds

Serving the pedia forces the library to produce exactly the structured shapes
[pedia-map.md §5](pedia-read-map.md) specifies — the per-entity page payload, the per-type index payload, the edge
lists, the rendered entry lines, the requires section object. **Nothing in the rest of the tree asks for a
*shape* the pedia does not already force.** Every non-pedia INFO consumer censused here is served by some
projection of those same five shapes. That is the real content of the owner's intuition and it survives contact
with the data: the pedia is the **shape** oracle.

It is also close to a *type* oracle for the modifier-carrying and enabler-carrying types — the critical set from
the [green gate](../plans/structural-cleanup/roadmap.md). For buildings, units, techs, promotions, bonuses, improvements, civics,
traits, terrains, features, routes, corporations, religions, projects and specialists, the pedia already reads
the widest field set of any consumer.

### 2.2 The residue — measured

Set-difference of INFO-plane method names used **outside** the pedia against those used **inside** it:

- INFO names used outside the pedia: **447**
- INFO names used inside the pedia: **257**
- **RESIDUE (outside-only): 293 names / 1,632 call sites**

That residue is not homogeneous. Grouped by what it actually is:

| Residue group | Names | Sites | Is it a library need? |
|---|---|---|---|
| Mutations on globals (`setDefineINT`, `changeDefineINT`) | 2 | 70 | **No** — a write. §4 MUTATION boundary. |
| Engine HANDLE accessors (`getMap`, `getMapByIndex`, …) | 7 | 285 | **No** — object handles, not data. |
| Global DEFINE / constant reads (`getMAX_PC_PLAYERS`, `getMAX_PLAYERS`, `getBARBARIAN_PLAYER`, …) | 6 | 350 | **Yes, but trivially** — a closed constants block. |
| **Whole info TYPES with no pedia page** | **59** | **328** | **YES — the real appendix.** |
| **Per-field reads on types the pedia DOES page** | **219** | **599** | **Partly — see 2.4.** |

### 2.3 The appendix that matters: 59 info types the pedia never touches

These types have **no pedia page at all**, so serving the pedia yields *nothing* for them. Highest-weight first:

`getWorldInfo` (60) · `getNumCivicOptionInfos` (31) · `getClimateInfo` (20) · `getEventTriggerInfo` (18) ·
`getCultureLevelInfo` (16) · `getProcessInfo` (11) · `getControlInfo` (10) · `getMissionInfo` (9) ·
`getVoteSourceInfo` (8) · `getVictoryInfo` (8) · `getEspionageMissionInfo` (7) · `getPropertyInfo` (4+6) ·
`getDomainInfo` (5) · `getAttitudeInfo` (5) · `getUnitAIInfo` (4) · `getEventInfo` (4) · `getColorInfo` (4) ·
`getSeaLevelInfo` (3) · `getVoteInfo` (3) · `getDiplomacyInfo` (3) · `getHurryInfo` (2) · `getMemoryInfo` (2) ·
`getGraphicOptionsInfo` (2) · `getMPOptionInfo` (2) · `getBonusClassInfo` (4) · `getGameOptionInfo` ·
`getCalendarInfo` · `getSpecialUnitInfo` · `getEffectInfo` · `getAdvisorInfo` · `getArtInfo` ·
`getActionInfo` · `getForceControlInfo` · plus the matching `getNum<X>Infos` counters.

Four clusters, each with a distinct owner:

1. **Map-generation types** — `WorldInfo`, `ClimateInfo`, `SeaLevelInfo`, `MapInfo`. Consumed by
   `CvMapGeneratorUtil.py` and the map scripts. See the §7 open question on whether map scripts sit behind
   this library at all.
2. **Game-configuration types** — `GameOptionInfo`, `MPOptionInfo`, `ForceControlInfo`, `GraphicOptionsInfo`,
   `PlayerOptionsInfo`, `CalendarInfo`, `HandicapInfo`, `GameSpeedInfo`. Consumed by WorldBuilder, `pyWB`,
   the options screen and `RevolutionInit`.
3. **Diplomacy / victory / vote types** — `VictoryInfo`, `VoteInfo`, `VoteSourceInfo`, `AttitudeInfo`,
   `MemoryInfo`, `DiplomacyInfo`, `EspionageMissionInfo`. Consumed by `CvVictoryScreen`, `CvForeignAdvisor`,
   `CvDiplomacy.py`, `BUG/AttitudeUtil.py`.
4. **Command / UI-action types** — `ControlInfo`, `MissionInfo`, `ActionInfo`, `CommandInfo`, `AdvisorInfo`,
   `ArtInfo`, `EffectInfo`, `ColorInfo`, `DomainInfo`, `UnitAIInfo`. Consumed by `CvMainInterface` and the
   debug/log surfaces.

**Note the overlap with the info rebuild's own scope:** `CvVictoryInfo`, `CvVoteInfo`, `CvHurryInfo`,
`CvWorldInfo`, `CvHandicapInfo` and `CvProcessInfo` are all rebuilt types per
the rebuilt info surface ([patterns.md](../architecture/patterns.md)) — so the library must serve them regardless; the pedia simply never
asks. That is the precise sense in which the pedia under-specifies.

### 2.4 The per-field residue, qualified

219 names / 599 sites read fields on types the pedia *does* page. **90 of those names (296 sites) had exactly
one consumer: the deleted `Screens/Debug/TestCode.py`** — a diagnostic screen that dumped the entire legacy
field contract of every info. Exhaustiveness was its whole purpose, so it inflated the residue with reads no
gameplay or UI surface needs. **It is DELETED (owner ruling — see §7), so those 90 names carry no obligation.**

**The per-field residue is therefore 129 names / 303 sites**, whose consumers are
`Contrib/RevDCM.py` (26), `Screens/CvVictoryScreen.py` (25), `Revolution/RevUtils.py` (25),
`RevolutionWatchAdvisor.py` (15), `Revolution/RevolutionInit.py` (13), `CvAdvisorUtils.py` (10),
`CvDiplomacy.py` (10). **That is the genuine appendix and it is dominated by the Revolution stack** (§6).

### 2.5 What the hypothesis is silent about

The pedia is **almost purely a static-info reader**: of its 934 unserved sites, 742 are INFO, against
**9 STATE, 0 COMPUTED and 0 MUTATION**. The rest of the tree is not:

| Kind | Whole tree | Pedia | **Non-pedia** | Pedia's share |
|---|--:|--:|--:|--:|
| INFO | 6,909 | 742 | 6,167 | 10.7% |
| STATE | 4,741 | 9 | 4,732 | **0.2%** |
| COMPUTED | 1,949 | 0 | 1,949 | **0%** |
| MUTATION | 987 | 0 | 987 | **0%** |
| other / UI | 6,595 | 182 | 6,413 | 2.8% |

**The pedia exercises essentially none of the LIVE-STATE, COMPUTED or MUTATION planes — 7,668 call sites the
pedia never touches.** Building only what the pedia needs would leave the majority of Python's engine
traffic unserved. This is not an argument against the hypothesis — those planes are not what an *info* library
is for — but a completeness claim scoped to "the reader surface" must say so explicitly, because
[the ONE-SURFACE ruling](../architecture/patterns.md) makes a single uncovered read a reach-around into legacy, and a
reach-around is the second live surface the ruling forbids.

### 2.6 The verdict, operationally

Serving the pedia gives: **every shape, and the INFO plane for the paged types.** It does not give: **59
unpaged info types, ~129 per-field reads concentrated in the Revolution stack, the global DEFINEs block, and
the entire STATE / COMPUTED / MUTATION surface.** The stage-4 tick-list is therefore
**pedia-map.md + §2.3 + §2.4 + §3 of this document**, not pedia-map.md alone.

## 3. Consumer families, ranked by weight

What each family must **be served**, expressed in the structured shapes of
[patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md).

### 3.1 `EntryPoints/` — 3,622 sites, 14 files — the engine's call-in surface

Dominated by **`CvRandomEventInterface.py` (2,942 sites)**, the single largest engine consumer in the tree, plus
`CvOutcomeInterface.py` (412) and `CvCultureLinkInterface.py` (170 — almost pure info).

Needs served: **per-entity payloads** for the event/trigger types (§2.3 cluster) and for every entity an event
names; **live-state reads** on the player/city/unit the event fires against; **availability verdicts** for the
`canDo*` half; and a **MUTATION boundary** (§4) for the `do*`/`apply*` half. This family is the clearest proof
that the library alone is insufficient: a single random-event handler reads static info, live state and a
computed verdict, then writes.

`CvCultureLinkInterface.py` is the cleanest case in the tree — 165 INFO sites, 8 STATE, nothing else: a pure
**per-type index payload + edge-list** consumer.

### 3.2 `Screens/Worldbuilder/` + `pyWB/` — 3,865 sites, 20 files — the editor

`WorldBuilder.py` (520), `pyWB/CvWBDesc.py` (421, the save/load serializer), `WBUnitScreen.py` (387),
`WBPlotScreen.py` (356), `WBCityEditScreen.py` (245), `WBInfoScreen.py` (231), `WBPlayerUnits.py` (184).

Needs served: **per-type index payloads** (every editor drop-down is "give me [(id, name, button)] for type T"
— 19 such scans), **live-state reads** for the current value of the field being edited, and a **write
boundary**: this family carries 447 MUTATION sites, the densest in the tree. `CvWBDesc.py` additionally needs
**stable type KEYS, not ids** — it serializes scenarios to text, so it reads `getType()` strings rather than
indices, which the identity block must keep serving.

### 3.3 The Revolution stack — 3,348 sites

**`Revolution.py` alone is 1,733 sites** — the second-largest consumer and the most STATE-heavy file in the
tree. Detail in §6.

### 3.4 Repo-root modules — 2,510 sites, 10 files — the gameplay callbacks

`CvEventManager.py` (953), `MapScriptToolsOld.py` (591), `CvAdvisorUtils.py` (323),
`CvMapGeneratorUtil.py` (261), `CvDiplomacy.py` (182), `CvGameUtils.py` (112), `OOSLogger.py`.

`CvEventManager.py` is the engine's primary Python callback host and the dispatch hub (§5.4). Needs served:
per-entity payloads on the entity an event concerns, plus heavy live-state and mutation traffic.
`CvDiplomacy.py` is INFO-heavy (148/187) — it needs the **diplomacy/attitude/memory types** from §2.3 cluster 3.

### 3.5 `Screens/Debug/` — 180 sites, 2 files — the diagnostic surface

`HelperFunctions.py` (131) hosts `getGOMReqs`, the condition-tree walker (§4.2), and is also a pedia helper.
`TestCode.py` is GONE (§7), which is why this family is now the smallest rather than one of the largest — it
carried the exhaustive per-field info dump and the 90 residue names of §2.4.

### 3.6 `Screens/` (non-pedia) — 2,674 sites, 23 files — the main UI

`CvMainInterface.py` (1,085), `CvVictoryScreen.py` (548), `CvInfoScreen.py` (298), `BuildListScreen.py` (115),
`CvSpaceShipScreen.py`, `CvHallOfFameScreen.py`, `CvOptionsScreen.py`, `Forgetful.py` (56).

This family is also the heaviest in screen CHROME — much of what it does is assembling localized strings, which
is the TEXT plane the library does not own (§4.1). `CvMainInterface.py` is the sole significant consumer of the command/UI-action types
(`ControlInfo`, `MissionInfo`, `ActionInfo`, `AdvisorInfo`, `ArtInfo`, `PropertyInfo`) from §2.3 cluster 4.
**`Screens/Forgetful.py` enumerates `getNum<X>Infos` for 51 distinct info types** — a whole-registry sweep, and
the widest type coverage of any single file in the tree (wider than the pedia hub's ~20). It is the one consumer
that needs the **complete per-type index across every registered type**, including a dozen the pedia never pages
(`Calendar`, `Climate`, `Command`, `Control`, `Effect`, `Emphasize`, `EspionageMission`, `Event`, `EventTrigger`,
`GameOption`, `Goody`, `Hurry`, `MPOption`, `Mission`, `SeaLevel`, `Season`, `SpecialBuilding`, `SpecialUnit`,
`Upkeep`, `Victory`, `Vote`, `VoteSource`, `World`, `Denial`). Treat it as the acceptance case for
"the library can enumerate every type", not as a long-tail screen.

### 3.7 `Screens/Advisors/` — 1,856 sites, 11 files

`RevolutionWatchAdvisor.py` (434), `CvDomesticAdvisor.py` (334), `CvForeignAdvisor.py` (236),
`CvTechChooser.py` (229), `CvEspionageAdvisor.py`, `CvMilitaryAdvisor.py`.

Needs served: **per-type index payloads + computed per-city/per-player values in table form.** Two of these
build their columns through `eval` on data tables (§5.2) — the highest-value grep-invisible finding in the tree.
`CvTechChooser.py` is INFO-heavy (102/199) and is an **edge-list** consumer (tech prerequisite graph).

### 3.8 `Contrib/` — 1,287 sites, 23 files

`autologEventManager.py` (358), `DynamicCivNames.py` (143), `Civ4lerts.py` (127), `RevDCM.py` (56),
`UnitUpgradesGraph.py` (pedia, excluded), `EventSigns.py`, `UnitNameEventManager.py`, `RandomNameUtils.py`.

Mixed. `DynamicCivNames.py` needs civic/civilization identity + **the civic-option index**;
`Civ4lerts.py` is live-state polling; `autologEventManager.py` is TEXT-heavy logging.

### 3.9 `BUG/` + `BUG/Tabs/` — 413 sites, 44 files — the options framework

Low direct engine weight, **high indirection weight**: this is the config-driven dispatch layer (§5.3). Its
needs are mostly *not* data — it needs enum resolution by name (`WidgetTypes`, `InputTypes`,
`InterfaceDirtyBits`) and the option store. See §7.

### 3.10 Map scripts — `CvMapGeneratorUtil.py` (261) + `MapScriptToolsOld.py` (591) + `Assets/Maps`

Needs served: the map-generation types (§2.3 cluster 1) + plot/terrain/feature/bonus placement, which is
**write-heavy**. Whether these belong behind the same library is a §7 open question.

### 3.11 Long tail

`Afforess/` (339 — settings screens driving `setDefineINT`), `DancingHoskuld/` (290), `PitBoss/` (126),
`Utilities/` (120), `Platyping/` (73), `EnhancedTechConquestUtils/` (33), `DataStorage/` (9), `Sparth/` (8).

## 4. The read-KIND split

With the bindings cut there is no owner class to key on, so the classification is derived from the **receiver**
plus the method-name prefix (§1.1). It is mechanical and re-derivable, and heuristic at the margin — the split
between STATE and COMPUTED moves, the totals do not.

| Kind | Sites | Distinct names | Receivers |
|---|--:|--:|---|
| **(a) INFO — static data** | 6,909 | 536 | `GC.`/`gc.` info registry + `*Info` objects |
| **(b) STATE — live game state** | 4,741 | 443 | city · player · plot · unit · team · game · area · deal |
| **(c) COMPUTED — verdicts/rates** | 1,949 | 245 | same objects, `can*`/`is*`/`AI_*`/`calculate*`/`find*`/`has*` |
| **(d) MUTATION — writes** | 987 | 176 | same objects, `set*`/`change*`/`do*`/`create*`/… |
| **(e) TEXT — residue only** | 98 | 5 | text-manager receivers; the PLANE itself is excluded — below |
| **other / UI widget** | 6,595 | 1,008 | `CyGInterfaceScreen` chrome and friends — **not this library's job** |
| **Total** | **21,279** | 2,070 | |

⚠ Distinct names do **not** sum down the column (2,413 > 2,070): one name reached on two receiver kinds counts
in both. Sites do sum.

⚠ **TEXT is absent by construction, not by being small** — `getText` is Python-defined, so §1.1's exclusion rule
drops all **3,168** of its sites; the 98 above are only what other text-manager receivers leave behind. TEXT
remains a separate plane the library does not own (§4.1).

### 4.1 (e) TEXT is its own kind — and the library should not own it

`.getText(` alone is **3,168 sites** — larger than MUTATION and INFO's non-registry half combined. It is not info
data, not live state, not a computed value and not a write: it is **resolution of a localized string (or an art
path) from a key**. (It sits outside §1.1's demand tables by construction, because Python defines a `getText` of
its own; the raw count here is the honest size of the plane.) Treating it as info data would pull the entire TXT plane into
the library's contract; treating it as state would be simply wrong.

**Recommendation: TEXT stays a separate, thin service the library does *not* own — with one seam.** The library
already owes **rendered entry lines** (ruling 29, `Sources/UI/CvEntryText.{h,cpp}`), and those arrive
*already localized*. So the split is: **the library returns rendered/localized display strings for everything it
serves**; free-standing `getText("TXT_KEY_…")` lookups for a screen's own chrome (labels, headers, button text)
remain a localization service call. That keeps the one-surface ruling intact — no consumer ever asks the
*library* for an entity's text and gets a raw key back — without making the library the TXT gateway.
Per [the todo](../plans/structural-cleanup/todo.md) the vocabulary TXT keys are sequenced after the stages complete, so the
renderer's spell-back fallback is the accepted output meanwhile.

The concentration is informative: `Screens/` 655 · `Revolution/Gameready/` 329 · `Contrib/` 302 ·
`Screens/Advisors/` 292 · `EntryPoints/` 201 · `Screens/Pedia/` 190 · `PitBoss/` 153. TEXT is a **UI-layer**
concern almost everywhere, which supports leaving it outside the data library.

### 4.2 (a′) REQ — the condition trees are an INFO need, not an API

The `BoolExpr` binding (`Sources/Python/CyBoolExprInterface.cpp`) exposes `getType` / `getGOMType` / `getID` /
`getFirstExpr` / `getSecondExpr` — a raw boolean-expression tree. Python walks it in exactly one place:

- **`Screens/Debug/HelperFunctions.py:464` `getGOMReqs(CyBoolExpr, GOMType, GOMReqList, eParentExpr)`** —
  recurses `getFirstExpr`/`getSecondExpr`, collecting `getID()` at `BOOLEXPR_HAS` leaves into AND/OR buckets.
- Entered from `CvBuildingInfo.getConstructCondition()` / `CvUnitInfo.getTrainCondition()` at
  `HelperFunctions.py:110, 225, 275` and `Screens/Debug/TestCode.py:149`, plus the pedia pages.

The kind-count of 3 is an artefact — the walker's method names (`getType`, `getID`) collide with `CvInfoBase`
names and are attributed to INFO, so the *tree walk itself* is nearly invisible to a name-keyed census. **The
real weight is the mechanism, not the site count.**

This is an **INFO-plane need answered by the `CvRequires` section object** rendered as a structured display
tree — independently the same conclusion as [pedia-map.md finding 3](pedia-read-map.md): *"no boolean-expression API
belongs on the new surface."* Confirmed here for the non-pedia consumers too: `TestCode.py` and
`HelperFunctions.py` want *the list of required techs/buildings/bonuses*, never the tree. The
[DEC-one-reverse-view](../architecture/decisions.md#dec-one-reverse-view) edge families answer the inverse
direction.

### 4.3 (d) MUTATION — out of scope for the library, but still needed

**987 sites / 176 distinct names** where Python tells the engine to *do* something, concentrated in the editor
(`Screens/Worldbuilder/` + `pyWB/`), the gameplay callbacks at `<root>` and `EntryPoints/`, and Revolution.

These are **not data fetching** and the library must not absorb them — that would pull gameplay into the DLL
boundary, which [the deliverable ruling](../architecture/patterns.md) explicitly forbids ("Python-authoritative gameplay
stays Python"). But they are a real boundary that stage 4 must design *beside* the library, because the same
handler that reads through the library writes through this path. Two sub-shapes:

1. **Entity mutation** — `setHasReligion`, `changeRevolutionIndex`, `createUnit`, `setImprovementType`. The
   WorldBuilder/scenario and gameplay-callback path.
2. **Global-define writes** — `setDefineINT` (**69 sites**: `Afforess/ANewDawnSettings.py`,
   `Afforess/DiplomacySettings.py`, `Contrib/RevDCM.py`). Options screens write engine tunables at runtime.
   This is the one that most deserves an explicit ruling (§7) — it is a settings-persistence mechanism wearing
   an engine-write costume.

### 4.4 (c) COMPUTED

1,949 sites / 245 distinct names of `can*` / `is*` / `AI_*` / `calculate*` / `find*` / `has*`, concentrated in
`EntryPoints/`, `<root>`, Revolution, the editor and `Screens/`.

The availability half (`canConstruct` / `canTrain` / `canResearch` / `canDo*`) is **the enabler's surface, not
the cascade's** — [DEC-enabler-not-cascade](../architecture/decisions.md#dec-enabler-not-cascade) — and
Python must read the enabler's own cached verdict, never re-derive it. The rate half (`calculateTotalCulture`,
`foodDifference`, growth/production turn estimates) is cascade-computed. **Both are live-context reads that sit
beside the info payload, never inside it** — the same conclusion as
[pedia-map.md finding 5](pedia-read-map.md), reached here at 800× the site count.

## 5. ⚑ The grep-invisible reads

A completeness claim built on static greps misses exactly this section. Every instance below is listed with
`file:line`; §5.7 states plainly what could not be proven.

### 5.1 Inventory of dynamic-access mechanisms

| Mechanism | Sites | Verdict |
|---|---|---|
| `getattr(...)` | 8 | 3 benign, 5 load-bearing (§5.2, §5.3) |
| `eval(...)` | 8 (+1 doc line) | **2 build engine calls from strings** (§5.2) |
| `__import__` | 1 | the BUG module resolver (§5.3) |
| `setattr` on an engine enum | 1 | **mints new `WidgetTypes` at runtime** (§5.3) |
| XML-declared callbacks | **1,052 declarations / 467 names** | the engine→Python entry graph (§5.4) |
| BUG XML handler bindings | **59 modules × 160 functions** | the config-driven dispatch graph (§5.3) |
| Int-keyed dispatch tables | 2 (`Events`, `OverrideEventApply`) | engine popup-ID → Python function (§5.4) |
| `apply()` | 10 | all `CvWBDesc` scenario methods — **not** the Python built-in. Benign. |

### 5.2 ⛔ The exhibit: engine method names built from strings and `eval`'d

**`Screens/Advisors/CvDomesticAdvisor.py:1331-1337`**

```python
expr = "CyCity." + columnDef[3] + "("
if columnDef[5] is not None:
    expr += str(columnDef[5])
expr += ")"
for i in cityRange:
    CyCity = cityList[i]
    szValue = self.ColorCityValues(unicode(eval(expr, globals(), locals())), key)
```

The engine method name is **element 3 of a column-definition tuple** in `COLUMNS_LIST`
(`CvDomesticAdvisor.py:144-207`, extended by generated rows at 210-271). The censused table names **19 distinct
`CyCity` methods**; of those, **three appear nowhere in `Assets/Python` as a literal `.name(` call and are
reachable ONLY through this string table**:

| Method named in the table | Literal `.name(` call sites in `Assets/Python` |
|---|---|
| `findYieldRateRank` | **0** |
| `findCommerceRateRank` | **0** |
| `getMilitaryHappinessUnits` | **0** |

**⛔ WHAT THE CATCH IS — a FETCH POINT the map records, NEVER a getter the library owes.** A name in that column
table is evidence that *this advisor demands this column of per-city data*, and the demand is what the coherent
surface answers. It is not a binding to keep, re-point or widen, and "the census would have dropped it" must not
be read as "the library must therefore carry it" — the whole map is
**[NEEDS, not getters to port](../architecture/decisions.md#dec-new-getter-surface)**, and a method name is the
form the demand happens to be written in, never its unit. The other 16 names in the same table
(`getPopulation`, `getX`, `getY`, `getMaintenance`, `getCommerceRate`, `foodDifference`, `getGreatPeopleRate`,
`getGreatPeopleProgress`, `getPlotYield`, `findBaseYieldRateRank`, `getRealPopulation`,
`getEspionageDefenseModifier`, `getNumWorldWonders`, `getMaxNumWorldWonders`, `getNumNationalWonders`,
`getMaxNumNationalWonders`) stand exactly the same way — several name engine getters that are already DELETED,
which changes nothing about the demand and is the point: the column is still wanted.

⚑ **Why the distinction is load-bearing here specifically:** this is the `revolution.distanceMod` class of catch
(a read no literal grep finds), so it is exactly where a reader is most tempted to "rescue" the getter it just
found. Rescuing it re-creates the per-getter surface the rebuild is deleting.

**What it needs served:** the domestic advisor is a **per-city computed-column table**. The string indirection
exists only because there was no way to ask for "these N values for these M cities" in one call. The library
answers it with a **columnar per-entity payload over a city set** — which also deletes the `eval`, and answers
all 19 columns as ONE fetch rather than 19 reads.

**`Screens/Advisors/RevolutionWatchAdvisor.py:703`** — `self.HEADER_DICT[column[0]] = eval(column[8], ...)`
evaluates element 8 of the same column-tuple shape. Here the evaluated string is a **header/icon expression**
(`u"<char>"`), not an engine call — so it is a TEXT-plane indirection, lower severity, but the same pattern and
the same fix.

**`MapScriptToolsOld.py:193`** — `iLat = abs(eval(mapGetLatitude))` evaluates a latitude expression supplied by
the *map script*. Map-script-driven, so its content is not statically enumerable from this tree at all.

**`BUG/BugTypes.py:139-142, 187`** — `eval` used to parse option VALUES (`TUPLE`/`LIST`/`SET`/`DICT` types) from
config strings. Not an engine read; a deserializer. Benign but worth knowing it exists.

### 5.3 The BUG config-driven dispatch graph

`Assets/Config/*.xml` binds handlers **by string**, resolved at runtime:

- **`BUG/BugUtil.py:437`** `__import__(module)` · **`:445`** `getattr(lookupModule(module), functionOrClass)` ·
  **`:452`** `getattr(obj, functionOrAttribute)` — module + function names arrive as config strings.
- Measured across `Assets/Config/*.xml`: **59 distinct `module="…"` values and 163 distinct `function="…"`
  values.** None of these bindings is visible to any Python-side grep.
- **`BUG/WidgetUtil.py:62-68`** — `getattr(WidgetTypes, name)` and **`setattr(WidgetTypes, name, widget)`**:
  BUG **mints new `WidgetTypes` enum members at runtime** and hands them to the engine as widget ids. The engine
  enum is therefore extended by Python at load, from names that live in config.
- **`BUG/InputUtil.py:111`** — `getattr(InputTypes, "KB_" + k)`: engine input enum resolved from key strings.
- **`BUG/BugOptions.py:751`** — `getattr(InterfaceDirtyBits, b + "_DIRTY_BIT")`: engine dirty-bit enum resolved
  from an option string.

**Consequence for stage 4:** the library's **enum/type resolution by NAME must be a first-class operation**
(`getInfoTypeForString` generalized), because three engine enums are already reached only this way. It is not an
edge case to be tidied away — it is how the options framework is wired.

### 5.4 The engine→Python entry graph (XML-declared callbacks)

The engine invokes Python functions **named in XML**. Measured over `Assets/XML/**/*.xml`:

| Tag | Declarations | Distinct names |
|---|--:|--:|
| `<PythonCallback>` | 458 | 140 |
| `<Python>` | 262 | 32 |
| `<PythonCanDo>` | 172 | 152 |
| `<PythonHelp>` | 135 | 118 |
| `<PythonCanDoCity>` | 15 | 15 |
| `<PythonExpireCheck>` | 7 | 7 |
| `<PythonCanDoUnit>` | 3 | 3 |
| **Total** | **1,052** | **467** |

Resolution against every `def` in `Assets/Python`: **all 467 resolve** — the entry graph is closed, so every
callback the engine can name from XML has a definition to land on. Host files:
`EntryPoints/CvRandomEventInterface.py` **399** · `EntryPoints/CvOutcomeInterface.py` **67** ·
`Contrib/EventSigns.py` 2.
⚠ `<PythonName>` (99 declarations / 44 names) is **not** a callback tag — it names map/build display entries, so
it is excluded here and its names are not expected to resolve to a `def`.

Also int-keyed dispatch: **`CvEventManager.py:180` `self.Events = {…}`** maps engine popup IDs to Python
functions (`beginEvent`/`applyEvent` at `:214`/`:227`, with `OverrideEventApply` at `:234`), and the commented
`EventHandlerMap` string-dispatch at `:94-207`.

**Why this matters:** these 467 functions are where `EntryPoints/`'s 3,622 engine call sites *live*. The reads
inside them ARE counted by this census — but **which of them run, and when, is decided by XML**, so no static
analysis of the Python tree can tell you the live subset. Any "these reads are dead" claim about
`CvRandomEventInterface.py` is unprovable from the Python side alone.

### 5.5 Structural blind spots (no dynamic trick required)

- **`inputClass.getData1()` / `getData2()` / `getButtonType()`** — 146+ sites. The popup-context object handed to
  Python by the engine is **not on the `.def` surface** this census parses, so these reads fall in the "unbound"
  bucket and are invisible to a binding-keyed census. They are genuine engine reads.
- **`**kwargs` forwarding** — `BUG/BugUtil.py:425` `self.call(*args, **kwargs)` (the `Function` wrapper). Every
  BUG-registered handler is invoked through it, so argument shapes are not statically checkable.
- **Method names colliding with bound names** — a Python-defined `def getValue(self)` is counted as the bound
  `getValue`. Small, and it inflates rather than hides.

### 5.6 The `revolution.distanceMod` standing exhibit, re-verified

`Revolution/Gameready/Revolution.py:1170` reads
`pPlayer.getRevIdxDistanceModifier() + pCity.getRevIndexDistanceMod()` — two spellings of one mechanic, consumed
by Python-authoritative gameplay and invisible to an engine-side read census. Verified live at that line.
Per [patterns.md](../architecture/patterns.md) **both distance kinds stay as-is, untouched by any stage (owner ruling)**;
Revolutions owns them in its own rework. **No stage-4 investigation.** Recorded here only as the calibration
case for §5.7.

### 5.7 ⚠ How much of the surface could NOT be statically proven

Stated plainly, because a completeness gate depends on it:

- **The read SITES are ~99% statically enumerable.** 21,279 unserved call sites are matched by name against the
  published surface. The known miss is bounded and named: the string-built calls in §5.2 (19 method names in
  one table, 3 of which have zero literal sites), the unbound popup-context reads in §5.5, and whatever a map
  script's `eval`'d expression contains.
- **REACHABILITY is NOT provable.** 590 XML callback bindings + 163 BUG config function bindings + 2 int-keyed
  dispatch tables decide what actually executes. **I cannot certify from the Python tree which reads are live.**
- **Therefore: a "this read is dead, drop it" judgement is NOT SAFE anywhere in this tree.** The safe direction
  is one-way — a read found is a read to serve; a read *not* found is not evidence of absence. The library must
  be built to the union, and the only trustworthy completeness signal is the one
  [patterns.md](../architecture/patterns.md) already specifies: the census list as tick-list, with the legacy surface
  disconnected in the same work item.
- **Adversarial check performed:** rather than assert completeness, I inverted the question — took the DLL's
  engine-shaped names and asked which are reached by *no* literal Python call, then hunted the mechanism that
  reaches them anyway. That is what surfaced §5.2. The same inversion over the BUG config and the XML callback
  tags produced §5.3 and §5.4. **I did not find a mechanism class beyond those listed; I cannot prove none
  remains.**

## 6. The Python-authoritative systems

These stay Python by [owner carve-out](../architecture/decisions.md#dec-no-deferred) and become **consumers**
of the library.

### 6.1 Revolution — 3,348 sites

`Revolution/Gameready/Revolution.py` (1,733) · `Screens/Advisors/RevolutionWatchAdvisor.py` (434) ·
`Revolution/RevEvents.py` (369) · `Revolution/RevUtils.py` (343) · `Revolution/Gameready/BarbarianCiv.py` (220) ·
`Contrib/RevDCM.py` · `Revolution/RevolutionInit.py` · `Revolution/RevData.py` ·
`Revolution/Gameready/AIAutoPlay.py` · `Revolution/Development/`.

**Profile: STATE-dominated** — the most state-heavy file in the tree. What it needs served is therefore **overwhelmingly live-state and
computed reads, not info payloads**: city/player/plot possession and counts, culture and religion state,
garrison and unit presence, war/peace and attitude state.

Its INFO needs are narrow but specific — the revolution-tuning fields:
`getRevLaborFreedom` (8) · `getRevDemocracyLevel` (8) · `getRevIdxLocal` (5) · `getRevIdxNational` (5) ·
`getRevReligiousFreedom` (4) · `getRevEnvironmentalProtection` (2) · `getRevIdxSwitchTo` (2) ·
`getRevIdxHolyCityGood` / `getRevIdxHolyCityBad` (2 each) · `getRevIdxGoodReligionMod` /
`getRevIdxBadReligionMod` (2 each) · `getRevIdxNationalityMod` · `getRevViolentMod` · `getRevReligionVal` ·
`getRevNationalityVal` · `getRevMaxCivs`. Plus the live pair `getRevolutionIndex` (77) /
`setRevolutionIndex` (24) / `changeRevolutionIndex` (24) / `getRevolutionCounter` (10) and the distance pair
of §5.6.

**⚑ Flag:** these `getRev*` fields sit on civic/handicap/leaderhead infos. `getRevLaborFreedom`,
`getRevDemocracyLevel` and `getRevIdxLocal` are in the §2.4 residue — read by the Revolution stack and by
nothing the pedia shows. **Whether the rebuilt info surface currently exposes them is UNVERIFIED here** (this is
a Python-side census; I did not audit the rebuilt `CvJson*Info` headers for these members). Given the owner
ruling that revolution data is untouched until the Revolution rework owns it, the actionable item is narrow:
**stage 4 must not drop these fields while wiring the library**, and the Revolution rework — not stage 4 — decides
their final shape.

### 6.2 Random events — 3,354 sites (`CvRandomEventInterface.py` 2,942 + `CvOutcomeInterface.py` 412)

**466 of the 467 XML-declared callbacks live here** (§5.4). It is the most *balanced* consumer in the tree,
exercising all five kinds heavily.

Needs served: **per-entity payloads** for the entity an event names (unit, building, bonus, improvement, tech,
religion, corporation), the **event/trigger types** (`getEventInfo`, `getEventTriggerInfo`, `getPrereqEvent` —
all §2.3 residue, no pedia page), live state on the target, availability verdicts for `canDo*`, and the
mutation boundary for `do*`.

**⚑ Flag:** `CvEventInfo` / `CvEventTriggerInfo` are bound (27 and 14 `.def`s) and read 22 times from Python,
but have **no pedia page**, so pedia-driven work would not serve them at all. They also carry the
`<PythonCallback>` strings themselves — i.e. the info type *contains* the dispatch graph.

### 6.3 Others that are effectively Python-authoritative

- **`Contrib/DynamicCivNames.py` (265)** — rewrites civ names from civic/religion state. Needs the civic-option
  index (`getNumCivicOptionInfos`, 31 sites, §2.3 residue).
- **`Revolution/Gameready/BarbarianCiv.py` (253)** — spawns barbarian civs; needs `getBARBARIAN_PLAYER`,
  world/handicap config, and heavy mutation.
- **`DancingHoskuld/Partisan.py` (156)**, **`Contrib/Civ4lerts.py` (192)**, **`CvAdvisorUtils.py` (321)** —
  gameplay-reactive Python reading live state each turn.
- **`pyWB/CvWBDesc.py` (466)** — scenario serialization; authoritative for the save/load text format and needs
  **stable type keys** (§3.2).

## 7. Open questions for the owner

1. ~~Do map scripts sit behind this library, or are they their own boundary?~~ **RULED (owner): MAP SCRIPTS ARE
   THEIR OWN BOUNDARY.** They are outside the data-fetching library entirely, and the census backs the split on
   four independent counts: `CvMapGeneratorUtil.py` (269) + `MapScriptToolsOld.py` (672) + the `Assets/Maps`
   scripts (a) consume map-generation info types (`WorldInfo` 60 · `ClimateInfo` 20 · `SeaLevelInfo` 3 ·
   `MapInfo`) **nothing else reads**, (b) run **before most game state exists** — a different lifetime than any
   screen or gameplay consumer, (c) are **write-dominated** (they BUILD the map; the library is a read surface),
   and (d) `MapScriptToolsOld.py:193` `eval`s an expression supplied by the script — an open extension surface
   by design.
   **Consequences:** those map-gen types leave the library's coverage appendix (they were counted among the 59
   unpaged info types); the map-generation contract stays what [engine.md](engine.md) already
   specs — **the named Python CALLBACKS are the contract, not the impl** — and it keeps its own DLL-fallback
   behaviour. Third-party map scripts therefore remain a supported surface on their own terms, unaffected by the
   `Cy*` cut. A future map-gen boundary redesign is its own work item, never a stage-4 rider.

2. ~~Is `Screens/Debug/TestCode.py` in scope for the library?~~ **RULED (owner): DELETED, not migrated** —
   *"nuke testcode, if we want that we do it properly"*, the Python refactor making it worthless. It was the
   largest INFO consumer after the pedia hub (1,488 INFO sites) and the sole consumer of 90 residue names /
   296 sites, all of which drop out of the library's obligations (**the appendix shrinks ~30%**). The whole
   feature chain went with it (`DebugBtn` → `showDebugScreen` → `DebugScreen` → `TestCode`, plus the dead
   `pythonDebugToggle`); `HelperFunctions.py` stays (the pedia uses it), and the orphaned
   `INTERFACE_DEBUG_SCREEN_BUTTON` art STAYS untouched (art is hands-off — roadmap § Scope decisions).
   ⚑ Its 50 checks encoded real design invariants the JSON spec does not state (a requirement may not unlock
   after the thing requiring it; replacements are explicit, never implicit; a replacing entity must be better).
   Those invariants belong in the SPEC first — not a stage-4 item.

3. **Global DEFINEs — the READS stay, the WRITES are OUT OF SCOPE (owner).**
   Reads: `getMAX_PC_PLAYERS` (176) · `getMAX_PLAYERS` (74) · `getMAX_PC_TEAMS` (44) · `getBARBARIAN_PLAYER`
   (40) · `getMAX_TEAMS` (13) — a small closed constants block, trivially served by the library.
   The **69 `setDefineINT` writes** are RULED OUT OF SCOPE: they are a MUTATION surface (not a data read), and
   all 69 sit in `Contrib/RevDCM.py` (32) + `Afforess/DiplomacySettings.py` (36) + `Afforess/ANewDawnSettings.py`
   (1) — the Python-authoritative contrib stacks, each due its own rework (Revolution explicitly). They are the
   BUG-option → global-define bridge.

   ⚑ **What they ARE — "LIVE" options (owner's term), a distinct KIND, not a duplicate of `GAMEOPTION_*`.**
   The verified chain: a BUG option declared in `Assets/Config/<mod>.xml` and persisted to its own `.ini`
   (`<options id="RevDCM" file="RevDCM.ini">`, each `<option>` carrying `get`/`set` + a `<change>` callback) →
   BUG fires that Python callback on change → `GC.setDefineINT(...)` → `cvInternalGlobals::setDefineINT`
   (`CvGlobals.cpp:2654`: MP-synced via `sendGlobalDefineUpdate`, then `cacheGlobals()`) → the DLL reads its
   cached accessor (`GC.isDCM_RANGE_BOMBARD()`, `CvUnitAI.cpp:26193`). So the user can flip one **at any time
   and it takes effect immediately**.

   That is the difference in kind: a **game option** is chosen at game setup and fixed for the game (so JSON may
   gate an entity on it, [DEC-entity-gate](../architecture/decisions.md#dec-entity-gate)); a **live option**
   is a user setting changeable mid-game. They are NOT to be folded into `GAMEOPTION_*` on the assumption that
   they are strays. The consequence worth knowing rather than re-deriving: **JSON cannot gate on a live option** —
   nothing static may depend on a value that moves under it.
   ⚑ A flip DOES announce: `SEVT_GLOBAL_DEFINE_CHANGED` ([event-spine.md](../specs/event-spine.md)) fires from the
   three `cvInternalGlobals::setDefine*` setters, so a consumer that needs to answer one can. That closes the
   reactability gap; it does not license gating authored data on a live option, which is a separate ruling and
   unchanged. The writes themselves belong to the contrib stacks' own reworks, not here.

3b. **A natural-disaster mechanic whose whole effect is loss of a plot improvement is authored as a §5 TRIGGER,
   never as a Python event — RULED (owner).** `trigger → chance → action` with the `destroy` verb
   ([json.md §5](../specs/json.md)) already expresses that shape exactly, so the capability belongs as DATA on
   the trigger plane. This does NOT reopen the events carve-out (#425 events stay Python) — it fixes where this
   one shape of capability lives if it is ever wanted.

4. **Does the library own TEXT, or only the rendered lines it produces?**
   §4.1 recommends the latter (library returns localized display strings for what it serves; screen chrome stays
   a localization service call). At 3,168 `getText` sites the answer materially changes the library's contract, so it
   wants an explicit ruling rather than an inherited assumption.

5. ~~Is name-based enum/type resolution a first-class library operation?~~ **RULED (owner): YES — enum
   operations are FIRST CLASS.** Name→value resolution is a supported operation of the library's surface, not an
   accident of `getattr` on a module, and the evidence in §5.3 is why: `WidgetTypes`, `InputTypes` and
   `InterfaceDirtyBits` are reached ONLY this way, so without it those reads have no path at all. ⚠ Note the
   shape this must cover is **resolution AND extension**: `BUG/WidgetUtil.py:62-68` does `getattr(WidgetTypes,
   name)` *and* `setattr(WidgetTypes, name, widget)` — BUG MINTS new enum members at runtime from config names
   and hands them back to the engine as widget ids. A read-only lookup would not serve it. This generalizes what
   the engine already does for infotypes (`getInfoTypeForString`) and pairs naturally with the load-minted
   classification registries ([DEC-classification-infos](../architecture/decisions.md#dec-classification-infos)),
   which are the same idea on the info plane: names minted to ids at load, resolved by id thereafter.

   And the completeness argument that makes it load-bearing: a library WITHOUT name→type resolution forces those
   consumers to keep a legacy reach-around — the second live surface the one-surface ruling forbids.

6. **What is the MUTATION boundary's shape, and is it stage 4's job?**
   987 sites are writes. They are explicitly out of scope for a *data-fetching* library, but the same handlers
   read through it, and the legacy `Cy*` surface cannot be disconnected while a write path still depends on it.
   Stage 4 needs a decision on whether the write boundary is designed alongside the library or sequenced after.
