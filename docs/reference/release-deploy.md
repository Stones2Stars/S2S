# The release deploy pipeline — how a build reaches players

> **Grounding:** `appveyor.yml`, `Tools/CI/DeployBuild.bat`, `Tools/CI/SvnBatchCommit.ps1`.

A release leaves this repo through **two channels**, in order: the SourceForge **SVN trunk** (the channel
players sync from) and a **GitHub distribution repo** mirror. SVN goes first on purpose — a GitHub failure must
not be able to block the proven channel, which is why the GitHub half only logs and continues on error.

Everything below runs from `Tools/CI/DeployBuild.bat`, invoked by `appveyor.yml`'s `deploy_script`, and only on
the `release` branch (`skip_tags` plus the branch filter keep it from looping on its own release tags).

## The sequence

1. **Guard** — abort unless the branch is `release` and this is not a PR build.
2. **Stamp the version** into `Assets/XML/A_New_Dawn_GlobalDefines.xml` (`update-c2c-version.ps1` rewrites the
   `C2C_VERSION` define).
3. **Build** `FinalRelease` and source-index the PDB.
4. **Check out** the SVN trunk into `%build_dir%` (`_build`), retrying up to 25 times with `svn cleanup`
   between attempts — SourceForge checkouts fail intermittently.
5. **Pack the FPKs.** The previously-published FPKs and `fpklive_token.txt` are copied *back* from SVN first, so
   `FpkBuilder.exe` builds a **patch** against them rather than from scratch. This is what keeps the delta small
   for players. A commit message containing `FPKCLEAN` skips the copy and forces a full rebuild.
6. **Stage** the payload into the working copy with `robocopy /MIR`.
7. **Generate changelogs** via `git-chglog`. A temporary git tag is created and pushed purely because the
   changelog generator resolves ranges from origin tags, then deleted again immediately.
8. **Detect** working-copy changes: `svn status` lines starting `!` become `svn delete`, then `svn add --force`.
9. **Commit — in batches.** See below.
10. **Update**, read `svnversion`, and re-tag git with the resulting `SVN-<rev>`.
11. **Mirror** the same payload into the GitHub distribution repo.

## Why the commit is batched

⚖ **It is the BULK push that breaks SourceForge, not a routine release.** A daily commit was never the
problem and still is not — it carries a modest change set and goes up in a handful of batches, or one. What
does not survive as a single transaction is a mass regeneration of `Assets/Data`: **~15000 JSON files at once
is guaranteed to fail.** Read the batching as insurance that only actually binds on the bulk case, never as
evidence the channel was broadly broken.

The failure is a `504 Gateway Time-out` on the trunk, followed by a `500` on the transaction:

```
svn: E175002: Unexpected HTTP status 504 'Gateway Time-out' on '/p/stones2stars/code/trunk'
svn: E175002: Unexpected server error 500 'Internal Server Error' on '/p/stones2stars/code/!svn/txn/...'
```

⚑ **Read where the 504 lands, because it rules out the obvious suspects.** It arrives *after* the client has
printed `Committing transaction...`, which is the **MERGE** request — every file had already been PUT to the
server successfully, so the uploads, the network and the individual files are all exonerated. What times out is
the server FINALIZING a transaction it has already received in full. The `500` on the `!svn/txn/...` URL that
follows is just the client failing to clean up the orphaned transaction afterwards; it is a consequence, not a
second fault. ⛔ So a 504 here is never fixed by retrying harder or by pacing the uploads — nothing is
contending. The only lever is how much work one transaction asks the server to finalize.

`Tools/CI/SvnBatchCommit.ps1` sends the same payload as a sequence of **bounded transactions**, each retried
independently.

⚑ **It is the INTERACTION that kills a commit, not either dimension alone.** A single file always
succeeds however large it is, and a great many small files are survivable; what fails is **many files that are
also large, in one transaction**. That is why a batch is bounded on **both** axes at once and closes on
whichever cap it meets first — the caps exist to keep the payload out of the many-and-large corner, not to
protect any individual file from its own size.

⛔ The corollary, because it reads backwards otherwise: a file bigger than the byte cap is committed **alone,
exceeding the cap**. That is not a hole in the bound — a single file cannot be split, and a lone file is
precisely the case that was never failing. Do not "fix" it.

