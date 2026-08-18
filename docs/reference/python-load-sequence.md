# The Python boundary — who publishes it, and the load sequence both sides run

> **How the C++/Python boundary is stood up today**, in execution order, on both sides. The DESIGN of the read
> boundary (what the library owes, what it does not own, the directional cut) is
> [patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md); this page is the MECHANISM and the
> ORDER — what runs, when, and what each step depends on having already run.

## ⛔ `CvPythonExtensions` HAS TWO PRODUCERS — the DLL is only half of it

The module every script star-imports is populated from **two independent sources**, and mistaking one for the
whole is the error that makes an intact surface look purged:

| producer | publishes | how to confirm |
|---|---|---|
| **`CvGameCoreDLL.dll`** (ours) | the `Cy*` classes reached from `DLLPublishToPython` (`Infrastructure/CvDLLPython.cpp`), the engine enum types, the container/iterator/debug helpers | read the composition root |
| **`Civ4BeyondSword.exe`** (closed) | the UI/host surface — `CyInterface`, `CyTranslator`, `CyEngine`, `CyUserProfile`, `CyGInterfaceScreen`, `CyPopup*`, `CyPythonMgr`, `CyFractal`, `CySign`, `CyCamera`, `CyAudioGame`, `CyStatistics`, `CyVariableSystem`, `CyUnitEntity`, `NiTextOut`, `InputTypes` + its `KB_*` members | the names appear as strings in the EXE image and in **no** repo file; `git log --all` finds them never |

⚑ **Consequence, and it is load-bearing in both directions.** A name we did not publish is not necessarily a
name that is missing — `CyTranslator().getText(...)` and `CyPythonMgr().allowDefaultImpl()` are the EXE's and
were never ours to cut, so TXT resolution and the map-script default-impl protocol are untouched by any binding
work here. Conversely, a name the EXE does **not** publish cannot be rescued by leaving Python alone: if a
script constructs it and we removed it, only we can put it back.

⛔ **Verify which producer owns a name before concluding anything about it.** The mechanical check is the one
above: absent from `Sources/` **and** absent from the whole git history ⇒ it is the EXE's.

## The C++ side — ordered

The EXE drives this; **no in-tree caller exists for any `DllExport` entry point**, so the ORDER BETWEEN the
exported calls is the EXE's and is not readable from this repo. What is pinned below is pinned by code or by a
comment that states the dependency.

1. **`DllMain` / `DLL_PROCESS_ATTACH`** (`CvGameCoreDLL.cpp`) — module handle, timer resolution, mod dir. The
   dev-only FPK/boot-check hooks can `return FALSE` here, which fails the DLL load outright.
2. **`cvInternalGlobals::init()`** (`Defines/CvGlobals.cpp`) — asserts `gDLL` is already set by the EXE, then
   builds the singletons: the variable system, the RNGs, `CvInitCore`, the FAStar finders, **`CvGameAI`**, the
   `CvMap`s, the player/team statics, `CyGlobalContext::initStatics()`.
3. **`SetGlobalDefines()`** — reads `GlobalDefines.xml` and its siblings + the modular overrides, and **ends in
   `GC.cacheGlobals()`**. Pinned to run BEFORE premenu (a comment at the premenu read site says so).
4. **`LoadBasicInfos()`** — the XML-only bootstrap infos plus the internal `register*` tables.
5. **`LoadPreMenuGlobals()`** — opens with **`spineRegisterConsumers()`** (moved here deliberately: in the
   `CvGame` ctor every load-time spine event went nowhere), then the `HTTP_SERVER_FROM_MENU` gate, then ~100
   categories in a fixed order — most on the **JSON** path, the rest legacy XML — and closes with
   `linkAllInfos()`, the event-only XML pass 3, and **`loadJson(JSON_LOAD_PREMENU)`**.
6. **The menu.** The split IS the boundary: premenu is "needed before the main menus", postmenu is "loaded as a
   second stage, when the game is launched". Premenu infos must be MAPPED before the menu — deferring the map
   leaves `InfoRepo` empty and `getType()` returns NULL at menu time.
7. **`LoadPostMenuGlobals()`** — the graphics/throne-room/interface-mode categories and the late types
   (processes, votes, espionage missions, spawns), then `cacheInfoTypes()`, then
   **`loadJson(JSON_LOAD_POSTMENU)`** — which completes every cross-category FK edge and **frees the parse
   store** — then **`buildLoadTimeIndexes()`**, which must follow the postmenu pass or the re-map overwrites it.
8. **`DLLPublishToPython()`** (`Infrastructure/CvDLLPython.cpp`) — the composition root. Publishes the int→enum
   coercions, the vector/`IDValueMap`/iterator interfaces, the debug helper, then the surfaces: **`CyEnums`
   first** (the vocabulary is a prerequisite — a script cannot name a slot until the enum types exist), then the
   read library, the kept TXT/ART boundaries, the config context, and the handles.
   ⚠ **Its position relative to steps 1–7 is the EXE's and is UNKNOWN from this tree.**
