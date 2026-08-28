# The Python read boundary — the hazards, the kinds, and the standing rulings

> Evidence base for the stage-4 Python library ([patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md)) — the evidence base for the stage-4
> Python surface, everything OUTSIDE the pedia. The pedia slice is mapped in
> [pedia-map.md](pedia-read-map.md) and is excluded from the detailed work here.
>
> Per [the Cy* surface is not a fixed contract](../architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library) and
> [build a new getter surface, never widen a legacy one](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers) this maps **NEEDS, not getters
> to port**. ⛔ **THE CUT IS HALF DONE, AND READING IT AS UNIFORM IS THE TRAP.** The legacy read bindings are gone
on **`CyCity`** (95 defs, group-shaped, zero mutations) and **`CyUnit`** (8) — and Python's channel-scalar read
demand on `CyCity` is down to a *handful of sites*, so that half is finished rather than merely cut. They are
**NOT** gone on **`CyPlayer`** (348 defs), **`CyGame`** (236), **`CyTeam`** (118) or **`CyPlot`** (111), which
still publish broad legacy channel surfaces and carry essentially all of the surviving demand. ⇒ Of the legacy
channel-shaped getters on `CvCity.h`/`CvPlayer.h`, **the `CvCity` side has no Python demand left to re-serve;
the `CvPlayer`/`CvTeam` side is where the work is.** The replacement surface stands beside that absence:
> the composition root (`DLLPublishToPython`, `Infrastructure/CvDLLPython.cpp`) publishes the enum
> int-conversions, the vector + `IDValueMap` container interfaces, the debug/Win32 helpers, and the four
> planes of the read library — **`CyEnums`** (the vocabulary, published FIRST because a group read is indexed
> by it), **`CyEnabler`** ("can I?"), each game object's OWN accessor ("what do I HAVE?") and **`CyInfo`** ("what do I CARRY?",
> the ONLY home for infos). Beside them stand the kept boundaries that were never the cut's target — TXT, ART,
> the command/net layer, and the CONFIG half of the global context.
> ⚠ **`GC.get<X>Info` is published NOWHERE**, so a surviving one is not a slow read — it is an `AttributeError`
> at the moment its handler fires. That is the shape of what is left to wire, and it is why the counts below
> are DEMAND rather than a surviving-call census.
> ⛔ **REPLACE ONE THE MOMENT YOU FIND IT — never leave it for the owner to hit by hand.** A found dead
> read is not a report, a census line, or a follow-up item: by the time you have grepped it you already know
> what answers it, so re-point it in the same change. ⚑ **The reason is that the ERROR LOG IS NOT A WORKLIST:**
> a traceback names only the FIRST dead read a code path reached, so fixing what threw leaves every read behind
> it live and hands the owner the next one on the next click — the defect arrives one screen at a time, forever.
> *(Measured: four advisor tracebacks named five reads; the same files actually carried a second `canResearch`,
> nine `isRevealed`, four `getBuildingOriginalTime`, three `hasBuilding` and two `plot()` — all of which would
> have thrown on the following click.)* ⇒ When you touch a file for ANY reason, clear its dead reads while you
> are in it, and grep the file rather than trusting the traceback that sent you there.
> The families known to have NO read today, so nobody re-derives the list:
> **the vote** (`isVoteSourceType` / `isSecretaryGeneral` — the rebuilt `CvVoteInfo` answers them as
> `hasVoteSource()` and `getRole() == VOTE_ROLE_SECRETARY_GENERAL`, so the DATA is there and only the binding is
> missing), **the vote source** (`VOTESOURCE_` is not a registered prefix at all — it needs its own accessor),
> **the victory thresholds** (authored on the building/project, wanted per victory — a reverse view, so it lands
> at load like the build's produces does, never a per-victory registry sweep), **the ERA** (no registered prefix;
> ⚠ contrast `WORLD_`, which is NOT a JSON prefix either yet IS mapped explicitly in `CyInfo.cpp` — so absence
> from the `readJson` X-macro list is not proof a prefix is unserved, CHECK the mapper).
> ⚑ **The odd ART reads that used to sit on that list are SERVED, and how they landed is the pattern to copy:**
> the leader art-define and diplo-music tags as `CyInfo::getLeaderHeadArt` / `getLeaderDiploPeaceMusicScriptId`,
> and the religion disabled-icon as `CyInfo::getReligionButtonDisabled`. A read belonging to ONE registry is a
> NAMED endpoint on `CyInfo` taking that registry's bare id — never a new per-type class, and never a generic
> `getButtonDisabled(prefix, id)` that would answer empty for every registry that has no such art.
> The `Cy*` WRAPPER classes stay for the engine→Python direction, and each carries its **IDENTITY SET** — owner,
> id, position, and nothing else ([patterns.md](../architecture/patterns.md) § THE IDENTITY SET). ⛔ The earlier
> reading here — *"a wrapper with no binding is the correct end state"* — is SUPERSEDED: it was right about the
> DIRECTION (the read surface is gone) and wrong about the ADDRESS, since a handle that cannot name which object
> it holds makes every legacy consumer a rewrite.
>
> ⛔ **AND AN EVENT PAYLOAD IS NOT A HANDLE AT ALL — IT IS A PLAIN TUPLE, so the identity set does NOT save it.**
> `DECLARE_PY_IDENTITY(CvCity*, getOwner(), getID())` (`CyCity.h`, `CyUnit.h`) routes every `Cy::Args` push through
> `add_identity` → `python::make_tuple(iOwner, iId)`. So in a HANDLER, `argsList[0].getID()` raises
> `AttributeError: 'tuple' object has no attribute 'getID'` — **the four identity methods fail there too.** The fix
> is to UNPACK (`iOwner, iCityId = argsList[0]`), never to re-point the call.
> ⚑ **The same expression is fine or broken depending on where the object CAME FROM**, which is why this reads as
> nondeterministic until you know it: a handle from a RETURN VALUE (`CyPlayer.cities()`, `CyPlot.getPlotCity()`)
> goes through the registered `class_<CyCity>` and its four methods work. One file can therefore carry both shapes —
> measured in `CvEventManager.py`, where `onChangeWar` reads `getX()` off a `cities()` handle correctly and
> `onCityRazed` reads `getX()` off an argsList tuple and dies.
>
> ⚠ `CySelectionGroup` is registered `python::no_init` with **ZERO** `.def`s (`CvDLLPython.cpp`), while
> `CyPlayer.groups()`, `CyPlayer.getSelectionGroup()` and `CyUnit.getGroup()` all still hand one back — a handle
> that can be asked nothing. Group ACTIVITY is served by `UnitReadKind.UNIT_READ_ACTIVITY`; there is no route for
> `setActivityType` or `readyToMove`.
>
> ⛔ **The COUNTED CENSUS THAT USED TO SIT HERE IS GONE, and it is not coming back.** A per-directory tally of
unserved reads drifts the moment the library grows, so it bound every reader to a picture that was no longer
true. The live answer is `python Tools/verify-python-bindings.py`, which recomputes it and cannot go stale; the
runtime answer is `PythonErr.log` / `PythonDbg.log`, which name the read that actually fired. What is kept below
is what neither of those can tell you: the hazards, the read KINDS, and the boundary rulings.

