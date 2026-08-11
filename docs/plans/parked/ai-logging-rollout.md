# Plan: roll out the tagged-logging structure across the AI codebase

## Context

The original goal here: extend the tagged-logging model pioneered by `CvWorkerAI`
(`[WAI/*]`), `CvHunterAI` (`[HAI/*]`) and `CvDecisionAI` (`[DAI/*]`) to every AI
surface that lost observability when the legacy free-text BBAI logging
(`logBBAI`/`logAiEvaluations` + `LOG_BBAI_*`/`LOG_EVALAI_*`/`LOG_*_BLOCK`) was deleted
— war, unit dispatch, city production, diplomacy, groups/army, espionage, founding,
combat. One convention, one tag registry, so any log reads the same way.

**This intent has been delivered — by a different mechanism than the one this plan
proposed.** The plan below specified a per-domain `log<Domain>AI` function in
`BetterBTSAI.{h,cpp}` writing via `gDLL->logMsg`, gated by the existing
player/team/city/unit verbosity globals. What actually shipped is the **event
spine** (`docs/specs/event-spine.md`, `docs/reference/observability.md`): every one
of the domains this plan named — war, city, unit, combat, group, founding,
diplomacy, espionage — is registered via `spineRegisterDomain` and rendered through
the spine's own per-domain tag registry, which supersedes the `BetterBTSAI`
per-function approach entirely. The live registry (tag → log file → scope global →
source) lives in `docs/reference/observability.md`, not here.

**What is still un-killed intent, if this convention is ever extended again:** the
underlying design principles this plan established remain worth keeping for any
*future* domain that isn't yet spine-registered —

- A short, readable uppercase tag mnemonic per domain (need not end in `AI`);
  nested detail after a slash (`[XXX/<group>/<detail>]`).
- A shared 3-level verbosity vocabulary, identical across domains so a reader who
  knows one domain's log can parse any other: **1** = headline (begin/end/best/final
  decision), **2** = per-decision (score/candidate/dedup/skip summary), **3** =
  per-candidate trace (cand/per-factor detail/skip detail).
- A shared sub-tag vocabulary before inventing domain-specific ones: `begin`, `end`,
  `baseline`, `cand`, `score`, `skip`, `best`, `decision`/`mission`, `dedup`.
- `begin` lines carry the actor id + turn context (`player=`/`team=`/`unit=`/`city=`
  + `turn=`); keep `k=v` space-separated for grep.

Any new domain should follow these principles but implement them via
`spineRegisterDomain`, not via the `BetterBTSAI`/`gDLL->logMsg` route this plan
originally specified.
