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
  see `docs/reference/ai-logging-reference.md`.
- `fbuild.bff` is the source-of-truth for compiled directories (the `.vcxproj` is IDE-only).
  New `Sources/<Dir>/` must be added to `fbuild.bff`'s `.UnityInputPath` (~line 201) **and** the `.vcxproj`(+`.filters`),
  or FastBuild fails at link with `LNK2001` while the IDE compiles fine.
- Full dev bootstrap: `DevSetup.bat`. XML validation: `Tools/XmlValidator.exe -a`.
  Python callbacks: `Tools/XMLTools/verify-python-callbacks.py`.
- See the root `AGENTS.md` for full build details.

## Conventions

- Prefer minimal, local changes in large core files.
- Preserve save compatibility by default; for intentional save breaks, coordinate and mark with `@SAVEBREAK` where relevant.
- If C++ changes affect XML/Python interfaces, validate related XML and callback references.

## Pitfalls

- Build dependencies are legacy and strict (VC++ toolkit + bundled deps in `Build/deps/`); avoid modern compiler assumptions.
- `DevSetup.bat` warns that `Mods/Stones2Stars` can be replaced/symlinked; keep edits in the git workspace.
- Some mod/runtime behavior depends on local tooling and setup scripts; avoid assuming clean-room runtime behavior without validation.

## Reference Docs

- **Developer docs index: [`docs/dev/README.md`](docs/README.md)** — split into
  `docs/reference/` (how the code works today, one note per class/system) and
  `docs/plans/` (refactor scopes, rollouts, removal maps, standing initiatives).
  Player-facing documentation lives separately under the top-level `docs/` folder.
- When you add a dev note: behaviour-as-it-is → `docs/reference/`; intended change → `docs/plans/`.
- Setup flow: `DevSetup.bat`
- CI flow: `appveyor.yml`
- Source formatting policy: `Sources/.editorconfig`
- AI overview: `Sources/Mainpage.dox`
- Save-break coordination notes: `Notes for the next breaking of save game compatability cycle.txt`
