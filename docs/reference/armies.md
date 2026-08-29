# Armies — the coordination layer above selection groups

> What the `CvArmy` layer is today, and which parts of it are not built. Engine behaviour as it is; the
> unit-side lifecycle it sits over is [unit-lifecycle.md](unit-lifecycle.md).
>
> ⚠ **This is a PARTIAL implementation, and reading it as a finished feature is the mistake this page exists
> to prevent.** The shape is sound and the concept is wanted; three specific things below were never built.

## What it is

An **army is a group of GROUPS**, not a group of units and not a renamed selection group. `CvArmy` holds
`std::vector<int> m_groupIDs` plus a separate `m_iLeaderGroupID`, so it is a coordination layer sitting
*above* `CvSelectionGroup` — which is what makes it a genuinely new concept rather than a relabelling.

**It is owned by the PLAYER**, and that is the right home: the player owns the units and the groups, so the
layer that coordinates several groups belongs at the same scope. `CvPlayer::m_armies` is an
`FFreeListTrashArray<CvArmy>`, reached by id through `CvPlayer::getArmy`; a group carries its
back-reference as `CvSelectionGroup::m_iArmyID` (`-1` = not in an army).

The whole class is compiled behind **`#ifdef CVARMY_BREAKSAVE`**, defined unconditionally in
`Sources/Defines/CvDefines.h`. ⚑ The guard's NAME is the useful part: it records that making armies
persist would break saves — i.e. serialization is the piece that was never finished, not an option anyone
is expected to switch off.

## The lifecycle

| step | where |
|---|---|
| formed | `CvPlayerAI::AI_formArmies` — picks a leader group, creates the army, assigns a mission |
| run | `CvArmy::doTurn`, driven from `CvPlayerAI` over `m_armies` |
| dissolved | `CvArmy::disband` |

A mission is one of `ARMY_MISSION_NONE` / `ATTACK_CITY` / `DEFEND_BORDER` / `ESCORT` / `PATROL`
(`CvEnums.h`). The army carries a `CvPlot* m_pTargetPlot` for that mission.

**Consumers are two, and that is all**: the `doTurn` drive above, and one read in `CvUnitAI` that takes the
army's target plot as the unit's target city (null-checked on both the army and the plot).

## ⛔ Nothing about an army is serialized

`CvArmy` declares **no `read`/`write` at all**, and `CvSelectionGroup::m_iArmyID` has **no wrapper read or
write** either — it is only reset to `-1`. So an army, and every group's membership in one, exists for the
current session and no longer.

⚑ **Both halves vanish together, which is what makes it safe rather than corrupting.** A group cannot come
back off a save pointing at an army that no longer exists, because the group's `m_iArmyID` does not come
back either. The failure mode is amnesia, not a dangling reference.

⚠ The consequence to hold on to when reading AI behaviour: **an army can never outlive a save/load.** Any
observation of "the AI does not campaign with armies" across a reload is explained by this before anything
else is suspected.

## ⛔ What is not built — two unfinished halves and one DECISION

⚖ **The three are not the same kind of gap, and treating them alike is the mistake to avoid.** Two are
mechanics that were started and left incomplete; the third is a deliberate not-yet.

**Unfinished — these are genuine holes:**

- **Persistence.** The above. This is what `CVARMY_BREAKSAVE` names.
- **The leader is not a member of its own army.** `AI_formArmies` calls `setLeader(pBestLeader)` and the
  line that would add that group to `m_groupIDs` is commented out beside it. So `getLeader()` answers, while
  the leader does not appear in the army's own group list.

**⛔ Deliberately NOT built — `ARMYAI_`, and the reason is what protects the decision:**

An AI-type axis for armies — the counterpart of `UNITAI_*` — was intended and never landed; the identifier
appears nowhere in the tree. **It stays that way for now, because minting it means writing the AI LOOPS that
would consume it**, and that work is not being taken on yet.

⇒ So the absence is a SCOPE decision, not an omission to close. ⛔ Do not add an `ARMYAI_` enum, and do not
read the missing axis as an unfinished half of the two above: an enum with no decision loops behind it is a
vocabulary nothing consumes. What an army is *for* is carried by `ARMY_MISSION_*` meanwhile, which is
sufficient for the coordination the layer actually performs today.

⚑ **The prize is recorded WITH the deferral, because it is what makes this worth returning to: an army-level
AI would likely be a significant PERFORMANCE win, since one decision taken for an army replaces the many
individual per-unit calculations it stands in for.** That is a direct claim on the objective every perf
decision answers to ([turn time is king](../cascade/16-package-model.md#-the-per-scope-package-model--the-cascades-founding-design-1-stated-as-cache-architecture)) —
the cost sits in the AI's per-unit loops, and coordinating N units as one is exactly the shape that removes
it rather than making it faster.
⚠ So this is a SEQUENCING decision with a named prize, not a judgement that the axis is unwanted. It is not
reopened by noticing the performance argument again — the argument is already here, and the blocker is the
loops.

⚠ **This is not the Thunderbrd work.** The code is a later, separate contribution (its comments are French,
and the sentinel removed in #364 was attributed to another modder). Do not read `CvArmy` as the realization
of the `ARMYAI_` design — that design has no implementation here.

## The defect this page was written from

`AI_formArmies` treated `getCurrentID() == 8192` as an "uninitialised" sentinel. 8192 is `FLTA_MAX_BUCKETS`
and is the **correct** initial value, so the test matched a healthy container — and `init()` begins with
`uninit()`, which deletes every element. Combined with a `setCurrentID(0)` in `CvPlayer::baseInit` that
overwrote `init()`'s own seed, the counter reached the sentinel after a single `add()`, so the army list was
destroyed once the first army formed. Both writes also violated the id-format invariants `setCurrentID`
asserts. Fixed in #364.

⚑ Worth keeping as the shape rather than the incident: **a magic-number sentinel for "is this container
initialised" is answering a question the container already answers**, and it read as a healthy state.

## See also
- [unit-lifecycle.md](unit-lifecycle.md) — the unit and selection-group lifecycle underneath this layer.
- [engine.md](engine.md) — the `FFreeListTrashArray` id format and the save constraints.