The knobs live in `appveyor.yml` (`svn_batch_files`, `svn_batch_megabytes`, `svn_batch_retries`) and have
in-script defaults, so the script also runs standalone. **If 504s return, `svn_batch_megabytes` is the lever to
reach for first** — the file count alone was never the thing that broke, so cutting it while leaving the byte
total untouched can leave the failing shape intact.

### Batch ordering — every transaction must be legal on its own

The order is not cosmetic. SVN imposes real constraints on what a partial commit may contain:

1. **Added directories first**, parents before children (a lexicographic sort achieves this, since a parent path
   is a prefix of its children). SVN rejects a commit containing a child whose parent directory neither exists in
   the repository nor is part of the same transaction.
2. **Deletions**, pruned to the top-most deleted path and committed at `--depth infinity`, so one target removes
   its whole subtree in a single cheap operation. Listing the descendants as well is both redundant and a source
   of "path not found" errors once the parent delete has landed.
3. **Everything else** — added, modified and replaced files, plus property-only changes.

Every non-deletion batch commits at **`--depth empty`**. This is load-bearing: without it a directory target
commits its entire subtree recursively, which silently rebuilds the exact giant transaction the batching exists
to prevent.

### Credentials

The script reads `svn_user` / `svn_pass` from the **environment**, which AppVeyor already exports. The password
is therefore never placed on a command line, where it would have to survive both batch-file and PowerShell
quoting.

### Recovering from a timeout that actually succeeded

A 504 can mean *"the server committed the transaction but the gateway gave up relaying the answer"*. Before
resending a failed batch the script runs `svn cleanup`, then `svn update`, then re-checks whether those paths
still have outstanding changes. If they do not, the batch landed and it moves on. Without this check a
successful-but-unacknowledged commit would be retried into an out-of-date failure.

## ⚠ A batched deploy is NOT atomic

This is the real cost of batching, and it cannot be engineered away — SVN has no cross-commit transaction. If
the deploy dies partway, the trunk is left holding **part** of a release, and a player syncing inside that
window gets a mixed tree.

Two things bound the damage:

- **The version stamp and the DLL are held back to the final batch.** A partial trunk therefore does not
  advertise a version whose payload never fully arrived; the version define only moves once everything else is
  up. `SvnBatchCommit.ps1` keeps this list in `$holdBackUntilLastBatch` — **if either path is ever renamed, that
  list must move with it**, and nothing will fail loudly if it does not.
- **Re-running the deploy is the repair.** The working copy is checked out fresh and `svn status` recomputes
  from the server, so an interrupted deploy simply commits the remainder next time. Only the batches that had
  not landed are resent.

The generated changelog is attached to the **final** batch, so the revision carrying the release notes is also
the revision that completes the release.

## Verifying the batcher without running a release

`Tools/CI/Test-SvnBatchCommit.ps1` builds a throwaway repository under `$env:TEMP`, stages a release-shaped
change set against it — nested new directory trees, a whole-directory delete, a lone file delete, a spread of
modifications, an oversized file, a `missing` entry — and runs the batcher over it with deliberately tiny caps
so the ordering rules are actually exercised. It then asserts against the *server*: no revision exceeds the
cap, the hold-back paths landed in the final revision, the changelog rode with it, and the resulting tree is
correct.

```
pwsh -NoProfile -ExecutionPolicy Bypass -File Tools/CI/Test-SvnBatchCommit.ps1
```

Needs `svn.exe` + `svnadmin.exe` on PATH (TortoiseSVN ships both) and touches nothing in the checkout.
**Run it after any change to `SvnBatchCommit.ps1`** — the failure modes here are ordering rules that a green
parse cannot see, and the real channel only exercises them during an actual release.

## Pitfalls

- **The `.vcxproj` files do not drive this** (or any) build — see the root [AGENTS.md](../../AGENTS.md).
- **`svn add --force`** is what picks up new files; anything it misses never reaches the batcher, because the
  batcher only ever commits what `svn status` already reports.
- **Batch count scales with the change set**, not the repo. A routine release is a handful of batches; a
  first-time or `FPKCLEAN` push of the full JSON set is dozens.
