# Unit & UNITAI Valuation — living report

**Status:** investigation / living report. Started from a concrete symptom: *too many dogs &
hunters persist into the late game (especially with **Size Matters** on), and foreign hunters
sit in my cities like scouts.* Goal is to understand the **actual** valuation the AI applies
to units and UNITAI roles, document it, then decide fixes.

Legend: **[V]** = quoted code read & verified at the cited line; **[A]** = agent-reported,
line not yet re-verified (structure consistent, treat exact line as approximate).

---

## A. Why hunters/dogs persist late-game

Three independent mechanisms stack, and **none of them looks at how many huntable animals
actually remain**:

### A1. The "needed hunters" target never decays with animals — **[V]** `CvPlayerAI::AI_neededHunters` (`CvPlayerAI.cpp:11653`)
```cpp
int iHuntersneeded = std::min(
        intSqrt(getNumCities()) + pArea->getNumUnownedTiles() / 16 + 1,
        getNumCities() / 2  + pArea->getNumUnownedTiles() / 64);
int iMaxhunters = 4 + int(NB_MAX_HUNTERS * pow((iWorldSize + 1) / 6.0, 0.8)); // NB_MAX_HUNTERS=10
return std::min(iHuntersneeded, iMaxhunters);
return (iHuntersneeded);   // <-- dead code after the return above
```
- Inputs: **city count, unowned-tile count, world size.** *Not* the number of animals/neutral
  units on the map. So once the animals are hunted out, the target does **not** fall.
- Late game `getNumUnownedTiles()→~0`, so the target collapses to `min(sqrt(cities)+1,
  cities/2)` ≈ **`sqrt(cities)`** — a persistent floor of ~5–6 hunters per landmass for a
  20-city empire, **forever**, capped around 11–12.
- Minor: the trailing `return (iHuntersneeded);` is dead (a [[dead-code-xml-pass]] nit).

### A2. The hunter unit value has no era/animal decay — **[V]** `AI_unitValue` UNITAI_HUNTER case (`CvPlayerAI.cpp:11187`)
```cpp
case UNITAI_HUNTER:
    iValue += iCombatValue;
    iValue += iCombatValue * (kUnitInfo.getMoves() - 1) / 2;
    iValue = getModifiedIntValue(iValue,
        kUnitInfo.getAnimalCombatModifier() + kUnitInfo.getUnitCombatModifier(GC.getUNITCOMBAT_ANIMAL()));
    if (kUnitInfo.hasUnitCombat(GC.getUNITCOMBAT_HUNTER())) iValue = iValue * 3/2;
```
- Pure function of combat strength, extra moves, and anti-animal modifiers. **No term decays
  as the era advances or as animals disappear.** A hunter is scored the same in the space age
  as in the stone age.

### A3. Production keeps firing because the deficit never closes — **[A]** `CvCityAI::AI_chooseProduction`
Hunter build gates are a ladder of `iHunterDeficitPercent` thresholds (agent-reported approx.
lines): `iOwnedHunters<1 && deficit>80%` (~1650), `<8 && >50%` (~1985), `<16 && >25%`
(~2446), and `iNeededHunters > iOwnedHunters*2/3` (~2613). With
`iHunterDeficitPercent = (need<=owned)?0:(need-owned)*100/need`, and **need** stuck high (A1),
the deficit stays positive, so cities keep queueing hunters.

### A4. Existing hunters don't retire — **[A]** `CvHunterAI::hunterMove` (`CvHunterAI.cpp:~250`, `~366`)
A hunter only reverts to its default UNITAI when `iOwnedHunters>5` (or deficit ≤80% with
>1 owned), and only **scraps** under `AI_isFinancialTrouble()` **and** positive upkeep **and**
surplus. So absent financial pressure, hunters persist on the roster indefinitely, which feeds
back into A3 (owned count) but never satisfies the inflated need.

