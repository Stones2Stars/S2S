# The Boost 1.32 / 1.55 situation — two Boosts in one binary, and why

> **Status:** reference   ·   **Verified against:** `Sources/fbuild.bff` (include roots `:47-49`, linked lib `:63`), `Sources/CvGameCoreDLL.h` (`BOOST_155_*` macros `:33-34`, `foreach_` alias `:65-69`), `Build/deps/Boost-1.32.0/libs/boost_python-vc71-mt-1_32.lib`, `Build/deps/Boost-{1.32.0,1.55.0}` (re-confirmed 2026-06-20)
> **Grounding:** the build facts (which Boost roots are on `-I`, which Boost `.lib` is linked, Python 2.4)
> come from **`fbuild.bff` only** — the `.vcxproj` is dead for build facts. Site counts (~425 `boost::`,
> ~6 `boost155::`) are carried from the original analysis and **not** re-counted here (flagged below).
> The DLL links **two Boost versions at once** — the original **1.32** and a namespace-renamed **1.55** — and
> they coexist deliberately. This is the **deep home** of the frozen-toolchain / dual-Boost fact that
> [`README.md` §0](../../README.md) summarizes: README states the lock loudly; this doc holds the full why,
> what depends on each, the implications, and what a future consolidation would actually require.

## TL;DR

- **Boost 1.32** (`#include <boost/...>`, `boost::` namespace) — the original BTS/Civ4-era Boost. The bulk of
  Boost use and the one **COMPILED Boost lib that is linked**: `boost_python-vc71-mt-1_32.lib` (Boost.Python —
  the C++↔Python 2.4 bridge), linked in `fbuild.bff:63`.
- **Boost 1.55** (`#include <boost155/...>`, `boost155::` namespace) — added later, **vendored with its
  namespace + config macros renamed `boost`→`boost155` / `BOOST_`→`BOOST_155_`** so it coexists with 1.32 with
  zero clash. Used almost entirely through the **`foreach_` / `reverse_foreach_`** macros
  (`CvGameCoreDLL.h:65-69`, aliased to `BOOST_155_FOREACH` / `BOOST_155_REVERSE_FOREACH`) — the codebase's
  range-for replacement — plus a few direct `boost155::` sites. **Header-only** (`BOOST_155_ALL_NO_LIB`,
  `CvGameCoreDLL.h:34`; no 1.55 `.lib` is built or linked).
- Both include roots are on `-I` (`fbuild.bff:47-49`, **1.55 listed before 1.32**), but there is **no
  shadowing**: 1.32's tree contains only `boost/`, 1.55's only `boost155/` — different paths, different
  namespaces.

So the codebase **straddles both**: 1.32 for general Boost + Boost.Python, 1.55 for `foreach_`.

## The ROOT constraint — the closed `.exe` freezes compiler, Python, AND Boost

None of this is fixable in isolation because **the one thing we cannot rebuild is the Civ4: Beyond the Sword
game `.exe`** (closed, Firaxis). The DLL (`CvGameCoreDLL.dll`) is loaded *into* that `.exe` and must stay
binary-compatible with it. That single immovable fact freezes the whole stack:

- **Compiler — locked to VC7.1 (MSVC 7.1 / VC++ Toolkit 2003).** The `.exe` was built with VC7.1; the DLL
  shares C++ classes / vtables / STL types with it across the process boundary, so the DLL must be built with
  the **same ABI/STL**. A newer MSVC changes name-mangling + STL layout → instant incompatibility. (This is
  why the codebase is genuinely C++03, 32-bit/x86 — a hard compiler limit, not a style choice. See
  [`README.md` §0](../../README.md) and root [`AGENTS.md`](../../../../AGENTS.md).)
- **Python — locked to 2.4.** The `.exe` **embeds the Python 2.4 interpreter** for its script callbacks, and
  the DLL shares that *same* interpreter instance/runtime (`fbuild.bff` includes/links `Python24`). The DLL
  must link the **same Python 2.4** (same C-API ABI). A newer Python (2.7, or 3.x which broke the C-API
  entirely) would mismatch the interpreter the `.exe` already loaded → crash. Python 2.4 is frozen by the
  `.exe`, independently of Boost.
- **Boost — locked by BOTH of the above.** Any Boost we use must (a) **compile under VC7.1** (caps us near
  1.55 — newer Boost drops VC7.1 support), and (b) for **Boost.Python**, link against **Python 2.4**. The
  1.32 Boost.Python lib satisfies both; that's why it survives. Boost is downstream of the compiler + Python
  locks, not a free choice.

**Bottom line:** "use newer Boost" and "use newer Python" are the same blocked move as "use a newer
compiler" — all three are pinned by ABI compatibility with the un-rebuildable `.exe`. We only ever rebuild
the DLL; the lock cannot be lifted in-place. It is a hard boundary on this codebase, not a toolchain bump.

## How they coexist (the rename trick)

The vendored 1.55 is a **renamed Boost**: every `boost` token became `boost155` (the `boost155/` header tree
+ the `boost155::` namespace + `BOOST_155_*` config macros). That makes 1.55 a *different library* to the
compiler/linker — no ODR violation, no symbol collision, no header shadowing against 1.32. It's the standard
"two Boosts in one binary" technique, done by hand/script here. `CvGameCoreDLL.h:33-34` sets
`BOOST_155_USE_WINDOWS_H` + `BOOST_155_ALL_NO_LIB` and `:65-69` aliases `foreach_ → BOOST_155_FOREACH`.

