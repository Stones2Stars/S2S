# MapScript process — the Python map-generation lifecycle (`C2C_World` and friends)

> **Status:** reference   ·   **Verified against:** `PrivateMaps/C2C_World.py`, `Sources/Engine/CvMap.cpp`, `Sources/Infrastructure/CvMapGenerator.cpp`, `Sources/Engine/CvGame.cpp`, `Assets/Python/CvMapGeneratorUtil.py` — DLL homes re-grounded 2026-06-20; the `C2C_World.py` line refs are the original authoring's and were NOT re-walked this pass.
> **Grounding:** the callback contract + call sites were read from the DLL drivers; the per-phase timeline from `C2C_World.py`. The `Cv*` engine files moved since the original authoring — `CvMap.cpp` and `CvGame.cpp` are now under `Sources/Engine/`, **`CvMapGenerator.cpp` under `Sources/Infrastructure/`**. Line numbers (both the `.py` refs and the DLL refs) **drift** — confirm the named callback/call site, not the integer.
>
> **BLUF.** A C2C mapscript is a Python module in `PrivateMaps/` that the DLL drives through a fixed sequence of **named callbacks**. The DLL resolves the active script via `CvMap::getMapScript()`, then fires option-discovery callbacks (launcher), then init/sizing (`CvMap::init`), then the heavy generation phases (`CvMapGenerator::generateRandomMap` → `generatePlotTypes`/`generateTerrainTypes`/`addRivers`/`addFeatures`/`addBonuses`/`afterGeneration`), then starts + eight normalize hooks (`CvGame`). Each callback is invoked via `Cy::call_override`/`call_optional`; **anything the script doesn't define falls back to the DLL/`CvMapGeneratorUtil.py` default.** Every `C2C_*.py` exposes the **same callback names** through the **same call sites** — only *how* each fills plot/terrain/feature data differs, not the contract.

> **Line numbers drift.** The `C2C_World.py:NNNN` refs below are "the function around this line"; confirm the callback name, not the integer. They were not re-verified this pass.

---

## 1. Where mapscripts live & how the active one is resolved

```
PrivateMaps/
    C2C_World.py            ← worked example below
    C2C_World_Remixed.py
    C2C_Pangaea.py
    C2C_Tectonics.py
    C2C_PerfectWorld2f.py
    C2C_PerfectMongoose_v310.py
    ...
```

The DLL resolves the active mapscript through `CvMap::getMapScript()` (`Sources/Engine/CvMap.cpp`). It first checks `CIV4MapInfo` for a fixed map script (used by WB-style scenarios in `PublicMaps/`); otherwise it falls back to the script the player chose in the launcher (`gDLL->getPythonIFace()->getMapScriptModule()`). The chosen module is then driven through the named callbacks below; if the script does not define a name, the DLL falls back to its own default implementation.

---

## 2. The callback contract (engine → mapscript)

Every name `C2C_World.py` exposes, the C++ call site that drives it, and what the engine expects back. **`CvMapGenerator` is under `Sources/Infrastructure/`** (the one moved home — the others are `Sources/Engine/`):