## 0. Reading the live count — the two caveats it will not tell you

The census that used to sit here is now `python Tools/verify-python-bindings.py`. Two things distort its total,
and neither is visible in its output:

⛔ **It keys UNSERVED against what THIS REPO publishes, so it over-reports by exactly the EXE's surface.**
`CvPythonExtensions` has two producers ([python-load-sequence.md](python-load-sequence.md)); a name the closed EXE
publishes — `CyInterface`, `CyTranslator`, `CyPythonMgr`, `CyFractal`, `NiTextOut`, `InputTypes` and their kin —
is SERVED in the running game and unserved to the census. Subtract that class before reading any total as a
worklist. ⚠ It also counts the Python builtins `find` and `has_key` as engine-shaped, and drops every name Python
defines itself — `getText` above all, which is why TEXT is not sized here at all (§1.1).

⚠ **It counts only `CyEnabler` as "the read half", which UNDER-COUNTS the library** — `CyInfo` and the per-type accessors
and the per-info accessors (`CyWorldInfo`) publish reads too. It does not distort the UNSERVED total, which keys on
what is published ANYWHERE under `Sources/`.

⛔ **AND THE OVER-REPORT HAS A SECOND MECHANISM — A NAME DECLARED ON A `CvDLL*` EXE-INTERFACE BASE.** The tool
matches a method NAME, so a name declared in one of our headers purely to describe the EXE's own vtable is read as
"declared on a Cy class, registered nowhere". `CyInterface`'s whole surface arrives this way:
`CvDLLInterfaceIFaceBase.h` declares `setDirty`, `isScoresVisible`, `shouldDisplayFlag`, `toggleBareMapMode` and
their kin, `CyInterface` is registered by NOTHING under `Sources/`, and every one of them works in the running
game because the EXE serves them. ⚑ **Measured: 41% of all Python calls on a `Cy*`/`Ni*` receiver go to an
EXE-published handle.**
⇒ **Before calling any flagged method a latent `AttributeError`, check the RECEIVER, not the name.** A call on
`CyInterface` / `CyTranslator` / `CyGameTextMgr` / `CyArtFileMgr` and their kin is served and always has been —
`CyIF.toggleBareMapMode()` has never once failed. The burndown is a ratchet on OUR surface, not a defect list.

⛔ **THE PREFIX PLANE HAS TWO HALVES, AND CHECKING ONLY ONE UNDER-REPORTS BADLY.** A JSON-backed registry is routed
GENERICALLY by the `RJ_REPO_TYPES` table (`Data/CvReadJson.cpp`) that `rjInfoForTypeConst` dispatches through — it
is never named in `CyInfo.cpp`. Only the XML-only registries are spelled out there (`cyi_xmlOnlyInfo`). ⚠ So a grep
of `CyInfo.cpp` alone reports every JSON registry as unrouted, which is exactly wrong for the biggest ones:
`CIVICOPTION_`, `CULTURELEVEL_`, `PROPERTY_`, `BONUSCLASS_`, `VOTE_`, `HURRY_` and `SPECIALUNIT_` are all routed and
were all mis-reported on one such grep. **Check BOTH tables, or check behaviour.**

