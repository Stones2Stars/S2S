# 9. The Firaxis reference contract (vanilla BTS source, `<BTS install>/CvGameCoreDLL/`)

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

The original Firaxis DLL — the code the closed EXE ships with and renders correctly against — defines the
contract. Traced in full (call census + verbatim bodies); the invariants, cited to vanilla source:

- **The DLL positions a unit node exactly twice in its life**: once via the `SetPosition` arm of the `setXY`
  called from `init` (the entity already exists — ctor-created — and `setup()` runs AFTER, `CvUnit.cpp:107-111`),
  and thereafter only via `QueueMove`+`ExecuteMove` pairs during actual movement. On a LOAD the DLL issues **no
  position at all**: the EXE calls the DllExport `CvPlayer::setupGraphical` → per-unit `setupGraphical()` →
  bare `setup()`, and **the engine's `setup()` reads the unit's coordinates itself and places the node**
  (`CvPlayer.cpp:738-759`; `CvUnit::read` is pure deserialization, `CvUnit.cpp:11254`).
- **`setupGraphical` is `setup()` + the intercept `airCircle` and NOTHING else** (`CvUnit.cpp:409-422`). No
  `ExecuteMove`, no position verb. `ExecuteMove` exists at exactly two sites in the whole vanilla tree:
  `groupMove` (flushing queued moves, `CvSelectionGroup.cpp:3092,3105`) and `updateCombat`
  (`ExecuteMove(0.5f,true)` before `AddMission`, `CvUnit.cpp:1375`). An empty-queue `ExecuteMove` at setup —
  the billw C2C refresh — is a call shape the reference does not contain.
- **A centre-unit change touches no entity, ever**: `setCenterUnit` = pointer + `updateMinimapColor` +
  `setFlagDirty` + `setInfoBarDirty` (`CvPlot.cpp:7773-7791`). The flag is the centre unit's derivative,
  rebuilt by destroy/`create(player)`/`setPlot(flag, plot)`/`updateUnitInfo` under the EXE-driven dirty sweep
  (`CvPlot.cpp:7650-7742`; `CvMap::updateFlagSymbols` has no in-DLL caller).
- **Never called by vanilla on a unit entity**: `addEntity` (declared, zero callers), `updatePosition` (used
  ONLY on feature/route/river symbols), `MoveTo`, `PlayAnimation`, `StopAnimation`, `setVisible` (one call,
  on a city). A fix reaching for any of these is outside the contract.
- **Selection makes zero entity calls** except `addUnit`'s `NotifyEntity(MISSION_MULTI_SELECT)` fan on the
  ownerless (NO_PLAYER) UI selection group (`CvSelectionGroup.cpp:3792-3847`); `selectUnit`/`selectGroup`/
  `cycleSelectionGroups` only mutate the selection list.
- **Vanilla `reloadEntity` exists** (destroy → create → `setupGraphical`, `CvUnit.cpp:68-82`) and is called
  ONLY for art rebuilds (warlord attach, `setLeaderUnitType`) — never from centre-unit changes; position
  survives a rebuild because the engine's `setup()` re-reads the unit's plot.
- ⚠ **The dummy-entity system (C2C) is structurally outside this contract**: it creates real entities after
  the two placement moments the contract provides (birth `setXY`; load-time pre-existing entities placed by
  the engine's init/setup). Every S2S "run in from world origin" observation traces to presentations of nodes
  introduced outside those two moments — see §7b for the measured behaviour.