| Phase | Python callback | C++ call site | Purpose |
|-------|-----------------|---------------|---------|
| Option discovery | `isAdvancedMap` | launcher / `CyInfoInterface3` | Show in advanced list only? |
| Option discovery | `getNumCustomMapOptions` | launcher | How many dropdowns to render |
| Option discovery | `getNumHiddenCustomMapOptions` | launcher | Hidden (debug-only) options |
| Option discovery | `getCustomMapOptionName(i)` | launcher | Label of option `i` |
| Option discovery | `getNumCustomMapOptionValues(i)` | launcher | Number of choices for option `i` |
| Option discovery | `getCustomMapOptionDefault(i)` | launcher | Default selection |
| Option discovery | `getCustomMapOptionDescAt(i, sel)` | launcher | Label for each choice |
| Option discovery | `isRandomCustomMapOption(i)` | launcher | Can this option be set to *Random*? |
| Climate / sea-level | `isClimateMap`, `isSeaLevelMap` | launcher | Whether engine shows its own climate / sea-level pickers |
| Init | `beforeInit` | `CvMap::init` — `Engine/CvMap.cpp:91` | Earliest hook; runs *before* `CvMap` allocates plots |
| Sizing | `getGridSize(eWorldSize)` | `CvMap::init` — `Engine/CvMap.cpp:177` | Override map dimensions per world size |
| Sizing | `getTopLatitude` / `getBottomLatitude` | `CvMap::init` — `Engine/CvMap.cpp:206` | Override climate latitude band |
| Sizing | `getWrapX` / `getWrapY` | `CvMap::init` — `Engine/CvMap.cpp:235` | Cylindrical / toroidal / flat |
| Generation | `beforeGeneration` | `CvMapGenerator::generateRandomMap` — `Infrastructure/CvMapGenerator.cpp:877` | Reseed RNG, build helper maps |
| Generation | `generatePlotTypes` | `Infrastructure/CvMapGenerator.cpp:893` | Returns the `[ocean/land/hill/peak]` array |
| Generation | `generateTerrainTypes` | `Infrastructure/CvMapGenerator.cpp:910` | Returns the terrain-id array |
| Generation | `addRivers` | `Infrastructure/CvMapGenerator.cpp:227` | Lay rivers (script supplies them, DLL skips its default) |
| Generation | `addLakes` | `Infrastructure/CvMapGenerator.cpp:203` | Lake placement (no-op here — lakes already placed in `generatePlotTypes`) |
| Generation | `addFeatures` | `Infrastructure/CvMapGenerator.cpp:540` | Ice, forest, jungle, oasis, swamp, etc. |
| Generation | `addBonuses` | `Infrastructure/CvMapGenerator.cpp:568` | Resource placement |
| Generation | `afterGeneration` | `Infrastructure/CvMapGenerator.cpp:929` | Post-pass — Natural Wonders are placed here |
| Starts | `assignStartingPlots` | `CvGame.cpp:1250` (`Engine/`) | Flags plots as starts; DLL hands them to players |
| Normalize | `normalizeAddRiver` … `normalizeAddExtras` | `CvGame.cpp:2116`–`2151` (`Engine/`) | Eight post-start touch-ups (all no-ops in `C2C_World`) |

Anything `C2C_World` *doesn't* implement is filled in by the engine defaults in `Assets/Python/CvMapGeneratorUtil.py` (`generatePlotTypes`, `addFeatures`, `normalizeAddExtras`, …).

---

## 3. End-to-end timeline (what actually runs)

Order of operations from "player clicks Launch" through "turn 1 begins", with `C2C_World.py` line refs (unverified this pass — anchor on the callback names).

### 3.1 Launcher / option dialog
At the map-options screen the DLL calls the option-discovery functions above on the chosen script (no heavy generators run yet). `C2C_World.py` answers from its module-level `mo = MapOptions()` instance (`:4013`), which on import reads previously-saved selections from `Mods/Stones2Stars/UserSettings/World_MapDefaults.cfg` via `pickle.load` (`MapOptions.__init__`, `:3940`). The 8 options in `optionList`: Hills, Peaks, Landform, World Wrap, Start, Rivers, Resources, Pangea Breaker.

### 3.2 `beforeInit` (`:4151`) — engine call `CvMap::init`
- Reads each selection via `getMap().getCustomMapOption(i)` and prints the human-readable form (the long `if/elif` ladder).
- Calls `mo.saveMapOptionDefaults()` (`:3969`) which writes the selections back to `World_MapDefaults.cfg` as next session's defaults.
- Constructs the singleton `mc = MapConstants()` and runs `mc.initInGameOptions()` (`:250`), translating the options into tuning variables: `HillPercent`, `PeakPercent`, landmass flags (`bDryland`, `bPangea`, `bEarthlike`, `bArchipelago`, `bWaterworld`), wrap flags (`bWrapX`, `bWrapY`), `bNewWorld`, `fRiverThreshold`, `fBonusMult`, `bPangeaBreaker`.

### 3.3 Sizing callbacks (still inside `CvMap::init`)
1. `getGridSize(eWorldSize)` (`:4258`) — returns a `(width, height)` tuple, 1.5 aspect ratio (e.g. STANDARD = 24×16).
2. `getTopLatitude` / `getBottomLatitude` (`:4276`) — fixed at ±90.
3. `getWrapX` / `getWrapY` (`:4280`) — return the wrap flags on `mc`.

After this the DLL allocates the `CvPlot` array.

