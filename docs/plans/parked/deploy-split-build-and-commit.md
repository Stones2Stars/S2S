# Parked — split the deploy into separate build and commit scripts

> **Parked:** *"we can consider running build and commit in 2 separate scripts, if that is required, but
> we deal with that if and when."* Batching is good enough; this is **not** open work. It is recorded so the
> idea is not rediscovered from scratch, and not half-built on a hunch.

## The intent

`Tools/CI/DeployBuild.bat` is one script that does everything: stamp the version, compile `FinalRelease`, check
out SVN, pack the FPKs, stage, generate changelogs, **commit in batches**, then mirror to GitHub. The idea is to
cut it at the commit boundary — one script that PRODUCES the payload, a second that PUBLISHES it.

## What would make it worth doing

The commit phase is the only part that talks to a server that can fail for reasons entirely outside this repo.
Everything before it is deterministic local work, and a `FinalRelease` build is ~8 minutes of it.

So the trigger is **the cost of a retry**. Today a batch that fails takes the whole deploy with it, and the
recovery is re-running the build — which redoes those 8 minutes to reach a commit step that may only have had a
few batches left. That is accepted: batch failures are not currently a recurring event, and a re-run is a
one-click operation that resolves it correctly (the working copy is checked out fresh and `svn status`
recomputes from the server, so only what had not landed is resent —
[release-deploy.md](../../reference/release-deploy.md)).

**If SourceForge failures become frequent enough that re-running builds is a real tax, split the scripts** so a
publish can be retried on its own against an already-built payload.

⚑ **The likeliest trigger is the #430 migration release.** Landing the full regenerated `Assets/Data` — over
13000 JSON files — is **guaranteed** to fail as a single transaction, which is precisely what the
batching exists for. But it is also the largest batch count the pipeline will ever run, so it carries the
highest chance that *some* batch fails mid-sequence, and it is where the cost of re-running an 8-minute build
to resume is felt hardest. If the split is ever going to earn itself, that is the release that proves it.

## What NOT to do

- ⛔ **Do not split it as a refactor for tidiness.** The two halves share a great deal of state — `%build_dir%`,
  `%C2C_VERSION%`, the generated `commit_desc.md`, the temporary git tag the changelog generator needs — and
  splitting means that state has to be handed between two processes. That is real work, and it buys nothing
  until retry cost is the actual problem.
- ⛔ **Do not treat a batch failure as evidence the split is needed.** Re-running the build is the sanctioned
  repair. The split becomes justified by the FREQUENCY of failures, never by a single one.
- ⛔ **Do not add pacing or delays between batches on the theory that the server needs to catch up.** The 504
  lands on the MERGE request, after every file has already been uploaded — nothing is contending
  ([release-deploy.md](../../reference/release-deploy.md)).