⛔ **`Screens/Worldbuilder/` is a block awaiting its own pass, NOT a family whose breakage is accepted** —
WorldBuilder may temporarily LAG a cut; a visible break may never stand. (The misreading this warns about has
already cost one pass.) ⛔ **NOTHING HOLDS IT — it is work, not a decision.** The shape is ruled in §7 (the
`Cy*` wrapper IS the surface; writes route through the engine's own mutator), and the membership is measured
rather than chosen: of the methods Python still calls unpublished, **97 are touched by WorldBuilder and 75 are
WB-ONLY** — `CyCity` 51 (the scenario state: wall-override, plundered, never-lost, bombarded, airlift-targeted,
drafted, the progress and anger counters, the order queue), `CyUnit` 22 (§7's own examples: cargo, transport,
made-attack, made-interception, promotion-ready, base combat strength, level, facing), `CySelectionGroup` 2.
⚑ **The other 22 are NOT WB-surface members** — `baseCombatStr`, `getDamage`, `getUnitType`, `getLevel`,
`setXY`, `isHasPromotion` and kin are called from gameplay and other screens too, so they are ordinary reads on
the object and serving them fixes the advisors in the same stroke. Recount with
`python Tools/verify-python-bindings.py --list`; never carry these numbers as fact.

⛔ **DO NOT SIZE THAT PASS FROM THE BURNDOWN CENSUS — IT UNDER-REPORTS THIS FAMILY BY DESIGN.**
`verify-python-bindings.py` takes the UNION of every `.def` and deliberately does not attribute a name to a
class ([AGENTS.md](../../AGENTS.md)), so a name published on ANOTHER class reads as
registered while being DEAD on the receiver actually used. `CyUnit` publishes only its 8-method identity set
(`getOwner`/`getID`/`getX`/`getY`/`canFight`/`getHP`/`getMaxHP`/`getName`), so `plot()`, `getButton()`,
`getCivilizationType()`, `getTeam()`, `getScriptData()`, `setScriptData()`, `setName()`, `kill()`, `convert()`
and `getUnit()` are all dead on a unit handle and NONE of them appear in the census.
⚑ **Measured on `Screens/Worldbuilder/`: 73 dead unit-handle sites, of which the census names 45 and MISSES 28.**
⇒ Size a WorldBuilder screen by diffing its calls against what the RECEIVER's class actually publishes; the
census is a floor, never the worklist.

## 0b. Two things a dead-code sweep will eat if this is not written down

⛔ **`Forgetful` IS A WANTED MODDER-INFO SURFACE AND IT STAYS — do not sweep it as dead code.** Modder info is
wanted in future, whether or not anyone knows of it today. The screen is an XML-tag REFERENCE
(`TXT_KEY_XML_TAGS`): a dropdown of every info category over a table of **ID · NAME · TYPE · TEXT** — the
`BUILDING_FORGE`-style type key and `TXT_KEY_` for any entity, *for when you forget one*.
⚠ **It reads EXACTLY like an abandoned screen, which is the hazard:** it is reached only by an undocumented
**Ctrl+F1** (`CvEventManager.onKbdEvent`), appears on no menu, and no XML or BUG config names it. Every mechanical
dead-code test flags it. It is un-killed forward intent
([the keep-unkilled-ideas policy](../plans/parked/README.md#parked--out-of-active-scope-plans-kept-for-intent)),
and the reason it LOOKS dead is recorded here precisely so the next sweep does not eat it.
⚑ **Its second job:** being the one file that enumerates every registry, converting it exercises `INFO.getIndex`
across all of them at once — which is what surfaced `COMMAND_` as the single unrouted registry.

⚑ **`CvEventManager.onKbdEvent` HIDES A WHOLE DEV KEYMAP, THREE OF IT UNGATED** — named once here rather than
rediscovered:

| keys | screen | gated? |
|---|---|---|
| **Ctrl+F1** | Forgetful — the XML tag reference above | **no — ships to players** |
| **Ctrl+F2** | `GameFontScreen` — the font-symbol sheet | **no** |
| **Ctrl+F3** | `TimeKeeper` — the era × gamespeed pacing table | **no** |
| Ctrl+F4…F7 | replay · `DebugInfo` · `DanQuayle` · `UnVictory` | debug mode |
| Shift+T / W / E, Ctrl+Shift+P | techs cheat · wonder movie · effect viewer · change-player | debug mode |
| **Ctrl+Shift+Alt+D** | toggles DEBUG MODE itself | **no — so a player can unlock the gated half** |

⚠ The three ungated ones are shipped, reachable in any game, and therefore ordinary consumers of the read surface
rather than debug-only code a cut may ignore.

⚑ **`CvWBDesc.py`'s city half is the worked precedent for the WorldBuilder family** — its write boundary is
the city's own accessor, the reads are `CyCity`/`CyInfo`, and the handle is used only to CREATE the city
and then to name it. ⚑ The SCREENS are the same job, and the only difference is VOLUME: they mutate through the
handle too, and the verbs they need are the enumerated WB-only set above rather than an open question.

⚑ **`WBPromotionScreen.py` is the worked precedent for a SCREEN** — it converted end to end with NO new
binding, because every field it touches was already served on the unit itself: the four copy-filter dropdowns
are one slot each of `pUnit.getRead()` (`UNIT_READ_TYPE`/`_COMBAT_CLASS`/`_DOMAIN`/`_GROUP_ID`), the promotion
list is `pUnit.isPromotionValid(i)` (the GRANT gate — an editor hands promotions out, so gating on
`canAcquirePromotion` would refuse every event-granted one), the held test is `pUnit.hasPromotion(i)`, and the
write is `pUnit.setPromotion(i, bAdd)`.
⇒ **So triage a WB screen by whether its fields are MODELLED, not by the folder.** The editor-only plane §7
names — cargo/transport, made-attack, facing direction, base-combat override, unit-AI type — is not blocked on
anything; it is added to the object's own wrapper as ordinary editor verbs, routed through the engine mutator.
⚠ **And check the CALLBACKS it fires, which no binding census sees:** the migrated event handlers take an
IDENTITY PAIR, so `onUnitPromoted([[iOwner, iUnitID], iPromotion])` is correct and the inherited
`onUnitPromoted([pUnit, item])` fails on unpacking — a silently dead button that no `.def` scan reports.

## 1. The read-KIND split

With the bindings cut there is no owner class to key on, so the classification is derived from the **receiver**
plus the method-name prefix. It is mechanical and re-derivable, and heuristic at the margin — the split
between STATE and COMPUTED moves, the totals do not.

| Kind | Sites | Distinct names | Receivers |
|---|--:|--:|---|
| **other / UI widget** | 4,385 | 770 | `CyGInterfaceScreen` chrome and friends — **not this library's job** |
| **(a) INFO — static data** | 1,476 | 358 | `GC.`/`gc.` info registry + `*Info` objects |
| **(b) STATE — live game state** | 769 | 148 | city · player · plot · unit · team · game · area · deal |
| **(d) MUTATION — writes** | 395 | 83 | same objects, `set*`/`change*`/`do*`/`create*`/… |
| **(c) COMPUTED — verdicts/rates** | 254 | 68 | same objects, `can*`/`is*`/`AI_*`/`calculate*`/`find*`/`has*` |
| **(e) TEXT — residue only** | 80 | 5 | text-manager receivers; the PLANE itself is excluded — below |
| **Total** | **7,359** | 1,292 | |

⚠ Distinct names do **not** sum down the column (1,432 > 1,292): one name reached on two receiver kinds counts
in both. Sites do sum.

⚑ **`other / UI` now DOMINATES the remainder, and that is the shape to read the table by.** It is the one row
the library explicitly does not own (screen chrome), so the residue is increasingly *not the library's work* —
the three planes it does own (INFO + STATE + COMPUTED) are down to 2,499 sites between them.

⚠ **TEXT is absent by construction, not by being small** — `getText` is Python-defined, so the read-census exclusion rule
drops all **3,131** of its sites; the 80 above are only what other text-manager receivers leave behind. TEXT
remains a separate plane the library does not own (§1.1).

### 1.1 (e) TEXT is its own kind — and the library should not own it

`.getText(` alone is **3,131 sites** — larger than MUTATION and INFO's non-registry half combined. It is not info
data, not live state, not a computed value and not a write: it is **resolution of a localized string (or an art
path) from a key**. (It sits outside the demand tables by construction, because Python defines a `getText` of
its own; the raw count here is the honest size of the plane.) Treating it as info data would pull the entire TXT plane into
the library's contract; treating it as state would be simply wrong.

**Recommendation: TEXT stays a separate, thin service the library does *not* own — with one seam.** The library
already owes **rendered entry lines** (ruling 29, `Sources/UI/CvEntryText.{h,cpp}`), and those arrive
*already localized*. So the split is: **the library returns rendered/localized display strings for everything it
serves**; free-standing `getText("TXT_KEY_…")` lookups for a screen's own chrome (labels, headers, button text)
remain a localization service call. That keeps the one-surface ruling intact — no consumer ever asks the
*library* for an entity's text and gets a raw key back — without making the library the TXT gateway.
The vocabulary TXT keys are still unauthored, so the renderer's spell-back fallback is the accepted output for
now.

The concentration is informative: `Screens/` 655 · `Revolution/Gameready/` 329 · `Contrib/` 302 ·
`Screens/Advisors/` 292 · `EntryPoints/` 201 · `Screens/Pedia/` 190 · `PitBoss/` 153. TEXT is a **UI-layer**
concern almost everywhere, which supports leaving it outside the data library.

### 1.2 (a′) REQ — the condition trees are an INFO need, answered by `CvRequires`, not a tree-walk API

**⛔ THE WHOLE OLD MECHANISM IS GONE — deleted, not left dangling.** The `BoolExpr` binding file no longer
exists in `Sources/Python/`; the `GOMTypes` / `BoolExprTypes` enums are published nowhere (`CyEnums.cpp`
carries neither); and `Screens/Debug/HelperFunctions.py` (the `getGOMReqs` tree walker) is deleted along with
its callers — `TestCode.py` and the pedia pages that reached it through `self.HF` (`PediaBonus`,
`PediaBuilding`, `PediaHeritage`, `PediaTech`, `PediaUnit`).

**The replacement is `CyInfo::getRequiresIdsInClause(prefix, id, bucket, clause)`**, which walks the same
`CvRequires` struct server-side and returns the flattened id list per `REQCLAUSE_ALL` / `REQCLAUSE_ANY` — the
AND/OR split the Python walker used to compute — so a pedia page still renders `&` runs and `{ … || … }`
groups, just off a structured server read instead of a tree walk. `PediaBuilding.py`'s requires panel is the
live example.

⚠ **Neither edge family preserves that split** ([CvEdges.h](../../Sources/Infos/CvEdges.h)):
`EDGEF_REQUIRED_BY` is the reverse direction and `EDGEF_ENABLED_BY` the forward one, and both are merged
buckets — `getRequiresIdsInClause` is the read that keeps the AND/OR split, not either edge family.

This confirms independently the same conclusion as [pedia-map.md finding 3](pedia-read-map.md): *"no
boolean-expression API belongs on the new surface."* The
[reverse lookups are populated once, at load](../cascade/01-deposit-and-read.md#1-one-step-deposit-down-accumulate-read-o1) edge families answer the inverse
direction (what requires me), never the forward requirement tree.

### 1.3 (d) MUTATION — out of scope for the library, but still needed

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
   This is the one that most deserves an explicit ruling (§4) — it is a settings-persistence mechanism wearing
   an engine-write costume.

### 1.4 (c) COMPUTED

1,949 sites / 245 distinct names of `can*` / `is*` / `AI_*` / `calculate*` / `find*` / `has*`, concentrated in
`EntryPoints/`, `<root>`, Revolution, the editor and `Screens/`.

The availability half (`canConstruct` / `canTrain` / `canResearch` / `canDo*`) is **the enabler's surface, not
the cascade's** — [the enabler and the modifier cascade are two separate systems](../specs/enabler.md) — and
Python must read the enabler's own cached verdict, never re-derive it. The rate half (`calculateTotalCulture`,
`foodDifference`, growth/production turn estimates) is cascade-computed. **Both are live-context reads that sit
beside the info payload, never inside it** — the same conclusion as
[pedia-map.md finding 5](pedia-read-map.md), reached here at 800× the site count.

## 2. ⚑ The grep-invisible reads

A completeness claim built on static greps misses exactly this section. Every instance below is listed with
`file:line`; §2.7 states plainly what could not be proven.

### 2.1 Inventory of dynamic-access mechanisms

| Mechanism | Sites | Verdict |
|---|---|---|
| `getattr(...)` | 8 | 3 benign, 5 load-bearing (§2.2, §2.3) |
| `eval(...)` | 8 (+1 doc line) | **2 build engine calls from strings** (§2.2) |
| `__import__` | 1 | the BUG module resolver (§2.3) |
| `setattr` on an engine enum | 1 | **mints new `WidgetTypes` at runtime** (§2.3) |
| XML-declared callbacks | **1,052 declarations / 467 names** | the engine→Python entry graph (§2.4) |
| BUG XML handler bindings | **59 modules × 160 functions** | the config-driven dispatch graph (§2.3) |
| Int-keyed dispatch tables | 2 (`Events`, `OverrideEventApply`) | engine popup-ID → Python function (§2.4) |
| `apply()` | 10 | all `CvWBDesc` scenario methods — **not** the Python built-in. Benign. |

### 2.2 ⛔ The exhibit: engine method names built from strings and `eval`'d

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
**[NEEDS, not getters to port](../architecture/patterns/05-the-two-read-roles-one-grammar-two.md#-the-two-read-roles--one-grammar-two-answers)**, and a method name is the
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

### 2.3 The BUG config-driven dispatch graph

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

### 2.4 The engine→Python entry graph (XML-declared callbacks)

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

### 2.5 Structural blind spots (no dynamic trick required)

- **`inputClass.getData1()` / `getData2()` / `getButtonType()`** — 146+ sites. The popup-context object handed to
  Python by the engine is **not on the `.def` surface** this census parses, so these reads fall in the "unbound"
  bucket and are invisible to a binding-keyed census. They are genuine engine reads.
- **`**kwargs` forwarding** — `BUG/BugUtil.py:425` `self.call(*args, **kwargs)` (the `Function` wrapper). Every
  BUG-registered handler is invoked through it, so argument shapes are not statically checkable.
- **Method names colliding with bound names** — a Python-defined `def getValue(self)` is counted as the bound
  `getValue`. Small, and it inflates rather than hides.

### 2.6 The `revolution.distanceMod` standing exhibit, re-verified

`Revolution/Gameready/Revolution.py:1170` reads
`pPlayer.getRevIdxDistanceModifier() + pCity.getRevIndexDistanceMod()` — two spellings of one mechanic, consumed
by Python-authoritative gameplay and invisible to an engine-side read census. Verified live at that line.
Per [patterns.md](../architecture/patterns.md) **both distance kinds stay as-is, untouched by any stage**;
Revolutions owns them in its own rework. **No stage-4 investigation.** Recorded here only as the calibration
case for §2.7.

### 2.7 ⚠ How much of the surface could NOT be statically proven

Stated plainly, because a completeness gate depends on it:

- **The read SITES are ~99% statically enumerable.** The unserved call sites are matched by name against the
  published surface. The known miss is bounded and named: the string-built calls in §2.2 (19 method names in
  one table, 3 of which have zero literal sites), the unbound popup-context reads in §2.5, and whatever a map
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
  reaches them anyway. That is what surfaced §2.2. The same inversion over the BUG config and the XML callback
  tags produced §2.3 and §2.4. **I did not find a mechanism class beyond those listed; I cannot prove none
  remains.**

## 3. The Python-authoritative systems

These stay Python by [owner carve-out](../../AGENTS.md#design) and become **consumers**
of the library.

### 3.1 Revolution

`Revolution/Gameready/Revolution.py` · `Screens/Advisors/RevolutionWatchAdvisor.py` ·
`Revolution/RevEvents.py` · `Revolution/RevUtils.py` · `Revolution/Gameready/BarbarianCiv.py` ·
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
of §2.6.

**⚑ Flag:** these `getRev*` fields sit on civic/handicap/leaderhead infos. `getRevLaborFreedom`,
`getRevDemocracyLevel` and `getRevIdxLocal` are in the per-field residue — read by the Revolution stack and by
nothing the pedia shows. **Whether the rebuilt info surface currently exposes them is UNVERIFIED here** (this is
a Python-side census; I did not audit the rebuilt `CvJson*Info` headers for these members). Given the owner
ruling that revolution data is untouched until the Revolution rework owns it, the actionable item is narrow:
**stage 4 must not drop these fields while wiring the library**, and the Revolution rework — not stage 4 — decides
their final shape.

### 3.2 Random events (`CvRandomEventInterface.py` + `CvOutcomeInterface.py`)

**466 of the 467 XML-declared callbacks live here** (§2.4). It is the most *balanced* consumer in the tree,
exercising all five kinds heavily.

Needs served: **per-entity payloads** for the entity an event names (unit, building, bonus, improvement, tech,
religion, corporation), the **event/trigger types** (`getEventInfo`, `getEventTriggerInfo`, `getPrereqEvent` —
no pedia page), live state on the target, availability verdicts for `canDo*`, and the
mutation boundary for `do*`.

**⚑ Flag:** `CvEventInfo` / `CvEventTriggerInfo` are bound (27 and 14 `.def`s) and read 22 times from Python,
but have **no pedia page**, so pedia-driven work would not serve them at all. They also carry the
`<PythonCallback>` strings themselves — i.e. the info type *contains* the dispatch graph.

### 3.3 Others that are effectively Python-authoritative

- **`Contrib/DynamicCivNames.py`** — rewrites civ names from civic/religion state. Needs the civic-option
  index (`getNumCivicOptionInfos`, 31 sites, pedia-untouched residue).
- **`Revolution/Gameready/BarbarianCiv.py`** — spawns barbarian civs; needs `getBARBARIAN_PLAYER`,
  world/handicap config, and heavy mutation.
- **`DancingHoskuld/Partisan.py`**, **`Contrib/Civ4lerts.py`**, **`CvAdvisorUtils.py`** —
  gameplay-reactive Python reading live state each turn.
- **`pyWB/CvWBDesc.py`** — scenario serialization; authoritative for the save/load text format and needs
  **stable type keys**.

## 4. The boundary rulings — and the two questions still open

1. **Map scripts keep their own CALLBACK contract — they are NOT outside the library.**
   ⛔ **THE OLD `GC.get<X>Info` ENDPOINTS ARE NOT COMING BACK, SO A MAP SCRIPT'S READS MOVE ONTO THE NAMED
   SURFACE LIKE EVERY OTHER CONSUMER'S.** What is separate is the **contract** — the named Python callbacks
   ([engine.md](engine.md)) — never the reads made inside them.
   ⚠ **The earlier wording here said they were "outside the data-fetching library entirely" and "unaffected by
   the `Cy*` cut", and BOTH were wrong.** The callback names are unaffected; the reads are not, and every one
   of them went dead with the cut. ⛔ **The measured cost of that wording: map GENERATION cannot read its data,
   so a NEW GAME cannot be generated at all** — `CvMapGeneratorUtil.py` is the DLL's own fallback
   implementation, so this is the generation path rather than one screen.
   ⚑ It reads as a scope boundary and is a BLOCKER, which is exactly why an agent files it as out-of-scope and
   moves on. That has now happened; do not repeat it.
   ⚑ The shape is the ordinary one: a per-info accessor per map-gen type, the `CyWorldInfo` shape — which
   already carries map-gen reads, so nothing new is being invented for them.
   ⚖ **WHAT IS GENUINELY DIFFERENT IS THE ENUMERATION, NOT THE SURFACE: a map script is the real case
   that HAS to iterate every bonus, terrain and feature.** Placing resources and laying terrain is a decision
   over the whole registry, so the whole-registry loop is CORRECT here and stays — exactly as it is in the
   pedia ([patterns.md](../architecture/patterns.md): *"it is the pedia, it is where all info is stored, as an
   encyclopedia"*). Map generation is the SECOND legitimate full-scan consumer, and the only other one.
   ⛔ So the [patterns.md](../architecture/patterns.md) rule that *a whole-registry loop is the actual defect*
   does NOT reach these files: there is no maintained set to convert them to, because the answer genuinely
   depends on every entity. ⚠ Do not "fix" a map-gen sweep into an edge read — that is the opposite error from
   the one above, and it would delete the mechanic.
   ⇒ **The loop stays; only where each value COMES FROM changes.** What still applies is the cost note the
   pedia carries: an enumeration that crosses the boundary once per entity wants ONE crossing, so a per-type
   read that fills a list beats a call per id.
   The observations the split was built on stand, and they say why the map-gen TYPES are read by nobody else,
   never that the reads go unserved: `CvMapGeneratorUtil.py` (269) + `MapScriptToolsOld.py` (672) + the `Assets/Maps`
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

2. **`Screens/Debug/TestCode.py` is DELETED, not migrated** —
   if that capability is wanted it gets built properly; the Python refactor made this one worthless. It was the
   largest INFO consumer after the pedia hub (1,488 INFO sites) and the sole consumer of 90 residue names /
   296 sites, all of which drop out of the library's obligations (**the appendix shrinks ~30%**). The whole
   feature chain went with it (`DebugBtn` → `showDebugScreen` → `DebugScreen` → `TestCode`, plus the dead
   `pythonDebugToggle`); `HelperFunctions.py` and its GOM walker are ALSO gone (§1.2), and the orphaned
   `INTERFACE_DEBUG_SCREEN_BUTTON` art STAYS untouched (art is hands-off — roadmap § Scope decisions).
   ⚑ Its 50 checks encoded real design invariants the JSON spec does not state (a requirement may not unlock
   after the thing requiring it; replacements are explicit, never implicit; a replacing entity must be better).
   Those invariants belong in the SPEC first — not a stage-4 item.

3. **Global DEFINEs — the READS stay, the WRITES are OUT OF SCOPE.**
   Reads: `getMAX_PC_PLAYERS` (176) · `getMAX_PLAYERS` (74) · `getMAX_PC_TEAMS` (44) · `getBARBARIAN_PLAYER`
   (40) · `getMAX_TEAMS` (13) — a small closed constants block, trivially served by the library.
   The **69 `setDefineINT` writes** are RULED OUT OF SCOPE: they are a MUTATION surface (not a data read), and
   all 69 sit in `Contrib/RevDCM.py` (32) + `Afforess/DiplomacySettings.py` (36) + `Afforess/ANewDawnSettings.py`
   (1) — the Python-authoritative contrib stacks, each due its own rework (Revolution explicitly). They are the
   BUG-option → global-define bridge.

   ⚑ **What they ARE — "LIVE" options (the established term), a distinct KIND, not a duplicate of `GAMEOPTION_*`.**
   The verified chain: a BUG option declared in `Assets/Config/<mod>.xml` and persisted to its own `.ini`
   (`<options id="RevDCM" file="RevDCM.ini">`, each `<option>` carrying `get`/`set` + a `<change>` callback) →
   BUG fires that Python callback on change → `GC.setDefineINT(...)` → `cvInternalGlobals::setDefineINT`
   (`CvGlobals.cpp:2654`: MP-synced via `sendGlobalDefineUpdate`, then `cacheGlobals()`) → the DLL reads its
   cached accessor (`GC.isDCM_RANGE_BOMBARD()`, `CvUnitAI.cpp:26193`). So the user can flip one **at any time
   and it takes effect immediately**.

   That is the difference in kind: a **game option** is chosen at game setup and fixed for the game (so JSON may
   gate an entity on it, [the whole-entity applicability gate](../specs/json/02-anatomy-of-an-entity.md#2-anatomy-of-an-entity)); a **live option**
   is a user setting changeable mid-game. They are NOT to be folded into `GAMEOPTION_*` on the assumption that
   they are strays. The consequence worth knowing rather than re-deriving: **JSON cannot gate on a live option** —
   nothing static may depend on a value that moves under it.
   ⚑ A flip DOES announce: `SEVT_GAME_GLOBAL_DEFINE_ADDED / _REMOVED` ([spine.md](../spine.md)) fires from the
   three `cvInternalGlobals::setDefine*` setters, so a consumer that needs to answer one can. That closes the
   reactability gap; it does not license gating authored data on a live option, which is a separate ruling and
   unchanged. The writes themselves belong to the contrib stacks' own reworks, not here.

3b. **A natural-disaster mechanic whose whole effect is loss of a plot improvement is authored as a `triggers` entry,
   never as a Python event — RULED.** `trigger → chance → action` with the `destroy` verb
   ([json.md §5](../specs/json.md)) already expresses that shape exactly, so the capability belongs as DATA on
   the trigger plane. This does NOT reopen the events carve-out (#425 events stay Python) — it fixes where this
   one shape of capability lives if it is ever wanted.

4. **Does the library own TEXT, or only the rendered lines it produces?**
   §1.1 recommends the latter (library returns localized display strings for what it serves; screen chrome stays
   a localization service call). At 3,168 `getText` sites the answer materially changes the library's contract, so it
   wants an explicit ruling rather than an inherited assumption.

5. **Name-based enum/type resolution is a FIRST-CLASS library operation.** Name→value resolution is a supported operation of the library's surface, not an
   accident of `getattr` on a module, and the evidence in §2.3 is why: `WidgetTypes`, `InputTypes` and
   `InterfaceDirtyBits` are reached ONLY this way, so without it those reads have no path at all. ⚠ Note the
   shape this must cover is **resolution AND extension**: `BUG/WidgetUtil.py:62-68` does `getattr(WidgetTypes,
   name)` *and* `setattr(WidgetTypes, name, widget)` — BUG MINTS new enum members at runtime from config names
   and hands them back to the engine as widget ids. A read-only lookup would not serve it. This generalizes what
   the engine already does for infotypes (`getInfoTypeForString`) and pairs naturally with the load-minted
   classification registries ([the classification-infos registry](../specs/json/09-classification-unit-skillstagsstate-building-a.md#8-classification--unit-skillstagsstate-building-attributes--empire-capabilities)),
   which are the same idea on the info plane: names minted to ids at load, resolved by id thereafter.

   And the completeness argument that makes it load-bearing: a library WITHOUT name→type resolution forces those
   consumers to keep a legacy reach-around — the second live surface the one-surface ruling forbids.

6. **~~What is the MUTATION boundary's shape, and is it stage 4's job?~~ — CLOSED: THE WRITE SURFACE EXISTS
.** ~144 `set*`/`change*`/`do*`/`create*` defs are published across
   `CvPythonPlayerLoader` / `CvPythonPlotLoader` / `CyGame` / `CyTeam` / `CyMap` / `CyArea` / `CyCity` / `CyUnit` — the cut
   was DIRECTIONAL and took the READ bindings only. ⛔ So this is not an open question and must not be cited as
   one: a mutating consumer that fails is WIRED, and a write it needs that is not published yet is ADDED to that
   surface ([patterns.md](../architecture/patterns.md)). The paragraph below is the
   ORIGINAL framing, kept only because its LAST sentence is the thing that was wrong:
   987 sites are writes. They are explicitly out of scope for a *data-fetching* library, but the same handlers
   read through it, and the legacy `Cy*` surface cannot be disconnected while a write path still depends on it.
   Stage 4 needs a decision on whether the write boundary is designed alongside the library or sequenced after.

7. **WORLDBUILDER IS ITS OWN SURFACE — and it MUST travel the SAME engine paths.**
   *"A dedicated worldbuilder surface is definitely the way to go, also when worldbuilder adds or removes, it has
   to emit events the same way as if things were normally constructed, or removed"* — *"so it should use the same
   paths."*
   ⛔ **"ITS OWN SURFACE" MEANS THE `Cy*` WRAPPER LAYER, NEVER A SEPARATE WORLDBUILDER CLASS.** The wrapper IS
   the surface: it is the CONTROLLER of the engine-model / `Cy*`-controller / Python-view split, and therefore
   the one place human-readable form becomes engine data
   ([the Cy* layer is the controller](../architecture/patterns/06-the-python-read-boundary-one.md#-the-python-read-boundary--one-complete-data-fetching-library)).
   So an editor write is homed on the object it addresses — `CyUnit`, `CyCity` — exactly like every other write,
   and there is no `CyWB`. ⚠ A separate editor class would have to be keyed by `(owner, id)` to avoid wrapping
   the same object twice, which rebuilds the flat address-keyed shape that page calls the mishomed failure.
   ⚑ **What IS out of scope is the modelled READ vocabulary, not the wrapper.** A scenario editor pokes
   ARBITRARY engine fields — cargo, facing direction, base combat strength, made-attack flags — where the read
   library models a bounded set of QUESTIONS ("what do I carry", "what do I have", "can I"). There is no
   `UnitFlagKind` for *is this unit cargo* because nothing in the game model asks it; only the editor does — so
   it stays a field asked of the unit, and never becomes a new read KIND or a group-read slot.
   Measured: **34 cargo/transport sites and 31 unit-write sites, every one of them in
   `Screens/Worldbuilder/` or `pyWB/`** — zero in gameplay.
   ⚑ **A `Times100` in a verb name is the boundary declining to do its job.** The wrapper is where the ×100
   fixed point reduces ([the ×100 fixed-point model](../specs/curators/fixed-point-and-scales.md#1-the-model--integer-100-for-amounts-human-only-at-the-in-and-out-boundaries)),
   so a scale belongs INSIDE the verb, not in its name. ⚖ The one legitimate exception is a scenario ROUND-TRIP,
   where the editor writes back the exact value it was handed and rescaling either side would corrupt it — and
   that is stated at the verb, never assumed.
   ⛔ **The binding half of the ruling — the editor writes through the engine's OWN mutator, never a field poke.**
   An editor that sets a member directly leaves the cascade unaware, so the derived caches diverge SILENTLY —
   precisely what the event spine exists to prevent. ⚠ And the fix is NOT "poke the field, then also emit": that
   is two implementations of one transition and they drift
   ([the DRY single-implementation law](../architecture/patterns/03-dry-one-implementation-per.md#dry--one-implementation-per-calculation--evaluation-the-single-source-law)). Routing through the
   mutator makes the emit STRUCTURAL — it cannot be forgotten by a future verb, because no verb owns it.
   ⚑ The pattern already exists and is the model to copy: every write verb resolves its handle, validates, then
   calls the real engine setter (`CyCity::setBuilding` → `CvCity::changeHasBuilding` → `setHasBuilding`, which
   runs the ledger, `setupBuilding` and `processBuilding(±1)`). Its neighbour's comment states the rule outright:
   *"The DOMAIN fact still fires -- that is the setter's job and is exactly what must not be skipped."*
   ⇒ Consequence for a would-be editor verb with no engine mutator behind it: the missing piece is the ENGINE
   path, and that is what gets built — never a Python-side shortcut that writes the member and fakes the event.

8. **REVOLUTION LIVE-STATE MIGRATES WHOLESALE TO THE PYTHON STORE.** *"Migrate wholesale."*
   The revolution counters and timers — `getLocalRevIndex`, `getNumRevolts`, `getRevolutionCounter`,
   `getReinforcementCounter`, `getRevRequestAngerTimer`, `getRevSuccessTimer`, `getRevIndexPercentAnger`,
   `getRevIndexDistanceMod` (~120 reads and ~110 writes) — move into `RevData`'s SdToolKit store, NOT into
   `CityCountRead` + new engine write verbs. Revolution is Python-authoritative, so its own state belongs on the
   Python side of the boundary.
   ⛔ **Wholesale means the engine's two surviving slots come OUT too** (`CITY_COUNT_REVOLUTION_INDEX`,
   `CITY_COUNT_REVOLUTION_AVERAGE`). Leaving them is the half-migration the drift detectors name: engine-persisted
   revolution state beside a Python store for the rest is two homes for one fact, and the save carries both.
   ⚠ **What this ruling does NOT cover:** Revolution's ~16 UNIT writes (`setUnitAIType`, `setXY`, `setMoves`,
   `setPromotionReady`). Setting a unit's AI type is an ENGINE mutation, not revolution state — SdToolKit cannot
   hold it, so those still need real write verbs on `CyUnit` and are unaffected by the wholesale move.

9. **⛔ PYTHON READS A UNIT'S *DEFAULT* UNITAI AND NOTHING ELSE — the multi-role membership test does not come
   back on this boundary.** `CyUnitInfo::getDefaultUnitAI(iUnit)` is the whole surface: it answers the ONE role
   a unit is created with. The legacy `getUnitAIType(UNITAI_X)` membership predicate — *may this unit also act
   as X?* — is gone, and a consumer that wants it is asking for the plane this ruling closes.
   ⚑ **The narrowing is the POINT, not a gap.** UnitAI has been one of the largest single bug sources in this
   codebase, and the mechanism is permissiveness: a role a unit was never designed for is reachable simply
   because nothing hard-DISALLOWED it. The worked case is a TANK acquiring `UNITAI_PROPERTY_CONTROL` — nothing
   about the unit invited it; the membership plane merely failed to refuse. A single authored default cannot
   fail that way, because there is no set to leak into.
   ⇒ So a Python consumer testing *"is this unit a counter/attack/defence unit?"* asks
   `getDefaultUnitAI(iUnit) == UNITAI_X`. ⚠ That is deliberately NARROWER than the legacy predicate and will
   classify fewer units; where it changes a list, the list was previously admitting units on a role they only
   incidentally carried.
   ⛔ It binds the PYTHON boundary only — the engine keeps its own UnitAI machinery, and this neither simplifies
   nor re-opens that. ⚠ Nor does it reach the WRITE side: ruling 8's `setUnitAIType` carve-out is a mutation on
   a different axis and stands as written.
