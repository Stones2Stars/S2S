---
name: add-bug-option
description: Add a new user option to the in-game BUG options menu (the BUG /
  BULL settings screens). Use when the user wants to expose a new toggle, number,
  or dropdown in the BUG options UI, make an existing behaviour configurable from
  that menu, or wire a BUG option through to the DLL. Covers the config XML, the
  options-tab Python, the translatable text keys, and the optional C++ read site.
---

# Add an option to the BUG options menu

BUG options are the per-user settings shown in the in-game **BUG Options** screens
(City Screen, Main Interface, Autolog, ACO, …). Each option is declared once in a
config XML, surfaced on a Python options tab, given translatable label/tooltip
text, and — if it must influence DLL logic — read in C++ via `getBugOption*`.

## The four pieces

| Piece | File | Needed when |
|---|---|---|
| 1. Option declaration | `Assets/Config/<BUG Module>.xml` | always |
| 2. UI control | `Assets/Python/BUG/Tabs/Bug<Module>OptionsTab.py` | always (so the user can see/change it) |
| 3. Translatable text | `Assets/XML/GameText/BUG_CIV4GameText.xml` | always (label + hover) |
| 4. DLL read | a `Sources/*.cpp` via `getBugOptionINT`/`getBugOptionBOOL` | only if C++ behaviour depends on it |

Pick the **module** that fits the option's topic (e.g. `CityScreen`,
`MainInterface`). The module's config file is `Assets/Config/BUG <Name>.xml` and
its tab is `Assets/Python/BUG/Tabs/Bug<Name>OptionsTab.py`. Adding to an existing
module is far simpler than creating a new one — prefer it.

## Naming rules (get these exact or the option silently shows blank)

- **Runtime id** = `<ModuleId>__<OptionId>` — a *double* underscore. The `ModuleId`
  is the `id` on the `<options>`/tab `__init__`, e.g. `CityScreen`. Example:
  `CityScreen__ProductionOverflowLimit`.
- **Text keys** = `TXT_KEY_BUG_OPT_` + `<runtime id>`.upper() + `_TEXT` (label) and
  `_HOVER` (tooltip). Example:
  `TXT_KEY_BUG_OPT_CITYSCREEN__PRODUCTIONOVERFLOWLIMIT_TEXT`.
  (Defined in `Assets/Python/BUG/BugOptions.py`: `xmlKey = "TXT_KEY_BUG_OPT_" + id.upper()`.)
  `addTextEdit`/`addCheckbox` pull the visible label from `option.getTitle()`, which
  resolves to these keys — so you do **not** add a separate `addLabel`.

## 1. Declare the option — `Assets/Config/BUG <Module>.xml`

Inside the relevant `<section>`, add an `<option>`. Types: `boolean`, `int`, plus
`<list>` for dropdowns. Minimal int / boolean (no Python getter/setter needed if
only the DLL or generic handler reads it):

```xml
<option id="ProductionOverflowLimit" key="ProductionOverflowLimit" type="int" default="2"/>
<option id="MyToggle" key="MyToggle" type="boolean" default="True"/>
```

- `id` → the `OptionId` half of the runtime id.
- `key` → the INI storage key (kept stable; conventionally same as `id`).
- `default` → the value used until the user changes it. **This default must match
  the default you pass in the DLL read (piece 4).**
- Optional `get="..."`/`set="..."` bind to Python accessor functions; omit them for
  plain DLL-read options (mirror the `BaseWeightFood` options in `BUG City Screen.xml`).

## 2. Surface it on the tab — `Assets/Python/BUG/Tabs/Bug<Module>OptionsTab.py`

In `create()`, attach a control to a panel using the **runtime id**:

```python
self.addTextEdit(screen, LEFT, LEFT, "CityScreen__ProductionOverflowLimit")  # int / text
self.addCheckbox(screen, LEFT, "CityScreen__MyToggle")                        # boolean
self.addTextDropdown(screen, LEFT, LEFT, "CityScreen__MyChoice", True)        # list
```

The label and tooltip render from the text keys (piece 3) automatically. Place the
control near related options; `self.addSpacer(...)`/`self.addLabel(...)` help with
layout (see neighbours in the file). `addLabel` with `bCustom=False` looks up
`TXT_KEY_BUG_OPTLABEL_<NAME>` for section headings.

## 3. Add translatable text — `Assets/XML/GameText/BUG_CIV4GameText.xml`

Add two `<TEXT>` tags (English is required; translators fill the rest later). Place
them next to sibling option text for the same module:

```xml
<TEXT>
    <Tag>TXT_KEY_BUG_OPT_CITYSCREEN__PRODUCTIONOVERFLOWLIMIT_TEXT</Tag>
    <English>Production Overflow Limit</English>
</TEXT>
<TEXT>
    <Tag>TXT_KEY_BUG_OPT_CITYSCREEN__PRODUCTIONOVERFLOWLIMIT_HOVER</Tag>
    <English>Tooltip explaining the option and its default.</English>
</TEXT>
```

`_TEXT` is the on-screen label; `_HOVER` is the mouse-over tooltip.

## 4. Read it in the DLL (only if C++ logic depends on it)

`getBugOptionINT`/`getBugOptionBOOL` are declared in `Sources/CvBugOptions.h`. Call
with the **runtime id** and the **same default** as the XML:

```cpp
#include "CvBugOptions.h"   // already pulled in by most TUs; only add if unresolved

const int iLimit = getBugOptionINT("CityScreen__ProductionOverflowLimit", 2);
const bool bOn  = getBugOptionBOOL("CityScreen__MyToggle", true);
```

- When BUG is active these call Python `getOptionINT/BOOL(id, default)`; otherwise
  they fall back to `GC.getDefineINT/BOOL("BULL__<id>", default)`, then the default.
- These do a Python round-trip per call (`PROFILE_FUNC` guarded). Fine for cold
  paths (turn processing, UI hovers). For a hot path, cache the value at an
  option-refresh point like the `CityScreen__BaseWeight*` reads in
  `CvGlobals.cpp` (~line 3044) instead of calling per use.

> **Multiplayer caveat:** BUG options are **per-user and not network-synced**. Do
> NOT let one drive core synced game state (production, combat results, yields) in
> a multiplayer game — different players' values will desync. Safe for display,
> UI, automation hints, and single-player/experimental tweaks.

## Validate

1. **XML well-formedness** of the two edited XML files (PowerShell):

   ```powershell
   @("C:\code\s2s\s2s\Assets\XML\GameText\BUG_CIV4GameText.xml","C:\code\s2s\s2s\Assets\Config\BUG <Module>.xml") |
     ForEach-Object { try { [xml](Get-Content -Raw -LiteralPath $_ -ErrorAction Stop) | Out-Null; "OK: $_" } catch { "FAIL: $_ -> $($_.Exception.Message)" } }
   ```

   Or `Tools/XmlValidator.exe -a` for the full schema pass.
2. **If you touched `Sources/`**, build with the `build-dll` skill (`Assert build`
   for a compile-check; `Release rebuild deploy` to test in-game). C++2003 only.
3. **In-game:** open the BUG Options screen for the module, confirm the control
   appears with the right label/tooltip and persists across save/reload.

## Worked reference

The `CityScreen__ProductionOverflowLimit` option (commit on branch
`feature/configurable-production-overflow-cap`) wires all four pieces and is a
clean copy-paste template for an int option read in the DLL.