| | Boost 1.32 | Boost 1.55 |
|---|---|---|
| Header path | `<boost/...>` | `<boost155/...>` |
| Namespace | `boost::` | `boost155::` |
| Vendored at | `Build/deps/Boost-1.32.0` | `Build/deps/Boost-1.55.0` |
| Source usage | ~425 `boost::` sites *(count not re-verified)* | `foreach_`/`reverse_foreach_` (everywhere) + ~6 direct `boost155::` *(count not re-verified)* |
| Compiled lib linked | **`boost_python-vc71-mt-1_32.lib`** (Boost.Python) | **none** (header-only, `BOOST_155_ALL_NO_LIB`) |
| Era | 2004 | 2013 |

## ⛔ Why 1.32 cannot just be removed — the Boost.Python blocker

A full "drop 1.32, use only 1.55" was reportedly attempted and **stopped by Python**:

- **Boost.Python is NOT header-only** — it ships as a **compiled library**, and the only one linked is the
  **1.32** build (`boost_python-vc71-mt-1_32.lib`, `fbuild.bff:63`), built for **VC7.1** against **Python
  2.4**.
- The vendored **1.55 has NO Python lib** — it's deliberately header-only (`BOOST_155_ALL_NO_LIB`). The
  moment you need Boost.Python on 1.55, there is nothing to link.
- To remove 1.32 you would have to **build Boost.Python 1.55 for VC7.1 + Python 2.4** — a 2013 Boost compiled
  with a 2003 toolchain against a 2004 Python. That is the wall: VC7.1 support in Boost-of-that-vintage is
  marginal at best, and the toolchain is **locked to VC7.1 to stay ABI-compatible with the closed `.exe`** —
  we can't just move to a newer compiler that 1.55's Boost.Python would prefer.

So 1.32 stays **because Boost.Python 1.32 is the live C++↔Python bridge** and re-creating it on 1.55 is
blocked by the VC7.1 / Python-2.4 constraint. 1.55 was layered on (renamed, header-only) to get newer
header-only features (`foreach`) *without* touching the Python bridge — the pragmatic move that left us with
the dual-Boost reality.

## Implications (the "be aware" list)

1. **Dual mental model.** Contributors must know *which* Boost a feature comes from: `boost::` = 1.32,
   `boost155::` = 1.55. The `boost155` namespace is non-obvious (it IS Boost 1.55, just renamed). `foreach_`
   hides this — most code never types `boost155::`.
2. **Mixed vintages, frozen by the toolchain.** 1.32 (2004) + 1.55 (2013), both bound to VC7.1. Newer Boost
   (post-1.55) generally drops VC7.1 support, so **1.55 is near the ceiling** of what this toolchain can take.
   "Just upgrade Boost" is not available.
3. **Boost.Python is the pinned dependency.** Anything touching the Python bindings is on 1.32's
   Boost.Python; that lib is the single hardest thing to move and the reason the split persists.
4. **PCH name-resolution footgun (already bitten — `CvHttpServer`).** The PCH pulls Boost in transitively and
   surfaces some names at global scope; a bare `bind`/`function` once resolved to `boost::` instead of
   winsock's. With *two* Boosts in play, never `using namespace boost*` and avoid generic identifiers. See
   [`reference/cascade/event-spine.md` §5](../cascade/event-spine.md) — the spine deliberately never *names*
   a Boost type for exactly this reason.
5. **Repo weight.** ~123 MB of vendored Boost headers across `Build/deps/Boost-1.32.0` +
   `Build/deps/Boost-1.55.0`.
6. **It works.** This is messy, not broken — both build clean under FastBuild today. The cost is comprehension
   + the inability to modernize Boost, not correctness.

## What a future consolidation would require (scope — NOT now)

> **Tracked: [Stones2Stars/S2S#442](https://github.com/Stones2Stars/S2S/issues/442)** — consolidate to a
> single Boost (sever the Boost.Python 1.32 pin). Cleanup, not modernization; the VC7.1 / Py2.4 lock stays.

- **The hard part:** produce a **Boost.Python for VC7.1 + Python 2.4 on a chosen single Boost version** (or
  sever the Boost.Python dependency entirely — e.g. a hand-rolled / different C++↔Python layer). Until that
  exists, 1.32 stays.
- Port the ~425 `boost::` sites to the chosen version's API (9 years of Boost API drift between 1.32 and
  1.55).
- Decide the target: (a) **everything on renamed 1.55** (drop 1.32) — blocked by Boost.Python above; (b)
  **drop 1.55, back to 1.32-only** — loses `foreach_` (would need a different range-for shim); (c) **stay
  dual** (today).
- Re-validate against the VC7.1 / ABI lock and the `.exe` compatibility constraint at every step.
- This is a real project with a genuine blocker, not a cleanup — hence: **documented, not done.**

## See also

- [`README.md` §0](../../README.md) — the non-negotiables; states the frozen-toolchain / dual-Boost lock
  loudly and links here as the deep home (this doc holds the full detail it summarizes).
- [`reference/cascade/event-spine.md` §5](../cascade/event-spine.md) — the spine's "no Boost types named"
  rule, the concrete avoidance of the PCH name-resolution footgun (implication 4).
- [`plans/structural-cleanup.md`](../../plans/structural-cleanup.md) — the structural-cleanup roadmap; the
  Boost consolidation (#442) is one of its flagged-but-deferred toolchain/structural items.
- root [`AGENTS.md`](../../../../AGENTS.md) — the toolchain note (the VC7.1 / Python-2.4 / Boost lock) at law
  altitude.
