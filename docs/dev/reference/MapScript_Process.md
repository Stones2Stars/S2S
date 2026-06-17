# MapScript Process — `C2C_World` (and friends)

This document describes the end-to-end lifecycle a Caveman2Cosmos mapscript
goes through, using `PrivateMaps/C2C_World.py` as the worked example. It also
catalogues the code outside the script file that the engine reaches into while
the script is running.

Other `C2C_*.py` scripts in `PrivateMaps/` follow the same protocol: they all
expose the same set of Python callback names, and the DLL drives them through
the same call sites. The differences between them are in *how* each callback
fills in plot / terrain / feature data, not in the contract.

---

## 1. Where mapscripts live

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

The DLL resolves the active mapscript through `CvMap::getMapScript()`
(`Sources/CvMap.cpp:1597`). It first checks `CIV4MapInfo` for a fixed map
script (used by WB-style scenarios in `PublicMaps/`); otherwise it falls back
to the script the player chose in the launcher
(`gDLL->getPythonIFace()->getMapScriptModule()`).

The chosen module is then driven through a sequence of named Python callbacks.
Each callback is invoked via `Cy::call_override` or `Cy::call_optional`. If
the script does not define a name, the DLL falls back to its own default
implementation.

---

## 2. The callback contract (engine → mapscript)

The table below lists every name `C2C_World.py` exposes, the C++ call site
that drives it, and what the engine expects back.

| Phase | Python callback | C++ call site | Purpose |
|-------|-----------------|---------------|---------|
| Option discovery | `isAdvancedMap` | `CyInfoInterface3` / launcher | Show in advanced list only? |
| Option discovery | `getNumCustomMapOptions` | launcher | How many dropdowns to render |
| Option discovery | `getNumHiddenCustomMapOptions` | launcher | Hidden (debug-only) options |
| Option discovery | `getCustomMapOptionName(i)` | launcher | Label of option `i` |
| Option discovery | `getNumCustomMapOptionValues(i)` | launcher | Number of choices for option `i` |
| Option discovery | `getCustomMapOptionDefault(i)` | launcher | Default selection |
| Option discovery | `getCustomMapOptionDescAt(i, sel)` | launcher | Label for each choice |
| Option discovery | `isRandomCustomMapOption(i)` | launcher | Can this option be set to *Random*? |
| Climate / sea-level | `isClimateMap`, `isSeaLevelMap` | launcher | Whether engine shows its own climate / sea-level pickers |
| Init | `beforeInit` | `CvMap::init` — `CvMap.cpp:91` | Earliest hook; runs *before* `CvMap` allocates plots |
| Sizing | `getGridSize(eWorldSize)` | `CvMap::init` — `CvMap.cpp:177` | Override map dimensions per world size |
| Sizing | `getTopLatitude` / `getBottomLatitude` | `CvMap::init` — `CvMap.cpp:206` | Override climate latitude band |
| Sizing | `getWrapX` / `getWrapY` | `CvMap::init` — `CvMap.cpp:235` | Cylindrical / toroidal / flat |
| Generation | `beforeGeneration` | `CvMapGenerator::generateRandomMap` — `CvMapGenerator.cpp:877` | Reseed RNG, build helper maps |
| Generation | `generatePlotTypes` | `CvMapGenerator.cpp:893` | Returns the `[ocean|land|hill|peak]` array |
| Generation | `generateTerrainTypes` | `CvMapGenerator.cpp:910` | Returns the terrain-id array |
| Generation | `addRivers` | `CvMapGenerator.cpp:227` | Lay rivers (script supplies them, DLL skips its default) |
| Generation | `addLakes` | `CvMapGenerator.cpp:203` | Lake placement (no-op here — lakes already placed in `generatePlotTypes`) |
| Generation | `addFeatures` | `CvMapGenerator.cpp:540` | Ice, forest, jungle, oasis, swamp, etc. |
| Generation | `addBonuses` | `CvMapGenerator.cpp:568` | Resource placement |
| Generation | `afterGeneration` | `CvMapGenerator.cpp:929` | Post-pass — Natural Wonders are placed here |
| Starts | `assignStartingPlots` | `CvGame.cpp:1250` | Flags plots as starts; DLL hands them to players |
| Normalize | `normalizeAddRiver` … `normalizeAddExtras` | `CvGame.cpp:2116`–`2151` | Eight post-start touch-ups (all no-ops in `C2C_World`) |