### 3.4 `beforeGeneration` (`:4286`) — engine call `CvMapGenerator::generateRandomMap`
- Ensures `mc` exists (normally from `beforeInit`).
- Calls `mc.AdaptToGameOptions()` (`:391`) which records the final grid dimensions (`iWidth`, `iHeight`, `iArea`, `iWorldSize`, `iMaxLakeSize` from `CIV4WorldInfo.xml`).
- Calls `PySeed()` (`:4698`). In MP it seeds Python's `random` from the synchronized `MapRand`; in SP it uses system-time entropy. Without this, two MP clients diverge.

### 3.5 `generatePlotTypes` (`:4297`) — engine call `CvMapGenerator.cpp:893` via `call_override`
The heaviest single phase. If the script returns a list the DLL uses it directly and skips its own default. It instantiates and runs, in order, helper classes that all live inside `C2C_World.py`:
1. `pb = PangaeaBreaker()` (`:1831`) — optional meteor-strike landmass splitter, gated by `mc.bPangeaBreaker`.
2. `em = ElevationMap()` (extends `FloatMap` + `SimplexNoise4D`; `:1079`/`:697`/`:823`) — height field.
3. `cm = ClimateMap()` (`:1354`) — `GenerateTemperatureMap()` then `GenerateRainfallMap()` (the latter is what makes rivers possible).
4. `tm = TerrainMap()` (`:1617`) — `GeneratePlotMap()` then `GenerateTerrainMap()` classify each plot into the `mc.*` enum (`OCEAN`, `SEA`, `COAST`, `LAKE`, `LAND/HILL/PEAK`, `DESERT`/`PLAINS`/`GRASS`/`TUNDRA`/…).
5. `lm = LakeMap()` (`:2158`) — carves lake depressions.
6. `rm = RiverMap()` (`:2661`) — flow-accumulates rainfall and freezes river segments.
7. If `mc.bNewWorld`, `coMa.generateContinentMap()` (`coMa = ContinentMap()` created at module load, `:2155`) — marks Old World vs. New World for the `Start: Old World` option.
8. Finally collapses `tm.plotData` into the `[PLOT_OCEAN|PLOT_LAND|PLOT_HILLS|PLOT_PEAK]` list the DLL expects.

Every step is wrapped in `BugUtil.Timer` (`Assets/Python/BUG/BugUtil.py`) so the timings show up in `PythonDbg.log`.

### 3.6 `generateTerrainTypes` (`:4361`) — engine call `CvMapGenerator.cpp:910`
Pure translation. Walks `tm.terrData`, converting each `mc.*` constant into the matching `TerrainTypes` id by name (`TERRAIN_OCEAN[_POLAR/_TROPICAL]`, `TERRAIN_TRENCH*`, `TERRAIN_SEA*`/`_DEEP*`, `TERRAIN_COAST*`, `TERRAIN_LAKE_SHORE`/`TERRAIN_LAKE`, the desert family `DESERT`/`SALT_FLATS`/`DUNES`/`SCRUB`/`BADLAND`/`JAGGED`/`BARREN`/`ROCKY`, `PLAINS`/`GRASSLAND`/`LUSH`/`MUDDY`/`MARSH`, the cold family `TUNDRA`/`TAIGA`/`PERMAFROST`/`ICE`, `TERRAIN_HILL`/`TERRAIN_PEAK`). **All defined in `Assets/XML/Terrain/CIV4TerrainInfos.xml` — if a name there changes, this returns `-1` for that plot and the engine crashes later.** Lookups go through `CyGlobalContext::getInfoTypeForString`.

### 3.7 `addRivers` (`:4476`) — engine call `CvMapGenerator.cpp:227`
Walks every plot and calls `placeRiversInPlot(x, y, i, S, N, W, E)` (`:4830`), which reads `rm.riverMap` (filled in 3.5) and calls into `CyPlot`: `plot.setWOfRiver(True, CARDINALDIRECTION_SOUTH|NORTH)` / `plot.setNOfRiver(True, CARDINALDIRECTION_EAST|WEST)`. These mutate plot data the renderer picks up through `CvPlot::isRiverSide`.

### 3.8 `addLakes` (`:4488`) — no-op
Lakes were already placed during `generatePlotTypes`. Returning without doing anything also suppresses the DLL's default lake-spawning loop (`CvMapGenerator.cpp:203`).