### A5. Merging deflates the count under a count-based target — **[H]** (owner hypothesis, 2026-06-11)
Hunter-line units can merge (`CvUnit::canMerge`/`doMerge`; note §B's dog exception — dogs
cannot). Every merge (e.g. 3 units → 1 stronger unit) drops `iOwnedHunters` while hunting
*capacity* is preserved, so the A3 deficit reopens and cities rebuild toward the count
target — a merge → deficit → rebuild loop that pumps hammers into hunters even when
capacity never fell. Benchmark consequence: census *count* series can't distinguish merges
from attrition (observed 2026-06-11 game: hunters 120→111 across t340–348 during prey
collapse — either reading fits). Any fix to A1/A3 should target hunting *capacity*
(count weighted by merged size/strength), not raw unit count. **→ Now owner-ruled and
scoped in [`size-matters-ai.md`](size-matters-ai.md) (#395):** strength-weighted effective
counts (×1.5/rank), hunters keep merging, `AI_neededHunters` goes capacity-based.

**Net:** the AI targets ~`sqrt(cities)` hunters per landmass for the whole game regardless of
animals, scores each hunter with no decay, keeps building toward the target, and rarely
retires the ones it has. That's the dog/hunter glut.

---

## B. Size Matters merge bonus — RULED OUT for dogs

> **Correction.** An earlier draft of this section claimed dogs get the Size Matters 2×
> merge bonus. **That is wrong** — confirmed from two angles below. Dogs **cannot merge**, so
> the merge factor never touches them. Size Matters / merge is a **dead end for the dog glut.**

### B1. The merge factor only applies to *mergeable* units — **[V]** `EVAL_MERGE_FACTOR` — **RETIRED (#395, 2026-06-12)**
```cpp
#ifndef NO_CAN_MERGE_BONUS
    #define EVAL_MERGE_FACTOR  2
#endif
...
if (kUnitInfo.canMergeSplit() && GC.getGame().isOption(GAMEOPTION_COMBAT_SIZE_MATTERS))
    iValue = int(iValue * EVAL_MERGE_FACTOR);   // doubles the value
```
Was applied in UNITAI_ATTACK, ATTACK_CITY, COUNTER, CITY_DEFENSE, ESCORT — gated on
`kUnitInfo.canMergeSplit()`. **Deleted by the #395 mix-sanity pass**
([`size-matters-ai.md`](size-matters-ai.md)): it biased the unit mix toward mergeable
types while the accounting treated every body the same; with strength-weighted force
ledgers and need-driven merges the thumb on the scale is gone. (B2/B3 below remain
relevant as history: the factor never applied to dogs.)

### B2. Dogs are non-mergeable — **[V]** `canMergeSplit()` vs `UNITCOMBAT_SUBDUED`
- `CvUnitInfo::canMergeSplit()` returns `m_bCanMergeSplit`, which is forced **`false` if any of
  the unit's combat types `isCannotMergeSplit()`** (`CvUnitInfo.cpp:4887`).
- `UNITCOMBAT_SUBDUED` sets `<bCannotMergeSplit>1</bCannotMergeSplit>`
  (`Assets/XML/Units/Subdue_Animals_CIV4UnitCombatInfos.xml:10`).
- ⇒ subdued animals / dogs have `canMergeSplit() == false` ⇒ the `EVAL_MERGE_FACTOR` branch is
  **never taken** for them. (Confirmed by the project owner: "dogs cannot be merged.")

### B3. The recent "Size Matters check" fix doesn't touch dogs — **[V]** git `f2542b71` (2026-06-05, #125)
The fix the dogs were suspected to be "remnants" of moved a `break;` to **after** the
`canMergeSplit()` block in **UNITAI_CITY_DEFENSE** and **UNITAI_ESCORT** (it was dead code), so
merge-capable defenders/escorts now get the same 2× that ATTACK/ATTACK_CITY/COLLATERAL already
gave. It changes behaviour **only for `canMergeSplit()==true` units** — i.e. proper military,
not dogs. So the dogs are **not** remnants of this change; if anything it shifts production
*toward* mergeable military and away from cheap non-merge units.

**Conclusion:** neither the merge bonus nor its recent fix explains the dogs. The spammed unit
is the **trainable War Dog** (owner-confirmed) — see §D for what it actually is and where the
spam must originate.

---

## C. Why foreign hunters park in your cities (the "scout" look)

A foreign civ's UNITAI_HUNTER, having run out of animals, falls through to explore/retreat
behaviour and — because you have **open borders** — treats your land (and cities) as a valid
place to wander and rest.

1. **Fallback cascade [A]** — `CvHunterAI::hunterMove` (`CvHunterAI.cpp:~381–407`): when hunts
   fail, it runs `AI_refreshExploreRange → AI_moveToBorders → AI_patrol → AI_retreatToCity →
   AI_safety`. So an idle hunter becomes an explorer/loiterer.
2. **Open borders = passable [V]** — `CvGameCoreUtils.cpp:2689`: `MOVE_NO_ENEMY_TERRITORY`
   only blocks plots owned by a team you are **at war with**. Friendly-foreign (open-borders)
   territory is fully traversable by the explore/refresh/retreat pathing.
3. **Cities win the "safe plot" score [V]** — `CvUnitAI::AI_safetyEval` (`CvUnitAI.cpp:15675`):
   `iValue += plotX->defenseModifier(...)` (15719, unconditional — **city plots score high**),
   `+50` for staying on the current plot (15721), and — *only when fleeing animal danger* —
   `+50` for a friendly-foreign plot (15714, gated on `bAnimalDanger`). Hunters operate around
   animals, so that gate is often live for them.

Result: an idle foreign hunter wanders into your open-borders territory, the safety/retreat
logic rates your high-defense city plot as the best rest spot, the stay-put bonus re-confirms
it each turn, and it "parks." It isn't formally a scout — it's an **unemployed hunter using
the explore/safety fallback**, with no rule preferring its *own/neutral* land over yours.

**Partially fixed (#392, 2026-06-11, live-traced unit 1835350):** two of the stranding paths
are closed — `hunterMove`'s refresh-explore now passes `bAvoidRivalTerritory` to
`AI_refreshExploreRange` (the stale-visibility bonus no longer values rival fog, so hunters
are not *sent* into foreign borders), and the escort-wait block has a foreign-territory
branch (adjacent kill → `AI_reachHome` → `AI_retreatToCity`) instead of falling through to
its skip-in-place — that skip left hunters advertising for an escort and idling in rival
land indefinitely ("lost contact with the mothership"). Still open from the list above:
the `AI_safetyEval` city-plot scoring (§3) and the broader fallback prefer-own-land rule
(direction **C** below).

---

## D. War Dog spam — what it is, and where the cause is NOT

The spammed "dog" is the trainable **`UNIT_WARDOG`** (`U_Land_CIV4UnitInfos.xml:24785`) — owner-
confirmed. **[V]** its actual profile:
- `<DefaultUnitAI>UNITAI_PILLAGE_COUNTER</DefaultUnitAI>`; eligible UnitAIs:
  **CITY_COUNTER, PILLAGE_COUNTER, HUNTER_ESCORT, SEE_INVISIBLE**. **No `UNITAI_COLLATERAL`,
  no `<iCollateralDamage>`** — it deals zero collateral, so the "collateral soak" framing
  doesn't fit this unit.
- Cheap, versatile harasser/detector: `iCombat 7`, `iMoves 2`, `iCost 163`, `bIgnoreTerrainCost`,
  `bPassage`, `iAnimalCombat 20`, and **`SeeInvisible INVISIBLE_CAMOUFLAGE`** + stealth/unnerve/
  enclose/lunge bonuses. Tech `MILITARY_TRAINING` + `BUILDING_WAR_DOG_TRAINER`. Upgrades to
  Guard Dog. (Primary combat `UNITCOMBAT_ANIMAL`; non-mergeable, consistent with §B.)

**Ruled out as the cause:**
- Size Matters merge bonus (§B) — non-mergeable.
- `UNITAI_COLLATERAL` valuation — the War Dog isn't a collateral unit.

**Valuation ignores the Size Matters quality penalty — [V].** `AI_unitValue` scores combat via
`GC.getGame().AI_combatValue(eUnit)` (`CvPlayerAI.cpp:10690`) — a **unit-type / base-stat**
value. Under Size Matters, dogs/hunters/etc. get a **−1 combat quality** (lower `qualityRank()`,
which feeds `getSizeMattersOffsetValue() = qualityRank+groupRank+sizeRank-15`, `CvUnit.cpp:27664`,
→ lower `getSMStrength()`), so they fight **weaker than base**. The valuation, working on the
unit *type*, never sees that instance-level penalty ⇒ the AI **overvalues** SM-quality-penalized
units relative to their real strength. (Owner: whether the −1 design is good is a separate
question, out of scope; the in-scope issue is that valuation is blind to it.) The valuation
logging should record base combat next to value so this gap is visible in the data.

**Where the cause must be (still open):** the production picker (`AI_unitValue` big switch
`~10700+`) and the **needed-counts** for the War Dog's real roles. Prime suspects:
- **`UNITAI_SEE_INVISIBLE`** — the War Dog is one of the cheapest camouflage-detectors; if the
  see-invisible *need* is uncapped or the value high, it gets mass-produced. (No
  `AI_neededSeeInvisible` cap was found by grep — verify.)
- **`UNITAI_HUNTER_ESCORT`** — ties straight back into the verified hunter glut (§A).
- **`UNITAI_PILLAGE_COUNTER` / `CITY_COUNTER`** — counter "soak/absorb" roles (closest to the
  owner's original soak intuition, just not via the COLLATERAL UNITAI).

**Separate real bug found while looking (NOT the War Dog cause):** in the tech-valuation switch
on `getDefaultUnitAIType()` (`CvPlayerAI.cpp:~5816–5851`), a cascade of **missing `break`s** —
`HEALER → HEALER_SEA → PROPERTY_CONTROL → PROPERTY_CONTROL_SEA → INVESTIGATOR → INFILTRATOR →
ESCORT → SEE_INVISIBLE → ATTACK` — makes a unit whose *default* UNITAI is any of those sum the
entire downstream chain (e.g. a SEE_INVISIBLE-default unit also banks full ATTACK military
value). This inflates **tech** value for those unit families. It does **not** hit the War Dog
(default `PILLAGE_COUNTER` breaks cleanly at 5882), but it's a genuine bug worth its own fix
(candidate GH issue per [[codebase-bug-hunt]]).

---

## Open questions / where logging would help

Per [[ai-logging-before-bug-audit]], before fixing, instrument and watch:
- Log `AI_neededHunters` vs owned per area per turn (does the target really stay ~sqrt(cities)
  with zero animals?) and the actual remaining-animal count for comparison.
- Log `AI_unitValue` per (unit, UNITAI) at production-choice time (the `[CIT]` path) to see the
  2× merge boost in action and which cheap units win.
- Tag the hunterMove fallback branch taken (`[HAI/spread]` already exists) and the plot owner
  it parks on, to quantify the foreign-city loitering.

## Candidate fixes (hypotheses, not yet decided)
- **A:** make `AI_neededHunters` actually decay with huntable-animal count in the area (and/or
  era), so the target falls once the map is tamed.
- **A/B:** add an era/obsolescence decay to cheap-unit `AI_unitValue` so dogs/hunters lose
  value as better units unlock — independent of the merge boost.
- **B:** ~~reconsider the flat `EVAL_MERGE_FACTOR=2` (or offset it in needed-counts) so merge
  potential isn't double-counted.~~ **DONE (#395):** retired outright; see
  [`size-matters-ai.md`](size-matters-ai.md).
- **C:** in the hunter explore/safety fallback, prefer own/neutral territory and avoid
  resting inside foreign cities even under open borders; or reduce the idle-hunter explore
  fallback once exploration is complete.

## Cross-references
- Memory: [[sea-ai-rework]], [[hunter-ai-module-split]], [[ai-logging-before-bug-audit]],
  [[ai-unit-movement-to-player-level]].
- Related: the planned `CvHunterAI` evolution (hunter+explorer module vs a dedicated army
  module) would be the natural home for fixes A/C — see the seam map in the turn-time work.