Anything `C2C_World` *doesn't* implement is filled in by the engine defaults
in `Assets/Python/CvMapGeneratorUtil.py`
(`generatePlotTypes` at line 172/444, `addFeatures` at 1204, `normalizeAddExtras`
at 1348, etc.).

---

## 3. End-to-end timeline (what actually runs)

Below is the order of operations starting from "player clicks Launch" through
"turn 1 begins", with `C2C_World.py` line refs for each step.

### 3.1 Launcher / option dialog

When the user reaches the map-options screen, the DLL calls the option-discovery
functions listed above on the chosen script (none of the heavy generators run
yet). `C2C_World.py` answers from its module-level `mo = MapOptions()` instance
(`C2C_World.py:4013`), which on import reads previously-saved selections from:

```
Mods/Stones2Stars/UserSettings/World_MapDefaults.cfg
```

via `pickle.load` (`MapOptions.__init__`, `C2C_World.py:3940`). The 8 options
declared in `optionList` are: Hills, Peaks, Landform, World Wrap, Start,
Rivers, Resources, Pangea Breaker.

### 3.2 `beforeInit` — `C2C_World.py:4151`

Engine call: `CvMap::init` (`CvMap.cpp:91`).

- Reads each selection through `CyGlobalContext().getMap().getCustomMapOption(i)`
  and prints the human-readable form (the long `if/elif` ladder).
- Calls `mo.saveMapOptionDefaults()` (`C2C_World.py:3969`) which writes the
  selections back to `World_MapDefaults.cfg` so they become next session's
  defaults.
- Constructs the singleton `mc = MapConstants()` and runs
  `mc.initInGameOptions()` (`C2C_World.py:250`) which translates the option
  selections into tuning variables: `HillPercent`, `PeakPercent`, landmass
  flags (`bDryland`, `bPangea`, `bEarthlike`, `bArchipelago`, `bWaterworld`),
  wrap flags (`bWrapX`, `bWrapY`), `bNewWorld`, `fRiverThreshold`,
  `fBonusMult`, `bPangeaBreaker`.

### 3.3 Sizing callbacks

Still inside `CvMap::init`, immediately after `beforeInit`:

1. `getGridSize(eWorldSize)` — `C2C_World.py:4258` — returns a
   `(width, height)` tuple. `C2C_World` uses a 1.5 aspect ratio
   (e.g. STANDARD = 24×16).
2. `getTopLatitude` / `getBottomLatitude` — `C2C_World.py:4276` — fixed at
   ±90.
3. `getWrapX` / `getWrapY` — `C2C_World.py:4280` — return the wrap flags
   stored on `mc`.

After this, the DLL allocates the `CvPlot` array.

### 3.4 `beforeGeneration` — `C2C_World.py:4286`

Engine call: `CvMapGenerator::generateRandomMap` (`CvMapGenerator.cpp:877`).

- Ensures `mc` exists (it normally does, from `beforeInit`).
- Calls `mc.AdaptToGameOptions()` (`C2C_World.py:391`) which records the
  final grid dimensions (`iWidth`, `iHeight`, `iArea`, `iWorldSize`,
  `iMaxLakeSize` from `CIV4WorldInfo.xml`).
- Calls `PySeed()` (`C2C_World.py:4698`). In multiplayer it seeds Python's
  `random` from the synchronized `MapRand`; in singleplayer it uses
  system-time entropy. Without this step, two clients in MP would diverge.

### 3.5 `generatePlotTypes` — `C2C_World.py:4297`

Engine call: `CvMapGenerator.cpp:893` via `call_override` — if the script
returns a list, the DLL uses it directly and skips its own default.

This is the heaviest single phase. It instantiates and runs, in order, the
following helper classes that all live inside `C2C_World.py`:

1. `pb = PangaeaBreaker()` — line 1831 — optional meteor-strike landmass
   splitter, gated by `mc.bPangeaBreaker`.
2. `em = ElevationMap()` (extends `FloatMap` + `SimplexNoise4D`) — lines
   1079 / 697 / 823 — builds the height field.
3. `cm = ClimateMap()` — line 1354 — runs `GenerateTemperatureMap()` then
   `GenerateRainfallMap()` (the latter is what makes rivers possible).