### 3.9 `addFeatures` (`:4492`) — engine call `CvMapGenerator.cpp:540`
For every plot, places one of: `FEATURE_ICE` (polar bands, chance decays inward); `FEATURE_FLOOD_PLAINS` (riverside desert); `FEATURE_OASIS` (desert surrounded by desert); `FEATURE_CACTUS` (desert near scrub rainfall); `FEATURE_SWAMP`/`FEATURE_PEAT_BOG` (wet, warm-enough land); `FEATURE_FOREST` (variant `FORESTLEAFY`/`FORESTEVERGREEN`/`FORESTSNOWY`) and `FEATURE_JUNGLE` (driven by `cm.RainfallMap`/`cm.TemperatureMap`); any feature whose XML `<iAppearance>` (via `getFeatureInfo(i).getAppearanceProbability()`) rolls in; `IMPROVEMENT_GOODY_ISLAND` on small water tiles if `GAMEOPTION_MAP_NO_GOODY_HUTS` is off. Names resolve through `getInfoTypeForString` against `CIV4FeatureInfos.xml` / `CIV4ImprovementInfos.xml`.

### 3.10 `addBonuses` (`:4657`) — engine call `CvMapGenerator.cpp:568`
If `mc.fBonusMult > 0`, runs `bp.AddBonuses()` on the module-level `bp = BonusPlacer()` (`:3014`, instantiated `:3409`). That class uses `BonusArea` (`:3412`), `AreaSuitability` (`:3420`) and reads `CIV4BonusInfos.xml` to pick valid plots.

### 3.11 `afterGeneration` (`:4665`) — engine call `CvMapGenerator.cpp:929`
Runs `NaturalWonders.NaturalWonders().placeNaturalWonders()` — implemented in `Assets/Python/Platyping/NaturalWonders.py:8`. This is the *only* file outside `C2C_World.py` the script imports for behaviour beyond logging.

### 3.12 `assignStartingPlots` (`:4670`) — engine call `CvGame.cpp:1250`
- Runs `spf.SetStartingPlots()` on the module-level `spf = StartingPlotFinder()` (`:3427`, instantiated `:3767`), which uses `StartingArea` (`:3769`) and `StartPlot` (`:3913`) to flag good plots via `plot.setStartingPlot(True)`.
- Then calls `getGame().assignStartingPlots(False, True)` — the DLL hands flagged plots to actual players.
- Finally drops the global helper references (`mc = em = cm = tm = lm = rm = pb = None`) so Python can GC ~100 MB of intermediate float arrays.

### 3.13 Normalize passes (`:4683`–`:4692`) — engine calls `CvGame.cpp:2116`–`2151`
The DLL invokes eight `normalizeAdd*`/`normalizeRemove*` hooks. `C2C_World` overrides every one with `return`, telling the DLL "I'm done, don't run your normalize defaults either." Intentional — the script's own start-finder already places balanced starts, so the DLL's fix-up logic (which expects vanilla terrain types) would degrade the result.

---

## 4. Files and modules the script touches

### 4.1 Python modules imported
- `CvPythonExtensions` — the C++→Python binding (`CyGlobalContext`, `CyGame`, `CyMap`, `CyPlot`, `PlotTypes`, `TerrainTypes`, `FeatureTypes`, `CardinalDirectionTypes`, `WorldSizeTypes`, `GameOptionTypes`).
- `array.array` — compact typed arrays for the plot/terrain id buffers.
- `random` — Python RNG; deterministically seeded by `PySeed`.
- `math`, `os`, `cPickle`, `_winreg` — standard library.
- `BugUtil` (`Assets/Python/BUG/BugUtil.py`) — `BugUtil.Timer` for per-phase timings.
- `NaturalWonders` (`Assets/Python/Platyping/NaturalWonders.py`) — natural-wonder placement, called from `afterGeneration`.

### 4.2 XML read through `getInfoTypeForString`
Rename or remove any of these and the script fails silently (returns `-1`, corrupt map):

| Domain | XML file | Tags consumed |
|--------|----------|---------------|
| Terrains | `Assets/XML/Terrain/CIV4TerrainInfos.xml` | All `TERRAIN_*` ids in §3.6 |
| Features | `Assets/XML/Terrain/CIV4FeatureInfos.xml` | `FEATURE_ICE`, `FEATURE_FOREST`, `FEATURE_JUNGLE`, `FEATURE_FLOOD_PLAINS`, `FEATURE_OASIS`, `FEATURE_CACTUS`, `FEATURE_PEAT_BOG`, `FEATURE_SWAMP`, + every feature with `<iAppearance> > -1` |
| Improvements | `Assets/XML/Buildings/CIV4ImprovementInfos.xml` | `IMPROVEMENT_GOODY_ISLAND` |
| Bonuses | `Assets/XML/Terrain/CIV4BonusInfos.xml` | Indirectly via `BonusPlacer` |
| World sizes | `Assets/XML/GameInfo/CIV4WorldInfo.xml` | `OceanMinAreaSize` (used as `iMaxLakeSize`) |
| Game options | game core | `GAMEOPTION_MAP_NO_GOODY_HUTS` |