9. **Game start** — `CvGame::init` (new) or `CvGame::read` (load). `CvGame::read` emits
   **`GAME_LOAD_STARTED`** as the earliest DLL load hook; the per-object `read()`s emit inside that bracket;
   `onFinalInitialized` closes it with **`GAME_LOAD_FINISHED`**.
10. **Consumer registration order is a CONTRACT** (dispatch follows registration): file log → `/events` stream →
    **contexts → enabler → modifier → triggers**. The contexts build their stores on `GAME_LOAD_FINISHED`; the
    enabler's load-end gate pass evaluates THROUGH those stores; the modifier drains against the same; triggers
    read both machines' output and APPLY, so they go last.

## The Python side — ordered

1. **`CvAppInterface`** is entered first by the EXE; its `init()` redirects `sys.stderr`/`stdout`/`excepthook`
   into `CvUtil`, which is why a later failure is reportable at all.
2. **`CvEventInterface` triggers the whole tree.** The trigger is NOT a top-level import — module scope calls
   `getEventManager(True)`, whose body imports `BugEventManager`. That cascade is the engine's first real
   contact with the Python tree and pulls in a large transitive closure: the BUG core, the main interface, the
   options/replay/overlay screens, the advisor and utility modules, and — via `CvEventManager`'s own
   module-scope `import WBPlayerScreen` / `import WBPlotScreen` — **the entire WorldBuilder screen tree**.
   ⚑ Most modules on that closure construct their engine globals at MODULE SCOPE
   (`GC = CyGlobalContext()`, `STATE = CyState()`, `ENABLER = CyEnabler()`, `ENUMS = CyEnums()`, and the
   EXE-side `CyTranslator()` / `CyInterface()`), so **every name they construct must be published before the
   first import**, and any module-scope engine CALL must already be served.
3. **`Init`** — the DLL's first event. Every event, without exception, is delivered as
   `CvEventInterface.onEvent` with the event name as an argument; Python dispatches it from
   `CvEventManager.EventHandlerMap` (BUG overrides four entries). `onInit` is where **`SystemPaths.init()`**
   runs — and `SystemPaths.modDir` is what the whole BUG config layer resolves paths against.
4. **`initBUG`** (`CvAppInterface`) — parses `Assets/Config/init.xml`, loads its mod configs, and binds every
   handler by STRING through `BugUtil.lookupModule` (`__import__`) / `lookupFunction` (`getattr`). None of that
   graph is visible to a static grep of the Python tree.
5. **`preGameStart`** → `CvScreensInterface.showMainInterface()` → `mainInterface.interfaceScreen()`.
6. **`windowActivation`** → `CvEventManager.onWindowActivation` → **`CvScreensInterface.lateInit()`** on the
   first activation. This is where the deferred screens are built and the screen factories are registered.
   ⚑ **`earlyInit()` builds the factory-owned screens EAGERLY, and that is deliberate** — a screen constructor
   reads the game, so constructing there is what puts those reads on the engine's entry path, where an info
   plane that cannot answer them fails at the MENU as a named Python error. Building them on first use instead
   moves the same failure to an access violation inside the EXE, deep in `interfaceScreen()`, while initializing
   nothing ([the info plane is write-once-at-load](../architecture/patterns.md#-write-once-at-load--a-read-never-creates-and-an-unanswerable-read-fails-loud)). Every screen
   access still goes through `getScreen`, which stays total.

## Where the two sides meet — the marshalling contract

- The DLL calls Python through `Cy::call` / `call_optional` / `call_override` (`Infrastructure/CvPython.h`),
  all bottoming on `gDLL->getPythonIFace()->callFunction`. `call_override` is the map-script protocol: the DLL
  default runs unless the call succeeded AND the script did not signal `allowDefaultImpl()`.
  ⚠ **A failed `call_override` is therefore SILENT** — it falls through to the DLL default. A broken map script
  does not crash; it produces a different map.
- Arguments cross as plain values (int/float/string/wstring/bool/enum-as-int/int-list) **except** the wrapped
  game objects: `DECLARE_PY_WRAPPER` makes `args << pCvCity` construct a `CyCity` BY VALUE and marshal it, and
  `CvGameObject::createPythonWrapper` dispatches the same way for the whole object family. Those types must be
  REGISTERED for the call to complete — see the callout in
  [patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md).
- **XML names Python functions too**: the event/outcome tags (`<PythonCallback>`, `<PythonCanDo>`,
  `<PythonHelp>`, `<PythonExpireCheck>`, `<PythonCanDoCity>`, `<PythonCanDoUnit>`, `<Python>`) are resolved
  against a module the DLL chooses — `CvRandomEventInterface` for the event tags, `CvOutcomeInterface` for the
  unit ones. **A name defined in some other module does not resolve**, however reachable it looks from Python.

## See also
- [patterns.md § THE PYTHON READ BOUNDARY](../architecture/patterns.md) — the design: one library, the
  directional cut, the registration-vs-binding rule, what the library does not own.
- [python-read-map.md](python-read-map.md) · [pedia-read-map.md](pedia-read-map.md) — the demand censuses.
- [engine.md](engine.md) — the frozen toolchain, the dual Boost, and the EXE-bound-symbol test.