4. `tm = TerrainMap()` — line 1617 — `GeneratePlotMap()` then
   `GenerateTerrainMap()` classify each plot into the `mc.*` enum
   (`OCEAN`, `SEA`, `COAST`, `LAKE`, `LAND/HILL/PEAK`,
   `DESERT`/`PLAINS`/`GRASS`/`TUNDRA`/…).
5. `lm = LakeMap()` — line 2158 — carves lake depressions.
6. `rm = RiverMap()` — line 2661 — flow-accumulates rainfall and freezes
   river segments.
7. If `mc.bNewWorld`, `coMa.generateContinentMap()` — `coMa = ContinentMap()`
   created at module load (line 2155) — marks the Old World vs. New World
   continents for the `Start: Old World` option.
8. Finally, the routine collapses `tm.plotData` into the
   `[PLOT_OCEAN|PLOT_LAND|PLOT_HILLS|PLOT_PEAK]` list the DLL expects.

Every step is wrapped in `BugUtil.Timer` (from `Assets/Python/BUG/BugUtil.py`)
so the generation timings show up in `PythonDbg.log`.

### 3.6 `generateTerrainTypes` — `C2C_World.py:4361`

Engine call: `CvMapGenerator.cpp:910`.

Pure translation step. Walks `tm.terrData` and converts each `mc.*` constant
into the matching `TerrainTypes` id by name:

- `TERRAIN_OCEAN`, `TERRAIN_OCEAN_POLAR`, `TERRAIN_OCEAN_TROPICAL`
- `TERRAIN_TRENCH`, `TERRAIN_TRENCH_POLAR`, `TERRAIN_TRENCH_TROPICAL`
- `TERRAIN_SEA`, `TERRAIN_SEA_POLAR`, `TERRAIN_SEA_TROPICAL`,
  `TERRAIN_SEA_DEEP`, `TERRAIN_SEA_DEEP_POLAR`, `TERRAIN_SEA_DEEP_TROPICAL`
- `TERRAIN_COAST`, `TERRAIN_COAST_POLAR`, `TERRAIN_COAST_TROPICAL`
- `TERRAIN_LAKE_SHORE`, `TERRAIN_LAKE`
- `TERRAIN_DESERT`, `TERRAIN_SALT_FLATS`, `TERRAIN_DUNES`, `TERRAIN_SCRUB`,
  `TERRAIN_BADLAND`, `TERRAIN_JAGGED`, `TERRAIN_BARREN`, `TERRAIN_ROCKY`
- `TERRAIN_PLAINS`, `TERRAIN_GRASSLAND`, `TERRAIN_LUSH`, `TERRAIN_MUDDY`,
  `TERRAIN_MARSH`
- `TERRAIN_TUNDRA`, `TERRAIN_TAIGA`, `TERRAIN_PERMAFROST`, `TERRAIN_ICE`
- `TERRAIN_HILL`, `TERRAIN_PEAK`

All of these are defined in
`Assets/XML/Terrain/CIV4TerrainInfos.xml` — if a name there changes, this
function silently returns `-1` for that plot type and the engine will crash
later. The lookups go through `CyGlobalContext::getInfoTypeForString` which
walks the registered infos tables.

### 3.7 `addRivers` — `C2C_World.py:4476`

Engine call: `CvMapGenerator.cpp:227`.

Walks every plot and calls `placeRiversInPlot(x, y, i, S, N, W, E)`
(`C2C_World.py:4830`). That helper reads `rm.riverMap` (filled in 3.5) and
calls into the DLL's `CyPlot`:

- `plot.setWOfRiver(True, CARDINALDIRECTION_SOUTH|NORTH)`
- `plot.setNOfRiver(True, CARDINALDIRECTION_EAST|WEST)`

These setters mutate the actual plot data, which the renderer then picks up
through `CvPlot::isRiverSide`.

### 3.8 `addLakes` — `C2C_World.py:4488`

No-op. Lakes were already placed during `generatePlotTypes`. Returning
without doing anything also suppresses the DLL's default lake-spawning loop
(`CvMapGenerator.cpp:203`).

### 3.9 `addFeatures` — `C2C_World.py:4492`

Engine call: `CvMapGenerator.cpp:540`.

For every plot, this places one of:

- `FEATURE_ICE` — at the polar bands; chance decays from the edge inward.
- `FEATURE_FLOOD_PLAINS` — on a riverside desert.
- `FEATURE_OASIS` — desert tile surrounded by desert.
- `FEATURE_CACTUS` — desert near scrub rainfall.
- `FEATURE_SWAMP` / `FEATURE_PEAT_BOG` — wet, warm-enough land.
- `FEATURE_FOREST` (variant `FORESTLEAFY`/`FORESTEVERGREEN`/`FORESTSNOWY`)
  and `FEATURE_JUNGLE` — driven by `cm.RainfallMap` and `cm.TemperatureMap`.
- Any feature whose XML `<iAppearance>` (read via
  `getFeatureInfo(i).getAppearanceProbability()`) rolls in.
- `IMPROVEMENT_GOODY_ISLAND` — on small water tiles if the
  `GAMEOPTION_MAP_NO_GOODY_HUTS` game option is off.

Every name above resolves through `CyGlobalContext.getInfoTypeForString`
against the XML in `Assets/XML/Terrain/CIV4FeatureInfos.xml` and
`Assets/XML/Buildings/CIV4ImprovementInfos.xml`.

### 3.10 `addBonuses` — `C2C_World.py:4657`

Engine call: `CvMapGenerator.cpp:568`.

If `mc.fBonusMult > 0`, runs `bp.AddBonuses()` on the module-level
`bp = BonusPlacer()` (`C2C_World.py:3014`, instantiated at 3409). That class
uses `BonusArea` (3412), `AreaSuitability` (3420) and reads
`CIV4BonusInfos.xml` to pick valid plots for each bonus.

### 3.11 `afterGeneration` — `C2C_World.py:4665`

Engine call: `CvMapGenerator.cpp:929`.

Runs `NaturalWonders.NaturalWonders().placeNaturalWonders()` — implemented in
`Assets/Python/Platyping/NaturalWonders.py:8` (Platyping's natural-wonder
placement routine). This is the *only* file outside `C2C_World.py` that the
script imports for behaviour beyond logging.

### 3.12 `assignStartingPlots` — `C2C_World.py:4670`

Engine call: `CvGame.cpp:1250`.

- Runs `spf.SetStartingPlots()` on the module-level `spf = StartingPlotFinder()`
  (`C2C_World.py:3427`, instantiated at 3767). That class uses `StartingArea`
  (3769) and `StartPlot` (3913) to flag good plots via
  `plot.setStartingPlot(True)`.
- Then calls `CyGlobalContext().getGame().assignStartingPlots(False, True)`
  which lets the DLL hand the flagged plots to actual players.
- Finally drops the global helper references (`mc = em = cm = tm = lm = rm =
  pb = None`) so Python can garbage-collect ~100 MB of intermediate float
  arrays.

### 3.13 Normalize passes — `C2C_World.py:4683`–`4692`

Engine calls: `CvGame.cpp:2116`–`2151`.

The DLL invokes eight `normalizeAdd*`/`normalizeRemove*` hooks. `C2C_World`
overrides every one with `return`, which means the script tells the DLL "I'm
done, don't run your normalize defaults either." This is intentional —
the script's own start-finder already places balanced starts, so the DLL's
fix-up logic (which expects vanilla terrain types) would degrade the result.

---

## 4. Files and modules the script touches

### 4.1 Python modules the script imports

- `CvPythonExtensions` — the C++→Python binding, exposes `CyGlobalContext`,
  `CyGame`, `CyMap`, `CyPlot`, `PlotTypes`, `TerrainTypes`, `FeatureTypes`,
  `CardinalDirectionTypes`, `WorldSizeTypes`, `GameOptionTypes`.
- `array.array` — compact typed arrays for the plot / terrain id buffers.
- `random` — Python RNG; deterministically seeded by `PySeed`.
- `math`, `os`, `cPickle`, `_winreg` — standard library.
- `BugUtil` (`Assets/Python/BUG/BugUtil.py`) — `BugUtil.Timer` for
  per-phase generation timings.
- `NaturalWonders` (`Assets/Python/Platyping/NaturalWonders.py`) — natural
  wonder placement, called from `afterGeneration`.

### 4.2 XML the script reads through `getInfoTypeForString`

If you rename or remove any of these entries the script will fail silently
(returning `-1` for the lookup) and the map will be corrupt:

| Domain | XML file | Tags consumed |
|--------|----------|---------------|
| Terrains | `Assets/XML/Terrain/CIV4TerrainInfos.xml` | All `TERRAIN_*` ids enumerated in §3.6 |
| Features | `Assets/XML/Terrain/CIV4FeatureInfos.xml` | `FEATURE_ICE`, `FEATURE_FOREST`, `FEATURE_JUNGLE`, `FEATURE_FLOOD_PLAINS`, `FEATURE_OASIS`, `FEATURE_CACTUS`, `FEATURE_PEAT_BOG`, `FEATURE_SWAMP`, plus every feature with `<iAppearance> > -1` |
| Improvements | `Assets/XML/Buildings/CIV4ImprovementInfos.xml` | `IMPROVEMENT_GOODY_ISLAND` |
| Bonuses | `Assets/XML/Terrain/CIV4BonusInfos.xml` | Indirectly via `BonusPlacer` |
| World sizes | `Assets/XML/GameInfo/CIV4WorldInfo.xml` | `OceanMinAreaSize` (used as `iMaxLakeSize`) |
| Game options | game core | `GAMEOPTION_MAP_NO_GOODY_HUTS` |

### 4.3 User-data files the script reads/writes

- `Mods/Stones2Stars/UserSettings/World_MapDefaults.cfg` — pickled
  list of last-chosen option indices. Created by `MapOptions.saveMapOptionDefaults`
  during `beforeInit`, read by `MapOptions.__init__` on module import.

### 4.4 DLL call sites that drive the script

| File | Lines | Callback(s) it fires |
|------|-------|----------------------|
| `Sources/CvMap.cpp` | 91 | `beforeInit` |
| `Sources/CvMap.cpp` | 177 | `getGridSize` |
| `Sources/CvMap.cpp` | 206 | `getTopLatitude`, `getBottomLatitude` |
| `Sources/CvMap.cpp` | 235 | `getWrapX`, `getWrapY` |
| `Sources/CvMapGenerator.cpp` | 203 | `addLakes` |
| `Sources/CvMapGenerator.cpp` | 227 | `addRivers` |
| `Sources/CvMapGenerator.cpp` | 540 | `addFeatures` |
| `Sources/CvMapGenerator.cpp` | 568 | `addBonuses` |
| `Sources/CvMapGenerator.cpp` | 877 | `beforeGeneration` |
| `Sources/CvMapGenerator.cpp` | 893 | `generatePlotTypes` |
| `Sources/CvMapGenerator.cpp` | 910 | `generateTerrainTypes` |
| `Sources/CvMapGenerator.cpp` | 929 | `afterGeneration` |
| `Sources/CvGame.cpp` | 1250 | `assignStartingPlots` |
| `Sources/CvGame.cpp` | 2116–2151 | the eight `normalize*` hooks |

Option-discovery callbacks (`getCustomMapOption*`, `isAdvancedMap`,
`isClimateMap`, `isSeaLevelMap`, `isRandomCustomMapOption`,
`getNumCustomMapOptionValues`, `getNumHiddenCustomMapOptions`) are fired from
the launcher / `CyGameInterface` glue when the player is still on the map-setup
screen.

---

## 5. Module-level state and lifetime

The script keeps a small set of global singletons. Knowing their lifetime
matters because each map generation reuses the *module*, not a fresh import:

| Global | Where created | Where cleared |
|--------|---------------|---------------|
| `mo` (MapOptions) | module load, line 4013 | never (kept across regens) |
| `mc` (MapConstants) | `beforeInit` line 4254 | end of `assignStartingPlots`, line 4679 |
| `coMa` (ContinentMap) | module load, line 2155 | never |
| `bp` (BonusPlacer) | module load, line 3409 | never |
| `spf` (StartingPlotFinder) | module load, line 3767 | never |
| `em`, `cm`, `tm`, `lm`, `rm`, `pb` | `generatePlotTypes` line 4299 | end of `assignStartingPlots`, line 4679 |

The deliberate teardown at the end of `assignStartingPlots` is what keeps the
process memory footprint from growing across "Regenerate Map" presses.

---

## 6. Adapting this document to other `C2C_*` mapscripts

The DLL contract in §2 is the same for every mapscript in `PrivateMaps/`. To
document a different script, only §3 and §5 need rewriting — replace the
helper-class catalogue and the timeline with whatever that script's
`generatePlotTypes` and friends actually do. The XML and DLL-side surface
in §4 is shared.
