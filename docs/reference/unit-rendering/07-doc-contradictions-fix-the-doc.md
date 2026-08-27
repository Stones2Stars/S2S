# 7. Doc contradictions (fix-the-doc items)

> Part of the **[unit-rendering](../unit-rendering.md)** spec.

| Doc cite | Claim | Code cite | Truth |
|---|---|---|---|
| `docs/reference/memory-footprint.md:125-127` | a real entity only for ON-SCREEN units; counted under `[PERF/entity]` | `Engine/CvUnit.cpp:310-317`; `Engine/CvPlot.cpp:4948-4952`; `UI/CvGraphicsTrace.cpp:169` | The criterion is `isActiveVisible(false)` (fog), not screen; the count is the `[GFX] entity` line at level 4; no `[PERF/entity]` tag exists. |
| `Assets/XML/GlobalDefines.xml:62-69` (comment) | `MAX_PAGING_FRAME_TIME_MS` is unused | `UI/CvPlotPaging.cpp:242` | The mechanism is live under `PAGING_FRAME_TIME_MS`, which is not authored; the XML key is inert. |
| `docs/plans/parked/multimap-zone-rework.md:14, 49` | proactive eviction shipped via `PAGING_RESIDENT_SOFT_CAP`; zone paging can "reuse the shipped proactive eviction" | `UI/CvPlotPaging.cpp:256-263`; `Assets/XML/GlobalDefines.xml:47-48` | The branch exists but the authored cap (3,000,000) is compared to a PLOT count; it can never trigger (`docs/reference/memory-footprint.md` states the same). Eviction is `NeedToFreeMemory`-only. |
| `docs/plans/parked/turn-time-optimization.md:612` | `isActiveVisible` is a single count read | `Engine/CvPlot.cpp:4932-4944` | It also ORs `getStolenVisibilityCount`. |
| `docs/specs/json.md:1122-1125` | `getArtInfo(iIndex, eEra, eStyle)` resolves a civilization art-style override first | `Infos/CvUnitInfo.cpp:207-227` | `eStyle` is unused; the function walks era bands then falls back to the unit's art tag. |
| `Engine/CvGame.cpp:4741, 2439-2441` (comments) | `setFinalInitialized` fires for a NEW GAME ONLY and marks final init | `Engine/CvGame.cpp:4741-4746, 472, 2452-2454` | Its body only prints; `m_bFinalInitialized` is set by `onFinalInitialized` from the first `CvGame::update` on BOTH paths. |
| `Tools/CvHttpServer.cpp:8-9`; `AI/BetterBTSAI.cpp:52` (comments) | `/computed/perf` serves the memory gauge | `Tools/CvHttpServer.cpp:409-419` | No such route exists in the route table; `docs/spine.md:949-952` is the correct side. |

