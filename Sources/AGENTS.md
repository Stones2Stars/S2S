# Sources/ Guidelines (C++ DLL)

These instructions apply to code under `Sources/`. For the project-wide guide
(repo layout, build commands, subsystem knowledge, project skills), see the
root `AGENTS.md`.

## Code Style

- Use only C++2003 language features.
- Never use post-C++2003 syntax or library features.
- Do not introduce post-C++2003 syntax (for example: `auto`, lambdas, range-for, `nullptr`, `override`).
- Keep one primary class per file.
- Keep include guards and `#pragma once` for headers.
- Follow formatting/style from `Sources/.editorconfig`.
- **One statement per line; one variable declaration per statement (owner).** Never smash multiple declarations
  or statements onto one line (`std::ostringstream ss; ss << f.rdbuf();` is the banned shape) — code is written
  for the human reading it.
- **No 2-letter or cryptic-abbreviation identifiers (owner)** — no `ss`, `fd`, `cx`, `pg`; names are spelled out
  in full for locals, parameters, members, and enum entries alike (the no-abbreviated-parameters ruling in
  [contexts.md](../docs/architecture/contexts.md), generalized to every identifier).
  **⛔ THE REASON IS ANTI-CONCEALMENT, not style (owner): *"it is not unknown for agents to hide poor
  implementation behind abbreviated variables, that I don't immediately catch."*** An unreadable name defeats
  REVIEW — the owner cannot audit what they cannot read, so the abbreviation is where a weak or wrong structure
  survives unexamined. ⚑ The worked case: a family of `s_op*` file-statics read as one uniform "operate index",
  and a plan doc accordingly described them as one thing to retire wholesale. Spelled out, they were **two
  genuinely different classes** — per-id reverse buckets, and coarse axis-flag lists that are correct as they are
  and must NOT be converged. The names were the only thing hiding that. ⇒ Treat an abbreviated identifier as a
  review-blocker on sight, and rename it before reasoning about the code it names.
  **⚖ THE ONE SANCTIONED ABBREVIATION — a FILE-SCOPE PREFIX, anchored by its own FILE (owner):** *"when the prefix
  is in the name of the file, it makes sense to have the prefix; it does not make sense to have it as a standalone
  collection somewhere."* A short prefix on the file-static helpers of ONE translation unit is legitimate — the
  unity build shares a TU, so file-scope helpers need collision-proofing, and **the FILENAME supplies the
  expansion**, so the reader is never guessing (`gt_` reads as gather because it lives in the gather file, and
  nowhere else). **The test is correspondence:** the prefix abbreviates the file it lives in, and appears in NO
  other file. ⛔ What is banned is the free-floating collection — a prefix naming some *concept* rather than its
  file, so nothing on screen expands it. That is what the renamed operate-index statics were: an `op` family
  inside the enabler-kernel file, anchored to nothing.
- The DLL must remain compliant with the existing build chain.
- Do not update, replace, or modernize the build chain/toolchain.

## Architecture

- `CvGameCoreDLL` is the DLL entrypoint. See `Sources/CvGameCoreDLL.cpp`.
- Core engine classes are generally `Cv*`; Python-facing wrappers are generally `Cy*`.
- AI work typically centers on `CvPlayerAI`, `CvCityAI`, `CvUnitAI`, and `CvTeamAI`.
- For AI overview, see `Sources/Mainpage.dox`.

## Build And Test

- Build entry point is `Tools/_Build.ps1`, run **from `Sources/`**:
  `powershell.exe -NoProfile -ExecutionPolicy Bypass -File "../Tools/_Build.ps1" <Config> <verb...>`.
  Configs: `Assert`/`Debug`/`Release`/`FinalRelease`/`Profile`/`ProfileExtra`; verbs: `clean`/`build`/`rebuild`/`deploy`.
- Quick compile check after editing: `Assert build` (~30s incremental). `MakeDLL*.bat` always rebuild+deploy.
- **`FASSERT`/`FAssertMsg` compile out of `Release` and `FinalRelease`** (only `Assert`/`Debug`/`Testing`
  define `FASSERT_ENABLE`, per `fbuild.bff` — *not* the `.vcxproj`). FinalRelease is the build players
  run, so to verify anything in a FinalRelease run use the gated logging system (`[PERF]` via
  `gPerfLogLevel`/`Autolog__LogLevelPerf`, or a `log<Domain>AI` helper), which ships in every DLL —
  see `docs/reference/observability.md`.
- `fbuild.bff` is the source-of-truth for compiled directories (the `.vcxproj` is IDE-only). **fbuild RECURSIVELY
  globs** every `.cpp` under `$SOURCE_DIR$`, so a new `Sources/<Dir>/` is compiled **automatically — no
  `.UnityInputPath` edit needed** (regen the `.vcxproj`(+`.filters`) for IDE display only).
  With recursive globbing an `LNK2001` means a genuinely **missing definition**, not a missing `.UnityInputPath` entry.
- Full dev bootstrap: `DevSetup.bat`. XML validation: `Tools/XmlValidator.exe -a`.
  Python callbacks: `Tools/XMLTools/verify-python-callbacks.py`.
- See the root `AGENTS.md` for full build details.

## Conventions

- Prefer minimal, local changes in large core files. **"Minimal, local" bounds the SIZE of an edit, NOT the
  SCOPE of the work:** a targeted fix inside a tightly-coupled core file stays minimal — don't sprawl it or
  gratuitously refactor around it — but this is **no brake** on deliberate structural rework (the cascade, the
  docs rebuild, dissolving the `Cv*AI` god-classes), which is large by design and answers to
  [DEC-proper-once](../docs/architecture/decisions.md#dec-proper-once).
- Preserve save compatibility by default; for intentional save breaks, coordinate and mark with `@SAVEBREAK` where relevant.
- If C++ changes affect XML/Python interfaces, validate related XML and callback references.

## Pitfalls

- Build dependencies are legacy and strict (VC++ toolkit + bundled deps in `Build/deps/`); avoid modern compiler assumptions.
- `DevSetup.bat` warns that `Mods/Stones2Stars` can be replaced/symlinked; keep edits in the git workspace.
- Some mod/runtime behavior depends on local tooling and setup scripts; avoid assuming clean-room runtime behavior without validation.

## Reference Docs

- **Developer docs index: [`docs/README.md`](../docs/README.md)** — split into
  `docs/reference/` (how the code works today, one note per class/system) and
  `docs/plans/` (refactor scopes, rollouts, removal maps, standing initiatives).
  Player-facing documentation lives separately under the top-level `docs/` folder.
- When you add a dev note: behaviour-as-it-is → `docs/reference/`; intended change → `docs/plans/`.
- Setup flow: `DevSetup.bat`
- CI flow: `appveyor.yml`
- Source formatting policy: `Sources/.editorconfig`
- AI overview: `Sources/Mainpage.dox`
- Save-break coordination notes: `Notes for the next breaking of save game compatability cycle.txt`