### 4.3 User-data files read/written
- `Mods/Stones2Stars/UserSettings/World_MapDefaults.cfg` — pickled list of last-chosen option indices. Created by `MapOptions.saveMapOptionDefaults` during `beforeInit`, read by `MapOptions.__init__` on module import.

### 4.4 DLL call sites that drive the script
`CvMapGenerator` lives in `Sources/Infrastructure/`; `CvMap` and `CvGame` in `Sources/Engine/`.

| File | Lines | Callback(s) fired |
|------|-------|-------------------|
| `Engine/CvMap.cpp` | 91 | `beforeInit` |
| `Engine/CvMap.cpp` | 177 | `getGridSize` |
| `Engine/CvMap.cpp` | 206 | `getTopLatitude`, `getBottomLatitude` |
| `Engine/CvMap.cpp` | 235 | `getWrapX`, `getWrapY` |
| `Infrastructure/CvMapGenerator.cpp` | 203 | `addLakes` |
| `Infrastructure/CvMapGenerator.cpp` | 227 | `addRivers` |
| `Infrastructure/CvMapGenerator.cpp` | 540 | `addFeatures` |
| `Infrastructure/CvMapGenerator.cpp` | 568 | `addBonuses` |
| `Infrastructure/CvMapGenerator.cpp` | 877 | `beforeGeneration` |
| `Infrastructure/CvMapGenerator.cpp` | 893 | `generatePlotTypes` |
| `Infrastructure/CvMapGenerator.cpp` | 910 | `generateTerrainTypes` |
| `Infrastructure/CvMapGenerator.cpp` | 929 | `afterGeneration` |
| `Engine/CvGame.cpp` | 1250 | `assignStartingPlots` |
| `Engine/CvGame.cpp` | 2116–2151 | the eight `normalize*` hooks |

Option-discovery callbacks (`getCustomMapOption*`, `isAdvancedMap`, `isClimateMap`, `isSeaLevelMap`, `isRandomCustomMapOption`, `getNumCustomMapOptionValues`, `getNumHiddenCustomMapOptions`) fire from the launcher / `CyGameInterface` glue while the player is on the map-setup screen.

---

## 5. Module-level state and lifetime

The script keeps a small set of global singletons. Lifetime matters because each map generation reuses the *module*, not a fresh import:

| Global | Where created | Where cleared |
|--------|---------------|---------------|
| `mo` (MapOptions) | module load, `:4013` | never (kept across regens) |
| `mc` (MapConstants) | `beforeInit`, `:4254` | end of `assignStartingPlots`, `:4679` |
| `coMa` (ContinentMap) | module load, `:2155` | never |
| `bp` (BonusPlacer) | module load, `:3409` | never |
| `spf` (StartingPlotFinder) | module load, `:3767` | never |
| `em`, `cm`, `tm`, `lm`, `rm`, `pb` | `generatePlotTypes`, `:4299` | end of `assignStartingPlots`, `:4679` |

The deliberate teardown at the end of `assignStartingPlots` is what keeps the process memory footprint from growing across "Regenerate Map" presses.

---

## 6. Adapting this to other `C2C_*` mapscripts

The DLL contract in §2 is the same for every mapscript in `PrivateMaps/`. To document a different script, only §3 and §5 need rewriting — replace the helper-class catalogue and the timeline with whatever that script's `generatePlotTypes` and friends actually do. The XML and DLL-side surface in §4 is shared.

## See also

- [`../../README.md`](../../README.md) — the docs2 comprehension map; this is its map-generation entry.
- [`declarative-info-loading.md`](declarative-info-loading.md) — how the `CIV4*Infos.xml` files §4.2 reads through `getInfoTypeForString` are themselves loaded into the engine's info tables.
- [`../../architecture/decisions.md`](../../architecture/decisions.md) — the rulings ledger (no map-gen-specific ruling today; linked for orientation).
